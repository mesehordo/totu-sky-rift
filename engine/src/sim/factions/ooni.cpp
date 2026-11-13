// SPDX-License-Identifier: GPL-3.0-or-later
// Ooni (MECH-LANT-1–3: lámpásfény, harci kedv, Taiko-dobos, őrök; védőfény) – M7, ADR-008; a régi játék és a
// spec/FAJMECHANIKAK.md szerint (G 1141–1176, 1818–1863, 1973–1997, 2279–2293, 2708–2737; E 176–182, 328–331):
//   - lámpásfény: az ooni épület (a `no_light` nélküli) csak saját fényforrás körébe rakható (a lámpás már épülve is
//     világít); 30 tickenként a sebesült ooni egység az első kész fényforrás körében `light_heal` életerőt kap;
//   - harci kedv: a nem munkás, támadó, nem idézett ooni egységnek; sérüléskor −sebzés / max életerő × `hurt`;
//     társ eleste (5 csempén belül) −`ally_death`, hős eleste (8 csempén belül) megtöri; 15 tickenként töltődik
//     (3 s óta nem sérült: `regen`/s, másik dobos közelében + `drum.regen`/s), fényben legalább `light_min`;
//     megtörve sebzés × 0,7, sebesség × 1,2, `break_until`-nál helyreáll;
//   - Taiko-dobos: a 4 csempén belüli épülő ooni épület × 1,5-tel épül;
//   - Védőfény (`ward`): a körben a saját egységek ideiglenes pajzsot kapnak.
#include <algorithm>

#include "content/json.hpp"
#include "sim/faction_module.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

// a harci kedv (ezred egység) és a sérülés követése (az életerő-változásból: a keretben nincs sérülés-horog)
struct Morale {
  int32_t value = 0;
  int64_t last_eff = 0;      // az előző tick ezred életereje
  int64_t last_hurt = -100000;
  uint8_t broken = 0;
  void hash(core::StateHash& h) const { h.add_i64(value); h.add_i64(last_eff); h.add_i64(last_hurt); h.add_u64(broken); }
  void save(core::ByteWriter& w) const { w.i32(value); w.i64(last_eff); w.i64(last_hurt); w.u8(broken); }
  static Morale load(core::ByteReader& r) {
    Morale m;
    m.value = r.i32(); m.last_eff = r.i64(); m.last_hurt = r.i64(); m.broken = r.u8();
    return m;
  }
};

// Védőfény: a pajzsba töltött ideiglenes rész (lejártakor a maradék elvész)
struct Ward {
  int32_t milli = 0;
  int64_t until = 0;
  void hash(core::StateHash& h) const { h.add_i64(milli); h.add_i64(until); }
  void save(core::ByteWriter& w) const { w.i32(milli); w.i64(until); }
  static Ward load(core::ByteReader& r) { Ward x; x.milli = r.i32(); x.until = r.i64(); return x; }
};

Fixed fnum(const content::Value* v, Fixed def) {
  if (!v || !v->is_number()) return def;
  const auto f = v->as_fixed();
  return f ? *f : def;
}

int64_t milli(Fixed f) { return (int64_t{f.raw()} * 1000) >> 16; }

class Module final : public FactionModule {
 public:
  const char* name() const override { return "ooni"; }

  void register_components(World& w) override {
    w.register_component<Morale>("OoniMorale");
    w.register_component<Ward>("OoniWard");
  }

  void on_unit_spawned(Sim& s, EntityId e) override {
    if (!load(s) || !is_ooni(s, e)) return;
    const EntityDef& d = def_of(s, e);
    if (d.worker || !d.attack.has || d.has_tag("summoned")) return;
    const Health& h = *s.world().get<Health>(e);
    Morale m;
    m.value = max_ * 1000;
    m.last_eff = int64_t{h.hp} * 1000;
    s.world().add(e, m);
  }

  bool can_place(Sim& s, uint8_t owner, uint16_t def, int tx, int ty, std::string* error) override {
    if (!load(s) || owner == 0 || owner > Sim::kMaxPlayers || s.player(owner).faction != "ooni") return true;
    const EntityDef& d = s.rules()->defs[def];
    if (d.faction != "ooni" || no_light(d)) return true;
    const Fixed cx = Fixed::from_int(tx) + Fixed::from_ratio(std::max(1, d.fw), 2);
    const Fixed cy = Fixed::from_int(ty) + Fixed::from_ratio(std::max(1, d.fh), 2);
    if (in_light(s, owner, cx, cy, true)) return true;
    if (error) *error = "csak lámpásfénybe építhető";
    return false;
  }

