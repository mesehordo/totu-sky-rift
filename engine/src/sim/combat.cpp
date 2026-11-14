// SPDX-License-Identifier: GPL-3.0-or-later
// Harc (M6, ADR-007 5. lépés). A régi játék szerint (G 807–832, 932–1150; E 107–151, 747–860, 1757–1800):
//   - célválasztás: a legközelebbi ellenséges egység a látótávon belül (a támadni nem tudók 4 csempével hátrébb),
//     ha nincs, épület (a támadók 3 csempével előrébb); 0,4 s-onként;
//   - tétlen harcos magától támad, a megtámadott visszaüt; a magától kezdett harc pórázon (9 csempe), utána haza;
//   - üldözés 0,8 s-onkénti útkéréssel; lőtávon belül (+3 képpont) megáll és üt, ha lejárt az újratöltés;
//   - sebzés: típusszorzó (mátrix, `type_bonus`), címkebónusz, pajzs, páncél (legalább 0,5), a terület fél erővel;
//   - magaslat (D33, ADR-006): lőtáv + 0,5 csempe szintenként (legfeljebb 1), sebzés ±10% szintenként, felfelé
//     lőve 25% hiba szintenként (legfeljebb 40%; közelharcra és a mágiára nem);
//   - lövedékek (D40): a fajta adja a viselkedést – követő (nyíl, mágia, sav, bumeráng: továbbpattan, majd
//     visszarepül), talajra érkező (szikla: a kilövéskori helyre, a mozgó célt elhibázhatja), egyenes vonalú
//     (dárda: az útjába kerülő első ellenség).
#include <algorithm>

#include "core/angle.hpp"
#include "map/map.hpp"
#include "map/pathfinder.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

const Fixed kRangeSlack = Fixed::from_ratio(3, 32);   // a régi: + 3 képpont
constexpr int kScanTicks = 12;                         // 0,4 s
constexpr int kRepathTicks = 24;                       // 0,8 s
const Fixed kLeash = Fixed::from_int(9);

int64_t milli_of(Fixed f) { return (int64_t{f.raw()} * 1000) >> 16; }
int64_t mul_fixed(int64_t milli, Fixed f) { return (milli * f.raw()) >> 16; }

Fixed rect_gap(Fixed px, Fixed py, int x, int y, int w, int h) {
  const Fixed x0 = Fixed::from_int(x), y0 = Fixed::from_int(y), x1 = Fixed::from_int(x + w), y1 = Fixed::from_int(y + h);
  const Fixed dx = std::max({x0 - px, Fixed{}, px - x1});
  const Fixed dy = std::max({y0 - py, Fixed{}, py - y1});
  return core::length(dx, dy);
}

}  // namespace

int Sim::level_at(EntityId e) {
  if (!map_) return 0;
  if (const Building* b = world_.get<Building>(e)) {   // a térképen kívüli lábnyom is a szélső csempe szintjét kapja
    return map_->level(std::clamp<int>(b->tx, 0, map_->width() - 1), std::clamp<int>(b->ty, 0, map_->height() - 1));
  }
  const Position* p = world_.get<Position>(e);
  if (!p) return 0;
  const int x = std::clamp(p->x.floor_int(), 0, map_->width() - 1), y = std::clamp(p->y.floor_int(), 0, map_->height() - 1);
  return map_->level(x, y);
}

Fixed Sim::gap(EntityId a, EntityId b) {
  const Position* pa = world_.get<Position>(a);
  const Position* pb = world_.get<Position>(b);
  if (!pa || !pb) return Fixed::from_int(1000);
  Fixed ra, rb;
  if (const Unit* u = world_.get<Unit>(a)) ra = rules_->defs[u->def].radius;
  if (const Building* bb = world_.get<Building>(b)) {
    return std::max(Fixed{}, rect_gap(pa->x, pa->y, bb->tx, bb->ty, bb->w, bb->h) - ra);
  }
  if (const Unit* u = world_.get<Unit>(b)) rb = rules_->defs[u->def].radius;
  return std::max(Fixed{}, core::length(pb->x - pa->x, pb->y - pa->y) - ra - rb);
}

Fixed Sim::attack_range(EntityId e, EntityId target) {
  const Unit* u = world_.get<Unit>(e);
  const Building* bu = u ? nullptr : world_.get<Building>(e);
  const uint16_t def = u ? u->def : bu ? bu->def : kNoDef;
  if (def == kNoDef) return Fixed{};
  const EntityDef& d = rules_->defs[def];
  const AttackDef& a = d.attack;
  const Health* h = world_.get<Health>(e);
  Fixed r = stat(e, "Attack.range", a.range);
  const int delta = level_at(e) - level_at(target);
  if (delta > 0 && (a.ranged() || bu)) {   // magaslatról messzebb (D33)
    const auto& el = rules_->elevation;
    const Fixed add = bu ? el.building_range_per_level * Fixed::from_int(delta)
                         : std::min(el.ranged_range_max, el.ranged_range_per_level * Fixed::from_int(delta));
    r = r + add;
  }
  if (a.ranged() && h) r = r * env_range_mult(h->owner, d);   // eső/zivatar/homokvihar (D35)
  if (u) r = r + unit_mods(e).range_add;                         // képesség (telepítés: +3)
  return r;
}

