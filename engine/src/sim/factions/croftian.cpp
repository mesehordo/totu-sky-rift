// SPDX-License-Identifier: GPL-3.0-or-later
// Croftianok (MECH-CREEP-1–8: a mélység terjedése, építés csak mélységre, mozgás és gyógyulás, rothadás, fertőzés,
// tüskék; átváltozás) – M7, ADR-008; a régi játék (G 754, 797, 1235–1240, 1781–1815, 2294, 2470–2580, 3003–3032;
// E 191–197, 387, 1674) és a spec/FAJMECHANIKAK.md 2. része szerint.
//   - a mélység: minden saját épület `Mechanics.creep_radius` körében; kész épületnél a sugár másodpercenként 0,7-del
//     nő a teljesig (épülőnél 1 + fél méret), csak építhető terepen; játékosonként külön mező (a komponensekből
//     származtatva – a hash és a mentés a sugarakat tartalmazza, a mező újraszámolható);
//   - építés csak saját mélységre (a `no_creep` épület kivétel); a mélységen a földi egység `creep_speed_mult` (1,3)
//     szorosan gyors, a gyógyulás `creep_regen_mult` (3) × a szabály `creep_regen_mult` (kutatás, irány) szoros – a
//     szorzók a nép adatából, különben a `game.json`-ból;
//   - a mélységen álló fák a főépülettől mért távolság szerint rothadnak (fa-bevétel, másodpercenként legfeljebb 6,5);
//   - fertőzés: a croftian ölés után a mélységen 30%-kal 6 s múlva Karmos kel ki;
//   - mérgező tüske: nem akadály; a rajta álló, nem tüske-immunis földi egység lassul és mérgeződik; a tűz- és
//     méregfegyver teljesen sebzi, más × 0,2 (`Legacy.spike_taken`), a nem tüske-immunis támadó megmérgeződik;
//   - `summons_permanent`: az idézések nem járnak le (hősönként legfeljebb 4: a legrégebbi pusztul);
//   - `spell_resource: hp`: a képesség energiája életerőből fizet (1 energia = 1 életerő; legalább 1 marad);
//   - átváltozás (`morph`): a költségért gubó lesz, `time` s múlva az új egység.
#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "map/map.hpp"
#include "map/pathfinder.hpp"
#include "sim/faction_module.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

// --- komponensek (a hash és a mentés része) ---------------------------------------------------------------------------
struct CroCreep {   // a croftian épület mélységének sugara; a lárvatartó épület (Kaptár) lárvái és a visszanövés ideje
  Fixed r;
  int32_t larva = 0;
  int32_t larva_t = 0;   // tick
  void hash(core::StateHash& h) const { h.add_i64(r.raw()); h.add_i64(larva); h.add_i64(larva_t); }
  void save(core::ByteWriter& w) const { w.i32(r.raw()); w.i32(larva); w.i32(larva_t); }
  static CroCreep load(core::ByteReader& rd) {
    CroCreep c;
    c.r = Fixed::from_raw(rd.i32()); c.larva = rd.i32(); c.larva_t = rd.i32();
    return c;
  }
};

struct CroRot {   // a rothadó fa tört vesztesége (ezred)
  int32_t milli = 0;
  void hash(core::StateHash& h) const { h.add_i64(milli); }
  void save(core::ByteWriter& w) const { w.i32(milli); }
  static CroRot load(core::ByteReader& rd) { CroRot c; c.milli = rd.i32(); return c; }
};

struct CroPlayer {   // játékosonként (egy saját entitáson): a rothadás tört bevétele (ezred)
  uint8_t owner = 0;
  int32_t rot_milli = 0;
  void hash(core::StateHash& h) const { h.add_u64(owner); h.add_i64(rot_milli); }
  void save(core::ByteWriter& w) const { w.u8(owner); w.i32(rot_milli); }
  static CroPlayer load(core::ByteReader& rd) { CroPlayer c; c.owner = rd.u8(); c.rot_milli = rd.i32(); return c; }
};

struct CroHatch {   // kikelő fertőzés (a helye a Position)
  uint8_t owner = 0;
  uint16_t def = kNoDef;
  int64_t at = 0;
  void hash(core::StateHash& h) const { h.add_u64(owner); h.add_u64(def); h.add_i64(at); }
  void save(core::ByteWriter& w) const { w.u8(owner); w.u32(def); w.i64(at); }
  static CroHatch load(core::ByteReader& rd) {
    CroHatch c;
    c.owner = rd.u8(); c.def = static_cast<uint16_t>(rd.u32()); c.at = rd.i64();
    return c;
  }
};

struct CroSummon {   // állandóvá tett idézés: az idéző hős (0: nincs a közelben), és mikor
  EntityId hero = 0;
  int64_t since = 0;
  void hash(core::StateHash& h) const { h.add_u64(hero); h.add_i64(since); }
  void save(core::ByteWriter& w) const { w.u32(hero); w.i64(since); }
  static CroSummon load(core::ByteReader& rd) { CroSummon c; c.hero = rd.u32(); c.since = rd.i64(); return c; }
};

struct CroBlood {   // életerőből fizető varázsló: a legutóbb beállított energia (ezred)
  int32_t set = 0;
  void hash(core::StateHash& h) const { h.add_i64(set); }
  void save(core::ByteWriter& w) const { w.i32(set); }
  static CroBlood load(core::ByteReader& rd) { CroBlood c; c.set = rd.i32(); return c; }
};

struct CroCocoon {   // átváltozó gubó: ebből lesz, ekkor
  uint16_t into = kNoDef;
  int64_t until = 0;
  void hash(core::StateHash& h) const { h.add_u64(into); h.add_i64(until); }
  void save(core::ByteWriter& w) const { w.u32(into); w.i64(until); }
  static CroCocoon load(core::ByteReader& rd) {
    CroCocoon c;
    c.into = static_cast<uint16_t>(rd.u32()); c.until = rd.i64();
    return c;
  }
};

// --- adat ---------------------------------------------------------------------------------------------------------------
const content::Value* faction_obj(const FactionDef& f) { return f.raw ? f.raw->find("Faction") : nullptr; }

