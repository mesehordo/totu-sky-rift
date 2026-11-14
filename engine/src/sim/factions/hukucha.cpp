// SPDX-License-Identifier: GPL-3.0-or-later
// Huk’ucha (MECH-POND-1–5: tavacska ásása, építés saját tóra, mozgás tavon, építésgyorsítás, Gyöngyház-pajzs,
// ikrarakás) – M7, ADR-008; a régi játék (G 1493–1508, 1729–1770, 2273–2278, 2425–2467, 1893–1934) és a
// spec/FAJMECHANIKAK.md 3. rész szerint.
//   - a tó csempénkénti tulajdonos (a tótérkép a hash és a mentés része, egy saját komponensben);
//   - tavacska: a `pond_tile` épület (`huk_tavacska`) a Tóásó ásása (4 s); elkészültekor a csempéi saját tóvá válnak,
//     és az épület eltűnik; a főépület a meccs kezdetén tavon áll, 2 csempés peremmel;
//   - a Huk’ucha-épület (a `no_pond` és a tavacska kivételével) minden csempéje saját tó;
//   - bármely tavon álló földi egység: a `pond_affinity` népé × 1,3, másé × 0,7;
//   - építés × (1 + 0,6 × a saját tó aránya az 1 csempés keretben); a `shield_zone: pond` pajzs csak saját tavon töltődik;
//   - ikra-akna (`lay_mine`): a pontra rakott ikra (legfeljebb `max`), 8 tickenként: ha ellenséges földi egység ér a
//     sugarába, `count` (saját tavon `pond_count`) ebihal kel ki `lifetime` időre, és az akna elfogy;
//   - Áradás (`bpower`, `effect: flood`): minden saját tó egy csempével kitágul (4-szomszédság, építhető, nem tó
//     csempékre); a tartam alatt a saját tavon álló sebesült egységek `heal`/s életerőt kapnak; a végén visszahúzódik,
//     kivéve ahol közben épület áll; ismételt használat csak meghosszabbít;
//   - Gyöngyérés (`bpower`, `effect: hasten`): az épület `duration` ideig `mult`-szor gyorsabban kutat és képez;
//   - Tóének (`hukucha.toenek` népi parancs): a munkások a szentély (`Mechanics.pray_slots`) mellé mennek, és ott
//     énekelnek; a bevétel a közös Áhítat-számítás szerint (MECH-FAITH, a faj `faith` adata);
//   - Tavak útja (kutatás `Rules.pond_dmg`): a saját tavon álló saját egység sebzése × `pond_dmg`;
//   - a tavi sebességszorzók a `game.json` `pond_speed` (`own`, `other`) értékei.
#include <algorithm>
#include <array>
#include <utility>
#include <string>

#include "map/map.hpp"
#include "map/pathfinder.hpp"
#include "sim/faction_module.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

constexpr int kDigTicks = 120;      // POND_TIME: 4 s
constexpr int kMineScan = 8;        // az ikra 8 tickenként figyel
constexpr int kSecond = 30;         // tick / s
const Fixed kPrayGap = Fixed::from_ratio(1, 2);   // a Tóéneklő ennyire áll meg a szentély lábnyomától

// a tótérkép: csempénként a tó gazdája (0 = nincs); egyetlen entitáson
struct PondMap {
  int32_t w = 0, h = 0;
  std::vector<uint8_t> owner;
  void hash(core::StateHash& hh) const {
    hh.add_i64(w); hh.add_i64(h);
    hh.add_bytes(std::string_view(reinterpret_cast<const char*>(owner.data()), owner.size()));
  }
  void save(core::ByteWriter& wr) const {
    wr.i32(w); wr.i32(h);
    wr.str(std::string(owner.begin(), owner.end()));
  }
  static PondMap load(core::ByteReader& r) {
    PondMap p;
    p.w = r.i32(); p.h = r.i32();
    const std::string s = r.str();
    p.owner.assign(s.begin(), s.end());
    return p;
  }
};

