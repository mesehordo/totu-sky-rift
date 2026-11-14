// SPDX-License-Identifier: GPL-3.0-or-later
// Építés (M6, ADR-007 3. lépés; a régi játék szerint: G 1464–1491, 2078–2098, 2223–2330, 3439–3466; E 1060–1150,
// 1660–1705): lerakás (pályán belül, építhető terep, szirt nélkül, egy szinten, szabad hely, előfeltételek), a
// költség a parancskor levonódik, a munkás odamegy (0,8 csempés közelség), lerakja a félkész épületet (10% életerő),
// és a népe építési stílusa szerint épít (MECH-BUILD: stay, stay_single, enter, consume, leave). Az Ursanok
// (`enter`) munkása bemászik az épületbe, és csak bentről épít; készen kijön, és ha gyűjtött, oda megy vissza.
// Az életerő a haladással nő (a hiányzó 90% arányosan). Megszakítás: a költség `cancel_refund` része vissza.
#include <algorithm>
#include <cstdlib>

#include "map/map.hpp"
#include "map/pathfinder.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

constexpr EntityId kSiteMove = 0xFFFFFFFFu;   // a munkás mozgásparancsa az építési helyre ment
const Fixed kReach = Fixed::from_ratio(22, 32);

// a pont és a csempe-téglalap távolságának négyzete (nyers fixpontos)
int64_t rect_dist_sq(Fixed px, Fixed py, int x, int y, int w, int h) {
  const Fixed x0 = Fixed::from_int(x), y0 = Fixed::from_int(y), x1 = Fixed::from_int(x + w), y1 = Fixed::from_int(y + h);
  const Fixed dx = std::max({x0 - px, Fixed{}, px - x1});
  const Fixed dy = std::max({y0 - py, Fixed{}, py - y1});
  return core::length_sq_raw(dx, dy);
}

bool overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
  return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

}  // namespace

bool Sim::missing_requirements(uint8_t owner, const EntityDef& d, std::string* what) {
  const PlayerState& p = players_[owner];
  for (const EntityDef::Req& r : d.reqs) {
    bool ok = false;
    switch (r.kind) {
      case EntityDef::Req::Building: {
        const uint16_t need = rules_->def(r.id);
        world_.pool<Building>().each([&](EntityId e, const Building& b) {
          const Health* h = world_.get<Health>(e);
          if (!ok && b.def == need && b.complete && h && h->owner == owner && h->hp > 0) ok = true;
        });
        break;
      }
      case EntityDef::Req::Research:
        ok = std::binary_search(p.researched.begin(), p.researched.end(), r.id);
        break;
      case EntityDef::Req::Path:
        ok = p.path == r.id;
        break;
      case EntityDef::Req::Ascended:
        ok = p.ascended;
        break;
      case EntityDef::Req::Ally:   // szövetséges kisnép eleme: csak a megkötött szövetséggel (D26)
        ok = p.ally == r.id;
        break;
    }
    if (!ok) {
      if (what) *what = r.id.empty() ? "felemelkedés" : r.id;
      return true;
    }
  }
  return false;
}

bool Sim::can_place(uint8_t owner, uint16_t def, int tx, int ty, std::string* error) {
  auto fail = [&](const char* msg) {
    if (error) *error = msg;
    return false;
  };
  if (!rules_ || !map_ || !nav_ || def >= rules_->defs.size()) return fail("nincs ilyen épület");
  const EntityDef& d = rules_->defs[def];
  if (!d.building) return fail("nem épület");
  const int w = std::max(1, d.fw), h = std::max(1, d.fh);
  if (tx < 0 || ty < 0 || tx + w > map_->width() || ty + h > map_->height()) return fail("a térképen kívül");
  const auto& terrains = nav_->nav().table().terrains();
  const int level = map_->level(tx, ty);
  for (int y = ty; y < ty + h; ++y) {
    for (int x = tx; x < tx + w; ++x) {
      const uint16_t t = map_->terrain(x, y);
      if (t >= terrains.size() || !terrains[t].buildable) return fail("nem építhető terep");
      if (map_->cliff(x, y) || map_->level(x, y) != level) return fail("szirtre és szintkülönbségre nem építhető");
    }
  }
  bool busy = false;
  world_.pool<Building>().each([&](EntityId, const Building& b) {
    busy = busy || overlap(tx, ty, w, h, b.tx, b.ty, b.w, b.h);
  });
  bool on_node = false;   // lelőhelyre épülő (Napfúró, Fertőzött tárna; a régi G 2240): a lelőhely sarkára, belefér
  world_.pool<ResourceNode>().each([&](EntityId, const ResourceNode& n) {
    if (n.amount <= 0 || !overlap(tx, ty, w, h, n.tx, n.ty, n.size, n.size)) return;
    if (d.on_resource && (d.on_resource & (1u << n.kind)) && n.tx == tx && n.ty == ty && w <= n.size && h <= n.size) on_node = true;
    else busy = true;
  });
  if (busy) return fail("foglalt hely");
  if (d.on_resource && !on_node) return fail("aranybányára vagy meteoritra kell építeni");
  if (owner) {
    std::string miss;
    if (missing_requirements(owner, d, &miss)) {
      if (error) *error = "hiányzik: " + miss;
      return false;
    }
  }
  return mod_can_place(owner, def, tx, ty, error);   // népmodul (mélység, tó, energiamező …)
}

