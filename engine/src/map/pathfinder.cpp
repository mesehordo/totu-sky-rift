// SPDX-License-Identifier: GPL-3.0-or-later
#include "map/pathfinder.hpp"

#include <algorithm>
#include <functional>
#include <queue>

namespace totu::map {

namespace {

constexpr int kStraight = 10;
constexpr int kDiagonal = 14;
constexpr int kSmoothWindow = 64;   // az útsimítás legfeljebb ennyi navcellát néz előre

int64_t octile(int dx, int dy) {
  dx = dx < 0 ? -dx : dx;
  dy = dy < 0 ? -dy : dy;
  const int mn = std::min(dx, dy), mx = std::max(dx, dy);
  return int64_t{kStraight} * (mx - mn) + int64_t{kDiagonal} * mn;
}

// a nyílt lista eleme: (f, index); a legkisebb f, döntetlennél a kisebb index – teljes rendezés, így a kivétel
// sorrendje minden szabványkönyvtárban ugyanaz
using Entry = std::pair<int64_t, int64_t>;
using MinHeap = std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>>;

constexpr int kDirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};

}  // namespace

Pathfinder::Pathfinder(NavGrid& nav, int cluster) : nav_(nav), cs_(cluster) {
  ccx_ = (nav_.width() + cs_ - 1) / cs_;
  ccy_ = (nav_.height() + cs_ - 1) / cs_;
  sizes_ = static_cast<int>(nav_.config().sizes.size());
  hiers_.resize(static_cast<size_t>(nav_.classes() * sizes_));
  const size_t n = static_cast<size_t>(nav_.width()) * static_cast<size_t>(nav_.height());
  for (int g = 0; g < nav_.classes(); ++g) {
    for (int s = 0; s < sizes_; ++s) {
      Hier& h = hier(g, s);
      h.label.assign(n, 0);
      h.reps.assign(static_cast<size_t>(ccx_ * ccy_), {});
      for (int c = 0; c < ccx_ * ccy_; ++c) label_cluster(g, s, c);
      rebuild_graph(g, s);
    }
  }
  stamp_.assign(n, 0);
  g_.assign(n, 0);
  parent_.assign(n, -1);
  closed_.assign(n, 0);
  compute_max_speed();
}

void Pathfinder::compute_max_speed() {
  max_speed_raw_.assign(static_cast<size_t>(nav_.classes()), 1);
  for (int g = 0; g < nav_.classes(); ++g) {
    int32_t best = 1;
    for (int y = 0; y < nav_.height(); ++y) {
      for (int x = 0; x < nav_.width(); ++x) best = std::max(best, nav_.speed(g, x, y).raw());
    }
    max_speed_raw_[static_cast<size_t>(g)] = best;
  }
}

NavCell Pathfinder::cell_of(Fixed x, Fixed y) {
  // csempe → navcella: × 2, lefelé kerekítve (a raw >> 15 a negatívra is lefelé kerekít)
  return {static_cast<int>(x.raw() >> 15), static_cast<int>(y.raw() >> 15)};
}

std::pair<Fixed, Fixed> Pathfinder::center_of(NavCell c) {
  // (n + 0,5) / 2 csempe
  return {Fixed::from_raw(c.x * 32768 + 16384), Fixed::from_raw(c.y * 32768 + 16384)};
}

uint32_t Pathfinder::region_index(const Hier& h, int x, int y) const {
  const uint16_t l = h.label[idx(x, y)];
  return l ? h.offset[static_cast<size_t>(cluster_of(x, y))] + l : 0;
}