// Áradás: játékosonként a vége (0 = nincs), a gyógyítás (életerő / s), és a kiáradt csempék (index, gazda)
struct PondFlood {
  std::array<int64_t, Sim::kMaxPlayers + 1> until{};
  std::array<int32_t, Sim::kMaxPlayers + 1> heal{};
  std::vector<std::pair<int32_t, uint8_t>> tiles;
  void hash(core::StateHash& hh) const {
    for (const int64_t u : until) hh.add_i64(u);
    for (const int32_t x : heal) hh.add_i64(x);
    hh.add_u64(tiles.size());
    for (const auto& [i, o] : tiles) { hh.add_i64(i); hh.add_u64(o); }
  }
  void save(core::ByteWriter& wr) const {
    for (const int64_t u : until) wr.i64(u);
    for (const int32_t x : heal) wr.i32(x);
    wr.u32(static_cast<uint32_t>(tiles.size()));
    for (const auto& [i, o] : tiles) { wr.i32(i); wr.u8(o); }
  }
  static PondFlood load(core::ByteReader& r) {
    PondFlood f;
    for (int64_t& u : f.until) u = r.i64();
    for (int32_t& x : f.heal) x = r.i32();
    const uint32_t n = r.u32();
    for (uint32_t k = 0; k < n && r.ok(); ++k) {
      const int32_t i = r.i32();
      const uint8_t o = r.u8();
      f.tiles.emplace_back(i, o);
    }
    return f;
  }
};

// Gyöngyérés: az épület sorának gyorsítása (ezrelékben a többlet, és a tört rész)
struct PondHaste {
  int64_t until = 0;
  int32_t extra = 0, acc = 0;
  void hash(core::StateHash& hh) const { hh.add_i64(until); hh.add_i64(extra); hh.add_i64(acc); }
  void save(core::ByteWriter& wr) const { wr.i64(until); wr.i32(extra); wr.i32(acc); }
  static PondHaste load(core::ByteReader& r) {
    PondHaste h;
    h.until = r.i64(); h.extra = r.i32(); h.acc = r.i32();
    return h;
  }
};

const content::Value* find(const content::Value* v, std::initializer_list<const char*> path) {
  for (const char* k : path) {
    if (!v || !v->is_object()) return nullptr;
    v = v->find(k);
  }
  return v;
}

bool flag(const content::Value* v) { return v && v->is_bool() && v->as_bool(); }

Fixed num(const content::Value* v, Fixed def) {
  if (!v || !v->is_number()) return def;
  const auto f = v->as_fixed();
  return f ? *f : def;
}

class Module final : public FactionModule {
 public:
  const char* name() const override { return "hukucha"; }

  void register_components(World& w) override {
    w.register_component<PondMap>("PondMap");
    w.register_component<PondFlood>("PondFlood");
    w.register_component<PondHaste>("PondHaste");
  }

  void on_match_start(Sim& s) override {
    if (!s.map_) return;
    bool any = false;
    for (int i = 1; i <= Sim::kMaxPlayers; ++i) any = any || pond_people(s, static_cast<uint8_t>(i));
    if (!any) return;
    PondMap pm;
    pm.w = s.map_->width();
    pm.h = s.map_->height();
    pm.owner.assign(static_cast<size_t>(pm.w * pm.h), 0);
    const EntityId e = s.world().create();
    s.world().add(e, pm);
    // a főépület tavon áll, 2 csempés peremmel
    s.world().pool<Building>().each([&](EntityId be, const Building& b) {
      const Health* h = s.world().get<Health>(be);
      if (!h || !pond_people(s, h->owner)) return;
      const FactionDef* f = s.rules()->faction(s.player(h->owner).faction);
      if (!f || b.def != f->hq) return;
      mark(s, h->owner, b.tx - 2, b.ty - 2, b.w + 4, b.h + 4);
    });
  }

  // --- tótérkép -----------------------------------------------------------------------------------------------------
  static PondMap* pond(Sim& s) {
    PondMap* out = nullptr;
    s.world().pool<PondMap>().each([&](EntityId, PondMap& p) { out = &p; });
    return out;
  }

  static uint8_t pond_at(Sim& s, int x, int y) {
    const PondMap* p = pond(s);
    if (!p || x < 0 || y < 0 || x >= p->w || y >= p->h) return 0;
    return p->owner[static_cast<size_t>(y * p->w + x)];
  }

