// SPDX-License-Identifier: GPL-3.0-or-later
// Meccs és gazdaság (M6, ADR-007 2. lépés): a meccs indítása a pályából, sablonból létrehozott egységek és épületek,
// a munkások gyűjtése (a régi játék `_o_gather`-je szerint: odamegy, rakodik, a legközelebbi leadóhelyre hord, a
// kimerült lelőhely után a közelben újat keres), bevétel a Hadtáppal (MECH-UPKEEP), létszám (MECH-SUPPLY).
#include <algorithm>

#include "core/angle.hpp"
#include "map/map.hpp"
#include "map/pathfinder.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

// „odaért”: a régi játékban 22 képpont a test széle és a cél téglalapja között (1 csempe = 32 képpont)
const Fixed kReach = Fixed::from_ratio(22, 32);
constexpr int kRetryDropTicks = 60;   // az elérhetetlen leadóhelyet ennyi ideig kihagyja (régi játék: 2 s)

Fixed half(int v) { return Fixed::from_ratio(v, 2); }

}  // namespace

void Sim::attach_rules(const Rules* rules) {
  if (rules_ || !rules) return;
  rules_ = rules;
  world_.register_component<Unit>("Unit");
  world_.register_component<Building>("Building");
  world_.register_component<ResourceNode>("ResourceNode");
  world_.register_component<Worker>("Worker");
  world_.register_component<Producer>("Producer");
  world_.register_component<Combat>("Combat");
  world_.register_component<Vital>("Vital");
  world_.register_component<Shot>("Shot");
  world_.register_component<Stealth>("Stealth");
}

int32_t Sim::carry_cap(const EntityDef& d, int kind) const {
  const int32_t base = d.carry[static_cast<size_t>(kind)] > 0 ? d.carry[static_cast<size_t>(kind)]
                                                              : rules_->resources[static_cast<size_t>(kind)].carry;
  return std::max(1, base);
}

EntityId Sim::spawn_unit(uint16_t def, uint8_t owner, Fixed x, Fixed y) {
  if (!rules_ || def >= rules_->defs.size()) return kNoEntity;
  const EntityDef& d = rules_->defs[def];
  const EntityId e = world_.create();
  world_.add(e, Position{x, y});
  world_.add(e, Movement{d.speed, x, y, false});
  world_.add(e, Health{d.hp, d.hp, owner});
  world_.add(e, Unit{def});
  if (d.worker) world_.add(e, Worker{});
  Vital vt;
  vt.shield = d.shield * 1000;
  world_.add(e, vt);
  if (d.attack.has) world_.add(e, Combat{});
  world_.add(e, Stealth{});
  if (nav_) {
    world_.add(e, Body{});
    world_.add(e, Path{});
    const auto air = nav_->nav().table().movement_class("air");
    set_body(e, d.radius, d.flying && air ? *air : 0xFF);
  }
  return e;
}

EntityId Sim::place_building(uint16_t def, uint8_t owner, int tx, int ty, bool complete) {
  if (!rules_ || def >= rules_->defs.size()) return kNoEntity;
  const EntityDef& d = rules_->defs[def];
  const int w = std::max(1, d.fw), h = std::max(1, d.fh);
  const EntityId e = world_.create();
  world_.add(e, Position{Fixed::from_int(tx) + half(w), Fixed::from_int(ty) + half(h)});
  world_.add(e, Health{complete ? d.hp : std::max(1, d.hp / 10), d.hp, owner});   // félkészen 10% (a régi szerint)
  Building b{def, static_cast<int16_t>(tx), static_cast<int16_t>(ty), static_cast<int16_t>(w), static_cast<int16_t>(h),
             static_cast<uint8_t>(complete ? 1 : 0)};
  b.progress = complete ? std::max(1, d.time_ticks) * 1000 : 0;
  world_.add(e, b);
  if (!d.produces.empty() || !d.researches.empty()) world_.add(e, Producer{});
  world_.add(e, Vital{});
  if (d.attack.has) world_.add(e, Combat{});
  if (nav_) nav_->add_obstacle({tx, ty, w, h});
  return e;
}

