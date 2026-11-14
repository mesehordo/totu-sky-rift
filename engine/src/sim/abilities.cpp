// SPDX-License-Identifier: GPL-3.0-or-later
// Képességek (M6, ADR-007 8. lépés). A régi játék szerint (KEPESSEGEK.md A. rész; E 281–330, 1301–1480, 1814–1869;
// G 970–1110, 2139–2216, 3629–3700; H 300–360):
//   - kapcsoló és ének (be/ki; az ének aura is), önerősítés (`duration` ideig), csapás körben (`nova`), célzott lövés
//     (`bolt`: lövedékkel vagy azonnal; `slow` 0 = kábítás), területi mező (`area`: 0,5 s-onként), idézés
//     (élettartammal), aura (a saját egységekre; azonos kulcs nem halmozódik), önműködő gyógyítás (0,5 s-onként);
//   - módosítók: a kapcsolók és az önerősítések szorzódnak / összeadódnak, az aurák a legerősebbet adják;
//   - a hős energiája lassan töltődik; a varázssebzés páncél és típusszorzó nélkül hat;
//   - elemek: jég (lassítás), félelem (menekülés az okozótól elfelé).
#include <algorithm>

#include "core/angle.hpp"
#include "map/map.hpp"
#include "map/pathfinder.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

constexpr int kPulseTicks = 15;       // 0,5 s
constexpr int kFieldSlowTicks = 21;   // 0,7 s
constexpr int kCastRepath = 24;
const Fixed kReach = Fixed::from_ratio(22, 32);

int64_t milli_of(Fixed f) { return (int64_t{f.raw()} * 1000) >> 16; }

int64_t ready_at(const Caster& c, uint16_t a) {
  for (const auto& [id, t] : c.ready) {
    if (id == a) return t;
  }
  return 0;
}

void set_ready(Caster& c, uint16_t a, int64_t t) {
  for (auto& [id, v] : c.ready) {
    if (id == a) {
      v = t;
      return;
    }
  }
  c.ready.emplace_back(a, t);
  std::sort(c.ready.begin(), c.ready.end());
}

// a régi `mod()`: szorzó szorzódik, összeadandó összeadódik
void stack(AbilityMods& m, const AbilityMods& x) {
  m.speed_mult = m.speed_mult * x.speed_mult;
  m.damage_mult = m.damage_mult * x.damage_mult;
  m.ranged_taken_mult = m.ranged_taken_mult * x.ranged_taken_mult;
  m.armor_add = m.armor_add + x.armor_add;
  m.range_add = m.range_add + x.range_add;
  m.regen_add = m.regen_add + x.regen_add;
}

// aura: azonos kulcs nem halmozódik – szorzónál ≥ 1 a legnagyobb, < 1 a legkisebb, összeadónál a legnagyobb
Fixed aura_mult(Fixed cur, Fixed v) {
  const Fixed one = Fixed::from_int(1);
  if (v == one) return cur;
  return v >= one ? std::max(cur, v) : std::min(cur, v);
}

void aura_merge(AbilityMods& m, const AbilityMods& x) {
  m.speed_mult = aura_mult(m.speed_mult, x.speed_mult);
  m.damage_mult = aura_mult(m.damage_mult, x.damage_mult);
  m.ranged_taken_mult = aura_mult(m.ranged_taken_mult, x.ranged_taken_mult);
  if (x.armor_add.raw() != 0) m.armor_add = std::max(m.armor_add, x.armor_add);
  if (x.range_add.raw() != 0) m.range_add = std::max(m.range_add, x.range_add);
  if (x.regen_add.raw() != 0) m.regen_add = std::max(m.regen_add, x.regen_add);
}

}  // namespace

// varázssebzés ezredben: Égkő-fényben × `spell_mult` (MECH-RIFT; a régi `spell_mult`)
int64_t Sim::spell_milli(EntityId e, Fixed dmg) {
  const Fixed m = world_.alive(e) && in_rift(e) ? rules_->rift.spell_mult : Fixed::from_int(1);
  return milli_of(dmg * m);
}

bool Sim::has_ability(EntityId e, uint16_t ability) {
  const Unit* u = world_.get<Unit>(e);
  const Building* b = u ? nullptr : world_.get<Building>(e);
  if ((!u && !b) || !rules_) return false;
  const EntityDef& d = rules_->defs[u ? u->def : b->def];
  if (const HeroState* hs = world_.get<HeroState>(e)) {   // hős: csak a megtanult (a szintváltozat)
    return std::find(hs->learned.begin(), hs->learned.end(), ability) != hs->learned.end();
  }
  if (std::find(d.abilities.begin(), d.abilities.end(), ability) == d.abilities.end()) return false;
  const Health* h = world_.get<Health>(e);
  for (const auto& [a, rid] : d.ability_research) {   // kutatáshoz kötött (pl. Fegyverbe! – Népfelkelés)
    if (a != ability) continue;
    const auto& done = players_[h ? h->owner : 0].researched;
    if (!std::binary_search(done.begin(), done.end(), rid)) return false;
  }
  return true;
}

bool Sim::toggled(EntityId e, uint16_t ability) {
  const Caster* c = world_.get<Caster>(e);
  return c && std::binary_search(c->toggles.begin(), c->toggles.end(), ability);
}

bool Sim::buffed(EntityId e, uint16_t ability) {
  const Caster* c = world_.get<Caster>(e);
  if (!c) return false;
  for (const auto& [a, until] : c->buffs) {
    if (a == ability && until > tick_) return true;
  }
  return false;
}

AbilityMods Sim::unit_mods(EntityId e) {
  AbilityMods m;
  if (!rules_) return m;
  if (const Caster* c = world_.get<Caster>(e)) {
    for (const uint16_t a : c->toggles) {   // az ének csak auraként hat (a régiben a használójára kétszer – D43)
      if (rules_->abilities[a].type != AbilityDef::Type::Chant) stack(m, rules_->abilities[a].mods);
    }
    for (const auto& [a, until] : c->buffs) {
      if (until > tick_) stack(m, rules_->abilities[a].mods);
    }
  }
  if (const auto it = aura_.find(e); it != aura_.end()) stack(m, it->second);
  return m;
}