  static EntityId pond_entity(Sim& s) {
    EntityId out = kNoEntity;
    s.world().pool<PondMap>().each([&](EntityId e, PondMap&) { out = e; });
    return out;
  }

  static bool buildable(Sim& s, int x, int y) {
    const map::TerrainTable& tt = s.nav_->nav().table();
    const uint16_t tr = s.map_->terrain(x, y);
    return !(tr < tt.terrains().size() && !tt.terrains()[tr].buildable);
  }

  static void mark(Sim& s, uint8_t owner, int x0, int y0, int w, int h) {
    PondMap* p = pond(s);
    if (!p || !s.nav_) return;
    for (int y = y0; y < y0 + h; ++y) {
      for (int x = x0; x < x0 + w; ++x) {
        if (x < 0 || y < 0 || x >= p->w || y >= p->h) continue;
        if (!buildable(s, x, y)) continue;   // csak építhető terepre
        p->owner[static_cast<size_t>(y * p->w + x)] = owner;
      }
    }
  }

  static Fixed pond_speed(Sim& s, const char* key) {
    const auto& g = s.rules()->game_raw;
    return g ? num(find(g.get(), {"pond_speed", key}), Fixed::from_int(1)) : Fixed::from_int(1);
  }

  static bool pond_people(Sim& s, uint8_t owner) {
    if (owner < 1 || owner > Sim::kMaxPlayers) return false;
    const FactionDef* f = s.rules() ? s.rules()->faction(s.player(owner).faction) : nullptr;
    const content::Value* m = f && f->raw ? find(f->raw.get(), {"Faction", "mechanic"}) : nullptr;
    return m && m->is_string() && m->text() == "pond";
  }

  static bool affinity(Sim& s, uint8_t owner) {
    if (owner < 1 || owner > Sim::kMaxPlayers) return false;
    const FactionDef* f = s.rules()->faction(s.player(owner).faction);
    return f && f->raw && flag(find(f->raw.get(), {"Faction", "pond_affinity"}));
  }

  static bool pond_tile_def(const EntityDef& d) { return d.raw && flag(find(d.raw.get(), {"Legacy", "pond_tile"})); }
  static bool no_pond(const EntityDef& d) { return d.raw && flag(find(d.raw.get(), {"Placement", "no_pond"})); }
  // a tavacska oldalhossza: a kutatás / irány `pond_size` szabálya (a régi G 2436, 2468), különben a lábnyom
  static int pond_side(Sim& s, uint8_t owner, int footprint) {
    const int n = owner >= 1 && owner <= Sim::kMaxPlayers ? s.rules_of(owner).pond_size : 0;
    return n > 0 ? n : footprint;
  }

  // --- horgok --------------------------------------------------------------------------------------------------------
  bool can_place(Sim& s, uint8_t owner, uint16_t def, int tx, int ty, std::string* error) override {
    if (!pond_people(s, owner)) return true;
    const EntityDef& d = s.rules()->defs[def];
    const int w = std::max(1, d.fw), h = std::max(1, d.fh);
    if (pond_tile_def(d)) {   // tavacska: legalább egy csempe még nem saját tó
      const int n = pond_side(s, owner, w);
      if (n > w || n > h) {   // a nagyobb tavacska (`pond_size`) minden csempéje a térképen, építhető terepen
        const PondMap* p = pond(s);
        for (int y = ty; y < ty + n; ++y) {
          for (int x = tx; x < tx + n; ++x) {
            if (!p || x < 0 || y < 0 || x >= p->w || y >= p->h || !buildable(s, x, y)) {
              if (error) *error = "nem építhető terep";
              return false;
            }
          }
        }
      }
      for (int y = ty; y < ty + std::max(h, n); ++y) {
        for (int x = tx; x < tx + std::max(w, n); ++x) {
          if (pond_at(s, x, y) != owner) return true;
        }
      }
      if (error) *error = "itt már tó van";
      return false;
    }
    if (!d.building || no_pond(d) || d.faction != s.player(owner).faction) return true;
    for (int y = ty; y < ty + h; ++y) {
      for (int x = tx; x < tx + w; ++x) {
        if (pond_at(s, x, y) != owner) {
          if (error) *error = "csak saját tóra építhető";
          return false;
        }
      }
    }
    return true;
  }

