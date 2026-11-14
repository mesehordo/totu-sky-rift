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
//   - Védőfény (`ward`): a körben a saját egységek ideiglenes pajzsot kapnak;
//   - Torii-kapupár (`ooni.torii` parancs): a kapuk lerakási sorrendben párt alkotnak (legfeljebb `torii_pairs` pár),
//     a pár második kapuja csak látott helyre kerülhet (fény nélkül is); a belépő 2 s-ot vár, majd (a kapu 8 s-os
//     újratöltése után) a párnál jelenik meg; utána mindkét kapu tölt; ha a pár elpusztul, a várakozók kijönnek;
//   - őrök: 30 tickenként a saját épület páncélbónusza a 2,5 csempén belüli Kőkutyák (`kokutya_guard`) és a bekapcsolt
//     Kapuőrzés (+3) összege, legfeljebb 6;
//   - kutatási szabályok (`ResearchDef::raw` → `Rules`): `light_add` (a fény sugara +), `lantern_magic` (fényben a
//     varázssebzés ×), `morale_hurt_mult`, `drum_mult`, `torii_pairs`, `kokutya_guard`;
//   - Tanuki tárgylelet (`Worker.find_items`): minden kibányászott rakománynál `chance` eséllyel egy tárgy a földre.
#include <algorithm>
#include <array>
#include <cstdint>

#include "content/json.hpp"
#include "map/map.hpp"
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

// Torii-kapu: a párja (0 = nincs) és az újratöltés vége
struct ToriiGate {
  EntityId pair = 0;
  int64_t ready = 0;
  void hash(core::StateHash& h) const { h.add_u64(pair); h.add_i64(ready); }
  void save(core::ByteWriter& w) const { w.u32(pair); w.i64(ready); }
  static ToriiGate load(core::ByteReader& r) { ToriiGate g; g.pair = r.u32(); g.ready = r.i64(); return g; }
};

// a kapu felé tartó (`wait` = 0) vagy a kapuban váró egység (`wait`: ettől a ticktől kelhet át)
struct ToriiTrip {
  EntityId gate = 0;
  int64_t wait = 0;
  void hash(core::StateHash& h) const { h.add_u64(gate); h.add_i64(wait); }
  void save(core::ByteWriter& w) const { w.u32(gate); w.i64(wait); }
  static ToriiTrip load(core::ByteReader& r) { ToriiTrip t; t.gate = r.u32(); t.wait = r.i64(); return t; }
};

// őrpáncél (30 tickenként újraszámolva)
struct Guard {
  int32_t armor = 0;
  void hash(core::StateHash& h) const { h.add_i64(armor); }
  void save(core::ByteWriter& w) const { w.i32(armor); }
  static Guard load(core::ByteReader& r) { Guard g; g.armor = r.i32(); return g; }
};

// tárgyleletes munkás: a gyűjtés előző tickbeli állapota (a kibányászott rakomány felismeréséhez)
struct Finder {
  uint8_t prev = 0;
  void hash(core::StateHash& h) const { h.add_u64(prev); }
  void save(core::ByteWriter& w) const { w.u8(prev); }
  static Finder load(core::ByteReader& r) { Finder f; f.prev = r.u8(); return f; }
};