bool Sim::can_hit(EntityId e, EntityId t) {
  if (!world_.alive(t) || e == t) return false;
  const Health* h = world_.get<Health>(t);
  if (!h || h->hp <= 0) return false;
  if (sheltered(t)) return false;   // épületben: nem célozható
  const Unit* u = world_.get<Unit>(e);
  const Building* bu = u ? nullptr : world_.get<Building>(e);
  const uint16_t def = u ? u->def : bu ? bu->def : kNoDef;
  if (def == kNoDef) return false;
  const AttackDef& a = rules_->defs[def].attack;
  if (!a.has) return false;
  bool flying = false;
  if (const Unit* tu = world_.get<Unit>(t)) flying = rules_->defs[tu->def].flying;
  else if (!world_.get<Building>(t)) return false;
  return flying ? a.air : a.ground;
}

EntityId Sim::find_enemy(EntityId e, Fixed radius) {
  const Position* p = world_.get<Position>(e);
  const Health* me = world_.get<Health>(e);
  if (!p || !me) return kNoEntity;
  EntityId best = kNoEntity;
  int64_t best_v = 0;
  auto consider = [&](EntityId o, Fixed g, Fixed bias) {
    if (g > radius) return;
    const int64_t v = int64_t{(g + bias).raw()};
    if (best == kNoEntity || v < best_v || (v == best_v && o < best)) {
      best = o;
      best_v = v;
    }
  };
  if (nav_ && nav_bw_ > 0) {
    const int reach = (radius.floor_int() + 2) / 2 + 1;
    const int bx = std::clamp(p->x.floor_int() / 2, 0, nav_bw_ - 1), by = std::clamp(p->y.floor_int() / 2, 0, nav_bh_ - 1);
    for (int yy = std::max(0, by - reach); yy <= std::min(nav_bh_ - 1, by + reach); ++yy) {
      for (int xx = std::max(0, bx - reach); xx <= std::min(nav_bw_ - 1, bx + reach); ++xx) {
        for (const EntityId o : buckets_[static_cast<size_t>(yy * nav_bw_ + xx)]) {
          const Health* oh = world_.get<Health>(o);
          if (!oh || !hostile(me->owner, oh->owner) || !can_hit(e, o) || !is_visible(me->owner, o)) continue;
          bool armed = false;
          if (const Unit* ou = world_.get<Unit>(o)) armed = rules_->defs[ou->def].attack.has;
          else armed = world_.get<Attack>(o) != nullptr;
          consider(o, gap(e, o), armed ? Fixed{} : Fixed::from_int(4));
        }
      }
    }
  }
  if (best != kNoEntity) return best;
  world_.pool<Building>().each([&](EntityId o, const Building& b) {
    const Health* oh = world_.get<Health>(o);
    if (!oh || !hostile(me->owner, oh->owner)) return;
    const Position* op = world_.get<Position>(o);
    if (!op || core::length(op->x - p->x, op->y - p->y) > radius + Fixed::from_int(4)) return;
    if (!can_hit(e, o) || !is_visible(me->owner, o)) return;
    consider(o, gap(e, o), rules_->defs[b.def].attack.has ? Fixed::from_int(-3) : Fixed{});
  });
  return best;
}

void Sim::halt(EntityId e) {
  Movement* m = world_.get<Movement>(e);
  const Position* p = world_.get<Position>(e);
  if (!m || !p || !m->moving) return;
  if (Path* path = world_.get<Path>(e)) {
    nav_arrive(e, *m, *path, *p);
  } else {
    m->moving = false;
    m->tx = p->x;
    m->ty = p->y;
  }
}

bool Sim::order_attack(EntityId e, EntityId target) {
  if (!rules_ || !world_.alive(e)) return false;
  Combat* c = world_.get<Combat>(e);
  const Health* h = world_.get<Health>(e);
  const Health* th = world_.get<Health>(target);
  if (!c || !h || !th || !can_hit(e, target) || !is_visible(h->owner, target)) return false;
  c->order = Combat::Attack;
  c->autonomous = 0;
  c->target = target;
  c->repath = 0;
  return true;
}

