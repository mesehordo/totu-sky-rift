// SPDX-License-Identifier: GPL-3.0-or-later
// Termelés (M6, ADR-007 4. lépés; a régi játék szerint: G 3486–3622, 3090–3104, 3160–3219; E 1738–1757): a
// termelési sor legfeljebb 5 elem, mindig az első halad; egység és kutatás ugyanabban a sorban. A költség és a
// létszám a parancskor foglalódik (a létszám a sorban lévő egységekkel együtt számít); visszavonáskor a teljes
// költség vissza. A legrövidebb sorú épület kapja az új egységet. Hős: egyszerre legfeljebb 3, mindegyik egyszer.
// A kész egység az épület szélén, a gyülekezőpont felőli oldalon jelenik meg (gyülekezőpont nélkül alul), és oda
// indul; ha a gyülekezőpont lelőhely és az egység gyűjthet, gyűjteni kezd.
#include <algorithm>

#include "map/pathfinder.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

bool Sim::train(uint8_t owner, uint16_t def, EntityId building, std::string* error) {
  auto fail = [&](const std::string& msg) {
    if (error) *error = msg;
    return false;
  };
  if (!rules_ || def >= rules_->defs.size() || owner == 0 || owner > kMaxPlayers) return fail("érvénytelen parancs");
  const EntityDef& d = rules_->defs[def];
  PlayerState& p = players_[owner];
  if (d.building) return fail("épületet építeni kell, nem képezni");
  if (d.hero) {   // hős: egyszerre egy példány, legfeljebb három
    int heroes = 0;
    bool busy = false;
    world_.pool<Unit>().each([&](EntityId e, const Unit& u) {
      const Health* h = world_.get<Health>(e);
      if (!h || h->owner != owner || h->hp <= 0 || !rules_->defs[u.def].hero) return;
      ++heroes;
      busy = busy || u.def == def;
    });
    world_.pool<Producer>().each([&](EntityId e, const Producer& pr) {
      const Health* h = world_.get<Health>(e);
      if (!h || h->owner != owner) return;
      for (const QueueItem& q : pr.queue) {
        if (q.research || !rules_->defs[q.def].hero) continue;
        ++heroes;
        busy = busy || q.def == def;
      }
    });
    if (busy) return fail("ez a hős már a seregedben van");
    if (heroes >= Rules::kMaxHeroes) return fail("legfeljebb három hősöd lehet egyszerre");
  }
  // a gyártó épületek: saját, kész, élő, és képezi ezt
  std::vector<std::pair<size_t, EntityId>> cands;
  bool any = false, blocked = false;
  world_.pool<Producer>().each([&](EntityId e, const Producer& pr) {
    const Building* b = world_.get<Building>(e);
    const Health* h = world_.get<Health>(e);
    if (!b || !h || h->owner != owner || h->hp <= 0 || !b->complete || occupied_against(e, owner)) return;
    const auto& pu = rules_->defs[b->def].produces;
    if (std::find(pu.begin(), pu.end(), def) == pu.end()) return;
    if (building && e != building) return;
    any = true;
    if (!modules_.empty() && !mod_can_train(e, def)) {   // népmodul (pl. nincs szabad lárva)
      blocked = true;
      return;
    }
    if (pr.queue.size() < static_cast<size_t>(Rules::kQueueMax)) cands.emplace_back(pr.queue.size(), e);
  });
  if (!any) return fail("nincs megfelelő épület");
  std::string miss;
  if (missing_requirements(owner, d, &miss)) return fail("hiányzik: " + miss);
  for (int r = 0; r < kResources; ++r) {
    if (p.res[static_cast<size_t>(r)] < int64_t{d.cost[static_cast<size_t>(r)]} * 1000) return fail("nincs elég nyersanyag");
  }
  if (p.supply_used + d.supply > p.supply_cap) return fail("elérted a létszámkorlátot");
  if (d.per_building != kNoDef) {   // épületenkénti darabkorlát (pl. Emukarámonként 5 emu)
    int nb = 0, have = 0;
    world_.pool<Building>().each([&](EntityId be, const Building& bb) {
      const Health* bh2 = world_.get<Health>(be);
      if (bb.def == d.per_building && bb.complete && bh2 && bh2->owner == owner && bh2->hp > 0) ++nb;
    });
    world_.pool<Unit>().each([&](EntityId ue, const Unit& uu) {
      const Health* uh = world_.get<Health>(ue);
      if (uu.def == def && uh && uh->owner == owner && uh->hp > 0) ++have;
    });
    world_.pool<Producer>().each([&](EntityId pe, const Producer& pr) {
      const Health* ph = world_.get<Health>(pe);
      if (!ph || ph->owner != owner) return;
      for (const QueueItem& qi : pr.queue) have += !qi.research && qi.def == def ? 1 : 0;
    });
    if (have >= nb * d.per_building_max) return fail("elérted az épületenkénti darabkorlátot");
  }
  if (cands.empty()) return fail(blocked ? "nincs szabad lárva" : "a gyártósor megtelt");
  std::sort(cands.begin(), cands.end());   // a legrövidebb sor; egyenlőnél a kisebb azonosító
  pay(owner, d.cost);
  QueueItem q;
  q.def = def;
  q.time = std::max(1, d.time_ticks);
  if (!modules_.empty()) {   // népi képzésiidő-szorzó (pl. a Raj iránya)
    const Fixed tm = mod_train_time(owner, def);
    if (tm != Fixed::from_int(1)) q.time = std::max(1, (Fixed::from_int(q.time) * tm).floor_int());
  }
  if (d.hero) {   // újraélesztés: idő × (0,6 + 0,12 × szint), ugyanazon az áron (HOS-1)
    for (const auto& [fd, hs] : p.fallen) {
      if (fd != def) continue;
      const Fixed m = rules_->heroes.revive_base + rules_->heroes.revive_per_level * Fixed::from_int(hs.level);
      q.time = std::max(1, (Fixed::from_int(q.time) * m).floor_int());
    }
  }
  q.cost = d.cost;
  q.supply = d.supply;
  world_.get<Producer>(cands.front().second)->queue.push_back(q);
  if (!modules_.empty()) mod_on_train(cands.front().second, def);
  p.supply_used += d.supply;   // azonnal (a következő tick is ugyanígy számolja)
  return true;
}

