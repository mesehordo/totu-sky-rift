// SPDX-License-Identifier: GPL-3.0-or-later
// Micéliák (MECH-NET-1–2, D28: fonalgráf, spórák, osztódás; összeolvadás) – M7, ADR-008; a régi játék
// (G 1179–1190, 1346–1380, 2262–2267, 2778–2815, 2917–2950, 2976–2986) és a spec/FAJMECHANIKAK.md 5. része szerint.
// A játszható főnép a `micelia` modban van (D28: `faction_mycel`, `mechanic: network`); a modul a `network`
// mechanikájú népekre hat, az adatot a nyers JSON-ból olvassa.
//   - fonalgráf: a kész `network_core` épületekből bejárás a kész épületeken; két épület össze van kötve, ha a
//     középpontjuk távolsága ≤ max(link_radius) (alap 4); a nem kapcsolt kész épület leáll (a termelése nem halad);
//   - mező: a kapcsolt kész épületek `field_radius` (alap 4) körében a csempék; a mezőn a faji gyógyulás
//     × `creep_regen_mult` (a nép adatából, különben a `game.json`-ból: 3) × a szabály `creep_regen_mult`;
//   - létszám: minden kapcsolt kész épület + `network_supply` + a szabály `network_supply_add` (kutatás, irány);
//   - lerakás: a nem mag épület középpontja egy kész, kapcsolt saját épület max(link_radius) körében;
//   - spórák: egységenként legfeljebb 6; +1 ölésért, 10 s-onként +1, ha kapcsolt saját épület 8 csempén belül van;
//   - osztódás (`split_kills`): ennyi ölés után, ha van létszám, kettéválik (fél-fél életerő, az iker a spórák felét
//     kapja, véletlen irányban 2 × sugárnyira);
//   - összeolvadás (`merge`): `count` egység → `into` (átlagolt életerő-aránnyal); ha messze vannak, összegyűlnek.
#include <algorithm>
#include <map>
#include <optional>
#include <utility>

#include "core/angle.hpp"
#include "sim/faction_module.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

// spóratöltet és az osztódáshoz számolt ölések (a hash és a mentés része)
struct Spores {
  uint8_t spores = 0, meals = 0;
  void hash(core::StateHash& h) const { h.add_u64(spores); h.add_u64(meals); }
  void save(core::ByteWriter& w) const { w.u8(spores); w.u8(meals); }
  static Spores load(core::ByteReader& r) {
    Spores s;
    s.spores = r.u8();
    s.meals = r.u8();
    return s;
  }
};

constexpr int kMaxSpores = 6;
constexpr int kSporeTicks = 300;   // 10 s
const Fixed kFieldRegenDefault = Fixed::from_int(3);   // tartalék, ha sem a nép, sem a `game.json` nem adja

const content::Value* faction_obj(const FactionDef& f) { return f.raw ? f.raw->find("Faction") : nullptr; }

Fixed num(const content::Value* v, Fixed def);

// a nép kulcsa, különben a `game.json` azonos kulcsa, különben a tartalék
Fixed tuning(const Rules& r, const FactionDef& f, const char* key, Fixed def) {
  const content::Value* o = faction_obj(f);
  if (const content::Value* v = o ? o->find(key) : nullptr) return num(v, def);
  return num(r.game_raw ? r.game_raw->find(key) : nullptr, def);
}

// egy szabálykulcs a kész kutatásokból (`Rules`) és a választott irányból (`Paths.<irány>.rules`)
template <class Fold>
void each_rule(const Rules& r, const FactionDef& f, const PlayerState& p, const char* key, Fold fold) {
  for (const std::string& id : p.researched) {
    const ResearchDef* rd = r.research(id);
    const content::Value* rr = rd && rd->raw ? rd->raw->find("Rules") : nullptr;
    if (const content::Value* v = rr ? rr->find(key) : nullptr) fold(v);
  }
  const content::Value* ps = !p.path.empty() && f.raw ? f.raw->find("Paths") : nullptr;
  const content::Value* pp = ps ? ps->find(p.path) : nullptr;
  const content::Value* pr = pp ? pp->find("rules") : nullptr;
  if (const content::Value* v = pr ? pr->find(key) : nullptr) fold(v);
}

std::string fstr(const FactionDef& f, const char* key) {
  const content::Value* o = faction_obj(f);
  const content::Value* v = o ? o->find(key) : nullptr;
  return v && v->is_string() ? v->text() : std::string();
}