bool Sim::setup_match(const map::Map& m, const MatchSetup& setup, std::string* error) {
  auto fail = [&](const std::string& msg) {
    if (error) *error = msg;
    return false;
  };
  if (!rules_) return fail("nincsenek szabályadatok (attach_rules)");
  upkeep_ = setup.upkeep;
  fog_ = setup.fog;
  map_ = &m;
  const auto& starts = m.objects().starts;
  std::vector<uint8_t> playing;
  for (size_t i = 0; i < setup.factions.size() && i < static_cast<size_t>(kMaxPlayers); ++i) {
    if (setup.factions[i].empty()) continue;
    if (!rules_->faction(setup.factions[i])) return fail("ismeretlen nép: " + setup.factions[i]);
    if (i >= starts.size()) return fail("a pályán kevesebb a kezdőhely, mint a játékos");
    PlayerState& p = players_[i + 1];
    p = PlayerState{};
    p.faction = setup.factions[i];
    p.active = true;
    const FactionDef& f = *rules_->faction(p.faction);
    for (int r = 0; r < kResources; ++r) p.res[static_cast<size_t>(r)] = int64_t{f.start_resources[static_cast<size_t>(r)]} * 1000;
    playing.push_back(static_cast<uint8_t>(i + 1));
  }
  // nyersanyagok: előbb a lelőhelyek, majd a fák (a pálya sorrendjében)
  auto add_node = [&](int kind, int x, int y, int size, int amount) {
    const EntityId e = world_.create();
    world_.add(e, Position{Fixed::from_int(x) + half(size), Fixed::from_int(y) + half(size)});
    world_.add(e, ResourceNode{static_cast<uint8_t>(kind), static_cast<int16_t>(x), static_cast<int16_t>(y),
                               static_cast<int16_t>(size), amount});
    if (nav_) nav_->add_obstacle({x, y, size, size});
  };
  for (const map::MapResource& r : m.objects().resources) {
    const int kind = resource_index(r.kind);
    if (kind < 0 || rules_->resources[static_cast<size_t>(kind)].special) return fail("ismeretlen lelőhely: " + r.kind);
    add_node(kind, r.x, r.y, r.size, r.amount >= 0 ? r.amount : rules_->resources[static_cast<size_t>(kind)].node_amount);
  }
  for (const auto& [x, y] : m.objects().trees) add_node(kWood, x, y, 1, rules_->resources[kWood].node_amount);
  // játékosonként a főépület és a kezdő egységek (a régi játék szerint: a pálya közepe felé, ívben)
  const Fixed cx = half(m.width()), cy = half(m.height());
  for (const uint8_t owner : playing) {
    const FactionDef& f = *rules_->faction(players_[owner].faction);
    if (f.hq == kNoDef) return fail("a népnek nincs főépülete: " + f.id);
    const EntityDef& hq = rules_->defs[f.hq];
    const int w = std::max(1, hq.fw), h = std::max(1, hq.fh);
    const auto [sx, sy] = starts[owner - 1u];
    const int tx = std::clamp(sx - w / 2, 0, m.width() - w), ty = std::clamp(sy - h / 2, 0, m.height() - h);
    const EntityId b = place_building(f.hq, owner, tx, ty, true);
    const Position bp = *world_.get<Position>(b);
    const core::Angle base = core::atan2(cy - bp.y, cx - bp.x);
    for (const auto& [def, n] : f.start_units) {
      for (int k = 0; k < n; ++k) {
        // a régi képlet: szög + (k − n/2) × 0,35 rad; 0,35 rad = 3651 szögegység (2^16 = 360°)
        const core::Angle a = base + core::Angle::from_units((2 * k - n) * 3651 / 2);
        const Fixed rx = half(w) + Fixed::from_ratio(40, 32), ry = half(h) + Fixed::from_ratio(40, 32);
        const EntityId u = spawn_unit(def, owner, bp.x + core::cos(a) * rx, bp.y + core::sin(a) * ry);
        const EntityDef& ud = rules_->defs[def];
        if (!ud.worker) continue;
        const int kinds[4] = {f.start_gather, kStone, kWood, kMeteor};
        for (const int kind : kinds) {
          if (!(ud.gathers & (1u << kind))) continue;
          const EntityId r = nearest_resource(kind, bp.x, bp.y, Fixed::from_int(16), kNoEntity);
          if (r != kNoEntity) order_gather(u, r);
          break;
        }
      }
    }
  }
  navigation_changed();
  supply_system();
  stealth_system();
  vision_system();
  return true;
}