bool Sim::research(uint8_t owner, const std::string& id, std::string* error) {
  auto fail = [&](const std::string& msg) {
    if (error) *error = msg;
    return false;
  };
  if (!rules_ || owner == 0 || owner > kMaxPlayers) return fail("érvénytelen parancs");
  const int ri = rules_->research_index(id);
  if (ri < 0) return fail("nincs ilyen kutatás");
  PlayerState& p = players_[owner];
  if (std::binary_search(p.researched.begin(), p.researched.end(), id)) return fail("már kész");
  bool running = false;
  std::vector<std::string> chosen(p.researched);   // a kész és a folyamatban lévő kutatások (a kizáró csoporthoz)
  std::vector<std::pair<size_t, EntityId>> cands;
  world_.pool<Producer>().each([&](EntityId e, const Producer& pr) {
    const Building* b = world_.get<Building>(e);
    const Health* h = world_.get<Health>(e);
    if (!b || !h || h->owner != owner) return;
    for (const QueueItem& q : pr.queue) {
      running = running || (q.research && q.def == ri);
      if (q.research) chosen.push_back(rules_->researches[q.def].id);
    }
    if (h->hp <= 0 || !b->complete || occupied_against(e, owner)) return;   // megszállva nem (D30)
    const auto& rl = rules_->defs[b->def].researches;
    if (std::find(rl.begin(), rl.end(), id) != rl.end()) cands.emplace_back(pr.queue.size(), e);
  });
  if (running) return fail("már folyamatban");
  // kizáró kutatás (`Exclusive`): ha ugyanabból a csoportból már választott (kész vagy folyamatban), a többi tiltott
  // (a régi G 3561–3569)
  if (const std::string& grp = rules_->researches[static_cast<size_t>(ri)].exclusive; !grp.empty()) {
    std::sort(chosen.begin(), chosen.end());
    for (const std::string& o : chosen) {
      const int oi = rules_->research_index(o);
      if (o != id && oi >= 0 && rules_->researches[static_cast<size_t>(oi)].exclusive == grp) {
        return fail("már ezt választottad: " + o + " – a többi nem fejleszthető");
      }
    }
  }
  if (cands.empty()) return fail("nincs megfelelő épület");
  const ResearchDef& rd = rules_->researches[static_cast<size_t>(ri)];
  EntityDef tmp;
  tmp.reqs = rd.reqs;
  std::string miss;
  if (missing_requirements(owner, tmp, &miss)) return fail("hiányzik: " + miss);
  std::sort(cands.begin(), cands.end());
  if (cands.front().first >= static_cast<size_t>(Rules::kQueueMax)) return fail("a gyártósor megtelt");
  if (!pay(owner, rd.cost)) return fail("nincs elég nyersanyag");
  QueueItem q;
  q.research = 1;
  q.def = static_cast<uint16_t>(ri);
  q.time = std::max(1, rd.time_ticks);
  q.cost = rd.cost;
  world_.get<Producer>(cands.front().second)->queue.push_back(q);
  return true;
}