bool Sim::pay(uint8_t owner, const std::array<int32_t, kResources>& cost) {
  PlayerState& p = players_[owner];
  for (int r = 0; r < kResources; ++r) {
    if (p.res[static_cast<size_t>(r)] < int64_t{cost[static_cast<size_t>(r)]} * 1000) return false;
  }
  for (int r = 0; r < kResources; ++r) p.res[static_cast<size_t>(r)] -= int64_t{cost[static_cast<size_t>(r)]} * 1000;
  return true;
}

void Sim::refund(uint8_t owner, const std::array<int32_t, kResources>& cost, int32_t permille) {
  PlayerState& p = players_[owner];
  for (int r = 0; r < kResources; ++r) p.res[static_cast<size_t>(r)] += int64_t{cost[static_cast<size_t>(r)]} * permille;
}

bool Sim::order_build(EntityId e, uint16_t def, int tx, int ty, std::string* error) {
  auto fail = [&](const char* msg) {
    if (error) *error = msg;
    return false;
  };
  if (!rules_ || !world_.alive(e) || def >= rules_->defs.size()) return fail("érvénytelen parancs");
  Worker* w = world_.get<Worker>(e);
  const Health* hp = world_.get<Health>(e);
  if (!w || !hp || hp->hp <= 0 || w->inside) return fail("ez az egység nem épít");
  if (const Unit* wu = world_.get<Unit>(e); wu && !rules_->defs[wu->def].builds) return fail("ez a munkás nem épít");   // `Worker.builds`
  const PlayerState& p = players_[hp->owner];
  const EntityDef& d = rules_->defs[def];
  if (!d.building || (d.faction != p.faction && (p.ally.empty() || d.faction != p.ally))) return fail("ezt a nép nem építheti");   // + a szövetséges kisnépé (D26)
  if (!can_place(hp->owner, def, tx, ty, error)) return false;
  if (w->state == Worker::Build) refund(hp->owner, rules_->defs[w->build_def].cost, 1000);   // az előző terv vissza
  if (!pay(hp->owner, d.cost)) return fail("nincs elég nyersanyag");
  // ha gyűjtött, az építés után oda megy vissza (a régi játék szerint)
  if (w->state == Worker::Go || w->state == Worker::Work || w->state == Worker::Return) w->resume = w->res;
  else if (w->state != Worker::Build && w->state != Worker::Construct) w->resume = 0;
  w->state = Worker::Build;
  w->build_def = def;
  w->btx = static_cast<int16_t>(tx);
  w->bty = static_cast<int16_t>(ty);
  w->move_to = 0;
  w->fails = 0;
  return true;
}

void Sim::place_at_free(EntityId u, Fixed x, Fixed y) {
  Position* p = world_.get<Position>(u);
  const Body* b = world_.get<Body>(u);
  if (!p || !b || !nav_ || b->grid < 0) return;
  const auto cell = nav_->nearest(b->grid, b->size, 0, map::Pathfinder::cell_of(x, y));
  if (!cell) return;
  const auto [cx, cy] = map::Pathfinder::center_of(*cell);
  p->x = cx;
  p->y = cy;
  if (Movement* m = world_.get<Movement>(u)) {
    m->tx = cx;
    m->ty = cy;
    m->moving = false;
  }
}