void Sim::restore_obstacles(const map::Map& m) {
  map_ = &m;
  if (!nav_ || !rules_) return;
  world_.pool<ResourceNode>().each([&](EntityId, const ResourceNode& n) { nav_->add_obstacle({n.tx, n.ty, n.size, n.size}); });
  world_.pool<Building>().each([&](EntityId, const Building& b) { nav_->add_obstacle({b.tx, b.ty, b.w, b.h}); });
  // nincs újratervezés: a mentett utak a mentés pillanatának akadályaival készültek, érvényesek (ugyanúgy folytatódik)
}

bool Sim::order_gather(EntityId e, EntityId res) {
  if (!rules_ || !world_.alive(e) || !world_.alive(res)) return false;
  Worker* w = world_.get<Worker>(e);
  const Unit* u = world_.get<Unit>(e);
  const ResourceNode* n = world_.get<ResourceNode>(res);
  const Position* rp = world_.get<Position>(res);
  if (!w || !u || !n || !rp || n->amount <= 0) return false;
  if (!(rules_->defs[u->def].gathers & (1u << n->kind))) return false;
  w->state = Worker::Go;
  w->res = res;
  w->move_to = 0;
  w->fails = 0;
  w->last_kind = n->kind;
  w->lx = rp->x;
  w->ly = rp->y;
  return true;
}

void Sim::stop(EntityId e) {
  if (!world_.alive(e)) return;
  if (Worker* w = world_.get<Worker>(e)) w->state = Worker::Idle;
  Movement* m = world_.get<Movement>(e);
  const Position* p = world_.get<Position>(e);
  if (!m || !p) return;
  if (Path* path = world_.get<Path>(e)) {
    nav_arrive(e, *m, *path, *p);
  } else {
    m->moving = false;
    m->tx = p->x;
    m->ty = p->y;
  }
}

void Sim::income(uint8_t owner, int kind, int32_t amount) {
  PlayerState& p = players_[owner];
  int64_t milli = int64_t{amount} * 1000;
  if (kind == kStone && upkeep_) milli = int64_t{amount} * rules_->upkeep_permille(p.supply_used);   // Hadtáp
  p.res[static_cast<size_t>(kind)] += milli;
  p.gathered[static_cast<size_t>(kind)] += milli;
}

EntityId Sim::nearest_resource(int kind, Fixed x, Fixed y, Fixed max_dist, EntityId exclude) {
  EntityId best = kNoEntity;
  int64_t best_d2 = int64_t{max_dist.raw()} * max_dist.raw();
  const auto& pos = world_.pool<Position>();
  world_.pool<ResourceNode>().each([&](EntityId e, const ResourceNode& n) {
    if (n.kind != kind || n.amount <= 0 || e == exclude) return;
    const Position* p = pos.get(e);
    if (!p) return;
    const int64_t d2 = core::length_sq_raw(p->x - x, p->y - y);
    if (d2 < best_d2 || (d2 == best_d2 && best != kNoEntity && e < best)) {
      best = e;
      best_d2 = d2;
    }
  });
  return best;
}

EntityId Sim::nearest_dropoff(uint8_t owner, int kind, Fixed x, Fixed y, EntityId exclude) {
  EntityId best = kNoEntity;
  int64_t best_d2 = 0;
  const auto& pos = world_.pool<Position>();
  const auto& hp = world_.pool<Health>();
  world_.pool<Building>().each([&](EntityId e, const Building& b) {
    if (e == exclude || !b.complete || !(rules_->defs[b.def].dropoff & (1u << kind))) return;
    const Health* h = hp.get(e);
    const Position* p = pos.get(e);
    if (!h || !p || h->owner != owner || h->hp <= 0) return;
    const int64_t d2 = core::length_sq_raw(p->x - x, p->y - y);
    if (best == kNoEntity || d2 < best_d2 || (d2 == best_d2 && e < best)) {
      best = e;
      best_d2 = d2;
    }
  });
  return best;
}