void Pathfinder::label_cluster(int grid, int size, int c) {
  Hier& h = hier(grid, size);
  const int x0 = (c % ccx_) * cs_, y0 = (c / ccx_) * cs_;
  const int x1 = std::min(nav_.width(), x0 + cs_), y1 = std::min(nav_.height(), y0 + cs_);
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) h.label[idx(x, y)] = 0;
  }
  auto& reps = h.reps[static_cast<size_t>(c)];
  reps.clear();
  const int cx2 = x0 + x1 - 1, cy2 = y0 + y1 - 1;   // a klaszter közepe kétszeres koordinátában
  std::vector<NavCell> queue;
  uint16_t next = 1;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      if (h.label[idx(x, y)] || !nav_.fits(grid, size, x, y)) continue;
      const uint8_t g0 = nav_.gate(x, y);   // a kapucellák külön régiót alkotnak (kapumaszkonként)
      NavCell best{x, y};
      int64_t best_d = -1;
      queue.assign(1, NavCell{x, y});
      h.label[idx(x, y)] = next;
      for (size_t q = 0; q < queue.size(); ++q) {
        const NavCell p = queue[q];
        const int64_t dx = 2 * p.x - cx2, dy = 2 * p.y - cy2;
        const int64_t d = dx * dx + dy * dy;
        if (best_d < 0 || d < best_d || (d == best_d && (p.y < best.y || (p.y == best.y && p.x < best.x)))) {
          best = p;
          best_d = d;
        }
        for (int k = 0; k < 4; ++k) {
          const int nx = p.x + kDirs[k][0], ny = p.y + kDirs[k][1];
          if (nx < x0 || ny < y0 || nx >= x1 || ny >= y1) continue;
          uint16_t& l = h.label[idx(nx, ny)];
          if (l || !nav_.fits(grid, size, nx, ny) || nav_.gate(nx, ny) != g0) continue;
          l = next;
          queue.push_back({nx, ny});
        }
      }
      reps.push_back({best, g0});
      ++next;
    }
  }
}

void Pathfinder::rebuild_graph(int grid, int size) {
  Hier& h = hier(grid, size);
  const size_t nc = h.reps.size();
  h.offset.assign(nc, 0);
  uint32_t total = 0;
  for (size_t c = 0; c < nc; ++c) {
    h.offset[c] = total;
    total += static_cast<uint32_t>(h.reps[c].size());
  }
  h.regions.assign(total, {});
  for (size_t c = 0; c < nc; ++c) {
    for (size_t i = 0; i < h.reps[c].size(); ++i) {
      h.regions[h.offset[c] + i].rep = h.reps[c][i].rep;
      h.regions[h.offset[c] + i].gate = h.reps[c][i].gate;
    }
  }
  // élek a klaszterhatárokon (4-szomszédság)
  std::vector<std::pair<uint32_t, uint32_t>> edges;
  const int W = nav_.width(), H = nav_.height();
  // a határ mentén az egymás utáni cellák többnyire ugyanazt a régiópárt adják: csak a változáskor kerül be
  uint32_t last_a = 0, last_b = 0;
  auto link = [&](uint32_t a, uint32_t b) {
    if (!a || !b || (a == last_a && b == last_b)) return;
    last_a = a;
    last_b = b;
    edges.emplace_back(a - 1, b - 1);
    edges.emplace_back(b - 1, a - 1);
  };
  for (int x = cs_; x < W; x += cs_) {
    last_a = last_b = 0;
    for (int y = 0; y < H; ++y) link(region_index(h, x - 1, y), region_index(h, x, y));
  }
  for (int y = cs_; y < H; y += cs_) {
    last_a = last_b = 0;
    for (int x = 0; x < W; ++x) link(region_index(h, x, y - 1), region_index(h, x, y));
  }
  // a kapurégiók a klaszteren belül is határosak más régiókkal: a kapucellák szomszédjai
  for (const auto& [gr, mask] : nav_.gates()) {
    for (int y = gr.y * kNavPerTile; y < (gr.y + gr.h) * kNavPerTile; ++y) {
      for (int x = gr.x * kNavPerTile; x < (gr.x + gr.w) * kNavPerTile; ++x) {
        const uint32_t a = region_index(h, x, y);
        if (!a) continue;
        for (int k = 0; k < 4; ++k) {
          const int nx = x + kDirs[k][0], ny = y + kDirs[k][1];
          if (!nav_.in_bounds(nx, ny)) continue;
          const uint32_t b = region_index(h, nx, ny);
          if (b && b != a) { edges.emplace_back(a - 1, b - 1); edges.emplace_back(b - 1, a - 1); }
        }
      }
    }
  }
  std::sort(edges.begin(), edges.end());
  edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
  h.adj_start.assign(total + 1, 0);
  for (const auto& e : edges) ++h.adj_start[e.first + 1];
  for (uint32_t i = 0; i < total; ++i) h.adj_start[i + 1] += h.adj_start[i];
  h.adj.resize(edges.size());
  for (size_t i = 0; i < edges.size(); ++i) h.adj[i] = edges[i].second;   // rendezett, így a kezdőpontok szerint sorban
  // összefüggő komponensek a régiók sorrendjében: [0] minden kapu nyitva, [1 + t] a t. csapat (a nem jogosult
  // kapurégió kimarad, komponense 0)
  std::vector<uint32_t> stack;
  for (int view = 0; view < 9; ++view) {
    const int team = view - 1;
    const auto vi = static_cast<size_t>(view);
    uint32_t comp = 0;
    for (uint32_t r = 0; r < total; ++r) {
      if (h.regions[r].comp[vi] || !gate_ok(h.regions[r].gate, team)) continue;
      ++comp;
      h.regions[r].comp[vi] = comp;
      stack.assign(1, r);
      while (!stack.empty()) {
        const uint32_t v = stack.back();
        stack.pop_back();
        for (uint32_t k = h.adj_start[v]; k < h.adj_start[v + 1]; ++k) {
          const uint32_t w = h.adj[k];
          if (h.regions[w].comp[vi] || !gate_ok(h.regions[w].gate, team)) continue;
          h.regions[w].comp[vi] = comp;
          stack.push_back(w);
        }
      }
    }
  }
}

