// SPDX-License-Identifier: GPL-3.0-or-later
// Wullof (M7, ADR-008 3. lépés; FAJMECHANIKAK.md 7.2, MECH-WUL-1–4) – a régi játék szerint (G 762–790, 1225–1240,
// 1295–1325, 2820–2900, 2988–2998, 3553–3565; E 880–930, 1157–1176):
//   - falkaszellem: a saját, nem munkás, nem jószág egységek száma a ± 4 csempés négyzetben (legfeljebb 4, 0,5 s-onként);
//     sebzés × (1 + 0,1 n), sebesség × (1 + 0,02 n);
//   - vándorszekér: a csomagolható épület szekérré válik (a sora visszavonva, az életereje arányosan), és bárhová
//     letelepíthető, ahol építhető: az épület az építési idő − 8 s-tól indul, munkás nélkül épül;
//   - felültetés (bölény + harcos → bölénylovas) és leszállás; javítás (a `repair` népeknél).
#include <algorithm>

#include "core/angle.hpp"
#include "map/map.hpp"
#include "map/pathfinder.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

const Fixed kReach = Fixed::from_ratio(22, 32);

Fixed absf(Fixed v) { return v.raw() < 0 ? Fixed{} - v : v; }

Fixed rect_gap(Fixed px, Fixed py, const Building& b) {
  const Fixed dx = std::max({Fixed::from_int(b.tx) - px, Fixed{}, px - Fixed::from_int(b.tx + b.w)});
  const Fixed dy = std::max({Fixed::from_int(b.ty) - py, Fixed{}, py - Fixed::from_int(b.ty + b.h)});
  return core::length(dx, dy);
}

}  // namespace

Fixed Sim::pack_damage_mult(EntityId e) {
  const Health* h = world_.get<Health>(e);
  if (h && h->owner >= 1 && h->owner <= kMaxPlayers && rules_of(h->owner).has_pack) {
    // szabályként adott falkabónusz (irány; a régi G 764–787): a nép falkabónusza helyett, a címkéseknek; a ± sugarú
    // négyzetben álló saját, nem munkás, nem jószág (címkés) egységek száma, legfeljebb max / per
    const RuleSet& rs = rules_of(h->owner);
    const Unit* u = world_.get<Unit>(e);
    const Position* p = world_.get<Position>(e);
    if (!u || !p || rs.pack_per.raw() <= 0) return Fixed::from_int(1);
    auto tagged = [&](const EntityDef& d) {
      if (rs.pack_tags.empty()) return true;
      for (const auto& t : rs.pack_tags) {
        if (d.has_tag(t)) return true;
      }
      return false;
    };
    const EntityDef& ed = rules_->defs[u->def];
    if (ed.worker || !tagged(ed)) return Fixed::from_int(1);
    const int cap = static_cast<int>(int64_t{rs.pack_max.raw()} / rs.pack_per.raw());   // a régi int(max / per)
    int n = 0;
    world_.pool<Unit>().each([&](EntityId o, const Unit& ou) {
      if (o == e || n >= cap) return;
      const Health* oh = world_.get<Health>(o);
      if (!oh || oh->owner != h->owner || oh->hp <= 0 || sheltered(o)) return;
      const EntityDef& od = rules_->defs[ou.def];
      if (od.worker || od.has_tag("livestock") || !tagged(od)) return;
      const Position& op = *world_.get<Position>(o);
      if (absf(op.x - p->x) <= rs.pack_radius && absf(op.y - p->y) <= rs.pack_radius) ++n;
    });
    return Fixed::from_int(1) + rs.pack_per * Fixed::from_int(std::min(n, cap));
  }
  const Pack* p = world_.get<Pack>(e);
  if (!p || !h || h->owner == 0 || h->owner > kMaxPlayers) return Fixed::from_int(1);
  const FactionDef* f = rules_->faction(players_[h->owner].faction);
  return f && f->pack ? Fixed::from_int(1) + f->pack_per * Fixed::from_int(p->n) : Fixed::from_int(1);
}