bool Sim::in_reach(EntityId unit, EntityId target) {
  const Position* p = world_.pool<Position>().get(unit);
  if (!p) return false;
  int x = 0, y = 0, w = 0, h = 0;
  if (const ResourceNode* n = world_.pool<ResourceNode>().get(target)) {
    x = n->tx; y = n->ty; w = h = n->size;
  } else if (const Building* b = world_.pool<Building>().get(target)) {
    x = b->tx; y = b->ty; w = b->w; h = b->h;
  } else {
    return false;
  }
  // a test széle és a téglalap közti távolság ≤ kReach
  const Fixed x0 = Fixed::from_int(x), y0 = Fixed::from_int(y), x1 = Fixed::from_int(x + w), y1 = Fixed::from_int(y + h);
  const Fixed dx = std::max({x0 - p->x, Fixed{}, p->x - x1});
  const Fixed dy = std::max({y0 - p->y, Fixed{}, p->y - y1});
  Fixed radius = Fixed::from_ratio(4, 10);
  if (const Unit* u = world_.pool<Unit>().get(unit)) radius = rules_->defs[u->def].radius;
  const Fixed lim = kReach + radius;
  return core::length_sq_raw(dx, dy) <= int64_t{lim.raw()} * lim.raw();
}

void Sim::remove_resource(EntityId e) {
  const ResourceNode* n = world_.get<ResourceNode>(e);
  if (!n) return;
  // a kivágott fa helye járható lesz; a meglévő utak érvényesek maradnak (csak nyílik hely), ezért nincs
  // általános újratervezés
  if (nav_) nav_->remove_obstacle({n->tx, n->ty, n->size, n->size});
  world_.destroy(e);
}