Fixed Sim::unit_speed_mult(EntityId e) {
  Fixed s = unit_mods(e).speed_mult;
  if (const Afflict* af = world_.get<Afflict>(e)) {
    if (af->stunned(tick_)) return Fixed{};
    if (af->root_until > tick_) return Fixed{};   // gyökér: nem mozdul (támadni tud)
    if (af->slow_until > tick_) s = s * af->slow_mult;
  }
  return s;
}

bool Sim::incapacitated(EntityId e) {
  const Afflict* af = world_.get<Afflict>(e);
  return af && (af->stunned(tick_) || af->feared(tick_));
}

std::vector<EntityId> Sim::enemy_units_in(uint8_t owner, Fixed x, Fixed y, Fixed r) {
  std::vector<EntityId> out;
  world_.pool<Unit>().each([&](EntityId o, const Unit& u) {
    const Health* h = world_.get<Health>(o);
    if (!h || h->hp <= 0 || !hostile(owner, h->owner)) return;
    if (sheltered(o)) return;
    const Position* p = world_.get<Position>(o);
    if (core::length(p->x - x, p->y - y) - rules_->defs[u.def].radius <= r) out.push_back(o);
  });
  return out;
}

bool Sim::ability_ready(EntityId e, const Caster& c, const AbilityDef& a) {
  if (c.energy < a.energy * 1000 || ready_at(c, static_cast<uint16_t>(&a - rules_->abilities.data())) > tick_) return false;
  // a régi `ab_ok`: életerő-ár (hp > ár + 5), az egységképesség áhítat-ára, a népi költség (spóra)
  const Health* h = world_.get<Health>(e);
  if (a.hp_cost > 0 && (!h || h->hp <= a.hp_cost + 5)) return false;
  if (a.cost[kFaith] > 0 && world_.get<Unit>(e) && (!h || players_[h->owner].resource(kFaith) < a.cost[kFaith])) return false;
  return mod_ability_affordable(e, a);
}

void Sim::ability_spend(Caster& c, uint16_t ability, const AbilityDef& a) {
  c.energy -= a.energy * 1000;
  set_ready(c, ability, tick_ + a.cooldown);
  if (a.hp_cost <= 0 && a.cost[kFaith] <= 0 && !mod_ability_costs()) return;
  EntityId e = kNoEntity;   // a varázsló (a ritka, költséges esetben megkeresve)
  world_.pool<Caster>().each([&](EntityId x, Caster& cc) {
    if (&cc == &c) e = x;
  });
  if (e == kNoEntity) return;
  Health* h = world_.get<Health>(e);
  if (h && a.hp_cost > 0) h->hp = std::max(1, h->hp - a.hp_cost);
  if (h && a.cost[kFaith] > 0 && world_.get<Unit>(e)) {
    std::array<int32_t, kResources> f{};
    f[kFaith] = a.cost[kFaith];
    pay(h->owner, f);
  }
  mod_ability_paid(e, a);
}

void Sim::apply_element(EntityId src, EntityId t, Element el, int64_t dmg) {
  Afflict* af = world_.get<Afflict>(t);
  if (!af || el == Element::None) return;
  const auto& fx = rules_->element_fx;
  const int64_t now = tick_;
  if (el == Element::Sunder) {   // páncéltörés: egységen és épületen is
    af->sunder_until = std::max(af->sunder_until, now + fx.sunder_ticks);
    af->sunder_armor = fx.sunder_armor;
    return;
  }
  const Unit* tu = world_.get<Unit>(t);
  if (!tu) return;
  const EntityDef& td = rules_->defs[tu->def];
  const bool wet = af->wet_until > now;
  auto dot = [&](Fixed ratio, Fixed lo, Fixed hi) {   // a találat sebzéséből, korlátok közt (ezred / s)
    const int64_t v = (dmg * ratio.raw()) >> 16;
    return static_cast<int32_t>(std::clamp<int64_t>(v, (int64_t{lo.raw()} * 1000) >> 16, (int64_t{hi.raw()} * 1000) >> 16));
  };
  switch (el) {
    case Element::Fire: {
      if (af->frozen_until > now) {   // a tűz felolvasztja a fagyást
        af->frozen_until = 0;
        af->slow_until = now;
      }
      if (wet) {   // vizes egység nem gyullad meg (a tűz felszárítja)
        af->wet_until = now;
        break;
      }
      const int32_t dps = dot(fx.fire_ratio, fx.fire_min, fx.fire_max);
      if (af->burn_until <= now || af->burn_dps <= dps) {
        af->burn_src = src;
        af->burn_dps = dps;
        af->burn_until = now + fx.fire_ticks;
      }
      break;
    }
    case Element::Ice: {   // fagyás: lassítás (a meglévőnél erősebb marad); kioltja az égést
      af->burn_until = 0;
      const Fixed cur = af->slow_until > now ? af->slow_mult : Fixed::from_int(1);
      af->slow_mult = std::min(cur, fx.ice_slow);
      af->slow_until = std::max(af->slow_until, now + fx.ice_ticks);
      af->frozen_until = std::max(af->frozen_until, now + fx.ice_ticks);
      break;
    }
    case Element::Lightning:   // sokk: rövid kábulás, célpontonként ritkán; vízben szünet nélkül és hosszabban
      if (wet || now >= af->shock_ready) {
        const int32_t d = wet ? (fx.shock_ticks * fx.shock_mult.raw()) >> 16 : fx.shock_ticks;
        af->stun_until = std::max(af->stun_until, now + d);
        af->shock_ready = now + fx.shock_cd;
      }
      break;
    case Element::Water:   // vizes: eloltja a tüzet
      af->burn_until = 0;
      af->wet_until = std::max(af->wet_until, now + fx.wet_ticks);
      break;
    case Element::Root:
      if (!td.flying) af->root_until = std::max(af->root_until, now + fx.root_ticks);
      break;
    case Element::Sleep:   // a hős nem alszik el, csak egy pillanatra kábul
      if (!td.hero) af->sleep_until = std::max(af->sleep_until, now + fx.sleep_ticks);
      else af->stun_until = std::max(af->stun_until, now + 30);
      break;
    case Element::Fear: {   // félelem: menekülés az okozótól elfelé
      const Position* sp = world_.alive(src) ? world_.get<Position>(src) : nullptr;
      const Position* tp = world_.get<Position>(t);
      if (!sp || !tp) break;
      af->fear_until = std::max(af->fear_until, now + fx.fear_ticks);
      const Fixed dx = tp->x - sp->x, dy = tp->y - sp->y;
      const Fixed d = std::max(core::length(dx, dy), Fixed::from_ratio(1, 64));
      af->fx = tp->x + dx * fx.fear_flee / d;
      af->fy = tp->y + dy * fx.fear_flee / d;
      if (map_) {
        af->fx = std::clamp(af->fx, Fixed::from_ratio(1, 2), Fixed::from_int(map_->width()) - Fixed::from_ratio(1, 2));
        af->fy = std::clamp(af->fy, Fixed::from_ratio(1, 2), Fixed::from_int(map_->height()) - Fixed::from_ratio(1, 2));
      }
      af->fleeing = 0;
      break;
    }
    case Element::Blind:
      af->blind_until = std::max(af->blind_until, now + fx.blind_ticks);
      break;
    case Element::Silence:   // a bekapcsolt ének és kapcsoló is elhallgat
      af->silence_until = std::max(af->silence_until, now + fx.silence_ticks);
      if (Caster* c = world_.get<Caster>(t)) c->toggles.clear();
      break;
    case Element::Mark:
      af->mark_until = std::max(af->mark_until, now + fx.mark_ticks);
      if (const Health* sh = world_.alive(src) ? world_.get<Health>(src) : nullptr) af->mark_by = sh->owner;
      break;
    case Element::Poison: {   // gépen nem
      if (td.has_tag("mech")) break;
      const int32_t dps = dot(fx.poison_ratio, fx.poison_min, fx.poison_max);
      if (af->poison_until <= now || af->poison_dps < dps) {
        af->poison_src = src;
        af->poison_dps = dps;
        af->poison_until = now + fx.poison_ticks;
      }
      break;
    }
    default:
      break;   // szél: a területi mező taszítja
  }
}