Fixed Sim::pack_speed_mult(EntityId e) {
  const Pack* p = world_.get<Pack>(e);
  const Health* h = world_.get<Health>(e);
  if (!p || !h || h->owner == 0 || h->owner > kMaxPlayers) return Fixed::from_int(1);
  const FactionDef* f = rules_->faction(players_[h->owner].faction);
  return f && f->pack ? Fixed::from_int(1) + f->pack_speed_per * Fixed::from_int(p->n) : Fixed::from_int(1);
}

// --- javítás ----------------------------------------------------------------------------------------------------------
bool Sim::order_repair(const std::vector<EntityId>& units, EntityId b) {
  const Building* bd = world_.get<Building>(b);
  const Health* bh = world_.get<Health>(b);
  if (!bd || !bh || !bd->complete || bh->hp >= bh->max) return false;
  const FactionDef* f = bh->owner <= kMaxPlayers ? rules_->faction(players_[bh->owner].faction) : nullptr;
  if (!f || !f->repair) return false;
  bool any = false;
  for (const EntityId e : units) {
    const Health* h = world_.get<Health>(e);
    const Worker* w = world_.get<Worker>(e);
    if (!h || !w || h->owner != bh->owner || sheltered(e) || !interrupt(e)) continue;
    world_.add(e, RepairOrder{b, 0});
    any = true;
  }
  return any;
}

// --- vándorszekér -----------------------------------------------------------------------------------------------------
EntityId Sim::pack_building(EntityId b) {
  const Building* bd = world_.get<Building>(b);
  const Health* bh = world_.get<Health>(b);
  if (!bd || !bh || !bd->complete || bh->hp <= 0 || bh->owner == 0 || bh->owner > kMaxPlayers) return kNoEntity;
  const FactionDef* f = rules_->faction(players_[bh->owner].faction);
  if (!f || f->wagon == kNoDef || !rules_->defs[bd->def].packable) return kNoEntity;
  if (Producer* pr = world_.get<Producer>(b)) {   // a sora visszavonva (teljes visszatérítés)
    while (!pr->queue.empty()) {
      if (!cancel_queue_item(b, static_cast<int>(pr->queue.size()) - 1)) break;
      pr = world_.get<Producer>(b);
    }
  }
  if (world_.get<Garrison>(b)) unload(b);
  const uint16_t def = bd->def;
  const uint8_t owner = bh->owner;
  const Fixed cx = Fixed::from_int(bd->tx) + Fixed::from_ratio(bd->w, 2), cy = Fixed::from_int(bd->ty) + Fixed::from_ratio(bd->h, 2);
  const int64_t hp = bh->hp, mx = std::max(1, bh->max);
  if (nav_) nav_->remove_obstacle({bd->tx, bd->ty, bd->w, bd->h});
  world_.destroy(b);
  const EntityId u = spawn_unit(f->wagon, owner, cx, cy);
  if (u == kNoEntity) return kNoEntity;
  place_at_free(u, cx, cy);
  Health& uh = *world_.get<Health>(u);
  uh.max = std::max(uh.max, rules_->defs[def].hp / 2);
  uh.hp = std::max<int32_t>(1, static_cast<int32_t>(int64_t{uh.max} * hp / mx));
  Wagon wg;
  wg.building = def;
  world_.add(u, wg);
  return u;
}

bool Sim::order_unpack(EntityId u, int tx, int ty) {
  Wagon* wg = world_.get<Wagon>(u);
  const Health* h = world_.get<Health>(u);
  if (!wg || !h || wg->building == kNoDef || !can_place(0, wg->building, tx, ty)) return false;
  wg->unpacking = 1;
  wg->tx = static_cast<int16_t>(tx);
  wg->ty = static_cast<int16_t>(ty);
  wg->repath = 0;
  return true;
}