void Pathfinder::update_area(TileRect r) {
  const int cap = nav_.config().clearance_cap;
  const int x0 = std::max(0, r.x * kNavPerTile - cap - 1), y0 = std::max(0, r.y * kNavPerTile - cap - 1);
  const int x1 = std::min(nav_.width() - 1, (r.x + r.w) * kNavPerTile + cap);
  const int y1 = std::min(nav_.height() - 1, (r.y + r.h) * kNavPerTile + cap);
  if (x1 < x0 || y1 < y0) return;
  for (int g = 0; g < nav_.classes(); ++g) {
    for (int s = 0; s < sizes_; ++s) {
      for (int cy = y0 / cs_; cy <= y1 / cs_; ++cy) {
        for (int cx = x0 / cs_; cx <= x1 / cs_; ++cx) label_cluster(g, s, cy * ccx_ + cx);
      }
      rebuild_graph(g, s);
    }
  }
}

void Pathfinder::add_obstacle(TileRect r) {
  nav_.add_obstacle(r);
  update_area(r);
}

void Pathfinder::remove_obstacle(TileRect r) {
  nav_.remove_obstacle(r);
  update_area(r);
}

void Pathfinder::terrain_changed(TileRect r) {
  nav_.terrain_changed(r);
  update_area({r.x - 1, r.y - 1, r.w + 2, r.h + 2});
  compute_max_speed();
}

uint32_t Pathfinder::component(int grid, int size, NavCell c, int team) const {
  if (grid < 0 || !nav_.in_bounds(c.x, c.y)) return 0;
  const Hier& h = hier(grid, size);
  const uint32_t r = region_index(h, c.x, c.y);
  return r ? h.regions[r - 1].comp[static_cast<size_t>(team < 0 ? 0 : team + 1)] : 0;
}

bool Pathfinder::reachable(int grid, int size, NavCell a, NavCell b, int team) const {
  if (grid < 0) return true;
  const uint32_t ca = component(grid, size, a, team);
  return ca != 0 && ca == component(grid, size, b, team);
}

bool Pathfinder::passable(int grid, int size, int x, int y, int team) const {
  return nav_.fits(grid, size, x, y) && gate_ok(nav_.gate(x, y), team);
}

void Pathfinder::set_gate(TileRect r, uint8_t mask) {
  nav_.set_gate(r, mask);
  update_area(r);
}

size_t Pathfinder::region_count(int grid, int size) const { return hier(grid, size).regions.size(); }