bool Sim::order_attack_move(EntityId e, Fixed x, Fixed y) {
  if (!rules_ || !world_.alive(e)) return false;
  Combat* c = world_.get<Combat>(e);
  if (!c) return order_move(e, x, y);
  c->order = Combat::AttackMove;
  c->autonomous = 0;
  c->target = 0;
  c->gx = x;
  c->gy = y;
  c->moving = 0;
  c->scan = 0;
  return true;
}

bool Sim::order_hold(EntityId e) {
  if (!rules_ || !world_.alive(e)) return false;
  Combat* c = world_.get<Combat>(e);
  if (!c) return false;
  halt(e);
  c->order = Combat::Hold;
  c->autonomous = 0;
  c->target = 0;
  c->scan = 0;
  return true;
}

void Sim::deal_damage(EntityId src, uint8_t owner, uint16_t src_def, EntityId t, int64_t dmg, bool ranged, bool magic,
                      Element el) {
  if (!world_.alive(t)) return;
  Health* h = world_.get<Health>(t);
  if (!h || h->hp <= 0) return;
  if (sheltered(t)) return;
  const Unit* tu = world_.get<Unit>(t);
  const Building* tb = tu ? nullptr : world_.get<Building>(t);
  if (!tu && !tb) return;
  const EntityDef& td = rules_->defs[tu ? tu->def : tb->def];
  Afflict* taf = world_.get<Afflict>(t);
  if (taf && dmg > 0 && taf->sleep_until > tick_) taf->sleep_until = 0;   // az első sebzésre felébred
  // elvakult lövész: a nem varázs lövése a `miss` eséllyel mellémegy
  if (ranged && !magic && src && world_.alive(src)) {
    if (const Afflict* saf = world_.get<Afflict>(src);
        saf && saf->blind_until > tick_ &&
        static_cast<int32_t>(combat_rng_.next() % 1000u) < rules_->element_fx.blind_miss) {
      return;
    }
  }
  // az elem (a képességé, különben a fegyveré) a sebzés előtt (a régi G 1065); a páncéltörés épületen is
  const AttackDef* weapon = !magic && src_def < rules_->defs.size() ? &rules_->defs[src_def].attack : nullptr;
  const Element hit_el = el != Element::None ? el : weapon ? weapon->element : Element::None;
  if (hit_el != Element::None && (tu || hit_el == Element::Sunder)) apply_element(src, t, hit_el, dmg);
  if (taf && taf->mark_until > tick_) dmg = mul_fixed(dmg, rules_->element_fx.mark_mult);   // megjelölt: többet kap
  if (weapon && tu && taf) {   // mérgező és lassító fegyver (a régi G 1068–1092)
    if (weapon->poison_dps.raw() > 0 && !td.has_tag("mech")) {
      taf->poison_src = src;
      taf->poison_dps = static_cast<int32_t>(milli_of(weapon->poison_dps));
      taf->poison_until = tick_ + weapon->poison_ticks;
    }
    if (weapon->slow.raw() > 0) {
      const Fixed cur = taf->slow_until > tick_ ? taf->slow_mult : Fixed::from_int(1);
      taf->slow_mult = std::min(cur, weapon->slow);
      taf->slow_until = tick_ + weapon->slow_ticks;
    }
  }
  if (!magic && src_def < rules_->defs.size()) {
    const EntityDef& sd = rules_->defs[src_def];
    // típusszorzó (Ö24): a támadó osztálya a célpont osztálya ellen; a `type_bonus` ennél erősebb lehet
    const std::string tt = tb ? std::string("epulet") : td.utype;
    if (!sd.utype.empty() && !tt.empty()) {
      Fixed m = rules_->type_mult(sd.utype, tt);
      for (const auto& [k, v] : sd.attack.type_bonus) {
        if (k == tt) m = std::max(m, v);
      }
      if (m > Fixed::from_int(1) && tu && rules_of(h->owner).has_no_weak) {   // gyengeség elvesztése (Tüskés pajzsfal)
        for (const auto& [who, classes] : rules_of(h->owner).no_weak) {
          if ((who == td.id || td.has_tag(who)) && std::find(classes.begin(), classes.end(), sd.utype) != classes.end()) {
            m = Fixed::from_int(1);
          }
        }
      }
      dmg = mul_fixed(dmg, m);
    }
    for (const auto& [tag, v] : sd.attack.bonus) {   // címkebónusz
      if (td.has_tag(tag) || (tb && tag == "building")) dmg += milli_of(v);
    }
  }
  if (src && world_.alive(src) && world_.get<Unit>(src)) {   // Medve-ösztön: sebesülten erősebb (a régi G 1106)
    const Health* sh = world_.get<Health>(src);
    const RuleSet& rs = rules_of(owner);
    if (sh && rs.has_low_hp_rage && int64_t{sh->hp} * 65536 < int64_t{sh->max} * rs.lhr_below.raw()) {
      dmg = mul_fixed(dmg, rs.lhr_mult);
    }
  }
  if (src != kNoEntity && (src_def < rules_->defs.size() ? !rules_->defs[src_def].building
                                                         : world_.alive(src) && world_.get<Unit>(src))) {   // Holdtölte (a régi G 1098)
    dmg = mul_fixed(dmg, moon_mult(owner));
  }
  if (tu && ranged) dmg = mul_fixed(dmg, unit_mods(t).ranged_taken_mult);
  if (!modules_.empty()) dmg = mul_fixed(dmg, mod_damage_taken(src, t, ranged, magic));   // népi sérüléshorog   // pajzsfal: kevesebb a távolsági sebzés
  Vital* vt = world_.get<Vital>(t);
  if (vt) {
    vt->last_hit = tick_;
    if (vt->shield > 0) {   // a pajzs előbb nyel
      const int64_t a = std::min<int64_t>(vt->shield, dmg);
      vt->shield -= static_cast<int32_t>(a);
      dmg -= a;
    }
  }
  if (dmg > 0) {
    if (!magic) {   // a varázssebzés páncél nélkül hat
      const int32_t armor = stat(t, "Health.armor", Fixed::from_int(td.armor)).floor_int() + mod_armor(t);
      int64_t add = tu ? milli_of(unit_mods(t).armor_add) : 0;   // képesség, aura
      if (taf && taf->sunder_until > tick_) add -= int64_t{taf->sunder_armor} * 1000;   // páncéltörés
      dmg = std::max<int64_t>(500, dmg - int64_t{armor} * 1000 - add);   // páncél; legalább 0,5
    }
    const int64_t eff = int64_t{h->hp} * 1000 - (vt ? vt->deficit : 0) - dmg;
    if (eff <= 0) {
      if (std::find(dying_.begin(), dying_.end(), t) == dying_.end()) dying_.push_back(t);
      killer_[t] = {src, owner};   // a tapasztalathoz (D25)
    } else {
      h->hp = static_cast<int32_t>((eff + 999) / 1000);
      if (vt) vt->deficit = static_cast<int32_t>(int64_t{h->hp} * 1000 - eff);
    }
  }
  // visszaütés: a tétlen, harcképes, nem munkás egység a támadóra
  if (Combat* c = tu ? world_.get<Combat>(t) : nullptr; c && c->order == Combat::None && src && world_.alive(src) && !world_.get<Worker>(t) &&
      hostile(h->owner, owner) && can_hit(t, src)) {
    const Position* p = world_.get<Position>(t);
    c->order = Combat::Attack;
    c->autonomous = 1;
    c->target = src;
    c->hx = p->x;
    c->hy = p->y;
    c->repath = 0;
  }
}