bool Sim::cancel_queue_item(EntityId e, int index) {
  if (!rules_ || !world_.alive(e)) return false;
  Producer* pr = world_.get<Producer>(e);
  const Health* h = world_.get<Health>(e);
  if (!pr || !h || index < 0 || index >= static_cast<int>(pr->queue.size())) return false;
  const QueueItem q = pr->queue[static_cast<size_t>(index)];
  pr->queue.erase(pr->queue.begin() + index);
  refund(h->owner, q.cost, 1000);
  players_[h->owner].supply_used = std::max(0, players_[h->owner].supply_used - q.supply);
  if (!q.research && !modules_.empty()) mod_train_cancel(e, q.def);   // pl. a lárva visszajár
  return true;
}

void Sim::add_resources(uint8_t owner, int kind, int32_t amount) {
  if (owner > kMaxPlayers || kind < 0 || kind >= kResources) return;
  int64_t& v = players_[owner].res[static_cast<size_t>(kind)];
  v = std::max<int64_t>(0, v + int64_t{amount} * 1000);
}

void Sim::set_rally(EntityId e, Fixed x, Fixed y, EntityId target) {
  if (!rules_ || !world_.alive(e)) return;
  Producer* pr = world_.get<Producer>(e);
  if (!pr) return;
  pr->has_rally = 1;
  pr->rx = x;
  pr->ry = y;
  pr->rally_target = target;
}

// a kész egység (a falka több egység: `spawn_group`), a gyülekezőpontra küldve; az első azonosítója
EntityId Sim::spawn_group(uint16_t def, uint8_t owner, Fixed x, Fixed y, Producer* pr) {
  const EntityDef& d = rules_->defs[def];
  const uint16_t sdef = d.spawn_unit != kNoDef ? d.spawn_unit : def;
  const int count = d.spawn_unit != kNoDef ? d.spawn_count : d.count;   // falka, illetve `Cost.count` (a Karmos párban jön)
  const bool rally = pr->has_rally;
  const Fixed rx = pr->rx, ry = pr->ry;
  const EntityId rt = pr->rally_target;
  EntityId first = kNoEntity;
  for (int k = 0; k < count; ++k) {
    const EntityId u = spawn_unit(sdef, owner, x, y);
    if (u == kNoEntity) continue;
    if (k > 0) place_at_free(u, x, y);
    if (first == kNoEntity) first = u;
    if (rally && !(rt && order_gather(u, rt))) order_move(u, rx, ry);
  }
  return first;
}