Fixed num(const content::Value* v, Fixed def) {
  if (!v) return def;
  const auto f = v->as_fixed();
  return f ? *f : def;
}

bool flag(const content::Value* v) { return v && v->is_bool() && v->as_bool(); }

const content::Value* path2(const content::Value* v, const char* a, const char* b) {
  const content::Value* x = v ? v->find(a) : nullptr;
  return x && b ? x->find(b) : x;
}

// kód-állandók (a régi játék): a sugár növekedése, a rothadás alapkerete, a fertőzés késése, hősönként az állandó
// idézések száma; a mélységi szorzók és a tüske sebzésszorzójának tartaléka (ha sem a nép, sem a `game.json` nem adja)
const Fixed kCreepSpeedDefault = Fixed::from_ratio(13, 10);
const Fixed kCreepRegenDefault = Fixed::from_int(3);
const Fixed kSpikeTakenDefault = Fixed::from_ratio(1, 5);
constexpr int kSummonsPerHero = 4;
const Fixed kGrow = Fixed::from_ratio(7, 10);
const Fixed kRotCap = Fixed::from_ratio(13, 2);   // 6,5 fa / s
constexpr int32_t kInfestDelayDefault = 6;

// a nép kulcsa, különben a `game.json` azonos kulcsa, különben a tartalék
Fixed tuning(Sim& s, const FactionDef* f, const char* key, Fixed def) {
  if (const content::Value* v = f ? path2(faction_obj(*f), key, nullptr) : nullptr) return num(v, def);
  const content::Value* g = s.rules()->game_raw.get();
  return num(g ? g->find(key) : nullptr, def);
}

// a játékos szabályszorzója: a kész kutatások (`Rules`) és a választott irány (`Paths.<irány>.rules`) szorzata
Fixed rule_mult(Sim& s, uint8_t owner, const FactionDef* f, const char* key) {
  Fixed m = Fixed::from_int(1);
  const PlayerState& p = s.player(owner);
  for (const std::string& id : p.researched) {
    const ResearchDef* r = s.rules()->research(id);
    if (const content::Value* v = r && r->raw ? path2(r->raw.get(), "Rules", key) : nullptr) m = m * num(v, Fixed::from_int(1));
  }
  if (!p.path.empty() && f && f->raw) {
    const content::Value* pr = path2(f->raw.get(), "Paths", p.path.c_str());
    if (const content::Value* v = pr ? path2(pr, "rules", key) : nullptr) m = m * num(v, Fixed::from_int(1));
  }
  return m;
}

// a játékos szabálya (a régi `p.rules.get`): a kész kutatások, majd az irány – az utolsó érvényes (felülírás)
const content::Value* rule_val(Sim& s, uint8_t owner, const char* key) {
  if (owner < 1 || owner > Sim::kMaxPlayers || !s.rules()) return nullptr;
  const content::Value* out = nullptr;
  const PlayerState& p = s.player(owner);
  for (const std::string& id : p.researched) {
    const ResearchDef* r = s.rules()->research(id);
    if (const content::Value* v = r && r->raw ? path2(r->raw.get(), "Rules", key) : nullptr) out = v;
  }
  if (!p.path.empty()) {
    const FactionDef* f = s.rules()->faction(p.faction);
    const content::Value* pr = f && f->raw ? path2(f->raw.get(), "Paths", p.path.c_str()) : nullptr;
    if (const content::Value* v = pr ? path2(pr, "rules", key) : nullptr) out = v;
  }
  return out;
}

// a címkés szabály vonatkozik-e a sablonra (a régi `rule_tags`: címke nélkül mindenre; a címke vagy az azonosító)
bool rule_applies(const content::Value* r, const EntityDef& d) {
  if (!r || !r->is_object()) return false;
  const content::Value* tags = r->find("tags");
  if (!tags || !tags->is_array() || tags->items().empty()) return true;
  for (const content::Value& t : tags->items()) {
    if (t.is_string() && (d.has_tag(t.text()) || d.id == t.text())) return true;
  }
  return false;
}

bool is_spike(const EntityDef& d) { return d.raw && flag(path2(d.raw.get(), "Legacy", "spike_tile")); }

// a lárvatartó épület adata (`Producer.larva`, `Producer.larva_time`)
int32_t larva_of(const EntityDef& d) {
  const content::Value* v = d.raw ? path2(d.raw.get(), "Producer", "larva") : nullptr;
  return v ? static_cast<int32_t>(v->as_int().value_or(0)) : 0;
}

// kód-állandók (a régi `SPIKE`): a tüskék alapkorlátja, a lassítás szorzója (0,6) és a méreg (4 / s)
constexpr int32_t kSpikeMaxDefault = 16;
const Fixed kSpikeSlow = Fixed::from_ratio(3, 5);
constexpr int32_t kSpikeDps = 4000;   // ezred / s

class Module final : public FactionModule {
 public:
  const char* name() const override { return "croftian"; }

  void register_components(World& w) override {
    w.register_component<CroCreep>("CroCreep");
    w.register_component<CroRot>("CroRot");
    w.register_component<CroPlayer>("CroPlayer");
    w.register_component<CroHatch>("CroHatch");
    w.register_component<CroCocoon>("CroCocoon");
    w.register_component<CroSummon>("CroSummon");
    w.register_component<CroBlood>("CroBlood");
  }

  // --- a mező -----------------------------------------------------------------------------------------------------
  const FactionDef* fdef(Sim& s, uint8_t owner) {
    if (owner < 1 || owner > Sim::kMaxPlayers || !s.rules()) return nullptr;
    return s.rules()->faction(s.player(owner).faction);
  }

  bool faction_flag(Sim& s, uint8_t owner, const char* key) {
    const FactionDef* f = fdef(s, owner);
    return f && flag(path2(faction_obj(*f), key, nullptr));
  }

  bool creep_faction(Sim& s, uint8_t owner) {
    if (owner < 1 || owner > Sim::kMaxPlayers || !s.rules()) return false;
    const FactionDef* f = s.rules()->faction(s.player(owner).faction);
    const content::Value* fo = f ? faction_obj(*f) : nullptr;
    const content::Value* m = fo ? fo->find("mechanic") : nullptr;
    return m && m->is_string() && m->text() == "creep";
  }