void Sim::splash_at(EntityId src, uint8_t owner, uint16_t src_def, Fixed x, Fixed y, Fixed radius, int64_t milli, EntityId exclude) {
  std::vector<EntityId> hit;
  world_.pool<Health>().each([&](EntityId o, const Health& oh) {
    if (o == exclude || oh.hp <= 0 || !hostile(owner, oh.owner)) return;
    const Position* p = world_.get<Position>(o);
    if (!p) return;
    Fixed d;
    if (const Building* b = world_.get<Building>(o)) {
      d = rect_gap(x, y, b->tx, b->ty, b->w, b->h);
    } else if (const Unit* u = world_.get<Unit>(o)) {
      d = core::length(p->x - x, p->y - y) - rules_->defs[u->def].radius;
    } else {
      return;
    }
    if (d <= radius) hit.push_back(o);
  });
  for (const EntityId o : hit) deal_damage(src, owner, src_def, o, milli / 2, true);   // fél erővel
}

void Sim::attack_now(EntityId e, Combat& c, EntityId t) {
  const Unit* u = world_.get<Unit>(e);
  const Building* bu = u ? nullptr : world_.get<Building>(e);
  const uint16_t def = u ? u->def : bu->def;
  const EntityDef& ed = rules_->defs[def];
  const AttackDef& a = ed.attack;
  const uint8_t owner = world_.get<Health>(e)->owner;
  c.cooldown = a.cooldown;
  int64_t dmg = milli_of(stat(e, "Attack.damage", a.damage));
  if (u) dmg = mul_fixed(dmg, unit_mods(e).damage_mult);   // képesség (önerősítés, kapcsoló, aura)
  if (u) dmg = mul_fixed(dmg, pack_damage_mult(e));         // falkaszellem (Wullof)
  dmg = mul_fixed(dmg, mod_damage(e, t));                   // népmodul
  if (Stealth* sst = world_.get<Stealth>(e)) sst->last_attack = tick_;
  if (u && rules_of(owner).has_rage) {   // vérszomj-irány: minél sebesültebb, annál erősebb (a régi E 131)
    const RuleSet& rs = rules_of(owner);
    bool tagged = rs.rage_tags.empty();
    for (const auto& tg : rs.rage_tags) tagged = tagged || ed.has_tag(tg);
    const Health& hh = *world_.get<Health>(e);
    if (tagged && hh.max > 0) {
      const Fixed missing = Fixed::from_ratio(hh.max - hh.hp, hh.max);
      dmg = mul_fixed(dmg, Fixed::from_int(1) + rs.rage_max * missing);
    }
  }
  const int delta = level_at(e) - level_at(t);
  if (delta > 0) {   // a magaslatról lövő rövid időre mindenkinek látható (D33, SC1)
    if (Stealth* st = world_.get<Stealth>(e)) st->revealed_until = tick_ + rules_->elevation.reveal_shooter_ticks;
  }
  dmg = dmg * std::max<int64_t>(0, 1000 + int64_t{delta} * rules_->elevation.damage_per_level) / 1000;   // D33
  const Position& tp = *world_.get<Position>(t);
  if (!a.ranged()) {   // közelharc: azonnal
    deal_damage(e, owner, def, t, dmg, false);
    if (a.splash.raw() > 0) splash_at(e, owner, def, tp.x, tp.y, a.splash, dmg, t);
    return;
  }
  launch_shot(e, owner, def, t, dmg, a.projectile, a.splash, attack_range(e, t));
}