int32_t fint(const FactionDef& f, const char* key) {
  const content::Value* o = faction_obj(f);
  const content::Value* v = o ? o->find(key) : nullptr;
  const auto i = v && v->is_number() ? v->as_int() : std::nullopt;
  return i ? static_cast<int32_t>(*i) : 0;
}

Fixed num(const content::Value* v, Fixed def) {
  const auto f = v && v->is_number() ? v->as_fixed() : std::nullopt;
  return f ? *f : def;
}

const content::Value* mech(const EntityDef& d) { return d.raw ? d.raw->find("Mechanics") : nullptr; }

bool is_core(const EntityDef& d) {
  const content::Value* m = mech(d);
  const content::Value* c = m ? m->find("network_core") : nullptr;
  return c && c->is_bool() && c->as_bool();
}

Fixed link_radius(const EntityDef& d) {
  const content::Value* m = mech(d);
  return num(m ? m->find("link_radius") : nullptr, Fixed::from_int(4));
}

Fixed field_radius(const EntityDef& d) {
  const content::Value* m = mech(d);
  return num(m ? m->find("field_radius") : nullptr, Fixed::from_int(4));
}

// a képesség `Spec` egy kulcsa
const content::Value* spec_key(const AbilityDef& a, const char* key) { return a.spec ? a.spec->find(key) : nullptr; }

class Module final : public FactionModule {
 public:
  const char* name() const override { return "micelia"; }

  void register_components(World& w) override { w.register_component<Spores>("MicSpores"); }

  // --- fonalgráf (származtatott, nem állapot: a kész saját épületek listájából; csak ha az változik, újra bejárja) ---
  struct Net {
    std::vector<EntityId> done;        // a kész, élő saját épületek (rendezve)
    std::vector<EntityId> connected;   // ebből a kapcsoltak (rendezve)
  };

  bool network(Sim& s, uint8_t owner) {
    if (owner < 1 || owner > Sim::kMaxPlayers || !s.rules_) return false;
    const FactionDef* f = s.rules_->faction(s.players_[owner].faction);
    return f && fstr(*f, "mechanic") == "network";
  }

  const Net& net(Sim& s, uint8_t owner) {
    std::vector<EntityId> done;
    s.world_.pool<Building>().each([&](EntityId e, const Building& b) {
      const Health* h = s.world_.get<Health>(e);
      if (b.complete && h && h->owner == owner && h->hp > 0) done.push_back(e);
    });
    std::sort(done.begin(), done.end());
    Net& n = nets_[owner];
    if (n.done == done && !n.done.empty()) return n;
    std::vector<EntityId> stack;
    for (const EntityId e : done) {
      if (is_core(s.rules_->defs[s.world_.get<Building>(e)->def])) stack.push_back(e);
    }
    std::vector<EntityId> seen = stack;
    while (!stack.empty()) {
      const EntityId a = stack.back();
      stack.pop_back();
      const Position pa = *s.world_.get<Position>(a);
      const Fixed la = link_radius(s.rules_->defs[s.world_.get<Building>(a)->def]);
      for (const EntityId o : done) {
        if (std::find(seen.begin(), seen.end(), o) != seen.end()) continue;
        const Position& po = *s.world_.get<Position>(o);
        const Fixed lr = std::max(la, link_radius(s.rules_->defs[s.world_.get<Building>(o)->def]));
        if (core::length(pa.x - po.x, pa.y - po.y) <= lr) {
          seen.push_back(o);
          stack.push_back(o);
        }
      }
    }
    std::sort(seen.begin(), seen.end());
    n.done = std::move(done);
    n.connected = std::move(seen);
    return n;
  }

  bool connected(Sim& s, EntityId b) {
    const Health* h = s.world_.get<Health>(b);
    if (!h) return false;
    const auto& c = net(s, h->owner).connected;
    return std::binary_search(c.begin(), c.end(), b);
  }

  bool in_field(Sim& s, uint8_t owner, Fixed x, Fixed y) {
    const int tx = x.floor_int(), ty = y.floor_int();   // a csempe közepe (a régi rács szerint)
    const Fixed cx = Fixed::from_int(tx) + Fixed::from_ratio(1, 2), cy = Fixed::from_int(ty) + Fixed::from_ratio(1, 2);
    for (const EntityId b : net(s, owner).connected) {
      const Position& bp = *s.world_.get<Position>(b);
      if (core::length(cx - bp.x, cy - bp.y) <= field_radius(s.rules_->defs[s.world_.get<Building>(b)->def])) return true;
    }
    return false;
  }