  int creep_radius_of(Sim& s, uint16_t def) {
    const EntityDef& d = s.rules()->defs[def];
    const content::Value* v = d.raw ? path2(d.raw.get(), "Mechanics", "creep_radius") : nullptr;
    return v ? static_cast<int>(v->as_int().value_or(0)) : 0;
  }

  void rebuild(Sim& s) {
    fields_.clear();
    dirty_ = false;
    map::Map* m = s.map_;
    if (!m || !s.nav_) return;
    const auto& terrains = s.nav_->nav().table().terrains();
    const int W = m->width(), H = m->height();
    s.world().pool<CroCreep>().each([&](EntityId e, const CroCreep& cc) {
      const Building* b = s.world().get<Building>(e);
      const Health* h = s.world().get<Health>(e);
      if (!b || !h || h->hp <= 0 || !creep_faction(s, h->owner)) return;
      auto& f = fields_[h->owner];
      if (f.empty()) f.assign(static_cast<size_t>(W * H), 0);
      const Fixed half = Fixed::from_ratio(std::max(b->w, b->h), 2);
      const Fixed r = b->complete ? cc.r + half : Fixed::from_int(1) + half;
      const Fixed cx = Fixed::from_int(b->tx) + Fixed::from_ratio(b->w, 2), cy = Fixed::from_int(b->ty) + Fixed::from_ratio(b->h, 2);
      const int ri = r.floor_int() + 1;
      for (int y = std::max(0, cy.floor_int() - ri); y <= std::min(H - 1, cy.floor_int() + ri); ++y) {
        for (int x = std::max(0, cx.floor_int() - ri); x <= std::min(W - 1, cx.floor_int() + ri); ++x) {
          const Fixed dx = Fixed::from_int(x) + Fixed::from_ratio(1, 2) - cx, dy = Fixed::from_int(y) + Fixed::from_ratio(1, 2) - cy;
          if (core::length(dx, dy) > r) continue;
          const uint16_t t = m->terrain(x, y);
          if (t >= terrains.size() || !terrains[t].buildable) continue;
          f[static_cast<size_t>(y * W + x)] = 1;
        }
      }
    });
  }

  bool on_creep(Sim& s, uint8_t owner, int x, int y) {
    if (dirty_) rebuild(s);
    const auto it = fields_.find(owner);
    if (it == fields_.end() || !s.map_) return false;
    if (x < 0 || y < 0 || x >= s.map_->width() || y >= s.map_->height()) return false;
    return it->second[static_cast<size_t>(y * s.map_->width() + x)] != 0;
  }

  bool unit_on_own_creep(Sim& s, EntityId e) {
    const Health* h = s.world().get<Health>(e);
    const Position* p = s.world().get<Position>(e);
    if (!h || !p || !creep_faction(s, h->owner)) return false;
    return on_creep(s, h->owner, p->x.floor_int(), p->y.floor_int());
  }

 public:
  // a próbáknak: a játékos mélységcsempéinek száma, és egy csempe
  int creep_tiles(Sim& s, uint8_t owner) {
    if (dirty_) rebuild(s);
    const auto it = fields_.find(owner);
    if (it == fields_.end()) return 0;
    int n = 0;
    for (const uint8_t c : it->second) n += c;
    return n;
  }

  bool creep_at(Sim& s, uint8_t owner, int x, int y) { return on_creep(s, owner, x, y); }

  // --- horgok -------------------------------------------------------------------------------------------------------
  void on_building_placed(Sim& s, EntityId e) override {
    const Building* b = s.world().get<Building>(e);
    const Health* h = s.world().get<Health>(e);
    if (!b || !h) return;
    const EntityDef& d = s.rules()->defs[b->def];
    // mérgező tüske: nem akadály (a régi `spike_tile`)
    if (is_spike(d) && s.nav_) s.nav_->remove_obstacle({b->tx, b->ty, b->w, b->h});
    const int cr = creep_radius_of(s, b->def);
    if (cr <= 0 || !creep_faction(s, h->owner)) return;
    s.world().add(e, CroCreep{b->complete ? Fixed::from_int(cr) : Fixed::from_int(2), b->complete ? larva_of(d) : 0, 0});
    dirty_ = true;
  }

  void on_building_complete(Sim& s, EntityId e) override {
    dirty_ = true;
    const Building* b = s.world().get<Building>(e);
    if (!b) return;
    if (CroCreep* cc = s.world().get<CroCreep>(e)) cc->larva = larva_of(s.rules()->defs[b->def]);   // a régi: kész → teli
    if (is_spike(s.rules()->defs[b->def])) lay_spikes(s, e);
  }

  // --- lárvák (a régi E 1671, 1738–1747; G 3519–3539, 3613): a lárvatartó épület csak szabad lárvával képez --------
  bool can_train(Sim& s, EntityId b, uint16_t) override {
    const CroCreep* cc = s.world().get<CroCreep>(b);
    const Building* bb = s.world().get<Building>(b);
    if (!cc || !bb || larva_of(s.rules()->defs[bb->def]) <= 0) return true;
    return cc->larva > 0;
  }

  void on_train(Sim& s, EntityId b, uint16_t) override {
    CroCreep* cc = s.world().get<CroCreep>(b);
    const Building* bb = s.world().get<Building>(b);
    if (cc && bb && larva_of(s.rules()->defs[bb->def]) > 0 && cc->larva > 0) --cc->larva;
  }

  void on_train_cancel(Sim& s, EntityId b, uint16_t) override {
    CroCreep* cc = s.world().get<CroCreep>(b);
    const Building* bb = s.world().get<Building>(b);
    if (cc && bb && larva_of(s.rules()->defs[bb->def]) > 0) ++cc->larva;
  }

  // a címkés egység képzési ideje × `train_time_mult.mult` (a régi G 3541; pl. a Raj iránya)
  Fixed train_time_mult(Sim& s, uint8_t owner, uint16_t def) override {
    const content::Value* r = rule_val(s, owner, "train_time_mult");
    if (!rule_applies(r, s.rules()->defs[def])) return Fixed::from_int(1);
    return num(r->find("mult"), Fixed::from_int(1));
  }