// determinisztikus keverés (a közös véletlenszám-folyamokat nem érinti)
uint64_t mix(uint64_t x) {
  x += 0x9E3779B97F4A7C15ull;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
  return x ^ (x >> 31);
}

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
    w.register_component<ToriiGate>("OoniTorii");
    w.register_component<ToriiTrip>("OoniToriiTrip");
    w.register_component<Guard>("OoniGuard");
    w.register_component<Finder>("OoniFinder");
  }

  void on_unit_spawned(Sim& s, EntityId e) override {
    if (!load(s) || !is_ooni(s, e)) return;
    const EntityDef& d = def_of(s, e);
    if (find_items(d)) s.world().add(e, Finder{});
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
    if (d.faction != "ooni") return true;
    if (is_torii(d)) {   // kapupár: a darabszám és a pár második kapuja
      int n = 0;
      s.world().pool<ToriiGate>().each([&](EntityId g, const ToriiGate&) {
        const Health* gh = s.world().get<Health>(g);
        if (gh && gh->owner == owner && gh->hp > 0) ++n;
      });
      if (n >= 2 * prules(s, owner).torii_pairs) {
        if (error) *error = "nem építhető több Torii-kapupár";
        return false;
      }
      if (n % 2 == 1) {   // a második kapu: csak látott helyre (fény nélkül is)
        if (tile_visible(s, owner, tx + std::max(1, d.fw) / 2, ty + std::max(1, d.fh) / 2)) return true;
        if (error) *error = "a kapu párja csak látott helyre építhető";
        return false;
      }
    }
    if (no_light(d)) return true;
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
    if (!near) return Fixed::from_int(1);
    // a fejlődési irány szabálya felülírja (`Paths.<irány>.rules.build_boost_mult`; a Vörös dojó 1,75)
    const std::string& path = s.player(h->owner).path;
    const FactionDef* f = s.rules()->faction(s.player(h->owner).faction);
    const content::Value* ps = f && f->raw && !path.empty() ? f->raw->find("Paths") : nullptr;
    const content::Value* pd = ps ? ps->find(path) : nullptr;
    const content::Value* pr = pd ? pd->find("rules") : nullptr;
    return fnum(pr ? pr->find("build_boost_mult") : nullptr, boost_mult_);
  }

  // Kőkutya, Kapuőrzés
  int32_t armor_add(Sim& s, EntityId t) override {
    const Guard* g = s.world().get<Guard>(t);
    return g ? g->armor : 0;
  }

  // lámpásmágia: a fényben álló ooni varázssebzése ×
  Fixed damage_taken_mult(Sim& s, EntityId src, EntityId, bool, bool magic) override {
    if (!magic || !src || !s.world().alive(src) || !load(s)) return Fixed::from_int(1);
    const Health* h = s.world().get<Health>(src);
    const Position* p = s.world().get<Position>(src);
    if (!h || !p || !owner_ooni(s, h->owner)) return Fixed::from_int(1);
    const Fixed m = prules(s, h->owner).lantern_magic;
    if (m == Fixed::from_int(1) || !in_light(s, h->owner, p->x, p->y, false)) return Fixed::from_int(1);
    return m;
  }

  bool hidden(Sim& s, EntityId e) override {   // a kapuban váró nem látszik
    const ToriiTrip* t = s.world().get<ToriiTrip>(e);
    return t && t->wait > 0;
  }

  void on_building_placed(Sim& s, EntityId b) override {   // a kapuk lerakási sorrendben párt alkotnak
    if (!load(s) || !is_ooni(s, b) || !is_torii(def_of(s, b))) return;
    const uint8_t owner = s.world().get<Health>(b)->owner;
    EntityId mate = 0;
    s.world().pool<ToriiGate>().each([&](EntityId g, const ToriiGate& tg) {
      const Health* gh = s.world().get<Health>(g);
      if (!mate && g != b && tg.pair == 0 && gh && gh->owner == owner && gh->hp > 0) mate = g;
    });
    s.world().add(b, ToriiGate{mate, 0});
    if (mate) s.world().get<ToriiGate>(mate)->pair = b;
  }

  // `ooni.torii`: a `units` a `target` kapuhoz mennek, és átkelnek a párjához
  bool on_command(Sim& s, const Command& c) override {
    if (c.def != "ooni.torii") return false;
    const ToriiGate* g = s.world().alive(c.target) ? s.world().get<ToriiGate>(c.target) : nullptr;
    const Health* gh = g ? s.world().get<Health>(c.target) : nullptr;
    const Building* gb = g ? s.world().get<Building>(c.target) : nullptr;
    if (!g || !gh || !gb || gh->hp <= 0 || !gb->complete || !g->pair) return true;
    const Position gp = *s.world().get<Position>(c.target);
    for (const EntityId u : c.units) {
      if (!s.world().alive(u) || !s.world().get<Unit>(u)) continue;
      const Health* uh = s.world().get<Health>(u);
      if (!uh || uh->owner != gh->owner || uh->hp <= 0) continue;
      if (ToriiTrip* t = s.world().get<ToriiTrip>(u)) *t = ToriiTrip{c.target, 0};
      else s.world().add(u, ToriiTrip{c.target, 0});
      s.order_move(u, gp.x, gp.y);
    }
    return true;
  }

  Fixed damage_mult(Sim& s, EntityId a, EntityId) override {
    const Morale* m = s.world().get<Morale>(a);
    return m && m->broken ? broken_damage_ : Fixed::from_int(1);
  }

  Fixed speed_mult(Sim& s, EntityId e) override {
    const Morale* m = s.world().get<Morale>(e);
    return m && m->broken ? broken_speed_ : Fixed::from_int(1);
  }

  void on_death(Sim& s, EntityId v, EntityId killer, uint8_t) override {
    if (!load(s)) return;
    // Vérszomj (kutatás, `kill_heal`; a régi G 1222): az oni harcos ölésenként életerőt kap vissza
    if (killer != v && s.world().alive(killer) && s.world().get<Unit>(killer) && is_ooni(s, killer)) {
      Health* kh = s.world().get<Health>(killer);
      const PRules& r = prules(s, kh->owner);
      const uint16_t kd = s.world().get<Unit>(killer)->def;
      if (kh->hp > 0 && r.kill_heal_hp > 0 && std::find(r.kill_heal_ids.begin(), r.kill_heal_ids.end(), kd) != r.kill_heal_ids.end()) {
        kh->hp = std::min(kh->max, kh->hp + r.kill_heal_hp);
      }
    }
    if (!is_ooni(s, v)) return;
    if (ToriiGate* g = s.world().get<ToriiGate>(v)) {   // a pár egyedül marad (a várakozókat a tick engedi ki)
      if (ToriiGate* p = s.world().alive(g->pair) ? s.world().get<ToriiGate>(g->pair) : nullptr; p && p->pair == v) p->pair = 0;
      g->pair = 0;
    }
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
        const int64_t loss = ((m.last_eff - eff) * hurt_ / h->max * prules(s, h->owner).hurt_mult.raw()) >> 16;
        m.value = static_cast<int32_t>(std::max<int64_t>(0, m.value - loss));
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
            add += static_cast<int32_t>((int64_t{drum_regen_} * 500 * prules(s, h->owner).drum_mult.raw()) >> 16);
            break;
          }
        }
        m.value = std::min(max_ * 1000, m.value + add);
        if (in_light(s, h->owner, p->x, p->y, false)) m.value = std::max(m.value, light_min_ * 1000);
        if (m.broken && m.value >= break_until_ * 1000) m.broken = 0;
      });
    }
    torii_tick(s);
    finder_tick(s);
    if (now % 30 == 0) guard_tick(s);
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
  // a kész kutatások szabályai játékosonként (származtatott, nem állapot: a kész kutatások számával frissül)
  struct PRules {
    size_t key = SIZE_MAX;
    Fixed light_add, lantern_magic = Fixed::from_int(1), drum_mult = Fixed::from_int(1), hurt_mult = Fixed::from_int(1);
    int32_t torii_pairs = 1, kokutya_guard = 0;
    std::vector<uint16_t> kill_heal_ids;   // Vérszomj (`kill_heal`): ölésenként ennyi életerő ezeknek
    int32_t kill_heal_hp = 0;
  };
  std::array<PRules, Sim::kMaxPlayers + 1> pr_{};
  const PRules& prules(Sim& s, uint8_t o) {
    const int i = std::min<int>(o, Sim::kMaxPlayers);
    PRules& r = pr_[static_cast<size_t>(i)];
    const auto& done = s.player(i).researched;
    if (r.key == done.size()) return r;
    r = PRules{};
    r.key = done.size();
    for (const std::string& id : done) {
      const ResearchDef* rd = s.rules() ? s.rules()->research(id) : nullptr;
      const content::Value* ru = rd && rd->raw ? rd->raw->find("Rules") : nullptr;
      if (!ru) continue;
      r.light_add += fnum(ru->find("light_add"), Fixed{});
      r.lantern_magic *= fnum(ru->find("lantern_magic"), Fixed::from_int(1));
      r.drum_mult *= fnum(ru->find("drum_mult"), Fixed::from_int(1));
      r.hurt_mult *= fnum(ru->find("morale_hurt_mult"), Fixed::from_int(1));
      r.torii_pairs = std::max(r.torii_pairs, fnum(ru->find("torii_pairs"), Fixed{}).floor_int());
      r.kokutya_guard += fnum(ru->find("kokutya_guard"), Fixed{}).floor_int();
      if (const content::Value* kh = ru->find("kill_heal"); kh && kh->is_object()) {
        r.kill_heal_hp = fnum(kh->find("hp"), Fixed::from_int(20)).floor_int();
        if (const content::Value* ids = kh->find("ids"); ids && ids->is_array()) {
          for (const auto& x : ids->items()) {
            if (x.is_string() && s.rules()->def(x.text()) != kNoDef) r.kill_heal_ids.push_back(s.rules()->def(x.text()));
          }
        }
      }
    }
    return r;
  }

  static constexpr int64_t kToriiWait = 60, kToriiCooldown = 240;   // 2 s, 8 s
  static constexpr int32_t kGateGuard = 3, kGuardMax = 6;

  // Torii: odaérés, várakozás, átkelés; a pár nélküli kapuból a várakozók kijönnek
  void torii_tick(Sim& s) {
    const int64_t now = s.tick();
    auto gate_ok = [&](EntityId g) {
      if (!s.world().alive(g)) return false;
      const Health* h = s.world().get<Health>(g);
      const Building* b = s.world().get<Building>(g);
      return h && b && h->hp > 0 && b->complete && s.world().get<ToriiGate>(g);
    };
    s.world().pool<ToriiGate>().each([&](EntityId g, ToriiGate& tg) {   // a megszűnt párok bontása
      if (!tg.pair) return;
      const ToriiGate* p = s.world().alive(tg.pair) ? s.world().get<ToriiGate>(tg.pair) : nullptr;
      const Health* ph = p ? s.world().get<Health>(tg.pair) : nullptr;
      if (!p || p->pair != g || !ph || ph->hp <= 0) tg.pair = 0;
    });
    std::vector<EntityId> drop;
    std::vector<std::pair<EntityId, EntityId>> go;   // (egység, kapu)
    s.world().pool<ToriiTrip>().each([&](EntityId u, ToriiTrip& t) {
      const Health* uh = s.world().get<Health>(u);
      if (!uh || uh->hp <= 0 || !gate_ok(t.gate) || !s.world().get<ToriiGate>(t.gate)->pair) {
        drop.push_back(u);
        return;
      }
      const Position& up = *s.world().get<Position>(u);
      const Movement* m = s.world().get<Movement>(u);
      if (t.wait > 0) {   // a kapuban: új mozgásparancsra kilép
        if (m && m->moving) drop.push_back(u);
        else if (now >= t.wait) go.push_back({u, t.gate});
        return;
      }
      const Position& gp = *s.world().get<Position>(t.gate);
      const Building& gb = *s.world().get<Building>(t.gate);
      const Fixed reach = Fixed::from_ratio(std::max(gb.w, gb.h), 2) + Fixed::from_ratio(3, 2);
      if (core::length(up.x - gp.x, up.y - gp.y) <= reach) {
        s.stop(u);
        t.wait = now + kToriiWait;
      } else if (!m || !m->moving) {
        drop.push_back(u);   // megállt vagy más parancsot kapott
      }
    });
    for (const EntityId u : drop) s.world().pool<ToriiTrip>().remove(u);
    // átkelés: kapunként a kész várakozók együtt, ha a kapu nem tölt
    std::vector<EntityId> fired;
    for (const auto& [u, g] : go) {
      const ToriiGate& tg = *s.world().get<ToriiGate>(g);
      const bool now_fired = std::find(fired.begin(), fired.end(), g) != fired.end();
      if ((now < tg.ready && !now_fired) || !gate_ok(tg.pair)) continue;
      const Position pp = *s.world().get<Position>(tg.pair);
      s.place_at_free(u, pp.x, pp.y);
      s.world().pool<ToriiTrip>().remove(u);
      if (!now_fired) fired.push_back(g);
    }
    for (const EntityId g : fired) {
      ToriiGate& tg = *s.world().get<ToriiGate>(g);
      tg.ready = now + kToriiCooldown;
      s.world().get<ToriiGate>(tg.pair)->ready = now + kToriiCooldown;
    }
  }

  // őrök: a saját épületek páncélbónusza újra (Kőkutya, Kapuőrzés), 2,5 csempén belül, legfeljebb 6
  void guard_tick(Sim& s) {
    struct Src { uint8_t owner; Fixed x, y; int32_t armor; };
    std::vector<Src> src;
    const uint16_t dog = s.rules()->def("oon_kokutya");
    const int gate_ab = s.rules()->ability_index("oon_kapuorzes");
    s.world().pool<Unit>().each([&](EntityId e, const Unit& u) {
      const Health* h = s.world().get<Health>(e);
      if (!h || h->hp <= 0 || !owner_ooni(s, h->owner)) return;
      int32_t a = 0;
      if (u.def == dog) a += prules(s, h->owner).kokutya_guard;
      if (gate_ab >= 0 && s.toggled(e, static_cast<uint16_t>(gate_ab))) {
        const AbilityDef& ad = s.rules()->abilities[static_cast<size_t>(gate_ab)];
        a += fnum(ad.spec ? ad.spec->find("guard_armor") : nullptr, Fixed::from_int(kGateGuard)).floor_int();
      }
      if (a <= 0) return;
      const Position& p = *s.world().get<Position>(e);
      src.push_back({h->owner, p.x, p.y, a});
    });
    const Fixed radius = Fixed::from_ratio(5, 2);
    std::vector<std::pair<EntityId, int32_t>> set;
    s.world().pool<Building>().each([&](EntityId b, const Building&) {
      const Health* h = s.world().get<Health>(b);
      if (!h || !owner_ooni(s, h->owner)) return;
      const Position& p = *s.world().get<Position>(b);
      int32_t a = 0;
      for (const Src& g : src) {
        if (g.owner == h->owner && core::length(g.x - p.x, g.y - p.y) <= radius) a += g.armor;
      }
      if (a > 0 || s.world().get<Guard>(b)) set.push_back({b, std::min(a, kGuardMax)});
    });
    for (const auto& [b, a] : set) {
      if (a <= 0) s.world().pool<Guard>().remove(b);
      else if (Guard* g = s.world().get<Guard>(b)) g->armor = a;
      else s.world().add(b, Guard{a});
    }
  }

  // Tanuki tárgylelet: a kibányászott rakománynál (munka → visszaút) `chance` eséllyel egy tárgy a lábához
  void finder_tick(Sim& s) {
    std::vector<std::pair<EntityId, uint16_t>> found;
    s.world().pool<Finder>().each([&](EntityId e, Finder& f) {
      const Worker* w = s.world().get<Worker>(e);
      const Unit* u = s.world().get<Unit>(e);
      if (!w || !u) return;
      if (f.prev == Worker::Work && w->state == Worker::Return && w->carry > 0) {
        const content::Value* fi = find_items(s.rules()->defs[u->def]);
        const content::Value* pool = fi ? fi->find("pool") : nullptr;
        const int64_t chance = milli(fnum(fi ? fi->find("chance") : nullptr, Fixed{}));   // ezrelék
        const uint64_t r = mix((static_cast<uint64_t>(s.tick()) << 32) ^ e);
        if (pool && pool->is_array() && !pool->items().empty() && static_cast<int64_t>(r % 1000) < chance) {
          const content::Value& it = pool->items()[(r >> 20) % pool->items().size()];
          const int idx = it.is_string() ? s.rules()->item_index(it.text()) : -1;
          if (idx >= 0) found.push_back({e, static_cast<uint16_t>(idx)});
        }
      }
      f.prev = w->state;
    });
    for (const auto& [e, item] : found) {
      const Position p = *s.world().get<Position>(e);
      s.drop_ground_item(item, p.x, p.y);
    }
  }

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
  static bool is_torii(const EntityDef& d) {
    const content::Value* m = mech(d);
    const content::Value* t = m ? m->find("torii") : nullptr;
    return t && t->is_bool() && t->as_bool();
  }
  static const content::Value* find_items(const EntityDef& d) {
    const content::Value* w = d.raw ? d.raw->find("Worker") : nullptr;
    return w ? w->find("find_items") : nullptr;
  }
  static bool tile_visible(Sim& s, uint8_t owner, int x, int y) {
    const auto& v = s.player(owner).visible;
    if (v.empty() || !s.map_ || s.fog_ == MatchSetup::Fog::None) return true;
    if (x < 0 || y < 0 || x >= s.map_->width() || y >= s.map_->height()) return false;
    return v[static_cast<size_t>(y * s.map_->width() + x)] != 0;
  }
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
      Fixed r = light_radius(d);
      if (r.raw() <= 0) return;
      r += prules(s, owner).light_add;
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
      Fixed r = light_radius(d);
      if (r.raw() <= 0) return;
      r += prules(s, owner).light_add;
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