  // --- horgok ---------------------------------------------------------------------------------------------------------
  int32_t supply_bonus(Sim& s, uint8_t owner) override {
    if (!network(s, owner)) return 0;
    // minden kapcsolt épület: `network_supply` + a szabály `network_supply_add` (kutatás, irány)
    const FactionDef* f = s.rules_->faction(s.players_[owner].faction);
    int32_t per = fint(*f, "network_supply");
    each_rule(*s.rules_, *f, s.players_[owner], "network_supply_add", [&](const content::Value* v) {
      per += static_cast<int32_t>(v->as_int().value_or(0));
    });
    return per * static_cast<int32_t>(net(s, owner).connected.size());
  }

  bool can_place(Sim& s, uint8_t owner, uint16_t def, int tx, int ty, std::string* error) override {
    if (!network(s, owner) || def >= s.rules_->defs.size()) return true;
    const EntityDef& d = s.rules_->defs[def];
    if (is_core(d)) return true;
    const Fixed cx = Fixed::from_int(tx) + Fixed::from_ratio(std::max(1, d.fw), 2);
    const Fixed cy = Fixed::from_int(ty) + Fixed::from_ratio(std::max(1, d.fh), 2);
    for (const EntityId b : net(s, owner).connected) {
      const Position& bp = *s.world_.get<Position>(b);
      const Fixed lr = std::max(link_radius(d), link_radius(s.rules_->defs[s.world_.get<Building>(b)->def]));
      if (core::length(cx - bp.x, cy - bp.y) <= lr) return true;
    }
    if (error) *error = "a hálózattól távol (fonal kell)";
    return false;
  }

  Fixed regen_add(Sim& s, EntityId e) override {
    const Health* h = s.world_.get<Health>(e);
    const Position* p = s.world_.get<Position>(e);
    if (!h || !p || !network(s, h->owner) || !s.world_.get<Unit>(e)) return Fixed{};
    if (!in_field(s, h->owner, p->x, p->y)) return Fixed{};
    const FactionDef* f = s.rules_->faction(s.players_[h->owner].faction);
    // a mezőn a faji gyógyulás × `creep_regen_mult` (nép, különben `game.json`) × a szabály `creep_regen_mult`
    Fixed mult = tuning(*s.rules_, *f, "creep_regen_mult", kFieldRegenDefault);
    each_rule(*s.rules_, *f, s.players_[h->owner], "creep_regen_mult",
              [&](const content::Value* v) { mult = mult * num(v, Fixed::from_int(1)); });
    return f->regen * (mult - Fixed::from_int(1));
  }

  void on_unit_spawned(Sim& s, EntityId e) override {
    const Health* h = s.world_.get<Health>(e);
    if (!h || h->owner < 1 || h->owner > Sim::kMaxPlayers) return;
    const FactionDef* f = s.rules_->faction(s.players_[h->owner].faction);
    if (f && fstr(*f, "spell_resource") == "spores") s.world_.add(e, Spores{});
  }

  void on_death(Sim& s, EntityId victim, EntityId killer, uint8_t) override {
    if (!killer || !s.world_.alive(killer) || !s.world_.get<Unit>(victim)) return;
    Spores* sp = s.world_.get<Spores>(killer);
    const Health* kh = s.world_.get<Health>(killer);
    const Health* vh = s.world_.get<Health>(victim);
    if (!sp || !kh || !vh || kh->hp <= 0 || !s.hostile(kh->owner, vh->owner)) return;
    sp->spores = static_cast<uint8_t>(std::min(kMaxSpores, sp->spores + 1));
    if (split_kills(s, killer) > 0) sp->meals = static_cast<uint8_t>(std::min(255, sp->meals + 1));
  }