  // --- beásás (a régi `burrow` kapcsoló, E 305, 730; G 1294): a beásott egység rejtett és tétlen ---------------------
  bool burrowed(Sim& s, EntityId e) {
    const Caster* c = s.world().get<Caster>(e);
    if (!c || c->toggles.empty()) return false;
    for (const uint16_t a : c->toggles) {
      const content::Value* sp = s.rules()->abilities[a].spec.get();
      if (sp && flag(sp->find("burrow"))) return true;
    }
    return false;
  }

  bool hidden(Sim& s, EntityId e) override { return burrowed(s, e); }
  bool inert(Sim& s, EntityId e) override { return burrowed(s, e); }
  void on_match_start(Sim& s) override {
    for (int i = 1; i <= Sim::kMaxPlayers; ++i) {
      if (!s.player(i).active || !creep_faction(s, static_cast<uint8_t>(i))) continue;
      const EntityId pe = s.world().create();
      s.world().add(pe, CroPlayer{static_cast<uint8_t>(i), 0});
    }
    dirty_ = true;
  }

  bool can_place(Sim& s, uint8_t owner, uint16_t def, int tx, int ty, std::string* error) override {
    if (owner == 0) return true;
    const EntityDef& d = s.rules()->defs[def];
    // a tüskék korlátja (a régi G 2500: `spike_max`, különben 16)
    if (is_spike(d) && spike_count(s, owner) + 1 > spike_max(s, owner)) {
      if (error) *error = "elérted a tüskék korlátját";
      return false;
    }
    if (!creep_faction(s, owner)) return true;
    if (d.faction != s.player(owner).faction) return true;   // más nép (szövetséges kisnép) épülete
    if (d.raw && flag(path2(d.raw.get(), "Placement", "no_creep"))) return true;
    const int w = std::max(1, d.fw), h = std::max(1, d.fh);
    for (int y = ty; y < ty + h; ++y) {
      for (int x = tx; x < tx + w; ++x) {
        if (!on_creep(s, owner, x, y)) {
          if (error) *error = "csak saját mélységre építhető";
          return false;
        }
      }
    }
    return true;
  }

  Fixed speed_mult(Sim& s, EntityId e) override {
    const Unit* u = s.world().get<Unit>(e);
    if (!u || s.rules()->defs[u->def].flying) return Fixed::from_int(1);
    if (!unit_on_own_creep(s, e)) return Fixed::from_int(1);
    return tuning(s, fdef(s, s.world().get<Health>(e)->owner), "creep_speed_mult", kCreepSpeedDefault);
  }

  Fixed regen_add(Sim& s, EntityId e) override {
    const Health* h = s.world().get<Health>(e);
    if (!h) return Fixed{};
    const Unit* u = s.world().get<Unit>(e);
    const Building* b = u ? nullptr : s.world().get<Building>(e);
    if (!u && !b) return Fixed{};
    bool on = false;
    if (u) {
      on = unit_on_own_creep(s, e);
    } else if (creep_faction(s, h->owner)) {
      on = on_creep(s, h->owner, b->tx, b->ty);
    }
    if (!on) return Fixed{};
    // a régi: (faji + stat) gyógyulás × creep_regen_mult × szabály – a közös számítás az alapot adja, a modul a különbözetet
    const FactionDef* f = fdef(s, h->owner);
    const EntityDef& d = s.rules()->defs[u ? u->def : b->def];
    const Fixed base = (f ? f->regen : Fixed{}) + d.regen;
    const Fixed mult = tuning(s, f, "creep_regen_mult", kCreepRegenDefault) * rule_mult(s, h->owner, f, "creep_regen_mult");
    return base * (mult - Fixed::from_int(1));
  }

  // mérgező tüske (MECH-CREEP-7): a tűz- és méregfegyver teljesen sebzi, más × 0,2; a nem tüske-immunis támadó
  // egységet megmérgezi (mint a rajta állót). Az égés és a méreg idővel ható sebzése nem ide fut (teljes).
  Fixed damage_taken_mult(Sim& s, EntityId src, EntityId t, bool, bool magic) override {
    poison_attack(s, src, t, magic);
    const Building* b = s.world().get<Building>(t);
    if (!b) return Fixed::from_int(1);
    const EntityDef& d = s.rules()->defs[b->def];
    if (!is_spike(d)) return Fixed::from_int(1);
    const Unit* su = src && s.world().alive(src) ? s.world().get<Unit>(src) : nullptr;
    if (su) {
      const Health* sh = s.world().get<Health>(src);
      const Health* th = s.world().get<Health>(t);
      if (sh && !faction_flag(s, sh->owner, "spike_immune")) {
        // a régi `spike_mult` (Bénító méreg): a tüske gazdájának szorzója
        const Fixed k = th ? rule_mult(s, th->owner, fdef(s, th->owner), "spike_mult") : Fixed::from_int(1);
        if (Afflict* af = s.world().get<Afflict>(src)) poison(s, *af, t, k);
      }
      const AttackDef& w = s.rules()->defs[su->def].attack;
      if (!magic && (w.element == Element::Fire || w.element == Element::Poison || w.poison_dps.raw() > 0)) return Fixed::from_int(1);
    }
    return num(path2(d.raw.get(), "Legacy", "spike_taken"), kSpikeTakenDefault);
  }