void Sim::apply_spell(EntityId src, uint8_t owner, const AbilityDef& a, EntityId t) {
  (void)owner;
  Afflict* af = world_.get<Afflict>(t);
  if (!af) return;
  if (a.poison_dps.raw() > 0) {   // a célzott lövés mérge (a régi `poison` [dps, idő]; gépet nem)
    const Unit* tu = world_.get<Unit>(t);
    if (tu && !rules_->defs[tu->def].has_tag("mech")) {
      af->poison_src = src;
      af->poison_dps = static_cast<int32_t>((int64_t{a.poison_dps.raw()} * 1000) >> 16);
      af->poison_until = tick_ + a.poison_ticks;
    }
  }
  if (!a.has_slow) return;
  if (a.slow.raw() <= 0) {   // `slow` 0: kábítás
    af->stun_until = tick_ + a.duration;
  } else {
    af->slow_until = tick_ + a.duration;
    af->slow_mult = a.slow;
  }
}

void Sim::give_buff(EntityId e, uint16_t ab, int32_t ticks) {
  Caster* c = world_.get<Caster>(e);
  if (!c) return;
  const AbilityDef& a = rules_->abilities[ab];
  bool found = false;
  for (auto& [id, until] : c->buffs) {
    if (id == ab) {
      until = tick_ + ticks;
      found = true;
    }
  }
  if (!found) {
    c->buffs.emplace_back(ab, tick_ + ticks);
    std::sort(c->buffs.begin(), c->buffs.end());
  }
  if (a.drop_work) {   // Fegyverbe!: a gyűjtést abbahagyja, a riadó után visszamegy
    Worker* w = world_.get<Worker>(e);
    if (w && !w->inside && (w->state == Worker::Go || w->state == Worker::Work || w->state == Worker::Return) && w->res) {
      c->resume = w->res;
      c->resume_buff = ab;
      w->state = Worker::Idle;
      halt(e);
    }
  }
}

bool Sim::activate_buff(EntityId e, uint16_t ab) {
  Caster* c = world_.get<Caster>(e);
  if (!c) return false;
  const AbilityDef& a = rules_->abilities[ab];
  if (!ability_ready(e, *c, a)) return false;
  ability_spend(*c, ab, a);
  give_buff(e, ab, a.duration);
  return true;
}

bool Sim::cast_nova(EntityId e, uint16_t ab) {
  Caster* c = world_.get<Caster>(e);
  const Position* p = world_.get<Position>(e);
  const Health* h = world_.get<Health>(e);
  if (!c || !p || !h) return false;
  const AbilityDef& a = rules_->abilities[ab];
  if (!ability_ready(e, *c, a)) return false;
  ability_spend(*c, ab, a);
  if (a.heal.raw() > 0) {   // a saját élőlények azonnal gyógyulnak
    world_.pool<Unit>().each([&](EntityId o, const Unit& u) {
      Health* oh = world_.get<Health>(o);
      const Position* op = world_.get<Position>(o);
      if (!oh || oh->owner != h->owner || oh->hp <= 0 || !rules_->defs[u.def].has_tag("organic")) return;
      if (core::length(op->x - p->x, op->y - p->y) <= a.radius) oh->hp = std::min(oh->max, oh->hp + a.heal.floor_int());
    });
  }
  if (a.damage.raw() > 0) {
    for (const EntityId t : enemy_units_in(h->owner, p->x, p->y, a.radius)) {
      deal_damage(e, h->owner, kNoDef, t, spell_milli(e, a.damage), false, true, a.element);
      Afflict* af = world_.get<Afflict>(t);
      if (!af) continue;
      if (a.stun > 0 && a.element != Element::Fear) {
        af->stun_until = std::max(af->stun_until, tick_ + a.stun);
      } else if (a.has_slow && a.stun == 0 && a.slow.raw() > 0) {
        af->slow_until = tick_ + a.duration;
        af->slow_mult = a.slow;
      }
    }
  }
  if (a.building_damage.raw() > 0) {   // Mészbombás manó: az ellenséges épületeket is rombolja (a régi E 1484)
    std::vector<EntityId> bs;
    world_.pool<Building>().each([&](EntityId b, const Building& bb) {
      const Health* bh = world_.get<Health>(b);
      if (!bh || bh->hp <= 0 || !hostile(h->owner, bh->owner)) return;
      const Fixed cx = Fixed::from_int(bb.tx) + Fixed::from_ratio(bb.w, 2), cy = Fixed::from_int(bb.ty) + Fixed::from_ratio(bb.h, 2);
      const Fixed half = Fixed::from_ratio(std::max(bb.w, bb.h), 2);
      if (core::length(cx - p->x, cy - p->y) <= a.radius + half) bs.push_back(b);
    });
    for (const EntityId b : bs) deal_damage(e, h->owner, kNoDef, b, (int64_t{a.building_damage.raw()} * 1000) >> 16, false, true);
  }
  if (a.self_destruct && world_.alive(e)) kill(e);   // robbanó atka: a robbanás megöli
  return true;
}

