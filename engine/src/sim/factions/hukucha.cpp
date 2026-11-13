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
//     sugarába, `count` (saját tavon `pond_count`) ebihal kel ki `lifetime` időre, és az akna elfogy.
#include <algorithm>
#include <string>

#include "map/map.hpp"
#include "map/pathfinder.hpp"
#include "sim/faction_module.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

const Fixed kOwnPond = Fixed::from_ratio(13, 10), kOtherPond = Fixed::from_ratio(7, 10);   // `game.json` → pond_speed
constexpr int kDigTicks = 120;      // POND_TIME: 4 s
constexpr int kMineScan = 8;        // az ikra 8 tickenként figyel

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

  void register_components(World& w) override { w.register_component<PondMap>("PondMap"); }

  void on_match_start(Sim& s) override {
    if (!s.map_) return;
    bool any = false;
    for (int i = 1; i <= Sim::kMaxPlayers; ++i) any = any || pond_people(s, static_cast<uint8_t>(i));
    if (!any) return;
    PondMap pm;
    pm.w = s.map_->width();
    pm.h = s.map_->height();
    pm.owner.assign(static_cast<size_t>(pm.w) * pm.h, 0);
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
    return p->owner[static_cast<size_t>(y) * p->w + x];
  }

  static void mark(Sim& s, uint8_t owner, int x0, int y0, int w, int h) {
    PondMap* p = pond(s);
    if (!p || !s.nav_) return;
    const map::TerrainTable& tt = s.nav_->nav().table();
    for (int y = y0; y < y0 + h; ++y) {
      for (int x = x0; x < x0 + w; ++x) {
        if (x < 0 || y < 0 || x >= p->w || y >= p->h) continue;
        const uint16_t tr = s.map_->terrain(x, y);
        if (tr < tt.terrains().size() && !tt.terrains()[tr].buildable) continue;   // csak építhető terepre
        p->owner[static_cast<size_t>(y) * p->w + x] = owner;
      }
    }
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

  // --- horgok --------------------------------------------------------------------------------------------------------
  bool can_place(Sim& s, uint8_t owner, uint16_t def, int tx, int ty, std::string* error) override {
    if (!pond_people(s, owner)) return true;
    const EntityDef& d = s.rules()->defs[def];
    const int w = std::max(1, d.fw), h = std::max(1, d.fh);
    if (pond_tile_def(d)) {   // tavacska: legalább egy csempe még nem saját tó
      for (int y = ty; y < ty + h; ++y) {
        for (int x = tx; x < tx + w; ++x) {
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
    return affinity(s, h->owner) ? kOwnPond : kOtherPond;
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
    mark(s, h->owner, b->tx, b->ty, b->w, b->h);
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

  void tick(Sim& s) override {
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