  // a képesség spóra-ára (a régi `ab_ok`/`ab_pay`: `spores`)
  static int spore_cost(const AbilityDef& a) {
    const content::Value* v = spec_key(a, "spores");
    const auto i = v && v->is_number() ? v->as_int() : std::nullopt;
    return i ? static_cast<int>(*i) : 0;
  }
  bool has_ability_costs() const override { return true; }
  bool ability_affordable(Sim& s, EntityId e, const AbilityDef& a) override {
    const int c = spore_cost(a);
    if (c <= 0) return true;
    const Spores* sp = s.world_.get<Spores>(e);
    return sp && sp->spores >= c;
  }
  void ability_paid(Sim& s, EntityId e, const AbilityDef& a) override {
    const int c = spore_cost(a);
    if (Spores* sp = c > 0 ? s.world_.get<Spores>(e) : nullptr) sp->spores = static_cast<uint8_t>(std::max(0, sp->spores - c));
  }

  int split_kills(Sim& s, EntityId e) {
    for (const uint16_t ab : s.abilities_of(e)) {
      const content::Value* v = spec_key(s.rules_->abilities[ab], "split_kills");
      if (const auto i = v && v->is_number() ? v->as_int() : std::nullopt) return static_cast<int>(*i);
    }
    return 0;
  }

  // osztódás: ha van létszám; az iker véletlen irányban, 2 × sugárnyira
  void try_split(Sim& s, EntityId e) {
    Spores* sp = s.world_.get<Spores>(e);
    const int n = split_kills(s, e);
    if (!sp || n <= 0 || sp->meals < n) return;
    const Health h = *s.world_.get<Health>(e);
    const uint16_t def = s.world_.get<Unit>(e)->def;
    const EntityDef& d = s.rules_->defs[def];
    const PlayerState& p = s.players_[h.owner];
    if (p.supply_used + d.supply > p.supply_cap) return;   // majd ha lesz hely
    sp->meals = 0;
    const uint8_t half_spores = static_cast<uint8_t>(sp->spores / 2);
    const Position pos = *s.world_.get<Position>(e);
    const core::Angle a = core::Angle::from_units(static_cast<int32_t>(s.combat_rng_.next() & 0xFFFF));
    const Fixed tx = pos.x + core::cos(a) * d.radius * Fixed::from_int(2);
    const Fixed ty = pos.y + core::sin(a) * d.radius * Fixed::from_int(2);
    s.world_.get<Health>(e)->hp = std::max(1, h.max / 2);
    const EntityId twin = s.spawn_unit(def, h.owner, tx, ty);   // (a spawn után a régi mutatók érvénytelenek)
    if (twin == kNoEntity) return;
    s.place_at_free(twin, tx, ty);
    Health& th = *s.world_.get<Health>(twin);
    th.hp = std::max(1, th.max / 2);
    if (Spores* ts = s.world_.get<Spores>(twin)) ts->spores = half_spores;
    s.players_[h.owner].supply_used += d.supply;
  }

  void tick(Sim& s) override {
    if (!s.rules_) return;
    // a leszakadt kész épület leáll: a termelése nem halad (a termelés e tickben már lépett – visszavesszük)
    std::vector<EntityId> stalled;
    s.world_.pool<Producer>().each([&](EntityId e, Producer& pr) {
      const Health* h = s.world_.get<Health>(e);
      const Building* b = s.world_.get<Building>(e);
      if (!h || !b || !b->complete || pr.queue.empty() || !network(s, h->owner)) return;
      if (!connected(s, e)) stalled.push_back(e);
    });
    for (const EntityId e : stalled) {
      QueueItem& q = s.world_.get<Producer>(e)->queue.front();
      q.progress = std::max(0, q.progress - 1);
    }
    // spórák a hálózat közelében, 10 s-onként
    if (s.tick_ % kSporeTicks == 0) {
      s.world_.pool<Spores>().each([&](EntityId e, Spores& sp) {
        const Health* h = s.world_.get<Health>(e);
        const Position* p = s.world_.get<Position>(e);
        if (!h || !p || h->hp <= 0 || sp.spores >= kMaxSpores) return;
        for (const EntityId b : net(s, h->owner).connected) {
          const Position& bp = *s.world_.get<Position>(b);
          if (core::length(bp.x - p->x, bp.y - p->y) < Fixed::from_int(8)) {
            ++sp.spores;
            break;
          }
        }
      });
    }
    // osztódás (az ölés utáni tickben, és amíg nincs létszám, újra próbálja)
    std::vector<EntityId> ready;
    s.world_.pool<Spores>().each([&](EntityId e, const Spores& sp) {
      if (sp.meals > 0) ready.push_back(e);
    });
    for (const EntityId e : ready) {
      if (s.world_.alive(e)) try_split(s, e);
    }
  }