void Sim::economy_system() {
  auto& workers = world_.pool<Worker>();
  workers.each([&](EntityId e, Worker& w) {
    if (w.state == Worker::Idle || !world_.alive(e)) return;
    const Position* pos = world_.get<Position>(e);
    const Movement* mv = world_.get<Movement>(e);
    const Unit* unit = world_.get<Unit>(e);
    const Health* hp = world_.get<Health>(e);
    if (!pos || !mv || !unit || !hp || hp->hp <= 0) return;
    const EntityDef& d = rules_->defs[unit->def];
    auto node_of = [&](EntityId r) -> const ResourceNode* {
      if (r == 0 || !world_.alive(r)) return nullptr;
      const ResourceNode* n = world_.get<ResourceNode>(r);
      return n && n->amount > 0 ? n : nullptr;
    };
    switch (w.state) {
      case Worker::Go: {
        const ResourceNode* n = node_of(w.res);
        if (!n) {   // kimerült: a közelben (12 csempe) ugyanilyet keres
          const EntityId nr = nearest_resource(w.last_kind, w.lx, w.ly, Fixed::from_int(12), kNoEntity);
          if (nr == kNoEntity) {
            stop(e);
            return;
          }
          w.res = nr;
          w.move_to = 0;
          n = world_.get<ResourceNode>(nr);
        }
        if (w.carry > 0 && w.carry_kind != n->kind) {   // más rakomány van nála: előbb leadja
          w.state = Worker::Return;
          w.move_to = 0;
          return;
        }
        if (!(d.gathers & (1u << n->kind))) {
          stop(e);
          return;
        }
        const Position& rp = *world_.get<Position>(w.res);
        w.last_kind = n->kind;
        w.lx = rp.x;
        w.ly = rp.y;
        if (in_reach(e, w.res)) {
          // nagyobb rakomány: arányosan tovább rakodik
          const ResourceDef& rd = rules_->resources[n->kind];
          const int64_t t = int64_t{rd.gather_time.raw()} * carry_cap(d, n->kind) / std::max(1, rd.carry);
          w.timer = std::max(1, seconds_to_ticks(Fixed::from_raw(static_cast<int32_t>(t))));
          w.state = Worker::Work;
          w.move_to = 0;
          const uint8_t keep = w.state;
          stop(e);
          w.state = keep;
          return;
        }
        if (w.move_to != w.res) {
          order_move(e, rp.x, rp.y);
          w.move_to = w.res;
          return;
        }
        if (!mv->moving) {   // elérhetetlen vagy foglalt: újra, majd másikat keres
          w.move_to = 0;
          if (++w.fails > 3) {
            const EntityId nr = nearest_resource(n->kind, pos->x, pos->y, Fixed::from_int(14), w.res);
            if (nr == kNoEntity) {
              stop(e);
              return;
            }
            w.res = nr;
            w.fails = 0;
          }
        }
        return;
      }
      case Worker::Work: {
        ResourceNode* n = node_of(w.res) ? world_.get<ResourceNode>(w.res) : nullptr;
        if (!n) {
          w.state = Worker::Go;
          w.move_to = 0;
          return;
        }
        if (--w.timer > 0) return;
        const int32_t take = std::min(carry_cap(d, n->kind), n->amount);
        n->amount -= take;
        w.carry_kind = n->kind;
        w.carry = take;
        if (n->amount <= 0) remove_resource(w.res);
        w.state = Worker::Return;
        w.move_to = 0;
        return;
      }
      case Worker::Return: {
        if (w.carry <= 0) {
          w.state = Worker::Go;
          w.move_to = 0;
          return;
        }
        const EntityId exclude = w.retry_tick > tick_ ? w.bad_drop : kNoEntity;
        const EntityId drop = nearest_dropoff(hp->owner, w.carry_kind, pos->x, pos->y, exclude);
        if (drop == kNoEntity) return;   // nincs leadóhely: vár
        if (in_reach(e, drop)) {
          income(hp->owner, w.carry_kind, w.carry);
          w.carry = 0;
          w.bad_drop = 0;
          w.state = Worker::Go;
          w.move_to = 0;
          const uint8_t keep = w.state;
          stop(e);
          w.state = keep;
          return;
        }
        if (w.move_to != drop) {
          const Position& dp = *world_.get<Position>(drop);
          order_move(e, dp.x, dp.y);
          w.move_to = drop;
          return;
        }
        if (!mv->moving) {   // nem érte el: ideiglenesen kihagyja
          w.bad_drop = drop;
          w.retry_tick = tick_ + kRetryDropTicks;
          w.move_to = 0;
        }
        return;
      }
      default:
        build_order_step(e, w);   // Build, Construct (build.cpp)
        return;
    }
  });
}

void Sim::supply_system() {
  std::array<int32_t, kMaxPlayers + 1> cap{}, used{};
  const auto& hp = world_.pool<Health>();
  world_.pool<Building>().each([&](EntityId e, const Building& b) {
    const Health* h = hp.get(e);
    if (!b.complete || !h || h->hp <= 0 || h->owner > kMaxPlayers) return;
    cap[h->owner] += rules_->defs[b.def].supply_provides;
  });
  world_.pool<Unit>().each([&](EntityId e, const Unit& u) {
    const Health* h = hp.get(e);
    if (!h || h->hp <= 0 || h->owner > kMaxPlayers) return;
    used[h->owner] += rules_->defs[u.def].supply;
  });
  world_.pool<Producer>().each([&](EntityId e, const Producer& pr) {   // a sorban lévő egységek is (MECH-SUPPLY)
    const Health* h = hp.get(e);
    if (!h || h->owner > kMaxPlayers) return;
    for (const QueueItem& q : pr.queue) used[h->owner] += q.supply;
  });
  for (int i = 0; i <= kMaxPlayers; ++i) {
    players_[static_cast<size_t>(i)].supply_cap = std::min(cap[static_cast<size_t>(i)], rules_->max_supply);
    players_[static_cast<size_t>(i)].supply_used = used[static_cast<size_t>(i)];
  }
}

}  // namespace totu::sim