bool Sim::finish_item(EntityId be, const QueueItem& item) {
  const Health* bh = world_.get<Health>(be);
  const Building* b = world_.get<Building>(be);
  Producer* pr = world_.get<Producer>(be);
  if (!bh || !b || !pr) return false;
  const uint8_t owner = bh->owner;
  if (item.research) {
    PlayerState& p = players_[owner];
    const ResearchDef& rd = rules_->researches[item.def];
    p.researched.insert(std::lower_bound(p.researched.begin(), p.researched.end(), rd.id), rd.id);
    // állandó hatás (D24): az életerő és a pajzs maximuma a már élő egységeken is (grant_modifiers); a többi
    // élőben, a használat helyén kérdeződik le (research_stat); a szabálymódosítók (KUT-RULE) újraszámolva
    grant_modifiers(owner, rd.modifiers);
    rebuild_rules(owner);
    return true;
  }
  const EntityDef& d = rules_->defs[item.def];
  // a régi játék helyjelöltjei: a lábnyom körül, az oldalak csempéinek közepén és a sarkokban, a gyülekezőponthoz
  // (anélkül lefelé) legközelebbi szabad hely
  const Fixed tx = pr->has_rally ? pr->rx : Fixed::from_int(b->tx) + Fixed::from_ratio(b->w, 2);
  const Fixed ty = pr->has_rally ? pr->ry : Fixed::from_int(b->ty + b->h + 31);
  const Fixed off = std::max(Fixed::from_ratio(1, 2), d.radius + Fixed::from_raw(1));
  const Fixed left = Fixed::from_int(b->tx), top = Fixed::from_int(b->ty);
  const Fixed right = Fixed::from_int(b->tx + b->w), bottom = Fixed::from_int(b->ty + b->h);
  std::vector<std::pair<Fixed, Fixed>> cand;
  for (int i = 0; i < b->w; ++i) {
    const Fixed x = left + Fixed::from_int(i) + Fixed::from_ratio(1, 2);
    cand.emplace_back(x, top - off);
    cand.emplace_back(x, bottom + off);
  }
  for (int i = 0; i < b->h; ++i) {
    const Fixed y = top + Fixed::from_int(i) + Fixed::from_ratio(1, 2);
    cand.emplace_back(left - off, y);
    cand.emplace_back(right + off, y);
  }
  for (const Fixed x : {left - off, right + off}) {
    for (const Fixed y : {top - off, bottom + off}) cand.emplace_back(x, y);
  }
  std::stable_sort(cand.begin(), cand.end(), [&](const auto& a, const auto& c) {
    return core::length_sq_raw(a.first - tx, a.second - ty) < core::length_sq_raw(c.first - tx, c.second - ty);
  });
  const map::TerrainTable& table = nav_->nav().table();
  const auto air = table.movement_class("air");
  const uint8_t mc = d.flying && air ? *air : table.movement_class("land").value_or(0);
  const int grid = nav_->nav().grid_of(mc);
  const int size = nav_->nav().size_class(d.radius);
  for (const auto& [x, y] : cand) {
    if (x < d.radius || y < d.radius || x > Fixed::from_int(map_->width()) - d.radius ||
        y > Fixed::from_int(map_->height()) - d.radius) {
      continue;
    }
    const auto cell = map::Pathfinder::cell_of(x, y);
    if (grid >= 0 && !nav_->passable(grid, size, cell.x, cell.y)) continue;
    bool taken = false;
    world_.pool<Body>().each([&](EntityId o, const Body& ob) {
      if (taken) return;
      const Position* op = world_.get<Position>(o);
      const Fixed rr = d.radius + ob.radius;
      if (op && core::length_sq_raw(op->x - x, op->y - y) < int64_t{rr.raw()} * rr.raw()) taken = true;
    });
    if (taken) continue;
    const EntityId u = spawn_group(item.def, owner, x, y, pr);
    if (u == kNoEntity) return false;
    return true;
  }
  // a nagy egységnek (mamut) a lábnyom melletti hely túl szűk lehet: a legközelebbi, a méretének járható cella (a régi
  // `landing_position` szerint)
  if (grid >= 0 && !cand.empty()) {
    if (const auto cell = nav_->nearest(grid, size, 0, map::Pathfinder::cell_of(cand.front().first, cand.front().second))) {
      const auto [x, y] = map::Pathfinder::center_of(*cell);
      bool taken = false;
      world_.pool<Body>().each([&](EntityId o, const Body& ob) {
        if (taken) return;
        const Position* op = world_.get<Position>(o);
        const Fixed rr = d.radius + ob.radius;
        if (op && core::length_sq_raw(op->x - x, op->y - y) < int64_t{rr.raw()} * rr.raw()) taken = true;
      });
      if (!taken) {
        const EntityId u = spawn_group(item.def, owner, x, y, pr);
        if (u == kNoEntity) return false;
        return true;
      }
    }
  }
  return false;   // nincs szabad hely: a következő tickben újra próbálja
}

void Sim::production_system() {
  std::vector<EntityId> ids;
  world_.pool<Producer>().each([&](EntityId e, const Producer& pr) {
    if (!pr.queue.empty()) ids.push_back(e);
  });
  std::sort(ids.begin(), ids.end());
  for (const EntityId e : ids) {
    Producer* pr = world_.get<Producer>(e);
    const Building* b = world_.get<Building>(e);
    const Health* h = world_.get<Health>(e);
    if (!pr || !b || !h || !b->complete || h->hp <= 0 || pr->queue.empty()) continue;
    if (rules_->defs[b->def].parallel) {   // a teljes sor egyszerre (a régi E 1750)
      for (size_t i = 0; i < pr->queue.size();) {
        QueueItem& q = pr->queue[i];
        q.progress = std::min(q.time, q.progress + 1);
        const QueueItem done = q;
        if (done.progress >= done.time && finish_item(e, done)) {
          pr = world_.get<Producer>(e);
          pr->queue.erase(pr->queue.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
          ++i;
        }
      }
      continue;
    }
    QueueItem& q = pr->queue.front();
    q.progress = std::min(q.time, q.progress + 1);
    if (q.progress < q.time) continue;
    const QueueItem done = q;
    if (finish_item(e, done)) pr->queue.erase(pr->queue.begin());
  }
}

}  // namespace totu::sim
