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
  bool any = false;
  world_.pool<Producer>().each([&](EntityId e, const Producer& pr) {
    const Building* b = world_.get<Building>(e);
    const Health* h = world_.get<Health>(e);
    if (!b || !h || h->owner != owner || h->hp <= 0 || !b->complete) return;
    const auto& pu = rules_->defs[b->def].produces;
    if (std::find(pu.begin(), pu.end(), def) == pu.end()) return;
    if (building && e != building) return;
    any = true;
    if (pr.queue.size() < static_cast<size_t>(Rules::kQueueMax)) cands.emplace_back(pr.queue.size(), e);
  });
  if (!any) return fail("nincs megfelelő épület");
  std::string miss;
  if (missing_requirements(owner, d, &miss)) return fail("hiányzik: " + miss);
  for (int r = 0; r < kResources; ++r) {
    if (p.res[static_cast<size_t>(r)] < int64_t{d.cost[static_cast<size_t>(r)]} * 1000) return fail("nincs elég nyersanyag");
  }
  if (p.supply_used + d.supply > p.supply_cap) return fail("elérted a létszámkorlátot");
  if (cands.empty()) return fail("a gyártósor megtelt");
  std::sort(cands.begin(), cands.end());   // a legrövidebb sor; egyenlőnél a kisebb azonosító
  pay(owner, d.cost);
  QueueItem q;
  q.def = def;
  q.time = std::max(1, d.time_ticks);
  q.cost = d.cost;
  q.supply = d.supply;
  world_.get<Producer>(cands.front().second)->queue.push_back(q);
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
  std::vector<std::pair<size_t, EntityId>> cands;
  world_.pool<Producer>().each([&](EntityId e, const Producer& pr) {
    const Building* b = world_.get<Building>(e);
    const Health* h = world_.get<Health>(e);
    if (!b || !h || h->owner != owner) return;
    for (const QueueItem& q : pr.queue) running = running || (q.research && q.def == ri);
    if (h->hp <= 0 || !b->complete) return;
    const auto& rl = rules_->defs[b->def].researches;
    if (std::find(rl.begin(), rl.end(), id) != rl.end()) cands.emplace_back(pr.queue.size(), e);
  });
  if (running) return fail("már folyamatban");
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

bool Sim::finish_item(EntityId be, const QueueItem& item) {
  const Health* bh = world_.get<Health>(be);
  const Building* b = world_.get<Building>(be);
  Producer* pr = world_.get<Producer>(be);
  if (!bh || !b || !pr) return false;
  const uint8_t owner = bh->owner;
  if (item.research) {
    auto& done = players_[owner].researched;
    const std::string& id = rules_->researches[item.def].id;
    done.insert(std::lower_bound(done.begin(), done.end(), id), id);   // a hatások a 7. lépésben
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
    const EntityId u = spawn_unit(item.def, owner, x, y);
    if (u == kNoEntity) return false;
    if (pr->has_rally) {
      if (!(pr->rally_target && order_gather(u, pr->rally_target))) order_move(u, pr->rx, pr->ry);
    }
    return true;
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
    QueueItem& q = pr->queue.front();
    q.progress = std::min(q.time, q.progress + 1);
    if (q.progress < q.time) continue;
    const QueueItem done = q;
    if (finish_item(e, done)) pr->queue.erase(pr->queue.begin());
  }
}

}  // namespace totu::sim