void Sim::launch_shot(EntityId e, uint8_t owner, uint16_t def, EntityId t, int64_t dmg, uint8_t projectile, Fixed splash,
                      Fixed range) {
  const Position& tp = *world_.get<Position>(t);
  const int delta = level_at(e) - level_at(t);
  const ProjectileKind& pk = rules_->projectiles[projectile - 1u];
  bool missed = false;
  if (delta < 0 && !pk.never_miss) {   // felfelé lőve hibázhat (D33)
    const int32_t chance = std::min(rules_->elevation.uphill_miss_max, -delta * rules_->elevation.uphill_miss_per_level);
    missed = static_cast<int32_t>(combat_rng_.next() % 1000u) < chance;
  }
  const Position& sp = *world_.get<Position>(e);
  Shot s;
  s.kind = static_cast<uint8_t>(projectile - 1u);
  s.owner = owner;
  s.src_def = def;
  s.src = e;
  s.target = t;
  s.sx = sp.x;
  s.sy = sp.y;
  s.ax = tp.x;
  s.ay = tp.y;
  s.damage = static_cast<int32_t>(dmg);
  s.splash = splash;
  s.missed = missed ? 1 : 0;
  s.bounces = static_cast<uint8_t>(std::clamp(pk.bounces, 0, 8));
  if (pk.ground && missed) {   // a hibás szikla mellé esik
    const core::Angle ang = core::Angle::from_units(static_cast<int32_t>(combat_rng_.next() & 0xFFFF));
    s.ax = s.ax + core::cos(ang) * Fixed::from_ratio(9, 10);
    s.ay = s.ay + core::sin(ang) * Fixed::from_ratio(9, 10);
  }
  if (pk.line) {   // egyenes vonalú: az irány a kilövéskor dől el
    const Fixed dx = tp.x - sp.x, dy = tp.y - sp.y;
    const Fixed len = std::max(core::length(dx, dy), Fixed::from_ratio(1, 64));
    s.dx = dx / len;
    s.dy = dy / len;
    s.max_travel = range * pk.range_mult + Fixed::from_int(1);
  }
  const EntityId se = world_.create();
  world_.add(se, Position{sp.x, sp.y});
  world_.add(se, s);
}

