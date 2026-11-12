// SPDX-License-Identifier: GPL-3.0-or-later
// Navigációs mozgás (M4 3. lépés, ADR-006 5–6. pont): útkérés-sor tickenkénti kerettel, útpontkövetés a terep
// sebességével, ütközés kemény sugárral (soha nem lapolódik át), kitérés rögzített szögsorrendben, az álló saját
// egység félretolása, beragadásfigyelő újratervezéssel, tömeg a célnál. Minden lépés entitásazonosító szerinti
// sorrendben, fixpontosan – determinisztikus.
#include <algorithm>
#include <array>
#include <map>
#include <tuple>

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

// csapat a kapukhoz (ADR-006 8. pont): az 1–8. játékos a 0–7. csapat; tulajdonos nélkül nincs csapat
int team_of_owner(uint8_t owner) { return owner ? (owner - 1) & 7 : map::Pathfinder::kNoTeam; }

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
  p->retries = 0;
  p->wait = 0;
  p->cap = Fixed{};
  p->group = 0;
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

void Sim::nav_block(int x, int y, int w, int h, bool add) {
  if (!nav_) return;
  if (add) nav_->add_obstacle({x, y, w, h});
  else nav_->remove_obstacle({x, y, w, h});
  navigation_changed();
}

void Sim::nav_gate(int x, int y, int w, int h, uint8_t mask) {
  if (!nav_) return;
  nav_->set_gate({x, y, w, h}, mask);
  navigation_changed();
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
  nav_stats_.expanded_this_tick = 0;
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
    const Health* hh = world_.get<Health>(e);
    const int team = team_of_owner(hh ? hh->owner : 0);
    const bool direct_possible = b.grid < 0;
    const map::NavCell sc = map::Pathfinder::cell_of(pos.x, pos.y), gc = map::Pathfinder::cell_of(p.gx, p.gy);
    const bool direct = direct_possible || (nav_->nav().in_bounds(gc.x, gc.y) && nav_->line_of_sight(b.grid, b.size, sc, gc, team));
    if (!direct && (budget <= 0 || nav_stats_.expanded_this_tick >= nav_params_.node_budget)) {
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
    auto r = nav_->find_path(b.grid, b.size, pos.x, pos.y, p.gx, p.gy, team);
    nav_stats_.expanded_total += static_cast<int64_t>(r.expanded);
    nav_stats_.expanded_this_tick += static_cast<int64_t>(r.expanded);
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
bool Sim::nav_free(EntityId e, const Body& b, uint8_t owner, Fixed x, Fixed y, Fixed cx, Fixed cy, EntityId* blocker) {
  const map::NavGrid& nav = nav_->nav();
  if (b.grid >= 0) {
    const map::NavCell c = map::Pathfinder::cell_of(x, y);
    if (!nav_->passable(b.grid, b.size, c.x, c.y, team_of_owner(owner))) return false;
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
      const Health* bh = world_.get<Health>(blocker);
      if (!nav_->passable(bb->grid, bb->size, c.x, c.y, team_of_owner(bh ? bh->owner : 0))) continue;
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

// Szemből jövő, mozgó saját egység: egy kis oldalirányú lökés (puha ütközés-szétválasztás), az útját nem változtatja
// meg – így a két irányú forgalom sávokra válik (híd, szoros), nem akad össze.
bool Sim::nav_nudge(EntityId blocker, Fixed vx, Fixed vy, Fixed px, Fixed py) {
  const Body* bb = world_.get<Body>(blocker);
  Position* pos = world_.get<Position>(blocker);
  Path* bp = world_.get<Path>(blocker);
  const Health* bh = world_.get<Health>(blocker);
  if (!bb || !pos || !bp) return false;
  const Fixed len = core::length(vx, vy);
  if (len.raw() == 0) return false;
  const Fixed step = Fixed::from_ratio(3, 20);
  const int64_t cross = int64_t{vx.raw()} * (pos->y - py).raw() - int64_t{vy.raw()} * (pos->x - px).raw();
  const int first = cross >= 0 ? 1 : -1;
  for (int side : {first, -first}) {
    const Fixed nx = pos->x + Fixed::from_int(-side) * vy * step / len, ny = pos->y + Fixed::from_int(side) * vx * step / len;
    if (!nav_free(blocker, *bb, bh ? bh->owner : 0, nx, ny, pos->x, pos->y, nullptr)) continue;
    nav_bucket_move(blocker, pos->x, pos->y, nx, ny);
    pos->x = nx;
    pos->y = ny;
    bp->moved_tick = tick_;
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
                                               map::Pathfinder::cell_of(nx2, ny2), team_of_owner(h ? h->owner : 0))) {
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
    if (p->cap.raw() > 0 && p->cap < speed) speed = p->cap;   // csoportsebesség
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
      p->moved_tick = tick_;
    }
    // 2. Ha egy előttünk ugyanarra haladó saját egység áll az útban: kivárjuk (sor), nem lépünk ki oldalra – az
    //    oldalazás a sorban csak ide-oda ingást okozna. Álló, idegen vagy szemből jövő akadálynál kitérünk; szemből
    //    jövőnél mindenki előbb jobbra (rögzített kézzel), így két sáv alakul ki (pl. hídon).
    const Movement* bm = blocker != kNoEntity ? world_.get<Movement>(blocker) : nullptr;
    bool follow = false, oncoming = false;
    if (!moved && bm && bm->moving) {
      const Health* bh = healths.get(blocker);
      const Path* bp = paths.get(blocker);
      const Position* bpos = positions.get(blocker);
      if (bp && bpos && bp->idx < bp->wps.size()) {
        const Fixed hx = bp->wps[bp->idx].first - bpos->x, hy = bp->wps[bp->idx].second - bpos->y;
        const int64_t dot = int64_t{hx.raw()} * vx.raw() + int64_t{hy.raw()} * vy.raw();
        follow = dot > 0 && bh && bh->owner == owner;
        oncoming = dot < 0;
      }
    }
    // Terep állja az utat (sarok): falmenti csúszás – a mozgás egyik tengelyirányú összetevője a teljes sebességgel.
    if (!moved && blocker == kNoEntity) {
      const Fixed ax = vx.raw() >= 0 ? speed : Fixed{} - speed, ay = vy.raw() >= 0 ? speed : Fixed{} - speed;
      const bool x_first = (vx.raw() < 0 ? -vx.raw() : vx.raw()) >= (vy.raw() < 0 ? -vy.raw() : vy.raw());
      for (int pass = 0; pass < 2 && !moved; ++pass) {
        const bool along_x = (pass == 0) == x_first;
        if ((along_x ? vx : vy).raw() == 0) continue;
        const Fixed nx = pos->x + (along_x ? ax : Fixed{}), ny = pos->y + (along_x ? Fixed{} : ay);
        if (nav_free(e, *b, owner, nx, ny, pos->x, pos->y, nullptr)) {
          nav_bucket_move(e, pos->x, pos->y, nx, ny);
          pos->x = nx;
          pos->y = ny;
          moved = true;
          p->moved_tick = tick_;
        }
      }
    }
    if (!moved && !follow) {
      static constexpr size_t kDefault[6] = {1, 2, 3, 4, 5, 6};   // +30, -30, +60, -60, +90, -90
      static constexpr size_t kRight[6] = {1, 3, 5, 2, 4, 6};     // előbb az egyik oldal végig
      const size_t* order = oncoming ? kRight : kDefault;
      for (size_t q = 0; q < 6; ++q) {
        const Turn& t = turns()[order[q]];
        const Fixed rx = vx * t.c - vy * t.s, ry = vx * t.s + vy * t.c;
        const Fixed nx = pos->x + rx, ny = pos->y + ry;
        if (nav_free(e, *b, owner, nx, ny, pos->x, pos->y, nullptr)) {
          nav_bucket_move(e, pos->x, pos->y, nx, ny);
          pos->x = nx;
          pos->y = ny;
          moved = true;
          p->moved_tick = tick_;
          break;
        }
      }
    }
    if (!moved && oncoming) {
      const Health* bh = healths.get(blocker);
      if (bh && bh->owner == owner) nav_nudge(blocker, vx, vy, pos->x, pos->y);
    }
    // az álló saját egységet félretoljuk
    bool shoved = false;
    if (blocker != kNoEntity && !(bm && bm->moving)) {
      const Health* bh = healths.get(blocker);
      if (bh && bh->owner == owner) shoved = nav_shove(blocker, vx, vy, pos->x, pos->y);
    }
    // Tömeg a célnál: az utolsó útpont felé egy egység útját állja (és nem tolható félre), már a célhoz közel –
    // megérkezett. Alakzat tagjának a saját helye pontos: neki csak 1 csempén belül.
    const Fixed slack = p->group ? Fixed::from_int(1) : nav_params_.arrive_slack;
    if (!moved && !shoved && blocker != kNoEntity && p->idx + 1 == p->wps.size() && dist <= slack) {
      nav_arrive(e, m, *p, *pos);
      return;
    }
    if (moved && straight && can_reach) {
      ++p->idx;
      p->best = Fixed::max();
      p->stuck = 0;
      p->repaths = 0;   // haladt: az újratervezések száma újraindul
      if (p->idx >= p->wps.size()) nav_arrive(e, m, *p, *pos);
      return;
    }
    // Mozgó egység miatti akadályoztatás (sor a szűkületben, szemből jövők a hídon) nem beragadás – az újratervezés
    // sem segítene (az útkereső az egységeket nem látja), a feladás pedig dugót csinálna (a megállt egység maga is
    // akadály). Ilyenkor vár; ha sokáig, a nagyobb azonosítójú enged: hátrább és jobbra lép (patthelyzet feloldása).
    // Beragadás (újratervezés, végül megállás) csak terep vagy álló akadály miatt lehet.
    const Path* blp = blocker != kNoEntity ? paths.get(blocker) : nullptr;
    const bool waiting = blocker != kNoEntity && bm && bm->moving && blp && tick_ - blp->moved_tick <= 3;
    const Fixed nd = core::length(wx - pos->x, wy - pos->y);
    if (nd < p->best) {
      p->best = nd;
      p->stuck = 0;
      p->wait = 0;
    } else if (waiting) {
      p->stuck = 0;
      if (++p->wait >= nav_params_.wait_ticks) {
        p->wait = 0;
        if (e > blocker) {
          const Turn& t = turns()[5];   // +90°: jobbra
          const Fixed bx = Fixed{} - vx + (vx * t.c - vy * t.s), by = Fixed{} - vy + (vx * t.s + vy * t.c);
          const Fixed nx = pos->x + bx / Fixed::from_int(2), ny = pos->y + by / Fixed::from_int(2);
          if (nav_free(e, *b, owner, nx, ny, pos->x, pos->y, nullptr)) {
            nav_bucket_move(e, pos->x, pos->y, nx, ny);
            pos->x = nx;
            pos->y = ny;
            p->best = Fixed::max();
          }
        }
      }
    } else {
      ++p->stuck;
    }
    if (p->stuck >= nav_params_.stuck_ticks) {
      // a tényleges végpont: elérhetetlen célnál a legközelebbi elérhető pont (ahol a többiek már összegyűltek)
      const auto [ex, ey] = p->wps.empty() ? std::make_pair(p->gx, p->gy) : p->wps.back();
      const Fixed gd = core::length(ex - pos->x, ey - pos->y);
      // Elfogyott újratervezés: csak mozdulatlan környezetben áll meg végleg – ha a közelben valaki még ténylegesen
      // halad (az utolsó 1 mp-ben; oldódó dugó), türelmesen próbálkozik tovább (különben a dugó után ottmaradna).
      bool alive_jam = false;
      if (p->repaths >= nav_params_.max_repaths && !p->shoved && gd > nav_params_.arrive_slack) {
        const int bx = std::clamp(tile_of(pos->x) / kBucket, 0, nav_bw_ - 1), by = std::clamp(tile_of(pos->y) / kBucket, 0, nav_bh_ - 1);
        for (int yy = std::max(0, by - 1); yy <= std::min(nav_bh_ - 1, by + 1) && !alive_jam; ++yy) {
          for (int xx = std::max(0, bx - 1); xx <= std::min(nav_bw_ - 1, bx + 1) && !alive_jam; ++xx) {
            for (const EntityId o : buckets_[static_cast<size_t>(yy * nav_bw_ + xx)]) {
              const Path* op = paths.get(o);
              if (o != e && op && tick_ - op->moved_tick <= 30) { alive_jam = true; break; }
            }
          }
        }
      }
      if (alive_jam) {
        p->stuck = 0;
      } else if (p->repaths >= nav_params_.max_repaths && !p->shoved &&
                 gd > nav_params_.arrive_slack && p->retries < nav_params_.max_retries) {
        // Egység tartja fel: az újratervezés nem segítene (az útkereső az egységeket nem látja) – kivár és
        // újrakezd. Megállt dugó (nem zsákutca): sem adja fel végleg, mert a dugó gyakran éppen az engedéstől oldódik.
        // megállt dugó (nem zsákutca): nem adja fel végleg, hanem tovább próbálkozik – a dugó gyakran éppen azért
        // oldódik, mert valaki enged; a zsákutcában a végpont közelében megáll (az előző ág)
        ++p->retries;
        p->repaths = 0;
        p->stuck = -nav_params_.retry_pause;
      } else if (p->shoved || gd <= nav_params_.arrive_slack || p->repaths >= nav_params_.max_repaths) {
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

void Sim::order_group_move(std::vector<EntityId> units, Fixed x, Fixed y, Formation f) {
  std::sort(units.begin(), units.end());
  units.erase(std::unique(units.begin(), units.end()), units.end());
  std::vector<EntityId> ids;
  for (const EntityId e : units) {
    if (!world_.alive(e) || !world_.get<Movement>(e) || !world_.get<Position>(e)) continue;
    if (nav_ && (!world_.get<Body>(e) || !world_.get<Path>(e))) continue;
    ids.push_back(e);
  }
  if (!nav_ || ids.size() < 2) {
    for (const EntityId e : ids) order_move(e, x, y);
    return;
  }
  const auto n = static_cast<int64_t>(ids.size());
  // a csoport közepe és a haladási irány
  int64_t sx = 0, sy = 0;
  Fixed max_r;
  for (const EntityId e : ids) {
    const Position& p = *world_.get<Position>(e);
    sx += p.x.raw();
    sy += p.y.raw();
    max_r = std::max(max_r, world_.get<Body>(e)->radius);
  }
  const Fixed cx = Fixed::from_raw(static_cast<int32_t>(sx / n)), cy = Fixed::from_raw(static_cast<int32_t>(sy / n));
  Fixed fx = Fixed{}, fy = Fixed::from_int(1);
  if (const Fixed len = core::length(x - cx, y - cy); len.raw() > 0) {
    fx = (x - cx) / len;
    fy = (y - cy) / len;
  }
  const Fixed rx = Fixed{} - fy, ry = fx;   // oldalirány
  const FormationSpec& spec = formations_[f];
  const auto slots = formation_slots(f, static_cast<int>(n), spec, max_r * Fixed::from_int(2) + Fixed::from_ratio(1, 5));
  const Fixed depth_center = slots.back().depth / Fixed::from_int(2);

  // a tagok sorrendje: (a WC3-szerű alakzatnál előbb a közelharcosok), elöl lévők előbb, azonosító
  struct Member {
    EntityId e;
    int cat;
    Fixed fwd, lat;
  };
  std::vector<Member> ms;
  Fixed cap = Fixed::max();
  for (const EntityId e : ids) {
    const Position& p = *world_.get<Position>(e);
    const Attack* a = world_.get<Attack>(e);
    const int cat = (f == Formation::Auto && a && a->range > Fixed::from_ratio(3, 2)) ? 1 : 0;
    ms.push_back({e, cat, (p.x - cx) * fx + (p.y - cy) * fy, (p.x - cx) * rx + (p.y - cy) * ry});
    cap = std::min(cap, world_.get<Movement>(e)->speed);
  }
  std::sort(ms.begin(), ms.end(), [](const Member& a, const Member& b) {
    if (a.cat != b.cat) return a.cat < b.cat;
    if (a.fwd != b.fwd) return a.fwd > b.fwd;
    return a.e < b.e;
  });
  // soronként: a sor tagjai oldalirány szerint rendezve kapják a sor helyeit (balról jobbra) – nincs keresztezés
  std::vector<std::pair<EntityId, std::pair<Fixed, Fixed>>> goals;
  size_t i = 0;
  while (i < slots.size()) {
    size_t j = i;
    while (j < slots.size() && slots[j].row == slots[i].row) ++j;
    std::vector<Member> row(ms.begin() + static_cast<std::ptrdiff_t>(i), ms.begin() + static_cast<std::ptrdiff_t>(j));
    std::sort(row.begin(), row.end(), [](const Member& a, const Member& b) { return a.lat != b.lat ? a.lat < b.lat : a.e < b.e; });
    for (size_t k = 0; k < row.size(); ++k) {
      const Slot& s = slots[i + k];
      const Fixed ahead = depth_center - s.depth;
      goals.push_back({row[k].e, {x + rx * s.lateral + fx * ahead, y + ry * s.lateral + fy * ahead}});
    }
    i = j;
  }
  std::sort(goals.begin(), goals.end());
  // a helyek járhatósága: ha a hely nem jó, a legközelebbi elérhető pont a tag saját komponensében
  for (auto& [e, g] : goals) {
    const Body& b = *world_.get<Body>(e);
    const Position& p = *world_.get<Position>(e);
    if (b.grid >= 0) {
      const Health* gh = world_.get<Health>(e);
      const int team = team_of_owner(gh ? gh->owner : 0);
      const map::NavCell gc = map::Pathfinder::cell_of(g.first, g.second);
      const uint32_t comp = nav_->component(b.grid, b.size, map::Pathfinder::cell_of(p.x, p.y), team);
      if (comp && nav_->component(b.grid, b.size, gc, team) != comp) {
        if (const auto nc = nav_->nearest(b.grid, b.size, comp, gc, team)) g = map::Pathfinder::center_of(*nc);
      }
    }
    Movement& m = *world_.get<Movement>(e);
    m.tx = g.first;
    m.ty = g.second;
    m.moving = true;
    nav_request(e, g.first, g.second);
    world_.get<Path>(e)->cap = spec.together ? cap : Fixed{};
    world_.get<Path>(e)->group = 1;
  }
  // közös út osztályonként (mozgásosztály, méret): egy útkeresés a csoport középhez legközelebbi tagjából
  std::map<std::tuple<int, int, int>, std::vector<EntityId>> classes;   // (navrács, méret, csapat)
  for (const auto& [e, g] : goals) {
    const Body& b = *world_.get<Body>(e);
    const Health* gh = world_.get<Health>(e);
    if (b.grid >= 0) classes[{b.grid, b.size, team_of_owner(gh ? gh->owner : 0)}].push_back(e);
  }
  for (const auto& [key, members] : classes) {
    if (members.size() < 2) continue;
    EntityId rep = members.front();
    int64_t best = -1;
    for (const EntityId e : members) {
      const Position& p = *world_.get<Position>(e);
      const int64_t d = core::length_sq_raw(p.x - cx, p.y - cy);
      if (best < 0 || d < best) { best = d; rep = e; }
    }
    const Position& rp = *world_.get<Position>(rep);
    const auto [kg, ks, kt] = key;
    auto r = nav_->find_path(kg, ks, rp.x, rp.y, x, y, kt);
    ++nav_stats_.paths_total;
    if (!r.found || r.waypoints.size() < 2) continue;   // egyenes út: a tagok úgyis látóvonalon mennek
    r.waypoints.pop_back();   // a csoport célpontja helyett a tag saját helye
    const map::NavCell last = map::Pathfinder::cell_of(r.waypoints.back().first, r.waypoints.back().second);
    for (const EntityId e : members) {
      const Position& p = *world_.get<Position>(e);
      Path& path = *world_.get<Path>(e);
      if (!nav_->line_of_sight(kg, ks, last, map::Pathfinder::cell_of(path.gx, path.gy), kt)) continue;
      // a közös út legkésőbbi, a tagból látható pontjához csatlakozik (az első néhány pontot nézve)
      const map::NavCell from = map::Pathfinder::cell_of(p.x, p.y);
      size_t join = r.waypoints.size();
      for (size_t q = std::min<size_t>(r.waypoints.size(), 4); q-- > 0;) {
        const auto [wx, wy] = r.waypoints[q];
        if (nav_->line_of_sight(kg, ks, from, map::Pathfinder::cell_of(wx, wy), kt)) { join = q; break; }
      }
      if (join == r.waypoints.size()) continue;
      path.wps.assign(r.waypoints.begin() + static_cast<std::ptrdiff_t>(join), r.waypoints.end());
      path.wps.emplace_back(path.gx, path.gy);
      path.idx = 0;
      path.pending = 0;
      path.best = Fixed::max();
      ++nav_stats_.shared_total;
    }
  }
}

}  // namespace totu::sim
