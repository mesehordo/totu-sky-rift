// SPDX-License-Identifier: GPL-3.0-or-later
// Navigációs mozgás (M4 3. lépés, ADR-006 5–6. pont): útkérés-sor tickenkénti kerettel, útpontkövetés a terep
// sebességével, ütközés kemény sugárral (soha nem lapolódik át), kitérés rögzített szögsorrendben, az álló saját
// egység félretolása, beragadásfigyelő újratervezéssel, tömeg a célnál. Minden lépés entitásazonosító szerinti
// sorrendben, fixpontosan – determinisztikus.
#include <algorithm>
#include <array>

#include "core/angle.hpp"
#include "map/pathfinder.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

constexpr int kBucket = 2;   // a térbeli rács vödrei 2 × 2 csempések (a legnagyobb kemény sugárösszeg < 2)
const Fixed kPassRadius = Fixed::from_int(1);   // közbülső útpont ennyin belül teljesült

struct Turn {
  Fixed c, s;
};

// kitérési irányok: egyenesen, majd ±30°, ±60°, ±90° (rögzített sorrend)
const std::array<Turn, 7>& turns() {
  static const std::array<Turn, 7> t = [] {
    std::array<Turn, 7> a{};
    const int deg[7] = {0, 30, -30, 60, -60, 90, -90};
    for (int i = 0; i < 7; ++i) {
      const core::Angle ang = core::Angle::from_degrees(deg[i]);
      a[static_cast<size_t>(i)] = {core::cos(ang), core::sin(ang)};
    }
    return a;
  }();
  return t;
}

int tile_of(Fixed v) { return static_cast<int>(v.raw() >> 16); }

}  // namespace

void Sim::attach_navigation(map::Pathfinder* pf, NavParams params) {
  nav_ = pf;
  nav_params_ = params;
  world_.register_component<Body>("Body");
  world_.register_component<Path>("Path");
  const map::Map& m = pf->nav().map();
  nav_bw_ = (m.width() + kBucket - 1) / kBucket;
  nav_bh_ = (m.height() + kBucket - 1) / kBucket;
  buckets_.assign(static_cast<size_t>(nav_bw_ * nav_bh_), {});
}

bool Sim::set_body(EntityId e, Fixed radius, uint8_t movement_class) {
  if (!nav_) return false;
  Body* b = world_.get<Body>(e);
  if (!b) return false;
  const map::TerrainTable& table = nav_->nav().table();
  if (movement_class == 0xFF) movement_class = table.movement_class("land").value_or(0);
  if (movement_class >= table.movement_classes().size()) return false;
  b->radius = radius;
  b->grid = static_cast<int8_t>(nav_->nav().grid_of(movement_class));
  b->size = static_cast<uint8_t>(nav_->nav().size_class(radius));
  return true;
}

void Sim::nav_request(EntityId e, Fixed x, Fixed y) {
  Path* p = world_.get<Path>(e);
  if (!p) return;
  p->gx = x;
  p->gy = y;
  p->wps.clear();
  p->idx = 0;
  p->pending = 1;
  p->shoved = 0;
  p->request_tick = tick_;
  p->stuck = 0;
  p->repaths = 0;
  p->best = Fixed::max();
}

void Sim::navigation_changed() {
  if (!nav_) return;
  auto& paths = world_.pool<Path>();
  world_.pool<Movement>().each([&](EntityId e, Movement& m) {
    Path* p = paths.get(e);
    if (!m.moving || !p || p->shoved) return;
    p->pending = 1;
    p->request_tick = tick_;
    p->stuck = 0;
  });
}

void Sim::nav_arrive(EntityId, Movement& m, Path& p, const Position& pos) {
  m.moving = false;
  m.tx = pos.x;
  m.ty = pos.y;
  p.wps.clear();
  p.idx = 0;
  p.pending = 0;
  p.shoved = 0;
  p.stuck = 0;
}