// --- felültetés / leszállás -------------------------------------------------------------------------------------------
bool Sim::mount_up(EntityId bison, uint16_t ab) {
  const AbilityDef& a = rules_->abilities[ab];
  const Position bp = *world_.get<Position>(bison);
  const Health bh = *world_.get<Health>(bison);
  if (a.into == kNoDef) return false;
  EntityId rider = kNoEntity;
  int64_t best = 0;
  world_.pool<Unit>().each([&](EntityId o, const Unit& u) {   // a legközelebbi lovas a hatótávon belül
    const Health* oh = world_.get<Health>(o);
    if (!oh || oh->owner != bh.owner || oh->hp <= 0 || sheltered(o)) return;
    if (std::find(a.riders.begin(), a.riders.end(), u.def) == a.riders.end()) return;
    const Position& op = *world_.get<Position>(o);
    const int64_t d = core::length_sq_raw(op.x - bp.x, op.y - bp.y);
    if (core::length(op.x - bp.x, op.y - bp.y) > a.range) return;
    if (rider == kNoEntity || d < best) {
      rider = o;
      best = d;
    }
  });
  if (rider == kNoEntity) return false;
  const uint16_t rider_def = world_.get<Unit>(rider)->def;
  const int64_t frac_hp = int64_t{bh.hp} * 1000 / std::max(1, bh.max);
  kill(bison);
  kill(rider);
  dying_.erase(std::remove_if(dying_.begin(), dying_.end(), [&](EntityId x) { return x == bison || x == rider; }), dying_.end());
  const EntityId m = spawn_unit(a.into, bh.owner, bp.x, bp.y);
  if (m == kNoEntity) return false;
  place_at_free(m, bp.x, bp.y);
  Health& mh = *world_.get<Health>(m);
  mh.hp = std::max<int32_t>(1, static_cast<int32_t>(int64_t{mh.max} * frac_hp / 1000));
  world_.add(m, Mounted{rider_def});
  return true;
}

bool Sim::dismount(EntityId e, uint16_t ab) {
  const AbilityDef& a = rules_->abilities[ab];
  const Position p = *world_.get<Position>(e);
  const Health h = *world_.get<Health>(e);
  const Mounted* mt = world_.get<Mounted>(e);
  const uint16_t rider = mt && mt->rider != kNoDef ? mt->rider : a.rider;
  if (a.mount == kNoDef || rider == kNoDef) return false;
  const int64_t frac = int64_t{h.hp} * 1000 / std::max(1, h.max);
  kill(e);
  int k = 0;
  for (const uint16_t d : {a.mount, rider}) {   // a régi: ± 10 képpontra oldalt, szabad helyre
    const Fixed x = p.x + Fixed::from_ratio(k == 0 ? -10 : 10, 32);
    const EntityId u = spawn_unit(d, h.owner, x, p.y);
    if (u == kNoEntity) continue;
    place_at_free(u, x, p.y);
    Health& uh = *world_.get<Health>(u);
    uh.hp = std::max<int32_t>(1, static_cast<int32_t>(int64_t{uh.max} * frac / 1000));
    ++k;
  }
  return true;
}