std::optional<NavCell> Pathfinder::nearest(int grid, int size, uint32_t comp, NavCell t, int team) const {
  const int W = nav_.width(), H = nav_.height();
  t.x = std::clamp(t.x, 0, W - 1);
  t.y = std::clamp(t.y, 0, H - 1);
  std::optional<NavCell> best;
  int64_t best_d = 0;
  auto consider = [&](int x, int y) {
    if (x < 0 || y < 0 || x >= W || y >= H || !passable(grid, size, x, y, team)) return;
    if (comp && component(grid, size, {x, y}, team) != comp) return;
    const int64_t dx = x - t.x, dy = y - t.y, d = dx * dx + dy * dy;
    if (!best || d < best_d || (d == best_d && (y < best->y || (y == best->y && x < best->x)))) {
      best = NavCell{x, y};
      best_d = d;
    }
  };
  const int maxd = std::max(W, H);
  for (int d = 0; d <= maxd; ++d) {
    if (best && int64_t{d} * d > best_d) break;   // a d. gyűrű minden pontja legalább d távol van
    if (d == 0) { consider(t.x, t.y); continue; }
    for (int x = t.x - d; x <= t.x + d; ++x) { consider(x, t.y - d); consider(x, t.y + d); }
    for (int y = t.y - d + 1; y <= t.y + d - 1; ++y) { consider(t.x - d, y); consider(t.x + d, y); }
  }
  return best;
}

int64_t Pathfinder::step_cost(int grid, int x, int y, bool diagonal) const {
  const int32_t sp = std::max<int32_t>(1, nav_.speed(grid, x, y).raw());
  return int64_t{diagonal ? kDiagonal : kStraight} * 65536 / sp;
}

int64_t Pathfinder::heuristic(int grid, NavCell a, NavCell b) const {
  return octile(a.x - b.x, a.y - b.y) * 65536 / max_speed_raw_[static_cast<size_t>(grid)];
}

std::vector<uint32_t> Pathfinder::region_route(const Hier& h, uint32_t from, uint32_t to, int team) const {
  const size_t n = h.regions.size();
  std::vector<int64_t> g(n, -1);
  std::vector<uint32_t> parent(n, UINT32_MAX);
  std::vector<uint8_t> closed(n, 0);
  const NavCell goal = h.regions[to].rep;
  auto hval = [&](uint32_t r) { const NavCell p = h.regions[r].rep; return octile(p.x - goal.x, p.y - goal.y); };
  MinHeap open;
  g[from] = 0;
  open.push({hval(from), from});
  while (!open.empty()) {
    const uint32_t v = static_cast<uint32_t>(open.top().second);
    open.pop();
    if (closed[v]) continue;
    closed[v] = 1;
    if (v == to) break;
    const NavCell pv = h.regions[v].rep;
    for (uint32_t k = h.adj_start[v]; k < h.adj_start[v + 1]; ++k) {
      const uint32_t w = h.adj[k];
      if (closed[w] || !gate_ok(h.regions[w].gate, team)) continue;
      const NavCell pw = h.regions[w].rep;
      const int64_t ng = g[v] + octile(pw.x - pv.x, pw.y - pv.y);
      if (g[w] < 0 || ng < g[w]) {
        g[w] = ng;
        parent[w] = v;
        open.push({ng + hval(w), w});
      }
    }
  }
  std::vector<uint32_t> route;
  if (g[to] < 0) return route;
  for (uint32_t v = to; v != UINT32_MAX; v = parent[v]) route.push_back(v);
  std::reverse(route.begin(), route.end());
  return route;
}