void Sim::nav_paths() {
  nav_stats_.paths_this_tick = 0;
  std::vector<std::pair<int64_t, EntityId>> queue;
  world_.pool<Path>().each([&](EntityId e, const Path& p) {
    if (p.pending && world_.alive(e)) queue.emplace_back(p.request_tick, e);
  });
  std::sort(queue.begin(), queue.end());   // a régebbi kérés előbb, azon belül azonosító szerint
  int budget = nav_params_.path_budget;
  int left = 0;
  for (const auto& [t, e] : queue) {
    Path& p = *world_.get<Path>(e);
    const Body& b = *world_.get<Body>(e);
    const Position& pos = *world_.get<Position>(e);
    Movement& m = *world_.get<Movement>(e);
    const bool direct_possible = b.grid < 0;
    const map::NavCell sc = map::Pathfinder::cell_of(pos.x, pos.y), gc = map::Pathfinder::cell_of(p.gx, p.gy);
    const bool direct = direct_possible || (nav_->nav().in_bounds(gc.x, gc.y) && nav_->line_of_sight(b.grid, b.size, sc, gc));
    if (!direct && budget <= 0) {
      ++left;
      continue;
    }
    p.pending = 0;
    p.wps.clear();
    p.idx = 0;
    p.best = Fixed::max();
    p.stuck = 0;
    if (direct) {   // látóvonal: keresés nélkül, egyenesen
      p.wps.emplace_back(p.gx, p.gy);
      ++nav_stats_.direct_total;
      continue;
    }
    --budget;
    ++nav_stats_.paths_this_tick;
    ++nav_stats_.paths_total;
    auto r = nav_->find_path(b.grid, b.size, pos.x, pos.y, p.gx, p.gy);
    if (!r.found || r.waypoints.empty()) {
      nav_arrive(e, m, p, pos);
      continue;
    }
    p.wps = std::move(r.waypoints);
  }
  nav_stats_.queued = left;
}

void Sim::nav_rebuild_buckets() {
  for (auto& v : buckets_) v.clear();
  auto& pos = world_.pool<Position>();
  world_.pool<Body>().each([&](EntityId e, const Body&) {
    const Position* p = pos.get(e);
    if (!p || !world_.alive(e)) return;
    const int bx = std::clamp(tile_of(p->x) / kBucket, 0, nav_bw_ - 1);
    const int by = std::clamp(tile_of(p->y) / kBucket, 0, nav_bh_ - 1);
    buckets_[static_cast<size_t>(by * nav_bw_ + bx)].push_back(e);
  });
}

void Sim::nav_bucket_move(EntityId e, Fixed ox, Fixed oy, Fixed nx, Fixed ny) {
  const int obx = std::clamp(tile_of(ox) / kBucket, 0, nav_bw_ - 1), oby = std::clamp(tile_of(oy) / kBucket, 0, nav_bh_ - 1);
  const int nbx = std::clamp(tile_of(nx) / kBucket, 0, nav_bw_ - 1), nby = std::clamp(tile_of(ny) / kBucket, 0, nav_bh_ - 1);
  if (obx == nbx && oby == nby) return;
  auto& from = buckets_[static_cast<size_t>(oby * nav_bw_ + obx)];
  from.erase(std::find(from.begin(), from.end(), e));
  buckets_[static_cast<size_t>(nby * nav_bw_ + nbx)].push_back(e);
}

// Szabad-e az (x, y) hely az egységnek: a terep (méretosztály szerint) és a többi egység kemény sugara. Egy már
// átfedő egységtől való távolodás megengedett (különben két összeszorult egység sosem válna szét).
bool Sim::nav_free(EntityId e, const Body& b, uint8_t, Fixed x, Fixed y, Fixed cx, Fixed cy, EntityId* blocker) {
  const map::NavGrid& nav = nav_->nav();
  if (b.grid >= 0) {
    const map::NavCell c = map::Pathfinder::cell_of(x, y);
    if (!nav.fits(b.grid, b.size, c.x, c.y)) return false;
  } else if (x.raw() < 0 || y.raw() < 0 || tile_of(x) >= nav.map().width() || tile_of(y) >= nav.map().height()) {
    return false;
  }
  const Fixed hr = b.radius * nav_params_.hard_ratio;
  const int bx = std::clamp(tile_of(x) / kBucket, 0, nav_bw_ - 1), by = std::clamp(tile_of(y) / kBucket, 0, nav_bh_ - 1);
  bool free = true;
  for (int yy = std::max(0, by - 1); yy <= std::min(nav_bh_ - 1, by + 1); ++yy) {
    for (int xx = std::max(0, bx - 1); xx <= std::min(nav_bw_ - 1, bx + 1); ++xx) {
      for (const EntityId o : buckets_[static_cast<size_t>(yy * nav_bw_ + xx)]) {
        if (o == e) continue;
        const Body* ob = world_.get<Body>(o);
        const Position* op = world_.get<Position>(o);
        const Health* oh = world_.get<Health>(o);
        if (!ob || !op || (oh && oh->hp <= 0)) continue;
        if ((b.grid < 0) != (ob->grid < 0)) continue;   // a repülők és a földiek nem ütköznek
        const Fixed sum = hr + ob->radius * nav_params_.hard_ratio;
        const int64_t lim = int64_t{sum.raw()} * sum.raw();
        const int64_t nd = core::length_sq_raw(op->x - x, op->y - y);
        if (nd >= lim) continue;
        if (nd >= core::length_sq_raw(op->x - cx, op->y - cy)) continue;   // nem közeledik
        free = false;
        if (blocker && (*blocker == kNoEntity || o < *blocker)) *blocker = o;
      }
    }
  }
  return free;
}