  Fixed build_rate_mult(Sim& s, EntityId b) override {   // Taiko-dobos a közelben
    if (!load(s) || drum_unit_ == kNoDef) return Fixed::from_int(1);
    const Health* h = s.world().get<Health>(b);
    const Position* p = s.world().get<Position>(b);
    if (!h || !p || !owner_ooni(s, h->owner)) return Fixed::from_int(1);
    bool near = false;
    s.world().pool<Unit>().each([&](EntityId o, const Unit& u) {
      if (near || u.def != boost_unit_) return;
      const Health* oh = s.world().get<Health>(o);
      const Position& op = *s.world().get<Position>(o);
      if (oh && oh->owner == h->owner && oh->hp > 0 && core::length(op.x - p->x, op.y - p->y) <= boost_radius_) near = true;
    });
    return near ? boost_mult_ : Fixed::from_int(1);
  }

  Fixed damage_mult(Sim& s, EntityId a, EntityId) override {
    const Morale* m = s.world().get<Morale>(a);
    return m && m->broken ? broken_damage_ : Fixed::from_int(1);
  }

  Fixed speed_mult(Sim& s, EntityId e) override {
    const Morale* m = s.world().get<Morale>(e);
    return m && m->broken ? broken_speed_ : Fixed::from_int(1);
  }

  void on_death(Sim& s, EntityId v, EntityId, uint8_t) override {
    if (!load(s) || !is_ooni(s, v)) return;
    const EntityDef& d = def_of(s, v);
    if (s.world().get<Lifetime>(v) || d.has_tag("summoned") || !s.world().get<Unit>(v)) return;
    const Position& vp = *s.world().get<Position>(v);
    const uint8_t owner = s.world().get<Health>(v)->owner;
    const bool hero = s.world().get<HeroState>(v) != nullptr;
    s.world().pool<Morale>().each([&](EntityId o, Morale& m) {
      if (o == v) return;
      const Health* oh = s.world().get<Health>(o);
      const Position& op = *s.world().get<Position>(o);
      if (!oh || oh->owner != owner || oh->hp <= 0) return;
      const Fixed d2 = core::length(op.x - vp.x, op.y - vp.y);
      if (hero && d2 <= Fixed::from_int(8)) {   // a hős eleste megtöri (D21)
        if (hero_breaks_) m.value = 0;
        else m.value = std::max(0, m.value - hero_death_ * 1000);
      } else if (d2 <= Fixed::from_int(5)) {
        m.value = std::max(0, m.value - ally_death_ * 1000);
      }
      if (m.value <= 0) m.broken = 1;
    });
  }

  bool use_ability(Sim& s, const std::vector<EntityId>& units, uint16_t ab, Fixed, Fixed, EntityId) override {
    const AbilityDef& a = s.rules()->abilities[ab];
    const content::Value* t = a.spec ? a.spec->find("type") : nullptr;
    if (!t || !t->is_string() || t->text() != "ward") return false;
    const int32_t amount = static_cast<int32_t>(milli(fnum(a.spec->find("amount"), Fixed::from_int(50))));
    for (const EntityId e : units) {   // az első kész használó
      Caster* c = s.world().get<Caster>(e);
      if (!c || s.incapacitated(e) || !s.ability_ready(e, *c, a)) continue;
      s.ability_spend(*c, ab, a);
      const Position p = *s.world().get<Position>(e);
      const uint8_t owner = s.world().get<Health>(e)->owner;
      std::vector<EntityId> hit;
      s.world().pool<Unit>().each([&](EntityId o, const Unit&) {
        const Health* oh = s.world().get<Health>(o);
        const Position& op = *s.world().get<Position>(o);
        if (oh && oh->owner == owner && oh->hp > 0 && core::length(op.x - p.x, op.y - p.y) <= a.radius) hit.push_back(o);
      });
      for (const EntityId o : hit) {
        Vital* vt = s.world().get<Vital>(o);
        if (!vt) continue;
        Ward* w = s.world().get<Ward>(o);
        if (w) {   // frissítés: a régi eltűnik, az új teljes
          vt->shield = std::max(0, vt->shield - std::min(vt->shield, w->milli));
          w->milli = amount;
          w->until = s.tick() + a.duration;
        } else {
          s.world().add(o, Ward{amount, s.tick() + a.duration});
        }
        vt->shield += amount;
      }
      return true;
    }
    return false;
  }