  Fixed speed_mult(Sim& s, EntityId e) override {
    const Unit* u = s.world().get<Unit>(e);
    const Position* p = s.world().get<Position>(e);
    const Health* h = s.world().get<Health>(e);
    if (!u || !p || !h || s.rules()->defs[u->def].flying) return Fixed::from_int(1);
    if (!pond_at(s, p->x.floor_int(), p->y.floor_int())) return Fixed::from_int(1);
    return pond_speed(s, affinity(s, h->owner) ? "own" : "other");
  }

  // Tavak útja: a saját tavon álló saját egység sebzése × a kész kutatások `Rules.pond_dmg` szorzója
  Fixed damage_mult(Sim& s, EntityId att, EntityId) override {
    const Unit* u = s.world().get<Unit>(att);
    const Position* p = s.world().get<Position>(att);
    const Health* h = s.world().get<Health>(att);
    if (!u || !p || !h || !pond_people(s, h->owner)) return Fixed::from_int(1);
    if (pond_at(s, p->x.floor_int(), p->y.floor_int()) != h->owner) return Fixed::from_int(1);
    Fixed m = Fixed::from_int(1);
    for (const std::string& id : s.player(h->owner).researched) {
      const ResearchDef* r = s.rules()->research(id);
      if (r && r->raw) m = m * num(find(r->raw.get(), {"Rules", "pond_dmg"}), Fixed::from_int(1));
    }
    return m;
  }

  Fixed build_rate_mult(Sim& s, EntityId be) override {
    const Building* b = s.world().get<Building>(be);
    const Health* h = s.world().get<Health>(be);
    if (!b || !h || !pond_people(s, h->owner)) return Fixed::from_int(1);
    const EntityDef& d = s.rules()->defs[b->def];
    if (pond_tile_def(d)) return Fixed::from_ratio(1, kDigTicks);   // az ásás 4 s (az ár a parancskor)
    const FactionDef* f = s.rules()->faction(s.player(h->owner).faction);
    const Fixed bonus = f && f->raw ? num(find(f->raw.get(), {"Faction", "pond_build_bonus"}), Fixed{}) : Fixed{};
    if (bonus.raw() == 0) return Fixed::from_int(1);
    int total = 0, wet = 0;   // az 1 csempés keret
    for (int y = b->ty - 1; y <= b->ty + b->h; ++y) {
      for (int x = b->tx - 1; x <= b->tx + b->w; ++x) {
        if (x >= b->tx && x < b->tx + b->w && y >= b->ty && y < b->ty + b->h) continue;
        ++total;
        wet += pond_at(s, x, y) == h->owner ? 1 : 0;
      }
    }
    return Fixed::from_int(1) + bonus * Fixed::from_ratio(wet, std::max(1, total));
  }

  bool shield_regen_allowed(Sim& s, EntityId e) override {
    const Unit* u = s.world().get<Unit>(e);
    if (!u) return true;
    const EntityDef& d = s.rules()->defs[u->def];
    const content::Value* z = d.raw ? find(d.raw.get(), {"Health", "shield_zone"}) : nullptr;
    if (!z || !z->is_string() || z->text() != "pond") return true;
    const Position& p = *s.world().get<Position>(e);
    return pond_at(s, p.x.floor_int(), p.y.floor_int()) == s.world().get<Health>(e)->owner;
  }