bool Sim::summon(EntityId e, uint16_t ab, bool at_point, Fixed x, Fixed y) {
  Caster* c = world_.get<Caster>(e);
  const Position* p = world_.get<Position>(e);
  const Health* h = world_.get<Health>(e);
  if (!c || !p || !h) return false;
  const AbilityDef& a = rules_->abilities[ab];
  if (a.unit == kNoDef || !ability_ready(e, *c, a)) return false;
  ability_spend(*c, ab, a);
  const uint8_t owner = h->owner;
  const Fixed px = p->x, py = p->y;
  for (int k = 0; k < (at_point ? 1 : a.count); ++k) {
    Fixed sx = x, sy = y;
    if (!at_point) {   // az egység elé, kissé szétszórva (a régi: 24 képpont, ±0,6 radián)
      const core::Angle ang = core::Angle::from_units(static_cast<int32_t>((k * 2 - 1) * 6258));
      sx = px + core::cos(ang) * Fixed::from_ratio(3, 4);
      sy = py + core::sin(ang) * Fixed::from_ratio(3, 4);
    }
    const EntityId u = spawn_unit(a.unit, owner, sx, sy);
    if (u == kNoEntity) continue;
    place_at_free(u, sx, sy);
    world_.add(u, Lifetime{tick_ + a.lifetime});
  }
  return true;
}

void Sim::fire_bolt(EntityId e, uint16_t ab, EntityId t) {
  const AbilityDef& a = rules_->abilities[ab];
  const Health* h = world_.get<Health>(e);
  const Position& sp = *world_.get<Position>(e);
  const Position& tp = *world_.get<Position>(t);
  const int64_t dmg = spell_milli(e, a.damage);
  if (!a.projectile) {   // azonnal (roham, zuhanó csapás)
    deal_damage(e, h->owner, kNoDef, t, dmg, false, true, a.element);
    apply_spell(e, h->owner, a, t);
    if (a.splash.raw() > 0) splash_at(e, h->owner, kNoDef, tp.x, tp.y, a.splash, dmg, t);
    return;
  }
  const ProjectileKind& pk = rules_->projectiles[a.projectile - 1u];
  Shot s;
  s.kind = static_cast<uint8_t>(a.projectile - 1u);
  s.owner = h->owner;
  s.src = e;
  s.target = t;
  s.sx = sp.x;
  s.sy = sp.y;
  s.ax = tp.x;
  s.ay = tp.y;
  s.damage = static_cast<int32_t>(dmg);
  s.splash = a.splash;                                              // a képesség saját területi sebzése
  s.bounces = static_cast<uint8_t>(std::clamp(a.bounces, 0, 8));   // és pattanása (a régi: a képességé, alapból 0)
  if (pk.line) {
    const Fixed dx = tp.x - sp.x, dy = tp.y - sp.y;
    const Fixed len = std::max(core::length(dx, dy), Fixed::from_ratio(1, 64));
    s.dx = dx / len;
    s.dy = dy / len;
    s.max_travel = a.range * pk.range_mult + Fixed::from_int(2);
  }
  const EntityId se = world_.create();
  world_.add(se, Position{sp.x, sp.y});
  world_.add(se, s);
  world_.add(se, Spell{ab});
}