  // összeolvadás (`merge`): a középponthoz legközelebbi `count` egység; ha messze vannak, odamennek
  bool use_ability(Sim& s, const std::vector<EntityId>& units, uint16_t ability, Fixed, Fixed, EntityId) override {
    const AbilityDef& a = s.rules_->abilities[ability];
    const content::Value* t = spec_key(a, "type");
    if (!t || !t->is_string() || t->text() != "merge") return false;
    const content::Value* into_v = spec_key(a, "into");
    const uint16_t into = into_v && into_v->is_string() ? s.rules_->def(into_v->text()) : kNoDef;
    const content::Value* cnt = spec_key(a, "count");
    const auto cn = cnt && cnt->is_number() ? cnt->as_int() : std::nullopt;
    const int n = cn ? static_cast<int>(*cn) : 3;
    const Fixed range = num(spec_key(a, "range"), Fixed::from_int(4));
    if (into == kNoDef || n <= 0) return false;
    std::vector<EntityId> pool;
    for (const EntityId e : units) {
      if (s.world_.alive(e) && s.world_.get<Unit>(e)) pool.push_back(e);
    }
    bool any = false;
    while (static_cast<int>(pool.size()) >= n) {
      Fixed cx, cy;
      for (const EntityId e : pool) {
        cx = cx + s.world_.get<Position>(e)->x;
        cy = cy + s.world_.get<Position>(e)->y;
      }
      cx = cx / Fixed::from_int(static_cast<int32_t>(pool.size()));
      cy = cy / Fixed::from_int(static_cast<int32_t>(pool.size()));
      std::stable_sort(pool.begin(), pool.end(), [&](EntityId x, EntityId y) {
        const Position& px = *s.world_.get<Position>(x);
        const Position& py = *s.world_.get<Position>(y);
        return core::length_sq_raw(px.x - cx, px.y - cy) < core::length_sq_raw(py.x - cx, py.y - cy);
      });
      std::vector<EntityId> grp(pool.begin(), pool.begin() + n);
      pool.erase(pool.begin(), pool.begin() + n);
      Fixed gx, gy;
      for (const EntityId e : grp) {
        gx = gx + s.world_.get<Position>(e)->x;
        gy = gy + s.world_.get<Position>(e)->y;
      }
      gx = gx / Fixed::from_int(n);
      gy = gy / Fixed::from_int(n);
      bool far = false;
      for (const EntityId e : grp) {
        const Position& p = *s.world_.get<Position>(e);
        far = far || core::length(p.x - gx, p.y - gy) > range;
      }
      if (far) {   // összegyűlnek
        for (const EntityId e : grp) {
          s.interrupt(e);
          s.order_move(e, gx, gy);
        }
        any = true;
        continue;
      }
      int64_t frac = 0;   // ezrelék: az életerő-arányok átlaga
      uint8_t owner = 0;
      for (const EntityId e : grp) {
        const Health& h = *s.world_.get<Health>(e);
        frac += int64_t{h.hp} * 1000 / std::max(1, h.max);
        owner = h.owner;
      }
      frac /= n;
      for (const EntityId e : grp) s.kill(e);
      const EntityId nu = s.spawn_unit(into, owner, gx, gy);
      if (nu != kNoEntity) {
        s.place_at_free(nu, gx, gy);
        Health& nh = *s.world_.get<Health>(nu);
        nh.hp = std::max<int32_t>(1, static_cast<int32_t>(int64_t{nh.max} * frac / 1000));
      }
      any = true;
    }
    return any;
  }

 private:
  std::map<uint8_t, Net> nets_;   // gyorsítótár (a `done` lista dönti el, érvényes-e)
};

}  // namespace

// a próbáknak: az egység spóratöltete és osztódáshoz számolt ölései ({-1, -1}, ha nincs ilyen komponense)
std::pair<int, int> micelia_spores(Sim& s, EntityId e);
std::pair<int, int> micelia_spores(Sim& s, EntityId e) {
  const Spores* sp = s.world_.get<Spores>(e);
  return sp ? std::pair<int, int>{sp->spores, sp->meals} : std::pair<int, int>{-1, -1};
}

std::unique_ptr<FactionModule> make_micelia_module() { return std::make_unique<Module>(); }

}  // namespace totu::sim