void Sim::enter_construction(EntityId e, EntityId b) {
  Worker* w = world_.get<Worker>(e);
  Building* bd = world_.get<Building>(b);
  Position* p = world_.get<Position>(e);
  const Position* bp = world_.get<Position>(b);
  if (!w || !bd || !p || !bp) return;
  stop(e);
  w->state = Worker::Construct;
  w->site = b;
  w->inside = b;
  bd->builder_in = e;
  // bent nem akadály és nem célpont: a test és az útvonal kikerül a navigációból
  world_.pool<Body>().remove(e);
  world_.pool<Path>().remove(e);
  p->x = bp->x;
  p->y = bp->y;
}

void Sim::leave_construction(EntityId b, bool hurt) {
  Building* bd = world_.get<Building>(b);
  if (!bd || bd->builder_in == 0) return;
  const EntityId e = bd->builder_in;
  bd->builder_in = 0;
  if (!world_.alive(e)) return;
  Worker* w = world_.get<Worker>(e);
  const Unit* u = world_.get<Unit>(e);
  if (!w || !u) return;
  w->inside = 0;
  w->site = 0;
  if (nav_) {
    world_.add(e, Body{});
    world_.add(e, Path{});
    const auto air = nav_->nav().table().movement_class("air");
    const EntityDef& d = rules_->defs[u->def];
    set_body(e, d.radius, d.flying && air ? *air : 0xFF);
  }
  // a régi játék szerint: az épület közepe alatt, a lábnyom 60%-ánál a legközelebbi szabad helyre
  place_at_free(e, Fixed::from_int(bd->tx) + Fixed::from_ratio(bd->w, 2), Fixed::from_int(bd->ty) + Fixed::from_ratio(bd->h * 6 + 5, 10));
  if (hurt) {
    if (Health* h = world_.get<Health>(e)) h->hp = std::max(1, h->hp / 2);
  }
  const EntityId res = w->resume;
  w->resume = 0;
  w->state = Worker::Idle;
  if (res && !order_gather(e, res)) {   // a régi lelőhely elfogyott: ugyanilyet keres a közelben
    w->state = Worker::Go;
    w->res = 0;
  }
}

void Sim::build_order_step(EntityId e, Worker& w) {
  const Health* hp = world_.get<Health>(e);
  const Position* pos = world_.get<Position>(e);
  const Movement* mv = world_.get<Movement>(e);
  if (!hp || !pos || !mv) return;
  const uint8_t owner = hp->owner;
  auto finish = [&]() {
    const EntityId res = w.resume;
    w.resume = 0;
    w.site = 0;
    stop(e);
    if (res) order_gather(e, res);
  };
  const FactionDef* fac = rules_->faction(players_[owner].faction);
  const std::string style = fac ? fac->builder : std::string("stay");
  if (w.state == Worker::Build) {
    const EntityDef& d = rules_->defs[w.build_def];
    const int bw = std::max(1, d.fw), bh = std::max(1, d.fh);
    Fixed radius = Fixed::from_ratio(4, 10);
    if (const Unit* u = world_.get<Unit>(e)) radius = rules_->defs[u->def].radius;
    const Fixed lim = radius + Fixed::from_ratio(8, 10);
    if (rect_dist_sq(pos->x, pos->y, w.btx, w.bty, bw, bh) <= int64_t{lim.raw()} * lim.raw()) {
      if (!can_place(owner, w.build_def, w.btx, w.bty)) {   // közben elfoglalták: a költség vissza
        refund(owner, d.cost, 1000);
        finish();
        return;
      }
      const EntityId b = place_building(w.build_def, owner, w.btx, w.bty, false);
      // az épület alá került egységek a legközelebbi szabad helyre (a régi: landing_position)
      std::vector<EntityId> under;
      world_.pool<Body>().each([&](EntityId o, const Body& body) {
        const Position* op = world_.get<Position>(o);
        if (op && body.grid >= 0 && rect_dist_sq(op->x, op->y, w.btx, w.bty, bw, bh) < int64_t{body.radius.raw()} * body.radius.raw()) {
          under.push_back(o);
        }
      });
      for (const EntityId o : under) place_at_free(o, world_.get<Position>(o)->x, world_.get<Position>(o)->y);
      if (style == "consume") {
        kill(e);
      } else if (style == "stay" || style == "stay_single" || style == "enter") {
        w.state = Worker::Construct;
        w.site = b;
        w.move_to = 0;
      } else {   // leave: lerakja, és megy a dolgára
        finish();
      }
      return;
    }
    if (w.move_to != kSiteMove) {
      order_move(e, Fixed::from_int(w.btx) + Fixed::from_ratio(bw, 2), Fixed::from_int(w.bty) + Fixed::from_ratio(bh, 2));
      w.move_to = kSiteMove;
      return;
    }
    if (!mv->moving) {   // nem ért oda: újra, háromszor; utána a költség vissza
      w.move_to = 0;
      if (++w.fails > 3) {
        refund(owner, d.cost, 1000);
        finish();
      }
    }
    return;
  }
  if (w.state == Worker::Construct) {
    if (w.inside) return;   // bent épít (a haladást a construction_system számolja)
    Building* b = world_.alive(w.site) ? world_.get<Building>(w.site) : nullptr;
    if (!b || b->complete) {
      finish();
      return;
    }
    if (in_reach(e, w.site)) {
      if (style == "enter") {
        if (b->builder_in == 0) enter_construction(e, w.site);
        else if (b->builder_in != e) finish();   // már dolgozik bent valaki
        return;
      }
      if (b->builders < 255) ++b->builders;
      if (mv->moving) {
        const uint8_t keep = w.state;
        stop(e);
        w.state = keep;
      }
      return;
    }
    if (w.move_to != w.site || !mv->moving) {
      const Position& bp = *world_.get<Position>(w.site);
      order_move(e, bp.x, bp.y);
      w.move_to = w.site;
    }
  }
}