void Sim::shot_system() {
  std::vector<EntityId> ids;
  world_.pool<Shot>().each([&](EntityId e, const Shot&) { ids.push_back(e); });
  std::sort(ids.begin(), ids.end());
  for (const EntityId se : ids) {
    Shot s = *world_.get<Shot>(se);
    Position& pos = *world_.get<Position>(se);
    const Spell* spell = world_.get<Spell>(se);
    const uint16_t spell_ab = spell ? spell->ability : kNoAbility;
    auto shot_hit = [&](EntityId t) {
      if (spell_ab == kNoAbility) {
        deal_damage(s.src, s.owner, s.src_def, t, s.damage, true);
        return;
      }
      if (!world_.get<Unit>(t)) return;   // a varázslövés csak egységet sebez
      const AbilityDef& sa = rules_->abilities[spell_ab];
      deal_damage(world_.alive(s.src) ? s.src : kNoEntity, s.owner, kNoDef, t, s.damage, true, true, sa.element);
      apply_spell(s.src, s.owner, sa, t);
    };
    const ProjectileKind& pk = rules_->projectiles[s.kind];
    Fixed speed = pk.speed;
    if (s.src_def < rules_->defs.size() && rules_->defs[s.src_def].attack.proj_speed.raw() > 0) {
      speed = rules_->defs[s.src_def].attack.proj_speed;
    }
    auto target_ok = [&](EntityId t) {
      if (!world_.alive(t)) return false;
      const Health* h = world_.get<Health>(t);
      const Worker* w = world_.get<Worker>(t);
      return h && h->hp > 0 && !(w && w->inside) && !world_.get<Sheltered>(t);
    };
    bool done = false;
    if (pk.line && !s.returning) {   // dárda: egyenesen, az első ellenség az útjában
      pos.x = pos.x + s.dx * speed;
      pos.y = pos.y + s.dy * speed;
      s.travelled = s.travelled + speed;
      EntityId hit = kNoEntity;
      int64_t best = 0;
      world_.pool<Health>().each([&](EntityId o, const Health& oh) {
        if (oh.hp <= 0 || !hostile(s.owner, oh.owner) || !target_ok(o)) return;
        if (const Sheltered* sh = world_.alive(s.src) ? world_.get<Sheltered>(s.src) : nullptr; sh && sh->building == o) return;   // a megszálló a saját épületét nem
        const Position* op = world_.get<Position>(o);
        if (!op) return;
        Fixed d;
        if (const Building* b = world_.get<Building>(o)) d = rect_gap(pos.x, pos.y, b->tx, b->ty, b->w, b->h);
        else if (const Unit* u = world_.get<Unit>(o)) {
          if (rules_->defs[u->def].flying && !(s.src_def < rules_->defs.size() && rules_->defs[s.src_def].attack.air)) return;
          d = core::length(op->x - pos.x, op->y - pos.y) - rules_->defs[u->def].radius - Fixed::from_ratio(1, 5);
        } else {
          return;
        }
        if (d > Fixed{}) return;
        const int64_t back = core::length_sq_raw(op->x - s.sx, op->y - s.sy);   // a kiindulóhoz legközelebbi
        if (hit == kNoEntity || back < best || (back == best && o < hit)) {
          hit = o;
          best = back;
        }
      });
      if (hit != kNoEntity && !s.missed) {
        shot_hit(hit);
        if (s.splash.raw() > 0) splash_at(s.src, s.owner, s.src_def, pos.x, pos.y, s.splash, s.damage, hit);
        done = true;
      } else if (s.travelled >= s.max_travel) {
        done = true;   // a pálya végén leesik
      }
    } else {
      if (s.returning) {   // a bumeráng visszarepül a dobóhoz (csak látvány)
        if (const Position* sp = world_.alive(s.src) ? world_.get<Position>(s.src) : nullptr) {
          s.ax = sp->x;
          s.ay = sp->y;
        }
      } else if (pk.homing && target_ok(s.target)) {
        const Position& tp = *world_.get<Position>(s.target);
        s.ax = tp.x;
        s.ay = tp.y;
      }
      const Fixed dx = s.ax - pos.x, dy = s.ay - pos.y;
      const Fixed d = core::length(dx, dy);
      if (d > speed) {
        pos.x = pos.x + dx * speed / d;
        pos.y = pos.y + dy * speed / d;
      } else {   // becsapódás
        pos.x = s.ax;
        pos.y = s.ay;
        if (s.returning) {
          done = true;
        } else if (pk.ground) {   // szikla: aki a becsapódás helyén áll
          EntityId primary = kNoEntity;
          if (target_ok(s.target)) {
            Fixed g;
            const Position& tp = *world_.get<Position>(s.target);
            if (const Building* b = world_.get<Building>(s.target)) g = rect_gap(s.ax, s.ay, b->tx, b->ty, b->w, b->h);
            else if (const Unit* u = world_.get<Unit>(s.target)) g = core::length(tp.x - s.ax, tp.y - s.ay) - rules_->defs[u->def].radius;
            if (g <= pk.impact) primary = s.target;
          }
          if (primary != kNoEntity) shot_hit(primary);
          if (s.splash.raw() > 0) splash_at(s.src, s.owner, s.src_def, s.ax, s.ay, s.splash, s.damage, primary);
          done = true;
        } else {   // követő
          const bool hit = !s.missed && target_ok(s.target);
          if (hit) shot_hit(s.target);
          if (s.splash.raw() > 0) splash_at(s.src, s.owner, s.src_def, s.ax, s.ay, s.splash, s.damage, hit ? s.target : kNoEntity);
          done = true;
          if (hit && s.bounces > 0) {   // továbbpattan a legközelebbi másik ellenséges egységre
            EntityId next = kNoEntity;
            int64_t best = int64_t{pk.bounce_range.raw()} * pk.bounce_range.raw();
            world_.pool<Unit>().each([&](EntityId o, const Unit&) {
              const Health* oh = world_.get<Health>(o);
              if (o == s.target || !oh || !hostile(s.owner, oh->owner) || !target_ok(o)) return;
              const Position* op = world_.get<Position>(o);
              const int64_t d2 = core::length_sq_raw(op->x - s.ax, op->y - s.ay);
              if (d2 < best || (d2 == best && next != kNoEntity && o < next)) {
                next = o;
                best = d2;
              }
            });
            if (next != kNoEntity) {
              s.target = next;
              s.damage = static_cast<int32_t>(int64_t{s.damage} * pk.bounce_falloff / 1000);
              --s.bounces;
              s.sx = s.ax;
              s.sy = s.ay;
              done = false;
            }
          }
          if (done && pk.returns && world_.alive(s.src)) {
            s.returning = 1;
            s.sx = s.ax;
            s.sy = s.ay;
            done = false;
          }
        }
      }
    }
    if (done) {
      world_.destroy(se);
    } else {
      *world_.get<Shot>(se) = s;
    }
  }
}