bool Sim::use_ability(const std::vector<EntityId>& units, const std::string& id, Fixed x, Fixed y, EntityId target) {
  if (!rules_) return false;
  const int ai = rules_->ability_index(id);
  if (ai < 0) return false;
  // hősnél a megtanult szintváltozat; a csoportból azok, akiknél ugyanaz
  uint16_t ab = kNoAbility;
  for (const EntityId e : units) {
    if (!world_.alive(e)) continue;
    const uint16_t v = ability_for(e, static_cast<uint16_t>(ai));
    if (v != kNoAbility && has_ability(e, v)) {
      ab = v;
      break;
    }
  }
  if (ab == kNoAbility) return false;
  const AbilityDef& a = rules_->abilities[ab];
  std::vector<EntityId> us;
  for (const EntityId e : units) {
    if (!world_.alive(e) || !world_.get<Caster>(e) || ability_for(e, static_cast<uint16_t>(ai)) != ab || !has_ability(e, ab)) continue;
    if (sheltered(e)) continue;
    if (const Afflict* af = world_.get<Afflict>(e); af && af->silence_until > tick_ && a.type != AbilityDef::Type::Toggle &&
                                                     a.type != AbilityDef::Type::Chant) {
      continue;   // elnémítva csak a kapcsoló és az ének (a passzív, aura, gyógyítás amúgy is magától)
    }
    us.push_back(e);
  }
  if (us.empty()) return false;
  using T = AbilityDef::Type;
  switch (a.type) {
    case T::Toggle:
    case T::Chant: {
      bool on = false;
      for (const EntityId e : us) on = on || !toggled(e, ab);
      for (const EntityId e : us) {
        Caster& c = *world_.get<Caster>(e);
        if (on) {
          if (a.type == T::Chant) {   // egyszerre csak egy ének
            c.toggles.erase(std::remove_if(c.toggles.begin(), c.toggles.end(),
                                           [&](uint16_t t) { return rules_->abilities[t].type == T::Chant; }),
                            c.toggles.end());
          }
          if (!std::binary_search(c.toggles.begin(), c.toggles.end(), ab)) {
            c.toggles.insert(std::upper_bound(c.toggles.begin(), c.toggles.end(), ab), ab);
          }
        } else {
          c.toggles.erase(std::remove(c.toggles.begin(), c.toggles.end(), ab), c.toggles.end());
        }
      }
      return true;
    }
    case T::SelfBuff: {
      bool any = false;
      for (const EntityId e : us) {
        if (!incapacitated(e)) any = activate_buff(e, ab) || any;
      }
      return any;
    }
    case T::Nova:
      for (const EntityId e : us) {
        if (!incapacitated(e) && cast_nova(e, ab)) return true;
      }
      return false;
    case T::Blink: {   // azonnali áthelyezés a pont felé, legfeljebb a hatótávig, szabad helyre (a régi E 1307)
      EntityId best = kNoEntity;
      int64_t best_d = 0;
      for (const EntityId e : us) {
        if (incapacitated(e) || !ability_ready(e, *world_.get<Caster>(e), a)) continue;
        const Position& p = *world_.get<Position>(e);
        const int64_t d = core::length_sq_raw(p.x - x, p.y - y);
        if (best == kNoEntity || d < best_d) { best = e; best_d = d; }
      }
      if (best == kNoEntity) return false;
      Position& p = *world_.get<Position>(best);
      Fixed tx = x, ty = y;
      const Fixed d = core::length(x - p.x, y - p.y);
      if (d > a.range && d.raw() > 0) {
        tx = p.x + (x - p.x) * a.range / d;
        ty = p.y + (y - p.y) * a.range / d;
      }
      ability_spend(*world_.get<Caster>(best), ab, a);
      interrupt(best);
      halt(best);
      place_at_free(best, tx, ty);
      if (a.land_damage.raw() > 0) {   // landoláskor a közeli ellenség (nem varázs, szorzó nélkül)
        const Health& h = *world_.get<Health>(best);
        const Position& lp = *world_.get<Position>(best);
        for (const EntityId t : enemy_units_in(h.owner, lp.x, lp.y, a.land_radius)) {
          deal_damage(best, h.owner, kNoDef, t, milli_of(a.land_damage), false, false, a.element);
        }
      }
      return true;
    }
    case T::Mount:   // felültetés: a bölény mellé álló harcos (a hatótávon belül)
      for (const EntityId e : us) {
        if (!incapacitated(e) && mount_up(e, ab)) return true;
      }
      return false;
    case T::Capture:   // elfoglalás, megszállás (D30): célzott épület
    case T::Occupy: {
      if (!target || !world_.get<Building>(target)) return false;
      bool any = false;
      for (const EntityId e : us) {
        Seize s;
        s.building = target;
        s.ability = ab;
        s.occupy = a.type == T::Occupy ? 1 : 0;
        if (incapacitated(e) || !seize_valid(e, s)) continue;
        interrupt(e);
        world_.add(e, s);
        any = true;
      }
      return any;
    }
    case T::Dismount: {
      bool any = false;
      for (const EntityId e : us) any = dismount(e, ab) || any;
      return any;
    }
    case T::Summon:
      if (!a.target_placement) {
        for (const EntityId e : us) {
          if (!incapacitated(e) && summon(e, ab, false, x, y)) return true;
        }
        return false;
      }
      [[fallthrough]];
    case T::Bolt:
    case T::Area: {
      if (a.type == T::Bolt) {   // csak ellenséges, látható egységre
        const Health* th = world_.get<Health>(target);
        const Health* mh = world_.get<Health>(us.front());
        if (!th || !world_.get<Unit>(target) || !hostile(mh->owner, th->owner) || !is_visible(mh->owner, target)) {
          const auto near = enemy_units_in(mh->owner, x, y, Fixed::from_ratio(8, 5));
          target = kNoEntity;
          for (const EntityId o : near) {
            if (is_visible(mh->owner, o)) {
              target = o;
              break;
            }
          }
          if (target == kNoEntity) return false;
        }
        const Position& tp = *world_.get<Position>(target);
        x = tp.x;
        y = tp.y;
      }
      EntityId best = kNoEntity;
      int64_t best_d = 0;
      for (const EntityId e : us) {
        const Caster& c = *world_.get<Caster>(e);
        if (!ability_ready(e, c, a)) continue;
        const Position& p = *world_.get<Position>(e);
        const int64_t d = core::length_sq_raw(p.x - x, p.y - y);
        if (best == kNoEntity || d < best_d) {
          best = e;
          best_d = d;
        }
      }
      if (best == kNoEntity) return false;
      interrupt(best);
      Caster& c = *world_.get<Caster>(best);
      c.cast = ab;
      c.cx = x;
      c.cy = y;
      c.ctarget = a.type == T::Bolt ? target : 0;
      c.crepath = 0;
      return true;
    }
    default:   // a keret által nem ismert típus: a népmodulok
      for (auto& m : modules_) {
        if (m->use_ability(*this, us, ab, x, y, target)) return true;
      }
      return false;
  }
}