  void on_death(Sim& s, EntityId victim, EntityId killer, uint8_t ko) override {
    if (s.world().get<CroCreep>(victim)) dirty_ = true;
    convert_kill(s, victim, killer, ko);
    if (!creep_faction(s, ko)) return;
    const FactionDef* f = s.rules()->faction(s.player(ko).faction);
    const content::Value* inf = f ? path2(faction_obj(*f), "infest", nullptr) : nullptr;
    if (!inf || !inf->is_object()) return;
    const Unit* vu = s.world().get<Unit>(victim);
    const Health* vh = s.world().get<Health>(victim);
    const Position* vp = s.world().get<Position>(victim);
    if (!vu || !vh || !vp || vh->owner == 0 || !s.hostile(ko, vh->owner)) return;
    const EntityDef& vd = s.rules()->defs[vu->def];
    if (!vd.has_tag("organic") || vd.hero || s.world().get<Lifetime>(victim)) return;
    if (!on_creep(s, ko, vp->x.floor_int(), vp->y.floor_int())) return;
    const content::Value* un = inf->find("unit");
    const uint16_t def = un && un->is_string() ? s.rules()->def(un->text()) : kNoDef;
    if (def == kNoDef) return;
    const int64_t chance = (int64_t{num(inf->find("chance"), Fixed::from_ratio(3, 10)).raw()} * 1000) >> 16;
    if (static_cast<int64_t>(s.combat_rng_.next() % 1000u) >= chance) return;
    const int32_t delay = seconds_to_ticks(num(inf->find("delay"), Fixed::from_int(kInfestDelayDefault)));
    const EntityId he = s.world().create();
    s.world().add(he, Position{vp->x, vp->y});
    s.world().add(he, CroHatch{ko, def, s.tick() + delay});
  }

  bool use_ability(Sim& s, const std::vector<EntityId>& units, uint16_t ability, Fixed, Fixed, EntityId) override {
    const AbilityDef& a = s.rules()->abilities[ability];
    const content::Value* sp = a.spec.get();
    const content::Value* t = sp ? sp->find("type") : nullptr;
    if (!t || !t->is_string() || t->text() != "morph") return false;
    const content::Value* into = sp->find("into");
    const content::Value* via = sp->find("via");
    const uint16_t into_def = into && into->is_string() ? s.rules()->def(into->text()) : kNoDef;
    const uint16_t via_def = via && via->is_string() ? s.rules()->def(via->text()) : kNoDef;
    if (into_def == kNoDef || via_def == kNoDef) return false;
    std::array<int32_t, kResources> cost{};
    if (const content::Value* c = sp->find("cost"); c && c->is_object()) {
      for (int r = 0; r < kResources; ++r) {
        if (const content::Value* x = c->find(resource_id(r))) cost[static_cast<size_t>(r)] = static_cast<int32_t>(x->as_int().value_or(0));
      }
    }
    const int32_t time = seconds_to_ticks(num(sp->find("time"), Fixed::from_int(15)));
    bool any = false;
    for (const EntityId e : units) {
      const Unit* u = s.world().get<Unit>(e);
      const Health* h = s.world().get<Health>(e);
      if (!u || !h || h->hp <= 0 || s.incapacitated(e) || s.sheltered(e)) continue;
      const EntityDef& d = s.rules()->defs[u->def];
      // fejlődési irányhoz kötött (`Abilities.path`)
      if (const content::Value* ap = d.raw ? path2(d.raw.get(), "Abilities", "path") : nullptr; ap && ap->is_object()) {
        if (const content::Value* need = ap->find(a.id); need && need->is_string() && s.player(h->owner).path != need->text()) continue;
      }
      const PlayerState& p = s.player(h->owner);
      bool afford = true;
      for (int r = 0; r < kResources; ++r) afford = afford && p.resource(r) >= cost[static_cast<size_t>(r)];
      if (!afford) continue;
      const int32_t need = s.rules()->defs[into_def].supply;
      if (p.supply_used - d.supply + need > p.supply_cap) continue;
      s.pay(h->owner, cost);
      const Position pos = *s.world().get<Position>(e);
      const uint8_t owner = h->owner;
      s.kill(e);
      const EntityId cocoon = s.spawn_unit(via_def, owner, pos.x, pos.y);
      if (cocoon == kNoEntity) continue;
      s.world().add(cocoon, CroCocoon{into_def, s.tick() + time});
      any = true;
    }
    return any;
  }

  void tick(Sim& s) override {
    if (!s.rules() || !s.map_) return;
    const int64_t now = s.tick();
    // betöltés után a közös `restore_obstacles` a tüskét is akadállyá teszi: az első tickben ismét levesszük
    // (épületek nem fedik egymást, így a csempe akadálya csak a tüskéé; a levétel 0-nál nem csökken tovább)
    if (!spikes_freed_ && s.nav_) {
      spikes_freed_ = true;
      s.world().pool<Building>().each([&](EntityId, const Building& b) {
        const EntityDef& d = s.rules()->defs[b.def];
        if (is_spike(d)) s.nav_->remove_obstacle({b.tx, b.ty, b.w, b.h});
      });
    }
    // a mező forrásainak lenyomata (épület eltűnése, elkészülte, pusztulása): eltérésnél újraszámolás
    uint64_t sig = 0;
    s.world().pool<CroCreep>().each([&](EntityId e, const CroCreep&) {
      const Building* b = s.world().get<Building>(e);
      const Health* h = s.world().get<Health>(e);
      const uint64_t st = (h && h->hp > 0 ? 1u : 2u) + (b && b->complete ? 4u : 0u);
      sig = sig * 1000003u + uint64_t{e} * 8u + st;
    });
    if (sig != sig_) {
      sig_ = sig;
      dirty_ = true;
    }
    // a sugár növekedése (1 s)
    if (now % 30 == 0) {
      s.world().pool<CroCreep>().each([&](EntityId e, CroCreep& cc) {
        const Building* b = s.world().get<Building>(e);
        if (!b || !b->complete) return;
        const Fixed full = Fixed::from_int(creep_radius_of(s, b->def));
        if (cc.r < full) {
          cc.r = std::min(full, cc.r + kGrow);
          dirty_ = true;
        }
      });
      rot(s);
    }
    // kikelő fertőzés
    std::vector<EntityId> hatch;
    s.world().pool<CroHatch>().each([&](EntityId e, const CroHatch& c) {
      if (c.at <= now) hatch.push_back(e);
    });
    for (const EntityId e : hatch) {
      const CroHatch c = *s.world().get<CroHatch>(e);
      const Position p = *s.world().get<Position>(e);
      s.world().destroy(e);
      const EntityId u = s.spawn_unit(c.def, c.owner, p.x, p.y);
      if (u != kNoEntity) s.place_at_free(u, p.x, p.y);
    }
    // gubó → új egység
    std::vector<EntityId> done;
    s.world().pool<CroCocoon>().each([&](EntityId e, const CroCocoon& c) {
      const Health* h = s.world().get<Health>(e);
      if (c.until <= now && h && h->hp > 0) done.push_back(e);
    });
    for (const EntityId e : done) {
      const CroCocoon c = *s.world().get<CroCocoon>(e);
      const Position p = *s.world().get<Position>(e);
      const uint8_t owner = s.world().get<Health>(e)->owner;
      s.kill(e);
      const EntityId u = s.spawn_unit(c.into, owner, p.x, p.y);
      if (u != kNoEntity) s.place_at_free(u, p.x, p.y);
    }
    // mérgező tüskék (0,5 s): lassítás és méreg a rajtuk álló földi, nem tüske-immunis egységnek
    if (now % 15 == 0) spikes(s);
    larvae(s);
    permanent_summons(s);
    blood_magic(s);
  }