bool Pathfinder::fine_astar(int grid, int size, NavCell s, NavCell goal, const std::vector<uint8_t>* allowed,
                            const Hier* h, std::vector<NavCell>& out, int64_t& cost, size_t& expanded, int team) {
  if (++gen_ == 0) {   // körbefordult: a bélyegek törlése
    std::fill(stamp_.begin(), stamp_.end(), 0);
    gen_ = 1;
  }
  const int W = nav_.width();
  auto ok = [&](int x, int y) {
    if (!passable(grid, size, x, y, team)) return false;
    if (!allowed) return true;
    const uint32_t r = region_index(*h, x, y);
    return r && (*allowed)[r - 1];
  };
  MinHeap open;
  const size_t si = idx(s.x, s.y), gi = idx(goal.x, goal.y);
  stamp_[si] = gen_;
  g_[si] = 0;
  parent_[si] = -1;
  closed_[si] = 0;
  // kulcs: f × 2^24 − g – azonos f-nél a célhoz közelebbi (nagyobb g) előbb; így a nyílt rácson nem bont ki széles
  // sávot az egyenlő költségű cellákból (döntetlen esetén a navcella-index dönt: teljes, determinisztikus rendezés)
  auto key = [](int64_t f, int64_t gg) { return f * (int64_t{1} << 24) - std::min<int64_t>(gg, (int64_t{1} << 24) - 1); };
  open.push({key(heuristic(grid, s, goal), 0), static_cast<int64_t>(si)});
  bool found = false;
  while (!open.empty()) {
    const auto i = static_cast<size_t>(open.top().second);
    open.pop();
    if (closed_[i]) continue;
    closed_[i] = 1;
    ++expanded;
    if (i == gi) { found = true; break; }
    const int x = static_cast<int>(i % static_cast<size_t>(W)), y = static_cast<int>(i / static_cast<size_t>(W));
    for (const auto& d : kDirs) {
      const int nx = x + d[0], ny = y + d[1];
      if (!ok(nx, ny)) continue;
      const bool diag = d[0] != 0 && d[1] != 0;
      if (diag && (!ok(x + d[0], y) || !ok(x, y + d[1]))) continue;   // nincs sarokvágás
      const size_t j = idx(nx, ny);
      const int64_t ng = g_[i] + step_cost(grid, nx, ny, diag);
      if (stamp_[j] != gen_) {
        stamp_[j] = gen_;
        closed_[j] = 0;
      } else if (closed_[j] || ng >= g_[j]) {
        continue;
      }
      g_[j] = ng;
      parent_[j] = static_cast<int32_t>(i);
      open.push({key(ng + heuristic(grid, {nx, ny}, goal), ng), static_cast<int64_t>(j)});
    }
  }
  out.clear();
  if (!found) return false;
  cost = g_[gi];
  for (int64_t i = static_cast<int64_t>(gi); i != static_cast<int64_t>(si); i = parent_[static_cast<size_t>(i)]) {
    out.push_back({static_cast<int>(i % W), static_cast<int>(i / W)});
  }
  std::reverse(out.begin(), out.end());
  return true;
}

bool Pathfinder::line_of_sight(int grid, int size, NavCell a, NavCell b, int team) const {
  // „supercover” egyenes: minden navcella, amelyet a két középpont közötti szakasz érint; pontos sarokátmenetnél
  // mindkét szomszédnak szabadnak kell lennie (mint a finom keresésben)
  auto fits = [&](int x, int y) { return passable(grid, size, x, y, team); };
  const int dx = std::abs(b.x - a.x), dy = std::abs(b.y - a.y);
  const int sx = b.x > a.x ? 1 : -1, sy = b.y > a.y ? 1 : -1;
  int x = a.x, y = a.y;
  if (!fits(x, y)) return false;
  int ix = 0, iy = 0;
  while (ix < dx || iy < dy) {
    const int64_t decision = int64_t{1 + 2 * ix} * dy - int64_t{1 + 2 * iy} * dx;
    if (decision == 0) {
      if (!fits(x + sx, y) || !fits(x, y + sy)) return false;
      x += sx;
      y += sy;
      ++ix;
      ++iy;
    } else if (decision < 0) {
      x += sx;
      ++ix;
    } else {
      y += sy;
      ++iy;
    }
    if (!fits(x, y)) return false;
  }
  return true;
}