void Sim::construction_system() {
  std::vector<EntityId> completed;   // az e tickben elkészültek (a népmodulok eseményéhez)
  world_.pool<Building>().each([&](EntityId e, Building& b) {
    if (b.complete) {
      b.builders = 0;
      return;
    }
    Health* h = world_.get<Health>(e);
    if (!h || h->hp <= 0) return;
    const FactionDef* fac = rules_->faction(players_[h->owner].faction);
    const std::string& style = fac ? fac->builder : std::string();
    // MECH-BUILD: a haladás sebessége ezrelékben
    int32_t rate = 1000;
    if (style == "stay") rate = b.builders == 0 ? 0 : 1000 + 500 * (std::min<int>(b.builders, 4) - 1);
    else if (style == "stay_single") rate = b.builders ? 1000 : 0;
    else if (style == "enter") rate = b.builder_in && world_.alive(b.builder_in) ? 1000 : 0;
    if (world_.get<SelfBuild>(e)) rate = 1000;   // letelepített Wullof-épület: munkás nélkül
    rate = static_cast<int32_t>((Fixed::from_int(rate) * mod_build(e)).floor_int());   // népmodul
    b.builders = 0;
    if (!rate) return;
    rate = static_cast<int32_t>((Fixed::from_int(rate) * env_build_mult(h->owner)).floor_int());   // éjjel/időjárás (D35)
    if (!rate) return;
    const EntityDef& d = rules_->defs[b.def];
    const int64_t total = int64_t{std::max(1, d.time_ticks)} * 1000;
    // az életerő a haladással nő: 10% + 90% × haladás (pontos egész lépésekben); a cél a tényleges (kutatással
    // módosított) maximum, nem a sablon alapértéke
    auto target = [&](int64_t p) { return int64_t{h->max} / 10 + int64_t{h->max} * 9 * std::min(p, total) / (10 * total); };
    const int64_t before = b.progress;
    b.progress = static_cast<int32_t>(std::min<int64_t>(total, before + rate));
    h->hp = static_cast<int32_t>(std::min<int64_t>(h->max, h->hp + target(b.progress) - target(before)));
    if (b.progress >= total) {
      b.complete = 1;
      h->hp = std::min(h->max, h->hp + h->max - static_cast<int32_t>(target(total)));   // a kerekítési maradék
      if (b.builder_in) leave_construction(e, false);
      completed.push_back(e);
    }
  });
  for (const EntityId c : completed) {
    for (auto& m : modules_) m->on_building_complete(*this, c);
  }
}

bool Sim::demolish(EntityId e) {
  if (!rules_ || !world_.alive(e)) return false;
  const Building* b = world_.get<Building>(e);
  const Health* h = world_.get<Health>(e);
  if (!b || !h || !b->complete || h->hp <= 0 || h->owner == 0 || h->owner > kMaxPlayers) return false;
  const FactionDef* f = rules_->faction(players_[h->owner].faction);
  if (!f || !f->demolish) return false;   // ennél a népnél nem bontható (D38)
  refund(h->owner, rules_->defs[b->def].cost, rules_->demolish_refund_permille);
  if (world_.get<Garrison>(e)) unload(e);   // a helyőrség kiszáll
  if (nav_) nav_->remove_obstacle({b->tx, b->ty, b->w, b->h});
  world_.destroy(e);
  return true;
}