void Sim::cast_step(EntityId e, Caster& c) {
  const AbilityDef& a = rules_->abilities[c.cast];
  const Position& p = *world_.get<Position>(e);
  const Health& h = *world_.get<Health>(e);
  auto cancel = [&] {
    c.cast = kNoAbility;
    c.ctarget = 0;
    halt(e);
  };
  if (!ability_ready(e, c, a) || !has_ability(e, c.cast)) return cancel();
  using T = AbilityDef::Type;
  Fixed reach = a.range;
  if (a.type == T::Bolt) {
    const Health* th = world_.get<Health>(c.ctarget);
    if (!th || th->hp <= 0 || !world_.alive(c.ctarget) || !is_visible(h.owner, c.ctarget)) {
      c.ctarget = kNoEntity;
      for (const EntityId o : enemy_units_in(h.owner, c.cx, c.cy, Fixed::from_ratio(8, 5))) {
        if (is_visible(h.owner, o)) {
          c.ctarget = o;
          break;
        }
      }
      if (c.ctarget == kNoEntity) return cancel();
    }
    const Position& tp = *world_.get<Position>(c.ctarget);
    c.cx = tp.x;
    c.cy = tp.y;
    reach = reach + rules_->defs[world_.get<Unit>(c.ctarget)->def].radius;
  } else if (a.type == T::Summon) {   // pontra idézés: a pont mellé áll
    const Unit* u = world_.get<Unit>(e);
    const Fixed ur = a.unit != kNoDef ? rules_->defs[a.unit].radius : Fixed::from_ratio(2, 5);
    reach = rules_->defs[u->def].radius + ur + kReach;
  }
  const Fixed d = core::length(c.cx - p.x, c.cy - p.y);
  if (d > reach) {
    const Movement* mv = world_.get<Movement>(e);
    if (--c.crepath <= 0 || !(mv && mv->moving)) {
      order_move(e, c.cx, c.cy);
      c.crepath = kCastRepath;
    }
    return;
  }
  halt(e);
  const uint16_t ab = c.cast;
  c.cast = kNoAbility;
  if (a.type == T::Bolt) {
    const EntityId t = c.ctarget;
    c.ctarget = 0;
    ability_spend(c, ab, a);
    fire_bolt(e, ab, t);
  } else if (a.type == T::Area) {
    ability_spend(c, ab, a);
    const EntityId f = world_.create();
    world_.add(f, Position{c.cx, c.cy});
    world_.add(f, Field{h.owner, e, ab, tick_ + a.duration, tick_ + 1});
  } else if (a.type == T::Summon) {
    summon(e, ab, true, c.cx, c.cy);
  }
}

void Sim::aura_system() {
  aura_.clear();
  std::vector<std::pair<EntityId, uint16_t>> srcs;   // forrás → aura (a régi H 335–361)
  world_.pool<Caster>().each([&](EntityId e, const Caster& c) {
    const Unit* u = world_.get<Unit>(e);
    const Health* h = world_.get<Health>(e);
    if (!u || !h || h->hp <= 0) return;
    if (sheltered(e)) return;
    for (const uint16_t ab : abilities_of(e)) {
      const AbilityDef& a = rules_->abilities[ab];
      if (a.type == AbilityDef::Type::Aura ||
          (a.type == AbilityDef::Type::Chant && std::binary_search(c.toggles.begin(), c.toggles.end(), ab))) {
        srcs.emplace_back(e, ab);
      }
    }
  });
  // Égkő-fény (MECH-RIFT): az élő meteorit-lelőhely és a kész Égkő körül; a varázsló (`caster`, támadó) +1 lőtávot kap
  rift_.clear();
  std::vector<std::pair<Fixed, Fixed>> rift_src;
  world_.pool<ResourceNode>().each([&](EntityId e, const ResourceNode& n) {
    if (n.kind == kMeteor && n.amount > 0) rift_src.emplace_back(world_.get<Position>(e)->x, world_.get<Position>(e)->y);
  });
  world_.pool<Building>().each([&](EntityId e, const Building& b) {
    const Health* h = world_.get<Health>(e);
    if (b.complete && h && h->hp > 0 && rules_->defs[b.def].rift_source) {
      rift_src.emplace_back(world_.get<Position>(e)->x, world_.get<Position>(e)->y);
    }
  });
  if (!rift_src.empty()) {
    world_.pool<Unit>().each([&](EntityId o, const Unit& u) {
      const Position& op = *world_.get<Position>(o);
      for (const auto& [sx, sy] : rift_src) {
        if (core::length(op.x - sx, op.y - sy) > rules_->rift.radius) continue;
        rift_.insert(o);
        const EntityDef& d = rules_->defs[u.def];
        if (d.has_tag("caster") && d.attack.has) {
          AbilityMods m;
          m.range_add = rules_->rift.mage_range;
          aura_merge(aura_[o], m);
        }
        break;
      }
    });
  }
  for (const auto& [src, ab] : srcs) {
    const AbilityDef& a = rules_->abilities[ab];
    const Position& sp = *world_.get<Position>(src);
    const uint8_t owner = world_.get<Health>(src)->owner;
    world_.pool<Unit>().each([&](EntityId o, const Unit&) {
      const Health* oh = world_.get<Health>(o);
      if (!oh || oh->owner != owner || oh->hp <= 0) return;
      const Position& op = *world_.get<Position>(o);
      if (core::length(op.x - sp.x, op.y - sp.y) > a.radius) return;
      aura_merge(aura_[o], a.mods);
    });
  }
}