void Sim::die(EntityId e) {
  if (!world_.alive(e)) return;
  {
    const auto kit = killer_.find(e);
    for (auto& m : modules_) {
      m->on_death(*this, e, kit != killer_.end() ? kit->second.first : 0, kit != killer_.end() ? kit->second.second : 0);
    }
  }
  if (const auto it = killer_.find(e); it != killer_.end()) {
    grant_kill_xp(e, it->second.second, it->second.first);
    heal_on_kill(e, it->second.first, it->second.second);
    const uint8_t ko = it->second.second;   // Wullof vadászzsákmány: a költség / darab × 0,2 × 0,5
    const Health* vh = world_.get<Health>(e);
    const FactionDef* kf = ko >= 1 && ko <= kMaxPlayers ? rules_->faction(players_[ko].faction) : nullptr;
    if (kf && kf->bounty.raw() > 0 && vh && hostile(ko, vh->owner)) {
      const Unit* vu = world_.get<Unit>(e);
      const Building* vb = vu ? nullptr : world_.get<Building>(e);
      if (vu || vb) {
        const EntityDef& vd = rules_->defs[vu ? vu->def : vb->def];
        int64_t cost = 0;
        for (const int32_t c : vd.cost) cost += c;
        const int64_t gain = (cost * 1000 * kf->bounty.raw() >> 16) / 2 / vd.count;   // ezred
        players_[ko].res[kf->bounty_res] += gain;
        const Fixed bg = rules_of(ko).bounty_gold;
        if (bg.raw() > 0) income(ko, kGold, static_cast<int32_t>((gain * bg.raw() >> 16) / 1000));
      }
    }
  }
  if (const HeroState* hs = world_.get<HeroState>(e)) {   // az elesett hős állapota megmarad (újraélesztés)
    const Health* h = world_.get<Health>(e);
    const Unit* u = world_.get<Unit>(e);
    if (h && u && h->owner >= 1 && h->owner <= kMaxPlayers) {
      auto& fl = players_[h->owner].fallen;
      fl.erase(std::remove_if(fl.begin(), fl.end(), [&](const auto& x) { return x.first == u->def; }), fl.end());
      fl.emplace_back(u->def, *hs);
    }
  }
  if (const Building* b = world_.get<Building>(e)) {
    if (b->builder_in) leave_construction(e, true);   // a lerombolt épületből sérülten ugrik ki
    if (world_.get<Garrison>(e)) unload(e);            // a helyőrség kiszáll
    if (world_.get<Occupants>(e)) evict(e, true);      // a megszállók fél életerővel (D30)
    if (nav_) nav_->remove_obstacle({b->tx, b->ty, b->w, b->h});
  }
  kill(e);
}