bool Sim::interrupt(EntityId e) {
  if (!rules_ || !world_.alive(e)) return true;
  world_.pool<Enter>().remove(e);              // a beszállás is elmarad
  world_.pool<Fetch>().remove(e);              // a tárgyért menés is
  world_.pool<RepairOrder>().remove(e);        // a javítás is
  world_.pool<Seize>().remove(e);              // az elfoglalás és a megszállás is
  if (Wagon* wg = world_.get<Wagon>(e)) wg->unpacking = 0;
  if (world_.get<Sheltered>(e)) return false;  // a helyőrségben lévő nem kap parancsot (előbb ki kell szállnia)
  if (Caster* cs = world_.get<Caster>(e)) {   // a folyamatban lévő célzott képesség és a munkába visszatérés elmarad
    cs->cast = kNoAbility;
    cs->ctarget = 0;
    cs->resume = 0;
    cs->resume_buff = kNoAbility;
  }
  if (Combat* c = world_.get<Combat>(e)) {   // a harci parancs is véget ér (az új parancs dönt)
    c->order = Combat::None;
    c->target = 0;
    c->moving = 0;
    c->autonomous = 0;
  }
  Worker* w = world_.get<Worker>(e);
  if (!w) return true;
  if (w->inside) return false;
  if (w->state == Worker::Build) {
    if (const Health* h = world_.get<Health>(e)) refund(h->owner, rules_->defs[w->build_def].cost, 1000);
  }
  w->state = Worker::Idle;
  w->resume = 0;
  w->site = 0;
  return true;
}

bool Sim::cancel_build(EntityId e) {
  if (!rules_ || !world_.alive(e)) return false;
  const Building* b = world_.get<Building>(e);
  const Health* h = world_.get<Health>(e);
  if (!b || !h || b->complete) return false;
  if (b->builder_in) leave_construction(e, false);
  refund(h->owner, rules_->defs[b->def].cost, rules_->cancel_refund_permille);
  if (nav_) nav_->remove_obstacle({b->tx, b->ty, b->w, b->h});
  world_.destroy(e);
  return true;
}

}  // namespace totu::sim

namespace totu::sim {

bool Sim::gather_nearest(EntityId worker, int kind) {
  const Position* p = world_.get<Position>(worker);
  if (!rules_ || !p || kind < 0 || kind >= kResources || !interrupt(worker)) return false;
  const EntityId r = nearest_resource(kind, p->x, p->y, Fixed::from_int(1000), kNoEntity);
  return r != kNoEntity && order_gather(worker, r);
}

bool Sim::build_near(uint8_t owner, uint16_t def, int x, int y) {
  if (!rules_ || !map_ || def >= rules_->defs.size()) return false;
  // a munkás: a tétlenek közül, ha nincs, a gyűjtők közül a legkisebb azonosítójú (determinisztikus)
  EntityId worker = kNoEntity;
  int best_rank = 3;
  world_.pool<Worker>().each([&](EntityId e, const Worker& w) {
    const Health* h = world_.get<Health>(e);
    if (!h || h->owner != owner || h->hp <= 0 || w.inside || sheltered(e)) return;
    if (const Unit* wu = world_.get<Unit>(e); wu && !rules_->defs[wu->def].builds) return;   // nem építő munkás
    const int rank = w.state == Worker::Idle ? 0 : (w.state == Worker::Build || w.state == Worker::Construct) ? 3 : 1;
    if (rank < best_rank) {
      best_rank = rank;
      worker = e;
    }
  });
  if (worker == kNoEntity) return false;
  // a hely: egyre táguló gyűrűkben a (x, y) körül, egy csempe hézaggal (a munkások átférjenek)
  for (int r = 0; r <= 14; ++r) {
    for (int dy = -r; dy <= r; ++dy) {
      for (int dx = -r; dx <= r; ++dx) {
        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
        const int tx = x + dx, ty = y + dy;
        if (!can_place(owner, def, tx, ty)) continue;
        if (!can_place(0, def, tx - 1, ty - 1) && !can_place(0, def, tx + 1, ty + 1)) continue;   // ne zárjon körbe
        return order_build(worker, def, tx, ty);
      }
    }
  }
  return false;
}

}  // namespace totu::sim