  void tick(Sim& s) override {
    if (!load(s)) return;
    const int64_t now = s.tick();
    // Védőfény: a pajzs-maradék követése, lejártakor elvész
    std::vector<EntityId> gone;
    s.world().pool<Ward>().each([&](EntityId e, Ward& w) {
      Vital* vt = s.world().get<Vital>(e);
      if (!vt) return;
      w.milli = std::min(w.milli, vt->shield);
      if (now >= w.until) {
        vt->shield -= w.milli;
        gone.push_back(e);
      }
    });
    for (const EntityId e : gone) s.world().pool<Ward>().remove(e);
    // harci kedv: a sérülés az életerő-változásból
    s.world().pool<Morale>().each([&](EntityId e, Morale& m) {
      const Health* h = s.world().get<Health>(e);
      const Vital* vt = s.world().get<Vital>(e);
      if (!h) return;
      const int64_t eff = int64_t{h->hp} * 1000 - (vt ? vt->deficit : 0);
      if (eff < m.last_eff && h->max > 0) {
        m.value = static_cast<int32_t>(std::max<int64_t>(0, m.value - (m.last_eff - eff) * hurt_ / h->max));
        m.last_hurt = now;
        if (m.value <= 0) m.broken = 1;
      }
      m.last_eff = eff;
    });
    if (now % 15 == 0) {   // töltődés 0,5 s-onként, fényben legalább `light_min`
      std::vector<std::pair<EntityId, std::pair<Fixed, Fixed>>> drums;
      s.world().pool<Unit>().each([&](EntityId o, const Unit& u) {
        if (u.def != drum_unit_) return;
        const Health* oh = s.world().get<Health>(o);
        if (oh && oh->hp > 0) drums.push_back({o, {s.world().get<Position>(o)->x, s.world().get<Position>(o)->y}});
      });
      s.world().pool<Morale>().each([&](EntityId e, Morale& m) {
        const Health* h = s.world().get<Health>(e);
        const Position* p = s.world().get<Position>(e);
        if (!h || !p) return;
        int32_t add = 0;
        if (now - m.last_hurt > 90) add += regen_ * 500;
        for (const auto& [d, dp] : drums) {
          const Health* dh = s.world().get<Health>(d);
          if (d != e && dh->owner == h->owner && core::length(dp.first - p->x, dp.second - p->y) <= drum_radius_) {
            add += drum_regen_ * 500;
            break;
          }
        }
        m.value = std::min(max_ * 1000, m.value + add);
        if (in_light(s, h->owner, p->x, p->y, false)) m.value = std::max(m.value, light_min_ * 1000);
        if (m.broken && m.value >= break_until_ * 1000) m.broken = 0;
      });
    }
    if (now % 30 == 0) {   // lámpásfény-gyógyulás
      s.world().pool<Unit>().each([&](EntityId e, const Unit&) {
        Health* h = s.world().get<Health>(e);
        if (!h || h->hp <= 0 || h->hp >= h->max || !owner_ooni(s, h->owner)) return;
        const Position& p = *s.world().get<Position>(e);
        const int heal = light_heal_at(s, h->owner, p.x, p.y);
        if (heal > 0) h->hp = std::min(h->max, h->hp + heal);
      });
    }
  }

 private:
  bool loaded_ = false, ok_ = false;
  int32_t max_ = 100, hurt_ = 60, ally_death_ = 10, hero_death_ = 25, regen_ = 2, drum_regen_ = 8, light_min_ = 30,
          break_until_ = 50;
  bool hero_breaks_ = true;
  uint16_t drum_unit_ = kNoDef, boost_unit_ = kNoDef;
  Fixed drum_radius_ = Fixed::from_int(4), boost_radius_ = Fixed::from_int(4), boost_mult_ = Fixed::from_ratio(3, 2);
  Fixed broken_damage_ = Fixed::from_ratio(7, 10), broken_speed_ = Fixed::from_ratio(6, 5);