  void on_building_complete(Sim& s, EntityId be) override {   // a tavacska tóvá válik
    const Building* b = s.world().get<Building>(be);
    const Health* h = s.world().get<Health>(be);
    if (!b || !h || !pond_tile_def(s.rules()->defs[b->def])) return;
    const int qw = std::max<int>(b->w, pond_side(s, h->owner, b->w)), qh = std::max<int>(b->h, pond_side(s, h->owner, b->h));
    mark(s, h->owner, b->tx, b->ty, qw, qh);
    if (PondFlood* f = flood_state(s); f && pond(s)) {   // a kiásott tó maradandó: nem húzódik vissza
      const int pw = pond(s)->w;
      std::erase_if(f->tiles, [&](const std::pair<int32_t, uint8_t>& t) {
        const int x = t.first % pw, y = t.first / pw;
        return x >= b->tx && x < b->tx + qw && y >= b->ty && y < b->ty + qh;
      });
    }
    if (s.nav_) s.nav_->remove_obstacle({b->tx, b->ty, b->w, b->h});
    s.world().destroy(be);
  }

  // ikrarakás (`lay_mine`)
  bool use_ability(Sim& s, const std::vector<EntityId>& units, uint16_t ab, Fixed x, Fixed y, EntityId) override {
    const AbilityDef& a = s.rules()->abilities[ab];
    const content::Value* t = a.spec ? a.spec->find("type") : nullptr;
    if (!t || !t->is_string() || t->text() != "lay_mine" || a.unit == kNoDef) return false;
    const int maxn = num(a.spec->find("max"), Fixed::from_int(12)).floor_int();
    for (const EntityId e : units) {
      Caster* c = s.world().get<Caster>(e);
      const Health* h = s.world().get<Health>(e);
      if (!c || !h || s.incapacitated(e) || !s.ability_ready(e, *c, a)) continue;
      int have = 0;
      s.world().pool<Unit>().each([&](EntityId o, const Unit& u) {
        const Health* oh = s.world().get<Health>(o);
        have += u.def == a.unit && oh && oh->owner == h->owner && oh->hp > 0 ? 1 : 0;
      });
      if (have >= maxn) return false;   // egyszerre legfeljebb ennyi
      s.ability_spend(*c, ab, a);
      const uint8_t owner = h->owner;
      const EntityId m = s.spawn_unit(a.unit, owner, x, y);
      if (m != kNoEntity) s.place_at_free(m, x, y);
      return true;
    }
    return false;
  }

  // --- épületképességek (bpower) --------------------------------------------------------------------------------------
  static PondFlood* flood_state(Sim& s) {
    PondFlood* out = nullptr;
    s.world().pool<PondFlood>().each([&](EntityId, PondFlood& f) { out = &f; });
    return out;
  }

  static std::string effect_of(const AbilityDef& a) {
    if (!a.effect.empty()) return a.effect;
    const content::Value* ef = a.spec ? a.spec->find("effect") : nullptr;
    return ef && ef->is_string() ? ef->text() : std::string();
  }

  bool use_power(Sim& s, uint8_t owner, EntityId b, uint16_t ab, Fixed, Fixed) override {
    const AbilityDef& a = s.rules()->abilities[ab];
    const std::string ef = effect_of(a);
    if (ef == "flood") return flood(s, owner, a);
    if (ef == "hasten") {
      const Fixed mult = num(a.spec ? a.spec->find("mult") : nullptr, Fixed::from_int(2));
      const auto extra = static_cast<int32_t>((int64_t{(mult - Fixed::from_int(1)).raw()} * 1000) >> 16);
      if (extra <= 0 || !s.world().get<Producer>(b)) return false;
      PondHaste* h = s.world().get<PondHaste>(b);
      if (!h) h = &s.world().add(b, PondHaste{});
      h->until = s.tick() + a.duration;
      h->extra = extra;
      return true;
    }
    return false;
  }