// Az álló saját egység félreáll a mozgó útjából (Pottinger): a mozgás irányára merőlegesen, arra az oldalra, ahol
// már amúgy is van; ha ott nem fér el, a másikra.
bool Sim::nav_shove(EntityId blocker, Fixed vx, Fixed vy, Fixed px, Fixed py) {
  Movement* bm = world_.get<Movement>(blocker);
  Path* bp = world_.get<Path>(blocker);
  const Body* bb = world_.get<Body>(blocker);
  const Position* pos = world_.get<Position>(blocker);
  if (!bm || !bp || !bb || !pos || bm->moving) return false;
  const Fixed len = core::length(vx, vy);
  if (len.raw() == 0) return false;
  const Fixed shift = bb->radius * Fixed::from_int(2);
  const int64_t cross = int64_t{vx.raw()} * (pos->y - py).raw() - int64_t{vy.raw()} * (pos->x - px).raw();
  const int first = cross >= 0 ? 1 : -1;
  for (int side : {first, -first}) {
    // bal oldali merőleges: (-vy, vx)
    const Fixed ox = Fixed::from_int(-side) * vy * shift / len, oy = Fixed::from_int(side) * vx * shift / len;
    const Fixed tx = pos->x + ox, ty = pos->y + oy;
    if (bb->grid >= 0) {
      const map::NavCell c = map::Pathfinder::cell_of(tx, ty);
      if (!nav_->nav().fits(bb->grid, bb->size, c.x, c.y)) continue;
    }
    bp->gx = tx;
    bp->gy = ty;
    bp->wps.assign(1, {tx, ty});
    bp->idx = 0;
    bp->pending = 0;
    bp->shoved = 1;
    bp->stuck = 0;
    bp->repaths = 0;
    bp->best = Fixed::max();
    bm->tx = tx;
    bm->ty = ty;
    bm->moving = true;
    return true;
  }
  return false;
}