void Sim::field_system() {
  std::vector<EntityId> ids;
  world_.pool<Field>().each([&](EntityId e, const Field&) { ids.push_back(e); });
  for (const EntityId f : ids) {
    Field fd = *world_.get<Field>(f);
    if (tick_ >= fd.until) {
      world_.destroy(f);
      continue;
    }
    if (tick_ < fd.next) continue;
    fd.next = tick_ + kPulseTicks;
    *world_.get<Field>(f) = fd;
    const AbilityDef& a = rules_->abilities[fd.ability];
    const Position p = *world_.get<Position>(f);
    if (a.heal.raw() > 0) {   // bármely saját egység (gép is)
      const int64_t heal = milli_of(a.heal) / 2;
      world_.pool<Unit>().each([&](EntityId o, const Unit&) {
        Health* oh = world_.get<Health>(o);
        const Position& op = *world_.get<Position>(o);
        if (!oh || oh->owner != fd.owner || oh->hp <= 0 || core::length(op.x - p.x, op.y - p.y) > a.radius) return;
        Vital* vt = world_.get<Vital>(o);
        const int64_t eff = std::min<int64_t>(int64_t{oh->max} * 1000, int64_t{oh->hp} * 1000 - (vt ? vt->deficit : 0) + heal);
        oh->hp = static_cast<int32_t>((eff + 999) / 1000);
        if (vt) vt->deficit = static_cast<int32_t>(int64_t{oh->hp} * 1000 - eff);
      });
    }
    for (const EntityId t : enemy_units_in(fd.owner, p.x, p.y, a.radius)) {
      if (a.dps.raw() > 0) deal_damage(world_.alive(fd.src) ? fd.src : kNoEntity, fd.owner, kNoDef, t, spell_milli(fd.src, a.dps) / 2,
                                       false, true, a.element);
      if (a.has_slow && a.slow.raw() > 0) {
        if (Afflict* af = world_.get<Afflict>(t)) {
          af->slow_until = tick_ + kFieldSlowTicks;
          af->slow_mult = a.slow;
        }
      }
      // irányító elemek sebzés nélkül is (gyökér, alvás, félelem, vakság, víz, némítás, jel, páncéltörés)
      const Element el = a.element;
      if (a.dps.raw() <= 0 && el != Element::None && el != Element::Wind && el != Element::Fire && el != Element::Ice &&
          el != Element::Lightning && el != Element::Poison) {
        apply_element(world_.alive(fd.src) ? fd.src : kNoEntity, t, el, 0);
      }
      if (el == Element::Wind && world_.alive(t)) {   // szél: kifelé taszít (a repülőt nem)
        const Unit* tu = world_.get<Unit>(t);
        if (tu && !rules_->defs[tu->def].flying) {
          const Position tp = *world_.get<Position>(t);
          const Fixed dx = tp.x - p.x, dy = tp.y - p.y;
          const Fixed dd = std::max(core::length(dx, dy), Fixed::from_ratio(1, 64));
          place_at_free(t, tp.x + dx * rules_->element_fx.wind_push / dd, tp.y + dy * rules_->element_fx.wind_push / dd);
        }
      }
    }
    if (a.hits_buildings && a.dps.raw() > 0) {   // épületeket is sújt (szorzó nélkül)
      std::vector<EntityId> bs;
      world_.pool<Building>().each([&](EntityId be, const Building& b) {
        const Health* bh = world_.get<Health>(be);
        if (!bh || bh->hp <= 0 || !hostile(fd.owner, bh->owner)) return;
        const Fixed gx = std::max({Fixed::from_int(b.tx) - p.x, Fixed{}, p.x - Fixed::from_int(b.tx + b.w)});
        const Fixed gy = std::max({Fixed::from_int(b.ty) - p.y, Fixed{}, p.y - Fixed::from_int(b.ty + b.h)});
        if (core::length(gx, gy) <= a.radius) bs.push_back(be);
      });
      for (const EntityId be : bs) deal_damage(kNoEntity, fd.owner, kNoDef, be, milli_of(a.dps) / 2, false, true);
    }
  }
}