std::vector<NavCell> Pathfinder::smooth(int grid, int size, NavCell start, const std::vector<NavCell>& cells,
                                        int team) const {
  std::vector<NavCell> pts;
  pts.reserve(cells.size() + 1);
  pts.push_back(start);
  pts.insert(pts.end(), cells.begin(), cells.end());
  std::vector<NavCell> out;
  size_t anchor = 0;
  const size_t last = pts.size() - 1;
  while (anchor < last) {
    size_t j = std::min(last, anchor + kSmoothWindow);
    while (j > anchor + 1 && !line_of_sight(grid, size, pts[anchor], pts[j], team)) --j;
    out.push_back(pts[j]);
    anchor = j;
  }
  return out;
}

PathResult Pathfinder::search(int grid, int size, Fixed sx, Fixed sy, Fixed gx, Fixed gy, bool hierarchical,
                              int team) {
  PathResult res;
  if (grid < 0) {   // repülő: egyenesen
    res.found = res.goal_reachable = true;
    res.waypoints.emplace_back(gx, gy);
    return res;
  }
  const int W = nav_.width(), H = nav_.height();
  NavCell s = cell_of(sx, sy);
  s.x = std::clamp(s.x, 0, W - 1);
  s.y = std::clamp(s.y, 0, H - 1);
  if (!passable(grid, size, s.x, s.y, team)) {   // az egység akadályba szorult: a legközelebbi szabad helyről indul
    const auto ns = nearest(grid, size, 0, s, team);
    if (!ns) return res;
    s = *ns;
  }
  const uint32_t comp = component(grid, size, s, team);
  const NavCell raw_goal = cell_of(gx, gy);
  const bool inside = nav_.in_bounds(raw_goal.x, raw_goal.y);
  NavCell goal{std::clamp(raw_goal.x, 0, W - 1), std::clamp(raw_goal.y, 0, H - 1)};
  res.goal_reachable = inside && component(grid, size, goal, team) == comp;
  if (!res.goal_reachable) goal = *nearest(grid, size, comp, goal, team);   // legalább a kezdőpont megfelel
  res.goal_cell = goal;
  res.found = true;
  auto finish = [&](const std::vector<NavCell>& cells) {
    for (const NavCell& c : cells) res.waypoints.push_back(center_of(c));
    if (res.goal_reachable) {
      if (res.waypoints.empty()) res.waypoints.emplace_back(gx, gy);
      else res.waypoints.back() = {gx, gy};
    }
  };
  if (goal == s) {
    finish({});
    if (!res.goal_reachable) res.waypoints.push_back(center_of(goal));
    return res;
  }
  std::vector<NavCell> cells;
  bool ok = false;
  if (hierarchical) {
    const Hier& h = hier(grid, size);
    const uint32_t rs = region_index(h, s.x, s.y) - 1, rg = region_index(h, goal.x, goal.y) - 1;
    const auto route = region_route(h, rs, rg, team);
    res.region_path = route.size();
    std::vector<uint8_t> allowed(h.regions.size(), 0);
    for (const uint32_t r : route) allowed[r] = 1;   // előbb csak a folyosó
    ok = fine_astar(grid, size, s, goal, &allowed, &h, cells, res.cost, res.expanded, team);
    if (!ok) {   // ritkán a folyosó túl szűk (sarkok): a szomszédos régiókkal bővítve
      for (const uint32_t r : route) {
        for (uint32_t k = h.adj_start[r]; k < h.adj_start[r + 1]; ++k) allowed[h.adj[k]] = 1;
      }
      ok = fine_astar(grid, size, s, goal, &allowed, &h, cells, res.cost, res.expanded, team);
    }
  }
  if (!ok) ok = fine_astar(grid, size, s, goal, nullptr, nullptr, cells, res.cost, res.expanded, team);
  if (!ok) {   // nem fordulhat elő (azonos komponens), de biztonságból
    res.found = false;
    return res;
  }
  finish(smooth(grid, size, s, cells, team));
  return res;
}

PathResult Pathfinder::find_path(int grid, int size, Fixed sx, Fixed sy, Fixed gx, Fixed gy, int team) {
  return search(grid, size, sx, sy, gx, gy, true, team);
}

PathResult Pathfinder::find_path_flat(int grid, int size, Fixed sx, Fixed sy, Fixed gx, Fixed gy, int team) {
  return search(grid, size, sx, sy, gx, gy, false, team);
}

}  // namespace totu::map