void Sim::nav_movement() {
  nav_rebuild_buckets();
  const map::NavGrid& nav = nav_->nav();
  auto& paths = world_.pool<Path>();
  auto& bodies = world_.pool<Body>();
  auto& positions = world_.pool<Position>();
  auto& healths = world_.pool<Health>();
  world_.pool<Movement>().each([&](EntityId e, Movement& m) {
    if (!m.moving) return;
    Path* p = paths.get(e);
    const Body* b = bodies.get(e);
    Position* pos = positions.get(e);
    const Health* h = healths.get(e);
    if (!p || !b || !pos || (h && h->hp <= 0)) return;
    if (p->pending) return;   // az útra vár
    if (p->idx >= p->wps.size()) {
      nav_arrive(e, m, *p, *pos);
      return;
    }
    // Közbülső útpont: 1 csempén belül teljesült, ha a következő már látóvonalban van – így a csoport tagjainak nem
    // kell mind pontosan ugyanazt a pontot érinteniük (különben ott torlódnak és ingadoznak).
    while (p->idx + 1 < p->wps.size()) {
      const auto [ix, iy] = p->wps[p->idx];
      if (core::length(ix - pos->x, iy - pos->y) > kPassRadius) break;
      const auto [nx2, ny2] = p->wps[p->idx + 1];
      if (b->grid >= 0 && !nav_->line_of_sight(b->grid, b->size, map::Pathfinder::cell_of(pos->x, pos->y),
                                               map::Pathfinder::cell_of(nx2, ny2))) {
        break;
      }
      ++p->idx;
      p->best = Fixed::max();
      p->stuck = 0;
      p->repaths = 0;
    }
    const auto [wx, wy] = p->wps[p->idx];
    const Fixed dx = wx - pos->x, dy = wy - pos->y;
    const Fixed dist = core::length(dx, dy);
    Fixed speed = m.speed;
    if (b->grid >= 0) {
      const map::NavCell c = map::Pathfinder::cell_of(pos->x, pos->y);
      const Fixed mult = nav.speed(b->grid, c.x, c.y);
      if (mult.raw() > 0) speed = speed * mult;   // akadályba szorult egység az alapsebességgel jut ki
    }
    if (dist.raw() == 0) {
      ++p->idx;
      p->best = Fixed::max();
      if (p->idx >= p->wps.size()) nav_arrive(e, m, *p, *pos);
      return;
    }
    Fixed vx = dx, vy = dy;
    const bool can_reach = dist <= speed;
    if (!can_reach) {
      vx = dx * speed / dist;
      vy = dy * speed / dist;
    }
    const uint8_t owner = h ? h->owner : 0;
    EntityId blocker = kNoEntity;
    bool moved = false, straight = false;
    // 1. egyenesen
    if (nav_free(e, *b, owner, pos->x + vx, pos->y + vy, pos->x, pos->y, &blocker)) {
      nav_bucket_move(e, pos->x, pos->y, pos->x + vx, pos->y + vy);
      pos->x += vx;
      pos->y += vy;
      moved = straight = true;
    }
    // 2. Ha egy előttünk ugyanarra haladó saját egység áll az útban: kivárjuk (sor), nem lépünk ki oldalra – az
    //    oldalazás a sorban csak ide-oda ingást okozna. Álló, idegen vagy szemből jövő akadálynál kitérünk.
    const Movement* bm = blocker != kNoEntity ? world_.get<Movement>(blocker) : nullptr;
    bool follow = false;
    if (!moved && bm && bm->moving) {
      const Health* bh = healths.get(blocker);
      const Path* bp = paths.get(blocker);
      const Position* bpos = positions.get(blocker);
      if (bh && bh->owner == owner && bp && bpos && bp->idx < bp->wps.size()) {
        const Fixed hx = bp->wps[bp->idx].first - bpos->x, hy = bp->wps[bp->idx].second - bpos->y;
        follow = int64_t{hx.raw()} * vx.raw() + int64_t{hy.raw()} * vy.raw() > 0;
      }
    }
    if (!moved && !follow) {
      for (size_t k = 1; k < turns().size(); ++k) {
        const Turn& t = turns()[k];
        const Fixed rx = vx * t.c - vy * t.s, ry = vx * t.s + vy * t.c;
        const Fixed nx = pos->x + rx, ny = pos->y + ry;
        if (nav_free(e, *b, owner, nx, ny, pos->x, pos->y, nullptr)) {
          nav_bucket_move(e, pos->x, pos->y, nx, ny);
          pos->x = nx;
          pos->y = ny;
          moved = true;
          break;
        }
      }
    }
    // Tömeg a célnál: az utolsó útpont felé, egy egység útját állja, és már a célhoz közel – megérkezett.
    if (!moved && blocker != kNoEntity && p->idx + 1 == p->wps.size() && dist <= nav_params_.arrive_slack) {
      nav_arrive(e, m, *p, *pos);
      return;
    }
    // az álló saját egységet félretoljuk
    if (blocker != kNoEntity && !(bm && bm->moving)) {
      const Health* bh = healths.get(blocker);
      if (bh && bh->owner == owner) nav_shove(blocker, vx, vy, pos->x, pos->y);
    }
    if (moved && straight && can_reach) {
      ++p->idx;
      p->best = Fixed::max();
      p->stuck = 0;
      p->repaths = 0;   // haladt: az újratervezések száma újraindul
      if (p->idx >= p->wps.size()) nav_arrive(e, m, *p, *pos);
      return;
    }
    // Egy mozgó egység mögötti várakozás (sor a szűkületben) nem beragadás – az újratervezés sem segítene, mert az
    // útkereső az egységeket nem látja. Csak a hosszú várakozás (pl. két szemből elakadt egység) számít annak.
    const bool waiting = !moved && bm && bm->moving;
    const Fixed nd = core::length(wx - pos->x, wy - pos->y);
    if (nd < p->best) {
      p->best = nd;
      p->stuck = 0;
      p->wait = 0;
    } else if (waiting) {
      if (++p->wait >= nav_params_.wait_ticks) {
        p->wait = 0;
        p->stuck = nav_params_.stuck_ticks;
      }
    } else {
      ++p->stuck;
    }
    if (p->stuck >= nav_params_.stuck_ticks) {
      // a tényleges végpont: elérhetetlen célnál a legközelebbi elérhető pont (ahol a többiek már összegyűltek)
      const auto [ex, ey] = p->wps.empty() ? std::make_pair(p->gx, p->gy) : p->wps.back();
      const Fixed gd = core::length(ex - pos->x, ey - pos->y);
      if (p->shoved || gd <= nav_params_.arrive_slack || p->repaths >= nav_params_.max_repaths) {
        nav_arrive(e, m, *p, *pos);   // tömeg a célnál, vagy nincs tovább: megáll (nincs körözés)
      } else {
        p->pending = 1;
        p->request_tick = tick_;
        ++p->repaths;
        p->stuck = 0;
      }
    }
  });
}

}  // namespace totu::sim