void Sim::ability_system() {
  // élettartam: az idézett lény lejárta után tetem nélkül eltűnik
  std::vector<EntityId> expired;
  world_.pool<Lifetime>().each([&](EntityId e, const Lifetime& l) {
    if (l.until <= tick_) expired.push_back(e);
  });
  for (const EntityId e : expired) kill(e);
  // életerő-visszatöltés (`Health.regen` + aura `regen_add`) és energia
  auto regen = [&](EntityId e, uint16_t def) {
    Health* h = world_.get<Health>(e);
    if (!h || h->hp <= 0) return;
    const EntityDef& d = rules_->defs[def];
    Fixed r = stat(e, "Health.regen", d.regen) + mod_regen(e);   // + kutatás, irány, hős-tárgy, népmodul
    if (h->owner == 0 || h->owner > kMaxPlayers) {   // a Vadon faji gyógyulása
      for (const FactionDef& f : rules_->factions) {
        if (f.neutral) r = r + f.regen;
      }
    } else if (const FactionDef* f = rules_->faction(players_[h->owner].faction)) {
      r = r + f->regen;
    }
    if (const auto it = aura_.find(e); it != aura_.end()) r = r + it->second.regen_add;
    if (r.raw() > 0 && h->hp < h->max) {
      Vital* vt = world_.get<Vital>(e);
      const int64_t add = (milli_of(r) + kTickHz / 2) / kTickHz;
      const int64_t eff = std::min<int64_t>(int64_t{h->max} * 1000, int64_t{h->hp} * 1000 - (vt ? vt->deficit : 0) + add);
      h->hp = static_cast<int32_t>((eff + 999) / 1000);
      if (vt) vt->deficit = static_cast<int32_t>(int64_t{h->hp} * 1000 - eff);
    }
  };
  world_.pool<Unit>().each([&](EntityId e, const Unit& u) { regen(e, u.def); });
  world_.pool<Building>().each([&](EntityId e, const Building& b) {
    if (b.complete) regen(e, b.def);
  });
  garrison_system();
  seize_system();
  occupation_system();
  item_system();
  camp_system();
  wullof_system();
  for (auto& m : modules_) m->tick(*this);   // népmodulok
  if (tick_ % 15 == 0) ascension_system();   // a régi: 0,5 s-onként
  std::vector<EntityId> ids;
  world_.pool<Caster>().each([&](EntityId e, const Caster&) { ids.push_back(e); });
  for (const EntityId e : ids) {
    Caster* cp = world_.get<Caster>(e);
    const Unit* u = world_.get<Unit>(e);
    const Health* h = world_.get<Health>(e);
    if (!cp || !u || !h || h->hp <= 0) continue;
    Caster& c = *cp;
    const EntityDef& d = rules_->defs[u->def];
    const int32_t emax = d.energy_max > 0 ? stat(e, "Energy.max", Fixed::from_int(d.energy_max)).floor_int() : 0;
    if (emax > 0 && c.energy < emax * 1000) {   // + a hős növekedése és tárgyai (Éterkő: energy_regen)
      Fixed er = in_rift(e) ? rules_->energy_regen_per_tick * rules_->rift.energy_mult : rules_->energy_regen_per_tick;
      er = er + stat(e, "energy_regen", Fixed{}) / Fixed::from_int(kTickHz);
      c.energy = static_cast<int32_t>(std::min<int64_t>(int64_t{emax} * 1000, c.energy + milli_of(er)));
    }
    // lejárt önerősítések; a Fegyverbe! után a munkás visszamegy dolgozni
    for (const auto& [bab, until] : c.buffs) {   // Gőztúlnyomás: a lejáratkor lehűl a kazán (a régi `slow_after`: × 0,6)
      const AbilityDef& ba = rules_->abilities[bab];
      if (until <= tick_ && ba.after_slow > 0) {
        if (Afflict* af = world_.get<Afflict>(e)) {
          af->slow_until = std::max(af->slow_until, tick_ + ba.after_slow);
          af->slow_mult = Fixed::from_ratio(3, 5);
        }
      }
    }
    c.buffs.erase(std::remove_if(c.buffs.begin(), c.buffs.end(), [&](const auto& b) { return b.second <= tick_; }), c.buffs.end());
    if (c.resume && !buffed(e, c.resume_buff)) {
      const EntityId res = c.resume;
      c.resume = 0;
      c.resume_buff = kNoAbility;
      const Worker* w = world_.get<Worker>(e);
      if (w && w->state == Worker::Idle && !order_gather(e, res)) {   // a lelőhely közben elfogyott: a közelben másik
        const EntityId alt = nearest_resource(w->last_kind, w->lx, w->ly, Fixed::from_int(10), kNoEntity);
        if (alt != kNoEntity) order_gather(e, alt);
      }
    }
    if (sheltered(e)) continue;
    if (incapacitated(e)) continue;
    if (c.cast != kNoAbility) cast_step(e, c);
    // az idézés új egységet hozhat létre: a komponenstárak átrendeződhetnek, a mutatók újra kellenek
    Caster* cp2 = world_.get<Caster>(e);
    h = world_.get<Health>(e);
    if (!cp2 || !h) continue;
    Caster& cc = *cp2;
    // önműködő gyógyítás (0,5 s-onként): a körzet legsebesültebb saját élőlénye
    if (--cc.heal_timer <= 0) {
      cc.heal_timer = kPulseTicks;
      for (const uint16_t ab : abilities_of(e)) {
        const AbilityDef& a = rules_->abilities[ab];
        if (a.type != AbilityDef::Type::Autoheal || !ability_ready(e, cc, a)) continue;
        const Position& p = *world_.get<Position>(e);
        EntityId best = kNoEntity;
        int64_t best_v = 980;   // ezrelék: csak 98% alatt
        world_.pool<Unit>().each([&](EntityId o, const Unit& ou) {
          const Health* oh = world_.get<Health>(o);
          if (!oh || oh->owner != h->owner || oh->hp <= 0) return;
          bool tag_ok = a.heal_tags.empty() && rules_->defs[ou.def].has_tag("organic");   // a célcímkék (`heal_tags`; alapból organic)
          for (const auto& tg : a.heal_tags) tag_ok = tag_ok || rules_->defs[ou.def].has_tag(tg);
          if (!tag_ok) return;
          if (sheltered(o)) return;
          const Position& op = *world_.get<Position>(o);
          if (core::length(op.x - p.x, op.y - p.y) > a.range) return;
          const int64_t v = int64_t{oh->hp} * 1000 / std::max(1, oh->max);
          if (v < best_v) {
            best = o;
            best_v = v;
          }
        });
        if (best != kNoEntity) {
          Health& bh = *world_.get<Health>(best);
          bh.hp = std::min(bh.max, bh.hp + a.heal.floor_int());
          ability_spend(cc, ab, a);
        }
      }
    }
  }
  // égés és méreg: sebzés idővel, páncél és pajzs nélkül (a régi E 694–712)
  std::vector<std::pair<EntityId, EntityId>> dot_dead;
  world_.pool<Afflict>().each([&](EntityId e, Afflict& af) {
    int64_t per = 0;
    EntityId killer = 0;
    if (af.burn_until > tick_) { per += af.burn_dps; killer = af.burn_src; }
    if (af.poison_until > tick_) { per += af.poison_dps; if (!killer) killer = af.poison_src; }
    if (per <= 0 || sheltered(e)) return;
    Health* h = world_.get<Health>(e);
    if (!h || h->hp <= 0) return;
    af.dot_milli += static_cast<int32_t>(per);
    const int64_t d = af.dot_milli / kTickHz;
    af.dot_milli -= static_cast<int32_t>(d * kTickHz);
    if (d <= 0) return;
    Vital* vt = world_.get<Vital>(e);
    const int64_t eff = int64_t{h->hp} * 1000 - (vt ? vt->deficit : 0) - d;
    if (eff <= 0) {
      dot_dead.emplace_back(e, killer);
    } else {
      h->hp = static_cast<int32_t>((eff + 999) / 1000);
      if (vt) vt->deficit = static_cast<int32_t>(int64_t{h->hp} * 1000 - eff);
    }
  });
  for (const auto& [e, k] : dot_dead) {
    if (std::find(dying_.begin(), dying_.end(), e) == dying_.end()) dying_.push_back(e);
    const Health* kh = world_.alive(k) ? world_.get<Health>(k) : nullptr;
    if (kh) killer_[e] = {k, kh->owner};
  }
  // félelem: a menekülő egység az okozótól elfelé fut, utána megáll
  world_.pool<Afflict>().each([&](EntityId e, Afflict& af) {
    if (af.feared(tick_)) {
      if (!af.fleeing) {
        if (Combat* cb = world_.get<Combat>(e)) {
          cb->order = Combat::None;
          cb->target = 0;
          cb->moving = 0;
        }
        order_move(e, af.fx, af.fy);
        af.fleeing = 1;
      }
    } else if (af.fleeing) {
      af.fleeing = 0;
      halt(e);
    }
  });
  field_system();
}

}  // namespace totu::sim