 private:
  void rot(Sim& s) {
    const int W = s.map_->width();
    std::vector<EntityId> pls;
    s.world().pool<CroPlayer>().each([&](EntityId e, const CroPlayer&) { pls.push_back(e); });
    for (const EntityId pe : pls) {
      const uint8_t owner = s.world().get<CroPlayer>(pe)->owner;
      const FactionDef* f = s.rules()->faction(s.player(owner).faction);
      const content::Value* cr = f ? path2(faction_obj(*f), "creep_rot", nullptr) : nullptr;
      if (!cr || !cr->is_object()) continue;
      // a kutatás (Mélyebb rothasztás) üteme `rot_mult` szoros, a keretet a `rot_cap` váltja (a régi G 1793, 1803)
      const Fixed rate = num(cr->find("rate"), Fixed::from_ratio(3, 50)) * rule_mult(s, owner, f, "rot_mult");
      const Fixed hq_mult = num(cr->find("hq_mult"), Fixed::from_ratio(5, 2));
      const Fixed hq_radius = num(cr->find("hq_radius"), Fixed::from_int(9));
      const Fixed cap = num(rule_val(s, owner, "rot_cap"), num(cr->find("cap"), kRotCap));
      // a saját főépületek
      std::vector<std::pair<Fixed, Fixed>> hqs;
      s.world().pool<Building>().each([&](EntityId e, const Building& b) {
        const Health* h = s.world().get<Health>(e);
        if (h && h->owner == owner && h->hp > 0 && b.def == f->hq) {
          hqs.emplace_back(Fixed::from_int(b.tx) + Fixed::from_ratio(b.w, 2), Fixed::from_int(b.ty) + Fixed::from_ratio(b.h, 2));
        }
      });
      // a saját mélységen álló fák a főépülettől mért távolság szerint (D21), majd sor, oszlop
      struct T { int64_t d; int16_t y, x; EntityId e; Fixed dist; };
      std::vector<T> trees;
      s.world().pool<ResourceNode>().each([&](EntityId e, const ResourceNode& n) {
        if (n.kind != kWood || n.amount <= 0 || !on_creep(s, owner, n.tx, n.ty)) return;
        const Fixed px = Fixed::from_int(n.tx) + Fixed::from_ratio(1, 2), py = Fixed::from_int(n.ty) + Fixed::from_ratio(1, 2);
        int64_t best = 0;
        Fixed bd = Fixed::from_int(10000);
        for (const auto& [hx, hy] : hqs) {
          const int64_t d2 = core::length_sq_raw(px - hx, py - hy);
          if (bd == Fixed::from_int(10000) || d2 < best) {
            best = d2;
            bd = core::length(px - hx, py - hy);
          }
        }
        trees.push_back({best, n.ty, n.tx, e, bd});
      });
      std::sort(trees.begin(), trees.end(), [](const T& a, const T& b) {
        if (a.d != b.d) return a.d < b.d;
        if (a.y != b.y) return a.y < b.y;
        return a.x < b.x;
      });
      int64_t budget = (int64_t{cap.raw()} * 1000) >> 16;   // ezred / s
      int64_t gained = 0;
      for (const T& t : trees) {
        if (budget <= 0) break;
        ResourceNode& n = *s.world().get<ResourceNode>(t.e);
        Fixed r = rate;
        if (!hqs.empty() && t.dist <= hq_radius) r = r * hq_mult;
        CroRot* cr2 = s.world().get<CroRot>(t.e);
        if (!cr2) {
          s.world().add(t.e, CroRot{});
          cr2 = s.world().get<CroRot>(t.e);
        }
        const int64_t left = int64_t{n.amount} * 1000 - cr2->milli;
        const int64_t take = std::min({left, (int64_t{r.raw()} * 1000) >> 16, budget});
        budget -= take;
        gained += take;
        cr2->milli += static_cast<int32_t>(take);
        while (cr2->milli >= 1000 && n.amount > 0) {
          cr2->milli -= 1000;
          --n.amount;
        }
        if (n.amount <= 0) s.remove_resource(t.e);
      }
      CroPlayer& cp = *s.world().get<CroPlayer>(pe);
      cp.rot_milli += static_cast<int32_t>(gained);
      if (cp.rot_milli >= 1000) {
        const int32_t whole = cp.rot_milli / 1000;
        cp.rot_milli -= whole * 1000;
        s.income(owner, kWood, whole);
      }
      (void)W;
    }
  }

  static void poison(Sim& s, Afflict& af, EntityId src, Fixed k) {   // a tüske mérge: 4 × `spike_mult` / s, 2 s
    const int32_t dps = std::max(1, (Fixed::from_int(kSpikeDps) * k).floor_int());
    af.poison_src = src;
    af.poison_dps = std::max(af.poison_until > s.tick() ? af.poison_dps : 0, dps);
    af.poison_until = s.tick() + 60;
  }