// --- tickenként -------------------------------------------------------------------------------------------------------
void Sim::wullof_system() {
  // falkaszellem 0,5 s-onként
  if (tick_ % 15 == 0) {
    world_.pool<Pack>().each([&](EntityId e, Pack& pk) {
      const Health* h = world_.get<Health>(e);
      const Position* p = world_.get<Position>(e);
      if (!h || !p || h->owner == 0 || h->owner > kMaxPlayers) return;
      const FactionDef* f = rules_->faction(players_[h->owner].faction);
      if (!f || !f->pack) return;
      const int cap = static_cast<int>((int64_t{f->pack_max.raw()} * 1000 / std::max(1, f->pack_per.raw()) + 500) / 1000);
      int n = 0;
      world_.pool<Unit>().each([&](EntityId o, const Unit& u) {
        if (o == e || n >= cap) return;
        const Health* oh = world_.get<Health>(o);
        if (!oh || oh->owner != h->owner || oh->hp <= 0 || sheltered(o)) return;
        const EntityDef& od = rules_->defs[u.def];
        if (od.worker || od.has_tag("livestock")) return;
        const Position& op = *world_.get<Position>(o);
        if (absf(op.x - p->x) <= f->pack_radius && absf(op.y - p->y) <= f->pack_radius) ++n;
      });
      pk.n = static_cast<uint8_t>(std::min(n, cap));
    });
  }
  // javítás: max életerő / max(20, építési idő) × 0,6 / s
  std::vector<EntityId> rep;
  world_.pool<RepairOrder>().each([&](EntityId e, const RepairOrder&) { rep.push_back(e); });
  for (const EntityId e : rep) {
    RepairOrder& o = *world_.get<RepairOrder>(e);
    Health* bh = world_.alive(o.building) ? world_.get<Health>(o.building) : nullptr;
    const Building* bd = bh ? world_.get<Building>(o.building) : nullptr;
    if (!bh || !bd || bh->hp <= 0 || bh->hp >= bh->max || !bd->complete) {
      world_.pool<RepairOrder>().remove(e);
      continue;
    }
    const Position& p = *world_.get<Position>(e);
    const Fixed r = rules_->defs[world_.get<Unit>(e)->def].radius;
    if (rect_gap(p.x, p.y, *bd) <= r + kReach) {
      halt(e);
      const int32_t t = std::max(600, rules_->defs[bd->def].time_ticks);   // legalább 20 s
      Vital* vt = world_.get<Vital>(o.building);
      const int64_t add = int64_t{bh->max} * 1000 * 6 / 10 / t;   // ezred / tick
      const int64_t eff = std::min<int64_t>(int64_t{bh->max} * 1000, int64_t{bh->hp} * 1000 - (vt ? vt->deficit : 0) + add);
      bh->hp = static_cast<int32_t>((eff + 999) / 1000);
      if (vt) vt->deficit = static_cast<int32_t>(int64_t{bh->hp} * 1000 - eff);
      continue;
    }
    const Movement* mv = world_.get<Movement>(e);
    if (--o.repath <= 0 || !(mv && mv->moving)) {
      order_move(e, Fixed::from_int(bd->tx) + Fixed::from_ratio(bd->w, 2), Fixed::from_int(bd->ty) + Fixed::from_ratio(bd->h, 2));
      o.repath = 24;
    }
  }
  // letelepítés: a szekér a helyre megy, ott az épület az építési idő − 8 s-tól, munkás nélkül
  std::vector<EntityId> wagons;
  world_.pool<Wagon>().each([&](EntityId e, const Wagon& w) {
    if (w.unpacking) wagons.push_back(e);
  });
  for (const EntityId e : wagons) {
    Wagon wg = *world_.get<Wagon>(e);
    const Health h = *world_.get<Health>(e);
    const EntityDef& bd = rules_->defs[wg.building];
    const int w = std::max(1, bd.fw), hh = std::max(1, bd.fh);
    Building rect;
    rect.tx = wg.tx;
    rect.ty = wg.ty;
    rect.w = static_cast<int16_t>(w);
    rect.h = static_cast<int16_t>(hh);
    const Position& p = *world_.get<Position>(e);
    const Fixed r = rules_->defs[world_.get<Unit>(e)->def].radius;
    if (rect_gap(p.x, p.y, rect) <= r + Fixed::from_ratio(25, 32)) {
      // a szekér ne álljon a saját épülete helyén: előbb a navrács-akadály nélkül ellenőriz
      if (!can_place(0, wg.building, wg.tx, wg.ty)) {
        world_.get<Wagon>(e)->unpacking = 0;
        continue;
      }
      const EntityId b = place_building(wg.building, h.owner, wg.tx, wg.ty, false);
      Building& nb = *world_.get<Building>(b);
      nb.progress = std::max(0, bd.time_ticks - rules_->faction(players_[h.owner].faction)->unpack_ticks) * 1000;
      Health& nh = *world_.get<Health>(b);
      nh.hp = std::max<int32_t>(1, static_cast<int32_t>(int64_t{nh.max} * h.hp / std::max(1, h.max)));
      world_.add(b, SelfBuild{1});
      kill(e);
      dying_.erase(std::remove(dying_.begin(), dying_.end(), e), dying_.end());
      continue;
    }
    Wagon& wr = *world_.get<Wagon>(e);
    const Movement* mv = world_.get<Movement>(e);
    if (--wr.repath <= 0 || !(mv && mv->moving)) {
      order_move(e, Fixed::from_int(wg.tx) + Fixed::from_ratio(w, 2), Fixed::from_int(wg.ty) + Fixed::from_ratio(hh, 2));
      wr.repath = 24;
    }
  }
}

}  // namespace totu::sim