  static bool flood(Sim& s, uint8_t owner, const AbilityDef& a) {
    PondMap* p = pond(s);
    if (!p || !s.nav_ || owner < 1 || owner > Sim::kMaxPlayers) return false;
    PondFlood* f = flood_state(s);
    if (!f) f = &s.world().add(pond_entity(s), PondFlood{});
    const bool active = f->until[owner] > s.tick();
    f->until[owner] = s.tick() + a.duration;   // ismételt használat: csak meghosszabbít
    f->heal[owner] = a.heal.floor_int();
    if (active) return true;
    constexpr int kDx[4] = {1, -1, 0, 0}, kDy[4] = {0, 0, 1, -1};
    std::vector<int32_t> grow;
    for (int y = 0; y < p->h; ++y) {
      for (int x = 0; x < p->w; ++x) {
        if (p->owner[static_cast<size_t>(y * p->w + x)] != owner) continue;
        for (int k = 0; k < 4; ++k) {
          const int nx = x + kDx[k], ny = y + kDy[k];
          if (nx < 0 || ny < 0 || nx >= p->w || ny >= p->h) continue;
          if (p->owner[static_cast<size_t>(ny * p->w + nx)] != 0 || !buildable(s, nx, ny)) continue;
          grow.push_back(ny * p->w + nx);
        }
      }
    }
    std::sort(grow.begin(), grow.end());
    grow.erase(std::unique(grow.begin(), grow.end()), grow.end());
    for (const int32_t i : grow) {
      p->owner[static_cast<size_t>(i)] = owner;
      f->tiles.emplace_back(i, owner);
    }
    return true;
  }

  // az áradás vége (visszahúzódás, kivéve az épület alatt) és a gyógyítás
  static void flood_tick(Sim& s) {
    PondFlood* f = flood_state(s);
    PondMap* p = pond(s);
    if (!f || !p) return;
    for (size_t o = 1; o <= size_t{Sim::kMaxPlayers}; ++o) {
      const auto owner = static_cast<uint8_t>(o);
      if (f->until[o] == 0) continue;
      if (s.tick() >= f->until[o]) {
        std::vector<uint8_t> built(p->owner.size(), 0);
        s.world().pool<Building>().each([&](EntityId, const Building& b) {
          for (int y = std::max(0, int{b.ty}); y < std::min(p->h, b.ty + b.h); ++y) {
            for (int x = std::max(0, int{b.tx}); x < std::min(p->w, b.tx + b.w); ++x) built[static_cast<size_t>(y * p->w + x)] = 1;
          }
        });
        for (const auto& [i, to] : f->tiles) {
          const auto k = static_cast<size_t>(i);
          if (to == owner && !built[k] && p->owner[k] == owner) p->owner[k] = 0;
        }
        std::erase_if(f->tiles, [&](const std::pair<int32_t, uint8_t>& t) { return t.second == owner; });
        f->until[o] = 0;
        f->heal[o] = 0;
        continue;
      }
      if (s.tick() % kSecond != 0 || f->heal[o] <= 0) continue;
      s.world().pool<Unit>().each([&](EntityId e, const Unit& u) {
        Health* h = s.world().get<Health>(e);
        const Position* pos = s.world().get<Position>(e);
        if (!h || !pos || h->owner != owner || h->hp <= 0 || h->hp >= h->max || s.rules()->defs[u.def].flying) return;
        if (pond_at(s, pos->x.floor_int(), pos->y.floor_int()) != owner) return;
        h->hp = std::min(h->max, h->hp + f->heal[o]);
      });
    }
  }

  // Gyöngyérés: a sor elejének többlethaladása (a termelési rendszer után)
  static void haste_tick(Sim& s) {
    std::vector<EntityId> done;
    s.world().pool<PondHaste>().each([&](EntityId e, PondHaste& h) {
      if (s.tick() >= h.until) {
        done.push_back(e);
        return;
      }
      Producer* pr = s.world().get<Producer>(e);
      const Building* b = s.world().get<Building>(e);
      const Health* hp = s.world().get<Health>(e);
      if (!pr || !b || !hp || !b->complete || hp->hp <= 0 || pr->queue.empty()) return;
      h.acc += h.extra;
      QueueItem& q = pr->queue.front();
      while (h.acc >= 1000) {
        h.acc -= 1000;
        q.progress = std::min(q.time, q.progress + 1);
      }
    });
    for (const EntityId e : done) s.world().pool<PondHaste>().remove(e);
  }