  // `poison_attacks` (a régi G 1074): a címkés egység nem varázs-támadása a szerves célpontot megmérgezi (a fegyver
  // saját mérge elsőbb)
  void poison_attack(Sim& s, EntityId src, EntityId t, bool magic) {
    if (magic || !src || !s.world().alive(src)) return;
    const Unit* su = s.world().get<Unit>(src);
    const Unit* tu = s.world().get<Unit>(t);
    const Health* sh = s.world().get<Health>(src);
    if (!su || !tu || !sh) return;
    const EntityDef& sd = s.rules()->defs[su->def];
    if (sd.attack.poison_dps.raw() > 0 || !s.rules()->defs[tu->def].has_tag("organic")) return;
    const content::Value* r = rule_val(s, sh->owner, "poison_attacks");
    if (!rule_applies(r, sd)) return;
    Afflict* af = s.world().get<Afflict>(t);
    if (!af) return;
    af->poison_src = src;
    af->poison_dps = (Fixed::from_int(1000) * num(r->find("dps"), Fixed{})).floor_int();
    af->poison_until = s.tick() + seconds_to_ticks(num(r->find("dur"), Fixed{}));
  }

  // `convert_kills` (a régi G 1211): az egység által ölt ellenséges egység esélyre a gyilkos gazdájához áll át
  // (`hp` arányú életerővel); hős, idézett, nagy, munkás, jószág és a semleges nem; csak a létszámkorlát alatt
  void convert_kill(Sim& s, EntityId victim, EntityId killer, uint8_t ko) {
    if (ko < 1 || ko > Sim::kMaxPlayers || !killer || !s.world().alive(killer) || !s.world().get<Unit>(killer)) return;
    const content::Value* ck = rule_val(s, ko, "convert_kills");
    if (!ck || !ck->is_object()) return;
    const Unit* vu = s.world().get<Unit>(victim);
    const Health* vh = s.world().get<Health>(victim);
    const Position* vp = s.world().get<Position>(victim);
    if (!vu || !vh || !vp || vh->owner == 0 || !s.hostile(ko, vh->owner)) return;
    const EntityDef& vd = s.rules()->defs[vu->def];
    if (vd.hero || vd.worker || vd.has_tag("large") || vd.has_tag("livestock") || s.world().get<Lifetime>(victim)) return;
    const PlayerState& p = s.player(ko);
    if (p.supply_used + vd.supply > p.supply_cap) return;
    const int64_t chance = (int64_t{num(ck->find("chance"), Fixed{}).raw()} * 1000) >> 16;
    if (static_cast<int64_t>(s.combat_rng_.next() % 1000u) >= chance) return;
    const Fixed x = vp->x, y = vp->y;
    const uint16_t def = vu->def;
    const EntityId u = s.spawn_unit(def, ko, x, y);
    if (u == kNoEntity) return;
    s.place_at_free(u, x, y);
    if (Health* uh = s.world().get<Health>(u)) {
      uh->hp = std::max(1, (Fixed::from_int(uh->max) * num(ck->find("hp"), Fixed::from_ratio(7, 20))).floor_int());
    }
  }

  // a tüskék: a játékos élő tüskéinek száma (félkészen is) és a korlátja
  int32_t spike_count(Sim& s, uint8_t owner) {
    int32_t n = 0;
    s.world().pool<Building>().each([&](EntityId e, const Building& b) {
      const Health* h = s.world().get<Health>(e);
      if (h && h->owner == owner && h->hp > 0 && is_spike(s.rules()->defs[b.def])) ++n;
    });
    return n;
  }

  int32_t spike_max(Sim& s, uint8_t owner) {
    const content::Value* v = rule_val(s, owner, "spike_max");
    return v ? static_cast<int32_t>(v->as_int().value_or(kSpikeMaxDefault)) : kSpikeMaxDefault;
  }

  // `spike_size` (Tüskemező, a régi G 2510, 2540): az elkészült tüske n × n-es mezővé egészül ki (a bal felső sarok a
  // lerakott tüske; a szabad, lerakható csempékre, a korlátig)
  void lay_spikes(Sim& s, EntityId e) {
    const Building* b = s.world().get<Building>(e);
    const Health* h = s.world().get<Health>(e);
    if (!b || !h || h->owner == 0) return;
    const content::Value* v = rule_val(s, h->owner, "spike_size");
    const int n = v ? static_cast<int>(v->as_int().value_or(1)) : 1;
    if (n <= 1) return;
    const uint16_t def = b->def;
    const uint8_t owner = h->owner;
    const int tx = b->tx, ty = b->ty;
    for (int y = ty; y < ty + n; ++y) {
      for (int x = tx; x < tx + n; ++x) {
        if ((x == tx && y == ty) || !s.can_place(owner, def, x, y)) continue;
        s.place_building(def, owner, x, y, true);
      }
    }
  }

  // a lárvák visszanövése (a régi E 1738–1747): a teli számig (`larva` + `larva_add`, a sorban lévő egységekkel
  // együtt) `larva_time` / `larva_mult` másodpercenként egy
  void larvae(Sim& s) {
    s.world().pool<CroCreep>().each([&](EntityId e, CroCreep& cc) {
      const Building* b = s.world().get<Building>(e);
      const Health* h = s.world().get<Health>(e);
      if (!b || !h || h->hp <= 0 || !b->complete) return;
      const EntityDef& d = s.rules()->defs[b->def];
      const int32_t base = larva_of(d);
      if (base <= 0) return;
      const content::Value* add = rule_val(s, h->owner, "larva_add");
      const int32_t maxl = base + (add ? static_cast<int32_t>(add->as_int().value_or(0)) : 0);
      int32_t used = 0;
      if (const Producer* pr = s.world().get<Producer>(e)) {
        for (const QueueItem& q : pr->queue) used += q.research ? 0 : 1;
      }
      if (cc.larva + used >= maxl) {
        cc.larva_t = 0;
        return;
      }
      ++cc.larva_t;
      const Fixed lt = num(d.raw ? path2(d.raw.get(), "Producer", "larva_time") : nullptr, Fixed::from_int(10));
      const Fixed need = lt * Fixed::from_int(kTickHz) / rule_mult(s, h->owner, fdef(s, h->owner), "larva_mult");
      if (Fixed::from_int(cc.larva_t) >= need) {
        cc.larva_t = 0;
        ++cc.larva;
      }
    });
  }