void Sim::match_combat_system() {
  // pajzs-visszatöltés (a régi: a játék alapja szerinti késleltetés után)
  world_.pool<Vital>().each([&](EntityId e, Vital& v) {
    const Unit* u = world_.get<Unit>(e);
    if (!u) return;
    const EntityDef& d = rules_->defs[u->def];
    const int64_t cap = int64_t{std::max(0, stat(e, "Health.shield", Fixed::from_int(d.shield)).floor_int())} * 1000;
    if (cap <= 0 || v.shield >= cap) return;
    const int64_t delay = d.shield_delay > 0 ? d.shield_delay : rules_->shield_regen_delay;
    if (tick_ - v.last_hit <= delay) return;
    if (!mod_shield_ok(e)) return;   // népmodul (Reaktorpajzs …)
    v.shield = static_cast<int32_t>(std::min<int64_t>(cap, v.shield + milli_of(rules_->shield_regen_per_tick)));
  });
  std::vector<EntityId> ids;
  world_.pool<Combat>().each([&](EntityId e, const Combat&) { ids.push_back(e); });
  std::sort(ids.begin(), ids.end());
  for (const EntityId e : ids) {
    if (!world_.alive(e)) continue;
    Combat* cp = world_.get<Combat>(e);
    const Health* h = world_.get<Health>(e);
    const Position* p = world_.get<Position>(e);
    if (!cp || !h || h->hp <= 0 || !p) continue;
    if (sheltered(e)) continue;
    const Unit* u = world_.get<Unit>(e);
    if (!u) continue;   // az épületek (tornyok, helyőrség) a tower_system-ben
    Combat& c = *cp;
    if (c.cooldown > 0) --c.cooldown;
    if (incapacitated(e)) continue;   // kábult vagy fél (az újratöltés közben is telik)
    if (const Caster* cs = world_.get<Caster>(e); cs && cs->cast != kNoAbility) continue;   // képességet használ
    const EntityDef& d = rules_->defs[u->def];
    const Fixed sight = stat(e, "Vision.sight", Fixed::from_int(std::max(1, d.sight))) *
                        env_sight_mult(h->owner, d);
    const Movement* mv = world_.get<Movement>(e);
    auto engage = [&](EntityId t) {
      if (gap(e, t) <= attack_range(e, t) + kRangeSlack) {
        halt(e);
        if (c.cooldown <= 0) attack_now(e, c, t);
        return;
      }
      if (--c.repath <= 0 || !(mv && mv->moving)) {
        const Position& tp = *world_.get<Position>(t);
        order_move(e, tp.x, tp.y);
        c.repath = kRepathTicks;
      }
    };
    switch (c.order) {
      case Combat::None: {
        if (world_.get<Worker>(e)) break;   // a munkás magától nem támad
        if (mv && mv->moving) break;        // csak a tétlen keres célpontot (a mozgásparancs alatt nem; a régi E 747)
        if (world_.get<CampMember>(e)) break;   // a tábor egysége a camp_system szerint keres
        if (--c.scan > 0) break;
        c.scan = kScanTicks;
        const EntityId t = find_enemy(e, sight);
        if (t != kNoEntity) {
          c.order = Combat::Attack;
          c.autonomous = 1;
          c.target = t;
          c.hx = p->x;
          c.hy = p->y;
          c.repath = 0;
        }
        break;
      }
      case Combat::Attack: {
        const Health* th = world_.get<Health>(c.target);
        if (!can_hit(e, c.target) || !th || !hostile(h->owner, th->owner) || !is_visible(h->owner, c.target)) {
          c.order = Combat::None;
          c.target = 0;
          halt(e);
          break;
        }
        if (c.autonomous && core::length(p->x - c.hx, p->y - c.hy) > kLeash) {   // póráz: haza
          c.order = Combat::Leash;
          c.target = 0;
          c.moving = 0;
          break;
        }
        engage(c.target);
        break;
      }
      case Combat::AttackMove: {
        if (c.target && (!can_hit(e, c.target) || !is_visible(h->owner, c.target) || gap(e, c.target) > sight * Fixed::from_ratio(8, 5))) {
          c.target = 0;
          c.moving = 0;
        }
        if (!c.target && --c.scan <= 0) {
          c.scan = kScanTicks;
          c.target = find_enemy(e, sight);
          if (c.target) c.repath = 0;
        }
        if (c.target) {
          engage(c.target);
          c.moving = 0;
        } else if (!c.moving) {
          order_move(e, c.gx, c.gy);
          c.moving = 1;
        } else if (!(mv && mv->moving)) {
          c.order = Combat::None;   // megérkezett
          c.moving = 0;
        }
        break;
      }
      case Combat::Hold: {
        halt(e);
        if (c.target && (!can_hit(e, c.target) || !is_visible(h->owner, c.target) ||
                         gap(e, c.target) > attack_range(e, c.target) + kRangeSlack)) {
          c.target = 0;
        }
        if (!c.target && --c.scan <= 0) {
          c.scan = kScanTicks;
          const Fixed r = d.attack.range + d.radius + Fixed::from_int(2);
          const EntityId t = find_enemy(e, r);
          if (t != kNoEntity && gap(e, t) <= attack_range(e, t) + kRangeSlack) c.target = t;
        }
        if (c.target && c.cooldown <= 0) attack_now(e, c, c.target);
        break;
      }
      case Combat::Leash: {
        if (!c.moving) {
          order_move(e, c.hx, c.hy);
          c.moving = 1;
        } else if (!(mv && mv->moving)) {
          c.order = Combat::None;
          c.moving = 0;
          c.autonomous = 0;
        }
        break;
      }
      default:
        break;
    }
  }
  tower_system();
  shot_system();
}

void Sim::death_system() {
  std::sort(dying_.begin(), dying_.end());
  dying_.erase(std::unique(dying_.begin(), dying_.end()), dying_.end());
  for (const EntityId e : dying_) die(e);
  dying_.clear();
  killer_.clear();
}

}  // namespace totu::sim