  // a nép adatai (`factions.json` → `faction_ooni.Faction`), egyszer
  bool load(Sim& s) {
    if (loaded_) return ok_;
    loaded_ = true;
    if (!s.rules()) return false;
    const FactionDef* f = s.rules()->faction("ooni");
    const content::Value* fa = f && f->raw ? f->raw->find("Faction") : nullptr;
    if (!fa) return false;
    ok_ = true;
    auto num = [&](const content::Value* o, const char* k, int32_t def) { return fnum(o ? o->find(k) : nullptr, Fixed::from_int(def)).floor_int(); };
    if (const content::Value* mo = fa->find("morale")) {
      max_ = num(mo, "max", max_);
      hurt_ = num(mo, "hurt", hurt_);
      ally_death_ = num(mo, "ally_death", ally_death_);
      hero_death_ = num(mo, "hero_death", hero_death_);
      regen_ = num(mo, "regen", regen_);
      light_min_ = num(mo, "light_min", light_min_);
      break_until_ = num(mo, "break_until", break_until_);
      const content::Value* hb = mo->find("hero_death_breaks");
      hero_breaks_ = !hb || !hb->is_bool() || hb->as_bool();
      if (const content::Value* dr = mo->find("drum")) {
        if (const content::Value* u = dr->find("unit"); u && u->is_string()) drum_unit_ = s.rules()->def(u->text());
        drum_radius_ = fnum(dr->find("radius"), drum_radius_);
        drum_regen_ = num(dr, "regen", drum_regen_);
      }
      if (const content::Value* br = mo->find("broken")) {
        broken_damage_ = fnum(br->find("damage_mult"), broken_damage_);
        broken_speed_ = fnum(br->find("speed_mult"), broken_speed_);
      }
    }
    if (const content::Value* bb = fa->find("build_boost")) {
      if (const content::Value* u = bb->find("unit"); u && u->is_string()) boost_unit_ = s.rules()->def(u->text());
      boost_radius_ = fnum(bb->find("radius"), boost_radius_);
      boost_mult_ = fnum(bb->find("mult"), boost_mult_);
    }
    return true;
  }

  static const EntityDef& def_of(Sim& s, EntityId e) {
    if (const Unit* u = s.world().get<Unit>(e)) return s.rules()->defs[u->def];
    return s.rules()->defs[s.world().get<Building>(e)->def];
  }
  static bool owner_ooni(Sim& s, uint8_t o) { return o >= 1 && o <= Sim::kMaxPlayers && s.player(o).faction == "ooni"; }
  static bool is_ooni(Sim& s, EntityId e) {
    const Health* h = s.world().get<Health>(e);
    return h && owner_ooni(s, h->owner) && (s.world().get<Unit>(e) || s.world().get<Building>(e));
  }
  static const content::Value* mech(const EntityDef& d) { return d.raw ? d.raw->find("Mechanics") : nullptr; }
  static bool no_light(const EntityDef& d) {
    const content::Value* pl = d.raw ? d.raw->find("Placement") : nullptr;
    const content::Value* nl = pl ? pl->find("no_light") : nullptr;
    return nl && nl->is_bool() && nl->as_bool();
  }
  // fényforrás-e és mekkora a köre; a lámpás épülve is világít (az építéshez)
  static Fixed light_radius(const EntityDef& d) {
    const content::Value* m = mech(d);
    return m ? fnum(m->find("light_radius"), Fixed{}) : Fixed{};
  }
  bool in_light(Sim& s, uint8_t owner, Fixed x, Fixed y, bool for_build) {
    bool lit = false;
    s.world().pool<Building>().each([&](EntityId b, const Building& bd) {
      if (lit) return;
      const Health* h = s.world().get<Health>(b);
      if (!h || h->owner != owner || h->hp <= 0) return;
      const EntityDef& d = s.rules()->defs[bd.def];
      const Fixed r = light_radius(d);
      if (r.raw() <= 0) return;
      const content::Value* m = mech(d);
      const content::Value* ln = m ? m->find("lantern") : nullptr;
      if (!bd.complete && !(for_build && ln && ln->is_bool() && ln->as_bool())) return;
      const Position& p = *s.world().get<Position>(b);
      if (core::length(p.x - x, p.y - y) <= r) lit = true;
    });
    return lit;
  }
  int light_heal_at(Sim& s, uint8_t owner, Fixed x, Fixed y) {
    int heal = 0;
    bool found = false;
    s.world().pool<Building>().each([&](EntityId b, const Building& bd) {   // az első (a legkisebb azonosítójú) kész fényforrás
      if (found) return;
      const Health* h = s.world().get<Health>(b);
      if (!h || h->owner != owner || h->hp <= 0 || !bd.complete) return;
      const EntityDef& d = s.rules()->defs[bd.def];
      const Fixed r = light_radius(d);
      if (r.raw() <= 0) return;
      const Position& p = *s.world().get<Position>(b);
      if (core::length(p.x - x, p.y - y) > r) return;
      found = true;
      heal = fnum(mech(d)->find("light_heal"), Fixed{}).floor_int();
    });
    return heal;
  }
};

}  // namespace

std::unique_ptr<FactionModule> make_ooni_module() { return std::make_unique<Module>(); }

}  // namespace totu::sim