  // `summons_permanent`: az új idézés (Lifetime) nem jár le, de idézettnek számít; az idéző a legközelebbi saját hős
  // 3 csempén belül; hősönként legfeljebb 4 – a legrégebbi pusztul
  void permanent_summons(Sim& s) {
    std::vector<EntityId> fresh;
    s.world().pool<Lifetime>().each([&](EntityId e, const Lifetime&) {
      const Health* h = s.world().get<Health>(e);
      if (h && h->hp > 0 && !s.world().get<CroSummon>(e) && faction_flag(s, h->owner, "summons_permanent")) fresh.push_back(e);
    });
    if (fresh.empty()) return;
    std::vector<EntityId> heroes;
    for (const EntityId e : fresh) {
      const uint8_t owner = s.world().get<Health>(e)->owner;
      const Position& p = *s.world().get<Position>(e);
      EntityId best = 0;
      int64_t bd = 0;
      s.world().pool<HeroState>().each([&](EntityId he, const HeroState&) {
        const Health* hh = s.world().get<Health>(he);
        const Position* hp = s.world().get<Position>(he);
        if (!hh || !hp || hh->owner != owner || hh->hp <= 0) return;
        if (core::length(hp->x - p.x, hp->y - p.y) > Fixed::from_int(3)) return;
        const int64_t d = core::length_sq_raw(hp->x - p.x, hp->y - p.y);
        if (!best || d < bd) {
          best = he;
          bd = d;
        }
      });
      s.world().get<Lifetime>(e)->until = INT64_MAX / 2;
      s.world().add(e, CroSummon{best, s.tick()});
      if (best && std::find(heroes.begin(), heroes.end(), best) == heroes.end()) heroes.push_back(best);
    }
    for (const EntityId he : heroes) {
      std::vector<std::pair<int64_t, EntityId>> mine;
      s.world().pool<CroSummon>().each([&](EntityId e, const CroSummon& c) {
        const Health* h = s.world().get<Health>(e);
        if (c.hero == he && h && h->hp > 0) mine.emplace_back(c.since, e);
      });
      std::sort(mine.begin(), mine.end());
      for (size_t i = 0; i + kSummonsPerHero < mine.size(); ++i) s.kill(mine[i].second);
    }
  }

  // `spell_resource: hp`: a varázsló energiája a felhasználható életerő (életerő − 1); amit a képességek a legutóbb
  // beállított értékből elköltöttek, azt az életerő fizeti
  void blood_magic(Sim& s) {
    s.world().pool<Caster>().each([&](EntityId e, Caster& c) {
      Health* h = s.world().get<Health>(e);
      if (!h || h->hp <= 0 || !s.world().get<Unit>(e)) return;
      const FactionDef* f = fdef(s, h->owner);
      const content::Value* sr = f ? path2(faction_obj(*f), "spell_resource", nullptr) : nullptr;
      if (!sr || !sr->is_string() || sr->text() != "hp") return;
      CroBlood* bl = s.world().get<CroBlood>(e);
      if (bl && bl->set > c.energy) h->hp = std::max(1, h->hp - (bl->set - c.energy + 999) / 1000);
      const int32_t avail = std::max(0, h->hp - 1) * 1000;
      c.energy = avail;
      if (bl) {
        bl->set = avail;
      } else {
        s.world().add(e, CroBlood{avail});
      }
    });
  }

  void spikes(Sim& s) {
    std::vector<std::pair<EntityId, std::pair<int, int>>> sp;
    s.world().pool<Building>().each([&](EntityId e, const Building& b) {
      const EntityDef& d = s.rules()->defs[b.def];
      const Health* h = s.world().get<Health>(e);
      if (!b.complete || !h || h->hp <= 0 || !is_spike(d)) return;
      sp.push_back({e, {b.tx, b.ty}});
    });
    if (sp.empty()) return;
    s.world().pool<Unit>().each([&](EntityId e, const Unit& u) {
      const Health* h = s.world().get<Health>(e);
      if (!h || h->hp <= 0 || s.rules()->defs[u.def].flying || s.sheltered(e)) return;
      if (faction_flag(s, h->owner, "spike_immune")) return;   // a tüske-immunis faj (Croftianok) nem sérül
      const Position& p = *s.world().get<Position>(e);
      const int tx = p.x.floor_int(), ty = p.y.floor_int();
      for (const auto& [se, xy] : sp) {
        if (xy.first != tx || xy.second != ty) continue;
        const Health* sh = s.world().get<Health>(se);
        if (!sh || !s.hostile(sh->owner, h->owner)) continue;
        Afflict* af = s.world().get<Afflict>(e);
        if (!af) break;
        // a tüske gazdájának `spike_mult` szorzója (Bénító méreg; a régi G 2581): lassítás max(0,2, 1 − 0,4 × k), méreg 4 × k
        const Fixed k = rule_mult(s, sh->owner, fdef(s, sh->owner), "spike_mult");
        const Fixed slow = std::max(Fixed::from_ratio(1, 5), Fixed::from_int(1) - (Fixed::from_int(1) - kSpikeSlow) * k);
        const Fixed cur = af->slow_until > s.tick() ? af->slow_mult : Fixed::from_int(1);
        af->slow_mult = std::min(cur, slow);
        af->slow_until = s.tick() + 21;                           // 0,7 s
        poison(s, *af, se, k);
        break;
      }
    });
  }

  std::map<uint8_t, std::vector<uint8_t>> fields_;   // a mező (származtatott, nem állapot)
  bool dirty_ = true;
  uint64_t sig_ = 0;
  bool spikes_freed_ = false;
};

}  // namespace

std::unique_ptr<FactionModule> make_croftian_module() { return std::make_unique<Module>(); }

int croftian_creep_tiles(Sim& s, uint8_t owner);
bool croftian_on_creep(Sim& s, uint8_t owner, int x, int y);

// a próbáknak: a játékos mélységcsempéinek száma
int croftian_creep_tiles(Sim& s, uint8_t owner) {
  for (auto& m : s.modules_) {
    if (std::string(m->name()) == "croftian") return static_cast<Module*>(m.get())->creep_tiles(s, owner);
  }
  return 0;
}

// a próbáknak: saját mélység-e a csempe
bool croftian_on_creep(Sim& s, uint8_t owner, int x, int y) {
  for (auto& m : s.modules_) {
    if (std::string(m->name()) == "croftian") return static_cast<Module*>(m.get())->creep_at(s, owner, x, y);
  }
  return false;
}

}  // namespace totu::sim