  // --- Tóének (népi parancs: `units` munkások → `target` szentély) -------------------------------------------------
  bool on_command(Sim& s, const Command& c) override {
    if (c.def != "hukucha.toenek") return false;
    const Building* b = s.world().get<Building>(c.target);
    const Health* bh = s.world().get<Health>(c.target);
    if (!b || !bh || !b->complete || bh->hp <= 0 || bh->owner != c.owner) return true;
    const EntityDef& d = s.rules()->defs[b->def];
    if (!d.raw || num(find(d.raw.get(), {"Mechanics", "pray_slots"}), Fixed{}).floor_int() <= 0) return true;
    const FactionDef* f = c.owner >= 1 && c.owner <= Sim::kMaxPlayers ? s.rules()->faction(s.player(c.owner).faction) : nullptr;
    if (!f || !f->raw || !find(f->raw.get(), {"Faction", "faith"})) return true;
    const Fixed x0 = Fixed::from_int(b->tx) - kPrayGap, x1 = Fixed::from_int(b->tx + b->w) + kPrayGap;
    const Fixed y0 = Fixed::from_int(b->ty) - kPrayGap, y1 = Fixed::from_int(b->ty + b->h) + kPrayGap;
    for (const EntityId e : c.units) {
      const Health* h = s.world().get<Health>(e);
      const Position* p = s.world().get<Position>(e);
      if (!h || !p || h->owner != c.owner || h->hp <= 0 || !s.world().get<Worker>(e)) continue;
      // a lábnyom körüli keret legközelebbi pontja
      Fixed tx = std::clamp(p->x, x0, x1), ty = std::clamp(p->y, y0, y1);
      if (tx > x0 && tx < x1 && ty > y0 && ty < y1) {   // a lábnyomon belül: a legközelebbi oldalra
        const Fixed dl = tx - x0, dr = x1 - tx, du = ty - y0, dd = y1 - ty;
        const Fixed m = std::min({dl, dr, du, dd});
        if (m == dl) tx = x0;
        else if (m == dr) tx = x1;
        else if (m == du) ty = y0;
        else ty = y1;
      }
      s.stop(e);
      s.order_move(e, tx, ty);
    }
    return true;
  }

  void tick(Sim& s) override {
    flood_tick(s);
    haste_tick(s);
    if (s.tick() % kMineScan != 0) return;
    std::vector<EntityId> mines;
    s.world().pool<Unit>().each([&](EntityId e, const Unit& u) {
      const EntityDef& d = s.rules()->defs[u.def];
      if (d.raw && find(d.raw.get(), {"Legacy", "mine_trigger"})) mines.push_back(e);
    });
    for (const EntityId e : mines) {
      if (!s.world().alive(e)) continue;
      const Health* h = s.world().get<Health>(e);
      if (!h || h->hp <= 0) continue;
      const EntityDef& d = s.rules()->defs[s.world().get<Unit>(e)->def];
      const content::Value* mt = find(d.raw.get(), {"Legacy", "mine_trigger"});
      const Fixed radius = num(mt->find("radius"), Fixed::from_int(1));
      const Position p = *s.world().get<Position>(e);
      const uint8_t owner = h->owner;
      std::vector<EntityId> foes;
      for (const EntityId o : s.enemy_units_in(owner, p.x, p.y, radius)) {
        if (!s.rules()->defs[s.world().get<Unit>(o)->def].flying) foes.push_back(o);
      }
      if (foes.empty()) continue;
      const content::Value* sp = mt->find("spawn");
      const uint16_t spawn = sp && sp->is_string() ? s.rules()->def(sp->text()) : kNoDef;
      const bool wet = pond_at(s, p.x.floor_int(), p.y.floor_int()) == owner;
      const int n = static_cast<int>(num(mt->find(wet ? "pond_count" : "count"), Fixed::from_int(3)).floor_int());
      const int32_t life = seconds_to_ticks(num(mt->find("lifetime"), Fixed::from_int(10)));
      s.kill(e);   // az akna elfogy
      for (int k = 0; k < n && spawn != kNoDef; ++k) {
        const EntityId u = s.spawn_unit(spawn, owner, p.x, p.y);
        if (u == kNoEntity) continue;
        s.place_at_free(u, p.x, p.y);
        s.world().add(u, Lifetime{s.tick() + life});
        s.order_attack(u, foes.front());
      }
    }
  }
};

}  // namespace

std::unique_ptr<FactionModule> make_hukucha_module() { return std::make_unique<Module>(); }

}  // namespace totu::sim
