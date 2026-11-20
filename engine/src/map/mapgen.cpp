// SPDX-License-Identifier: GPL-3.0-or-later
#include "map/mapgen.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <map>
#include <utility>
#include <vector>

namespace totu::map {

namespace {

constexpr uint8_t kGrass = 0, kDirt = 1, kWater = 2, kRock = 3;   // a régi GRASS, DIRT, WATER, ROCK
constexpr uint8_t kFree = 0, kBlockTerrain = 1, kBlockRes = 2;
constexpr double kPi = 3.14159265358979323846;

using P = std::pair<double, double>;
using C = std::pair<int, int>;

struct Rng {   // a régi `random.Random` szerepében (splitmix64)
  uint64_t s;
  uint64_t next() {
    s += 0x9E3779B97F4A7C15ull;
    uint64_t z = s;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }
  double random() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }
  double uniform(double a, double b) { return a + (b - a) * random(); }
  int64_t randrange(int64_t n) { return n <= 0 ? 0 : static_cast<int64_t>(next() % static_cast<uint64_t>(n)); }
  int64_t randrange(int64_t a, int64_t b) { return a + randrange(b - a); }
};

struct Noise {   // a régi `_ValueNoise` (simított értékzaj)
  int64_t seed;
  double h(int64_t ix, int64_t iy) const {
    uint64_t n = (static_cast<uint64_t>(ix) * 374761393ull + static_cast<uint64_t>(iy) * 668265263ull +
                  static_cast<uint64_t>(seed) * 2147483647ull) & 0xFFFFFFFFull;
    n = ((n ^ (n >> 13)) * 1274126177ull) & 0xFFFFFFFFull;
    return static_cast<double>(n & 0xFFFF) / 65535.0;
  }
  double at(double x, double y) const {
    const double fx0 = std::floor(x), fy0 = std::floor(y);
    const int64_t ix = static_cast<int64_t>(fx0), iy = static_cast<int64_t>(fy0);
    const double fx = x - fx0, fy = y - fy0;
    const double sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    const double a = h(ix, iy), b = h(ix + 1, iy), c = h(ix, iy + 1), d = h(ix + 1, iy + 1);
    return (a + (b - a) * sx) * (1 - sy) + (c + (d - c) * sx) * sy;
  }
};

double num(const content::Value* v, double def) {
  return v && v->is_number() ? std::strtod(v->text().c_str(), nullptr) : def;
}
bool truthy(const content::Value* v, bool def) {
  if (!v) return def;
  if (v->is_bool()) return v->as_bool();
  if (v->is_null()) return false;
  if (v->is_number()) return num(v, 0) != 0;
  return true;
}
double pymod(double a, double m) { return a - std::floor(a / m) * m; }
int ipy(double v) { return static_cast<int>(v); }   // a Python `int()` (nulla felé csonkít)
int fl(double v) { return static_cast<int>(std::floor(v)); }

struct Res {
  std::string kind;   // wood, stone, meteor (a régi nevek)
  int tx = 0, ty = 0, size = 1, amount = 0;
  bool alive = true;
};

struct Grid {
  int W = 0, H = 0;
  std::vector<uint8_t> terr, block;
  std::vector<int> rg;   // csempénként a lelőhely sorszáma (-1: nincs)
  std::vector<Res> res;
  bool inb(int x, int y) const { return x >= 0 && y >= 0 && x < W && y < H; }
  bool passable(int x, int y) const { return inb(x, y) && block[static_cast<size_t>(y * W + x)] == kFree; }
  bool add(Res r) {
    if (r.size <= 0) return false;
    for (int y = r.ty; y < r.ty + r.size; ++y) {
      for (int x = r.tx; x < r.tx + r.size; ++x) {
        if (!inb(x, y) || block[static_cast<size_t>(y * W + x)] != kFree || rg[static_cast<size_t>(y * W + x)] >= 0) return false;
      }
    }
    const int id = static_cast<int>(res.size());
    for (int y = r.ty; y < r.ty + r.size; ++y) {
      for (int x = r.tx; x < r.tx + r.size; ++x) {
        block[static_cast<size_t>(y * W + x)] = kBlockRes;
        rg[static_cast<size_t>(y * W + x)] = id;
      }
    }
    res.push_back(std::move(r));
    return true;
  }
  void remove(int id) {
    Res& r = res[static_cast<size_t>(id)];
    if (!r.alive) return;
    r.alive = false;
    for (int y = r.ty; y < r.ty + r.size; ++y) {
      for (int x = r.tx; x < r.tx + r.size; ++x) {
        const size_t i = static_cast<size_t>(y * W + x);
        if (rg[i] == id) {
          rg[i] = -1;
          if (block[i] == kBlockRes) block[i] = kFree;
        }
      }
    }
  }
  std::optional<C> nearest_free(int tx, int ty, int maxr) const {
    if (passable(tx, ty)) return C{tx, ty};
    for (int r = 1; r <= maxr; ++r) {
      for (int x = tx - r; x <= tx + r; ++x) {
        for (const int y : {ty - r, ty + r}) {
          if (passable(x, y)) return C{x, y};
        }
      }
      for (int y = ty - r + 1; y < ty + r; ++y) {
        for (const int x : {tx - r, tx + r}) {
          if (passable(x, y)) return C{x, y};
        }
      }
    }
    return std::nullopt;
  }
  bool reachable(C a, C b) const {
    if (!inb(a.first, a.second)) return false;
    std::vector<uint8_t> seen(static_cast<size_t>(W * H), 0);
    std::deque<C> q{a};
    seen[static_cast<size_t>(a.second * W + a.first)] = 1;
    while (!q.empty()) {
      const C c = q.front();
      q.pop_front();
      if (c == b) return true;
      static const int kD[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
      for (const auto& d : kD) {
        const int nx = c.first + d[0], ny = c.second + d[1];
        if (passable(nx, ny) && !seen[static_cast<size_t>(ny * W + nx)]) {
          seen[static_cast<size_t>(ny * W + nx)] = 1;
          q.push_back({nx, ny});
        }
      }
    }
    return false;
  }
};

}  // namespace

std::optional<content::Value> generate_map(const content::Value& doc, uint64_t seed, std::string* error) {
  auto fail = [&](const char* m) -> std::optional<content::Value> {
    if (error) *error = m;
    return std::nullopt;
  };
  const content::Value* gen = doc.find("generator");
  if (!gen || !gen->is_object()) return fail("a pályán nincs generátor-leírás");
  const content::Value* chars = gen->find("chars");
  if (!chars || !chars->is_string() || chars->text().size() != 4) return fail("hibás terepjelek");
  const int players = static_cast<int>(num(doc.find("players"), 2));
  const int W = std::clamp(static_cast<int>(num(gen->find("size"), 96)), 32, 512), H = W;
  const std::string layout = gen->find("layout") && gen->find("layout")->is_string() ? gen->find("layout")->text() : "ring";
  static const content::Value kEmpty = content::Value::object();
  const content::Value& feats = gen->find("features") && gen->find("features")->is_object() ? *gen->find("features") : kEmpty;
  auto feat = [&](const char* k) { return feats.find(k); };
  auto amount = [&](const char* k, int def) {
    const content::Value* a = gen->find("amounts");
    return a ? static_cast<int>(num(a->find(k), def)) : def;
  };
  Rng rng{seed ^ 0x6D61706765ull};
  (void)rng.randrange(1 << 20);   // a régi `tsalt` (a fafajták keverése; itt nem kell, de a sorrend a régié)
  Grid m;
  m.W = W;
  m.H = H;
  m.terr.assign(static_cast<size_t>(W * H), kGrass);
  m.block.assign(static_cast<size_t>(W * H), kFree);
  m.rg.assign(static_cast<size_t>(W * H), -1);
  const double cx = (W - 1) / 2.0, cy = (H - 1) / 2.0;
  const int kfold = layout == "corners" ? 4 : std::max(2, players);
  auto sym = [&](double x, double y) {   // a régi `_Sym.apply`
    std::vector<P> out;
    if (layout == "rows") return std::vector<P>{{x, y}, {W - 1 - x, H - 1 - y}};
    const double dx = x - cx, dy = y - cy;
    for (int i = 0; i < kfold; ++i) {
      const double a = 2 * kPi * i / kfold, ca = std::cos(a), sa = std::sin(a);
      out.emplace_back(cx + dx * ca - dy * sa, cy + dx * sa + dy * ca);
    }
    return out;
  };
  auto fold = [&](double x, double y) -> P {   // a régi `_fold`
    if (layout == "rows") return y < (2 * cy + 1) / 2 ? P{x, y} : P{2 * cx - x, 2 * cy - y};
    const double a0 = kfold == 2 ? kPi : 0.0, S = 2 * kPi / kfold;
    const double r = std::hypot(x - cx, y - cy);
    double a = pymod(pymod(std::atan2(y - cy, x - cx) - a0, 2 * kPi), S);
    a = std::min(a, S - a);
    return {cx + r * std::cos(a), cy + r * std::sin(a)};
  };
  // ---- kezdőhelyek
  std::vector<C> starts;
  std::vector<int> sides;
  if (layout == "rows") {
    const int k = std::max(1, players / 2);
    for (int i = 0; i < k; ++i) starts.emplace_back(ipy(W * (i + 1) / static_cast<double>(k + 1)), 12);
    for (int i = 0; i < k; ++i) starts.emplace_back(W - 1 - starts[static_cast<size_t>(i)].first, H - 1 - 12);
    sides.assign(static_cast<size_t>(k), 0);
    sides.resize(static_cast<size_t>(2 * k), 1);
  } else if (layout == "corners") {
    for (const auto& [x, y] : sym(12, 12)) starts.emplace_back(ipy(x), ipy(y));
  } else {
    const double r0 = std::min(W, H) * num(feat("start_radius"), 0.38);
    const double a0 = num(feat("start_angle"), 225) * kPi / 180;
    for (const auto& [x, y] : sym(cx + std::cos(a0) * r0, cy + std::sin(a0) * r0)) {
      starts.emplace_back(static_cast<int>(std::nearbyint(x)), static_cast<int>(std::nearbyint(y)));
    }
  }
  auto near_start = [&](double x, double y, double d) {
    for (const auto& [sx, sy] : starts) {
      if (std::hypot(x - sx, y - sy) < d) return true;
    }
    return false;
  };
  auto paint = [&](double x, double y, uint8_t t) {
    for (const auto& [px, py] : sym(x, y)) {
      const int ix = fl(px + 0.5), iy = fl(py + 0.5);
      if (m.inb(ix, iy)) m.terr[static_cast<size_t>(iy * W + ix)] = t;
    }
  };
  auto blob = [&](double bx, double by, double r, uint8_t t, double keep) {
    for (int y = ipy(by - r - 1); y < ipy(by + r + 2); ++y) {
      for (int x = ipy(bx - r - 1); x < ipy(bx + r + 2); ++x) {
        if (std::hypot(x - bx, y - by) <= r + rng.uniform(-0.7, 0.7) && !(keep > 0 && near_start(x, y, keep))) paint(x, y, t);
      }
    }
  };
  struct Ford { double x, y, w; };
  std::vector<Ford> fords_at;
  auto line = [&](double x0, double y0, double x1, double y1, uint8_t t, double width, const std::vector<double>& gaps) {
    const int steps = std::max(1, ipy(std::hypot(x1 - x0, y1 - y0) * 2));
    for (const double g : gaps) fords_at.push_back({x0 + (x1 - x0) * g, y0 + (y1 - y0) * g, width});
    for (int i = 0; i <= steps; ++i) {
      const double f = static_cast<double>(i) / steps;
      bool gap = false;
      for (const double g : gaps) gap = gap || std::fabs(f - g) < 0.06;
      if (gap) continue;
      const double x = x0 + (x1 - x0) * f, y = y0 + (y1 - y0) * f;
      for (int yy = ipy(y - width); yy <= ipy(y + width); ++yy) {
        for (int xx = ipy(x - width); xx <= ipy(x + width); ++xx) {
          if (std::hypot(xx - x, yy - y) <= width) paint(xx, yy, t);
        }
      }
    }
  };
  auto in_domain = [&]() -> P {
    if (layout == "rows") return {rng.uniform(2, W - 3), rng.uniform(2, H / 2.0)};
    const double a = rng.uniform(0, 2 * kPi / kfold) + (kfold == 2 ? kPi : 0);
    const double r = rng.uniform(4, std::min(W, H) * 0.5);
    return {cx + std::cos(a) * r, cy + std::sin(a) * r};
  };
  // ---- domborzat
  const Noise dnoise{rng.randrange(int64_t{1} << 30)};
  const double dthr = 0.78 - static_cast<int>(num(feat("dirt"), 14)) * 0.006;
  for (int y = 0; y < H; ++y) {   // nagy, organikus földfoltok
    for (int x = 0; x < W; ++x) {
      const auto [fx, fy] = fold(x, y);
      const double v = dnoise.at(fx / 9.0, fy / 9.0) * 0.75 + dnoise.at(fx / 3.5 + 17, fy / 3.5) * 0.25;
      if (v > dthr) m.terr[static_cast<size_t>(y * W + x)] = kDirt;
    }
  }
  auto arr3 = [&](const char* k, double a, double b, double c) {
    const content::Value* v = feat(k);
    if (!v || !v->is_array() || v->items().size() < 3) return std::array<double, 3>{a, b, c};
    return std::array<double, 3>{num(&v->items()[0], a), num(&v->items()[1], b), num(&v->items()[2], c)};
  };
  const auto lakes = arr3("lakes", 2, 2.0, 4.5);
  for (int i = 0; i < static_cast<int>(lakes[0]); ++i) {
    const auto [x, y] = in_domain();
    if (!near_start(x, y, 16)) blob(x, y, rng.uniform(lakes[1], lakes[2]), kWater, 13);
  }
  const auto ridges = arr3("ridges", 4, 5, 11);
  for (int i = 0; i < static_cast<int>(ridges[0]); ++i) {
    auto [x, y] = in_domain();
    double ang = rng.uniform(0, 2 * kPi);
    const int len = static_cast<int>(static_cast<double>(rng.randrange(static_cast<int64_t>(ridges[1]), static_cast<int64_t>(ridges[2]) + 1)) * W / 96.0);
    for (int k = 0; k < len; ++k) {
      if (!near_start(x, y, 13)) {
        paint(x, y, kRock);
        if (rng.random() < 0.5) paint(x + 1, y, kRock);
      }
      ang += rng.uniform(-0.5, 0.5);
      x += std::cos(ang);
      y += std::sin(ang);
    }
  }
  std::vector<const content::Value*> rivers;
  if (const content::Value* rv = feat("river")) {
    if (rv->is_object()) rivers.push_back(rv);
    else if (rv->is_array()) for (const auto& r : rv->items()) rivers.push_back(&r);
  }
  for (const content::Value* river : rivers) {
    const content::Value* tv = river->find("terrain");
    const uint8_t tt = tv && tv->is_string() && tv->text() == "rock" ? kRock : kWater;
    const int nf = static_cast<int>(num(river->find("fords"), 2));
    std::vector<double> fords;
    for (int i = 0; i < nf; ++i) fords.push_back((i + 0.5) / nf);
    const double wd = num(river->find("width"), 1.6);
    if (layout == "rows") {   // vízszintes folyó a két csapat között
      std::vector<double> g;
      for (const double f : fords) {
        if (f < 0.5) g.push_back(f * 2);
      }
      line(0, cy, W / 2.0, cy + rng.uniform(-2, 2), tt, wd, g);
    } else {   // sugárirányú folyók a szektorok határán
      const int k = layout == "ring" ? std::max(2, players) : 4;
      const double a = 2 * kPi / k * 0.5 + num(feat("start_angle"), 225) * kPi / 180 + num(river->find("turn"), 0);
      const double r0 = num(river->find("inner"), 8);
      line(cx + std::cos(a) * r0, cy + std::sin(a) * r0, cx + std::cos(a) * W * 0.75, cy + std::sin(a) * H * 0.75, tt, wd, fords);
    }
  }
  for (const Ford& f : fords_at) {   // a gázlók minden szimmetrikus másolatban nyitva
    for (const auto& [px, py] : sym(f.x, f.y)) {
      const double rr = f.w + 1.6;
      for (int yy = ipy(py - rr - 1); yy < ipy(py + rr + 2); ++yy) {
        for (int xx = ipy(px - rr - 1); xx < ipy(px + rr + 2); ++xx) {
          if (!m.inb(xx, yy) || std::hypot(xx - px, yy - py) > rr) continue;
          uint8_t& t = m.terr[static_cast<size_t>(yy * W + xx)];
          if (t == kWater || t == kRock) t = kDirt;
        }
      }
    }
  }
  if (const content::Value* ring = feat("ring_wall"); ring && ring->is_object()) {
    const double rr = std::min(W, H) * num(ring->find("radius"), 0.2);
    const int gates = static_cast<int>(num(ring->find("gates"), std::max(2, players)));
    const int steps = ipy(2 * kPi * rr * 2);
    for (int i = 0; i < steps; ++i) {
      const double a = 2 * kPi * i / steps;
      bool gate = false;
      for (int g = 0; g < gates; ++g) gate = gate || std::fabs(pymod((a - (2 * kPi * g / gates + 0.4)) + kPi, 2 * kPi) - kPi) < 0.17;
      if (gate) continue;
      const int ix = fl(cx + std::cos(a) * rr + 0.5), iy = fl(cy + std::sin(a) * rr + 0.5);
      for (int yy = iy - 1; yy <= iy + 1; ++yy) {
        for (int xx = ix - 1; xx <= ix + 1; ++xx) {
          if (m.inb(xx, yy)) m.terr[static_cast<size_t>(yy * W + xx)] = kRock;
        }
      }
    }
  }
  const content::Value* crater = feat("crater");
  if (crater && !crater->is_object()) crater = nullptr;
  if (crater) {   // középső meteorkráter: poros teknő
    const double cr = std::min(W, H) * num(crater->find("radius"), 0.1);
    for (int y = ipy(cy - cr - 1); y < ipy(cy + cr + 2); ++y) {
      for (int x = ipy(cx - cr - 1); x < ipy(cx + cr + 2); ++x) {
        if (m.inb(x, y) && std::hypot(x - cx, y - cy) <= cr + rng.uniform(-0.8, 0.8) && m.terr[static_cast<size_t>(y * W + x)] != kRock) {
          m.terr[static_cast<size_t>(y * W + x)] = kDirt;
        }
      }
    }
  }
  for (const auto& [sx, sy] : starts) {   // a kezdőhelyek körül tiszta talaj
    for (int y = sy - 7; y < sy + 8; ++y) {
      for (int x = sx - 7; x < sx + 8; ++x) {
        if (m.inb(x, y) && (m.terr[static_cast<size_t>(y * W + x)] == kWater || m.terr[static_cast<size_t>(y * W + x)] == kRock)) {
          m.terr[static_cast<size_t>(y * W + x)] = kGrass;
        }
      }
    }
  }
  for (int i = 0; i < W * H; ++i) m.block[static_cast<size_t>(i)] = m.terr[static_cast<size_t>(i)] == kWater || m.terr[static_cast<size_t>(i)] == kRock ? kBlockTerrain : kFree;
  // ---- nyersanyagok
  auto put = [&](const char* kind, double x, double y) {
    const int size = 2;
    const int tx = fl(x + size / 2.0), ty = fl(y + size / 2.0);
    for (int yy = ty - 1; yy < ty + size + 1; ++yy) {
      for (int xx = tx - 1; xx < tx + size + 1; ++xx) {
        if (!m.inb(xx, yy)) continue;
        const size_t i = static_cast<size_t>(yy * W + xx);
        if (m.terr[i] == kWater || m.terr[i] == kRock) {
          m.terr[i] = kGrass;
          m.block[i] = kFree;
        }
      }
    }
    Res r;
    r.kind = kind;
    r.tx = tx;
    r.ty = ty;
    r.size = size;
    r.amount = std::string(kind) == "stone" ? amount("gold", 1500) : amount("meteor", 1500);
    m.add(r);
    (void)rng.randrange(4);   // a régi változat-sorsolás
  };
  for (const auto& [sx, sy] : starts) {   // bázisonként ugyanaz az elrendezés, a középpont felé tájolva
    double ux = cx - sx, uy = cy - sy;
    const double d = std::hypot(ux, uy) > 0 ? std::hypot(ux, uy) : 1;
    ux /= d;
    uy /= d;
    const double vx = -uy, vy = ux;
    put("stone", sx + ux * 6 + vx * 7 - 1, sy + uy * 6 + vy * 7 - 1);
    put("stone", sx + ux * 6 - vx * 7 - 1, sy + uy * 6 - vy * 7 - 1);
    put("meteor", sx - ux * 7 - 1, sy - uy * 7 - 1);
    if (truthy(feat("expansions"), true)) {   // terjeszkedési hely félúton
      const double ex = sx + ux * d * 0.45 + vx * 6, ey = sy + uy * d * 0.45 + vy * 6;
      put("stone", ex - 1, ey - 1);
      put("meteor", ex + vx * 4 - 1, ey + vy * 4 - 1);
    }
  }
  struct Spot { double x, y; int lvl; };
  auto camp_spots = [&]() {   // a régi `camp_spots`
    std::vector<Spot> spots;
    for (const auto& [sx, sy] : starts) {
      const double dx = cx - sx, dy = cy - sy, d = std::hypot(dx, dy) > 0 ? std::hypot(dx, dy) : 1;
      const double ux = dx / d, uy = dy / d;
      spots.push_back({sx + ux * 15 - uy * 7, sy + uy * 15 + ux * 7, 1});
      spots.push_back({sx + ux * 17 + uy * 9, sy + uy * 17 - ux * 9, 2});
    }
    const size_t n = starts.size();
    std::vector<std::pair<size_t, size_t>> pairs;
    std::vector<size_t> order(n);
    for (size_t k = 0; k < n; ++k) order[k] = k;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
      return std::atan2(starts[a].second - cy, starts[a].first - cx) < std::atan2(starts[b].second - cy, starts[b].first - cx);
    });
    if (n == 2) pairs = {{0, 1}};
    else for (size_t k = 0; k < n; ++k) pairs.emplace_back(order[k], order[(k + 1) % n]);
    for (const auto& [i, j] : pairs) {
      const double mx = (starts[i].first + starts[j].first) / 2.0, my = (starts[i].second + starts[j].second) / 2.0;
      const double dx = starts[j].first - starts[i].first, dy = starts[j].second - starts[i].second;
      const double d = std::hypot(dx, dy) > 0 ? std::hypot(dx, dy) : 1;
      const double px = -dy / d, py = dx / d;
      spots.push_back({mx + px * 14, my + py * 14, 3});
      spots.push_back({mx - px * 14, my - py * 14, n == 2 ? 4 : 3});
    }
    spots.push_back({cx, cy, 5});
    return spots;
  };
  if (const content::Value* rich = feat("rich"); truthy(rich, false)) {   // gazdag, de kitett bővítés a táboroknál
    std::vector<Spot> sel;
    for (const Spot& s : camp_spots()) {
      if (s.lvl == 3 || s.lvl == 4) sel.push_back(s);
    }
    if (starts.size() > 2) {   // páronként a középtől távolabbi
      std::vector<Spot> far;
      for (size_t k = 0; k + 1 < sel.size(); k += 2) {
        far.push_back(std::hypot(sel[k].x - cx, sel[k].y - cy) >= std::hypot(sel[k + 1].x - cx, sel[k + 1].y - cy) ? sel[k] : sel[k + 1]);
      }
      sel = far;
    }
    const bool only_meteor = rich->is_string() && rich->text() == "meteor";
    for (const Spot& s : sel) {
      const double ox = s.x - cx, oy = s.y - cy, dd = std::hypot(ox, oy) > 0 ? std::hypot(ox, oy) : 1;
      const double vx = -oy / dd, vy = ox / dd;
      put("meteor", s.x + vx * 4 - 1, s.y + vy * 4 - 1);
      put("meteor", s.x - vx * 4 - 1, s.y - vy * 4 - 1);
      if (!only_meteor) put("stone", s.x + ox / dd * 4 - 1, s.y + oy / dd * 4 - 1);
    }
  }
  if (crater) {
    const int cnt = static_cast<int>(num(crater->find("meteors"), 4));
    const double cr = std::min(W, H) * num(crater->find("radius"), 0.1) * 0.55;
    for (int i = 0; i < cnt; ++i) {
      const double a = 2 * kPi * i / cnt + 0.3;
      put("meteor", cx + std::cos(a) * cr - 1, cy + std::sin(a) * cr - 1);
    }
  } else {
    put("meteor", cx - 1, cy - 1);
  }
  // ---- erdők: összefüggő erdőtömbök zajból, a pálya szélén zajos erdőfal, a bázisok körül tisztás
  const int wood = amount("wood", 120);
  auto tree = [&](int x, int y) {
    Res r;
    r.kind = "wood";
    r.tx = x;
    r.ty = y;
    r.size = 1;
    r.amount = wood;
    m.add(r);
  };
  const double cover = num(feat("forest_cover"), std::min(0.42, 0.16 + static_cast<int>(num(feat("forests"), 14)) * 0.008));
  const Noise noise{rng.randrange(int64_t{1} << 30)};
  const double thr = 0.95 - cover * 1.4;
  const bool border = truthy(feat("forest_border"), true);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      if (near_start(x, y, 10)) continue;
      const auto [fx, fy] = fold(x, y);
      double v = noise.at(fx / 11.0, fy / 11.0) * 0.7 + noise.at(fx / 4.5 + 40, fy / 4.5 + 40) * 0.3;
      const int edge = std::min({x, y, W - 1 - x, H - 1 - y});
      if (edge < 3 + noise.at(x / 6.0 + 90, y / 6.0) * 4 && border) v = 1.0;
      const size_t i = static_cast<size_t>(y * W + x);
      if (v > thr && (m.terr[i] == kGrass || m.terr[i] == kDirt) && m.block[i] == kFree) tree(x, y);
    }
  }
  for (const auto& [sx, sy] : starts) {   // minden bázis mellett erdő
    double ux = cx - sx, uy = cy - sy;
    const double d = std::hypot(ux, uy) > 0 ? std::hypot(ux, uy) : 1;
    ux /= d;
    uy /= d;
    const double bx = sx - uy * 9 - ux * 3, by = sy + ux * 9 - uy * 3;
    for (int yy = fl(by - 3.5); yy < fl(by + 4.5); ++yy) {
      for (int xx = fl(bx - 3.5); xx < fl(bx + 4.5); ++xx) {
        if (!m.inb(xx, yy)) continue;
        const size_t i = static_cast<size_t>(yy * W + xx);
        if (std::hypot(xx - bx, yy - by) <= 3.2 && m.block[i] == kFree && !near_start(xx, yy, 7) &&
            (m.terr[i] == kGrass || m.terr[i] == kDirt)) {
          tree(xx, yy);
        }
      }
    }
  }
  // ---- összeköttetés: minden bázisból vezessen út a középre (ha bármelyik el van zárva, mind ugyanazt kapja)
  auto carve = [&](C s, double off) {
    const int steps = ipy(std::hypot(cx - s.first, cy - s.second)) + 1;
    const double ux = (cx - s.first) / steps, uy = (cy - s.second) / steps;
    for (int i = 0; i <= steps; ++i) {
      const double k = std::sin(kPi * i / steps) * off;
      const double x = s.first + ux * i - uy * k, y = s.second + uy * i + ux * k;
      const int ix = fl(x + 0.5), iy = fl(y + 0.5);
      for (int yy = iy - 1; yy < iy + 2; ++yy) {
        for (int xx = ix - 1; xx < ix + 2; ++xx) {
          if (!m.inb(xx, yy)) continue;
          const size_t idx = static_cast<size_t>(yy * W + xx);
          if (m.rg[idx] >= 0 && m.res[static_cast<size_t>(m.rg[idx])].kind == "wood") m.remove(m.rg[idx]);
          if (m.terr[idx] == kWater || m.terr[idx] == kRock) {
            m.terr[idx] = kDirt;
            m.block[idx] = kFree;
          }
        }
      }
    }
  };
  C centre = m.nearest_free(ipy(cx), ipy(cy), 20).value_or(C{ipy(cx), ipy(cy)});
  auto linked = [&]() {
    for (const C& s : starts) {
      if (!m.reachable(s, centre)) return false;
    }
    for (size_t i = 1; i < starts.size(); ++i) {
      if (!m.reachable(starts[0], starts[i])) return false;
    }
    return true;
  };
  for (const double off : {0.0, 3.0, -3.0, 6.0}) {
    if (linked()) break;
    for (const C& s : starts) carve(s, off);
    for (int yy = fl(cy - 5); yy < fl(cy + 6); ++yy) {   // tisztás a középen
      for (int xx = fl(cx - 5); xx < fl(cx + 6); ++xx) {
        if (!m.inb(xx, yy) || std::hypot(xx - cx, yy - cy) > 4.6) continue;
        const size_t idx = static_cast<size_t>(yy * W + xx);
        if (m.rg[idx] >= 0 && m.res[static_cast<size_t>(m.rg[idx])].kind == "wood") m.remove(m.rg[idx]);
        if (m.terr[idx] == kWater || m.terr[idx] == kRock) {
          m.terr[idx] = kDirt;
          m.block[idx] = kFree;
        }
      }
    }
    centre = m.nearest_free(ipy(cx), ipy(cy), 20).value_or(centre);
  }
  // ---- díszletek és háttérszereplők (a régi `_decorate`): minden kezdőhelynél / párnál ugyanúgy
  auto place_rel = [&](const content::Value& spec) {   // a régi `_place_rel`
    const content::Value* atv = spec.find("at");
    const std::string at = atv && atv->is_string() ? atv->text() : "base";
    const double d = num(spec.find("d"), 0), s = num(spec.find("s"), 0);
    std::vector<P> out;
    const size_t n = starts.size();
    if (at == "centre") {
      const double a0 = n ? std::atan2(starts[0].second - cy, starts[0].first - cx) : 0.0;
      for (size_t k = 0; k < ((d != 0 || s != 0) ? n : 1); ++k) {
        const double a = a0 + 2 * kPi * static_cast<double>(k) / static_cast<double>(n);
        const double ux = std::cos(a), uy = std::sin(a);
        out.emplace_back(cx + ux * d - uy * s, cy + uy * d + ux * s);
      }
      return out;
    }
    if (at == "pair") {
      std::vector<size_t> order(n);
      for (size_t k = 0; k < n; ++k) order[k] = k;
      std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return std::atan2(starts[a].second - cy, starts[a].first - cx) < std::atan2(starts[b].second - cy, starts[b].first - cx);
      });
      std::vector<std::pair<size_t, size_t>> prs;
      if (n == 2) prs = {{order[0], order[1]}};
      else for (size_t k = 0; k < n; ++k) prs.emplace_back(order[k], order[(k + 1) % n]);
      for (const auto& [i, j] : prs) {
        const double mx = (starts[i].first + starts[j].first) / 2.0, my = (starts[i].second + starts[j].second) / 2.0;
        double ox = mx - cx, oy = my - cy, dd = std::hypot(ox, oy);
        if (dd < 1) {   // két játékos: a felező merőlegesen, mindkét irányba
          ox = -(starts[j].second - starts[i].second);
          oy = starts[j].first - starts[i].first;
          dd = std::hypot(ox, oy) > 0 ? std::hypot(ox, oy) : 1;
        }
        const double ux = ox / dd, uy = oy / dd;
        out.emplace_back(mx + ux * d - uy * s, my + uy * d + ux * s);
        if (n == 2) out.emplace_back(mx - ux * d + uy * s, my - uy * d - ux * s);
      }
      return out;
    }
    for (const auto& [sx, sy] : starts) {
      double ux = cx - sx, uy = cy - sy;
      const double dd = std::hypot(ux, uy) > 0 ? std::hypot(ux, uy) : 1;
      ux /= dd;
      uy /= dd;
      out.emplace_back(sx + ux * d - uy * s, sy + uy * d + ux * s);
    }
    return out;
  };
  content::Value decor_out = content::Value::array(), ambient_out = content::Value::array();
  centre = m.nearest_free(ipy(cx), ipy(cy), 20).value_or(C{ipy(cx), ipy(cy)});
  const content::Value* sizes = gen->find("decor_sizes");
  if (const content::Value* dl = gen->find("decor"); dl && dl->is_array()) {
    for (const content::Value& sp : dl->items()) {
      const content::Value* id = sp.find("id");
      const content::Value* sz = id && id->is_string() && sizes ? sizes->find(id->text()) : nullptr;
      if (!sz || !sz->is_array() || sz->items().size() != 2) continue;
      const int w = static_cast<int>(num(&sz->items()[0], 1)), h = static_cast<int>(num(&sz->items()[1], 1));
      struct Placed { std::vector<C> cells; std::vector<uint8_t> old; int tx, ty; };
      std::vector<Placed> placed;
      const auto spots = place_rel(sp);
      for (const auto& [x, y] : spots) {
        const int tx = static_cast<int>(std::nearbyint(x)) - w / 2, ty = static_cast<int>(std::nearbyint(y)) - h / 2;
        bool ok = true;
        for (int yy = ty - 1; yy < ty + h + 1 && ok; ++yy) {
          for (int xx = tx - 1; xx < tx + w + 1 && ok; ++xx) {
            if (!m.inb(xx, yy)) { ok = false; break; }
            const size_t i = static_cast<size_t>(yy * W + xx);
            if (m.terr[i] == kWater || m.terr[i] == kRock) ok = false;
            else if (m.rg[i] >= 0 && m.res[static_cast<size_t>(m.rg[i])].kind != "wood") ok = false;
          }
        }
        if (!ok || near_start(x, y, 9)) continue;
        Placed p{{}, {}, tx, ty};
        for (int yy = ty; yy < ty + h; ++yy) {
          for (int xx = tx; xx < tx + w; ++xx) {
            p.cells.emplace_back(xx, yy);
            p.old.push_back(m.block[static_cast<size_t>(yy * W + xx)]);
            m.block[static_cast<size_t>(yy * W + xx)] = kBlockTerrain;
          }
        }
        placed.push_back(std::move(p));
      }
      bool all_reach = true;
      for (const C& s : starts) all_reach = all_reach && m.reachable(s, centre);
      if (placed.size() < spots.size() || !all_reach) {   // nem fér el mindenhol, vagy utat zárna: sehova
        for (const Placed& p : placed) {
          for (size_t k = 0; k < p.cells.size(); ++k) m.block[static_cast<size_t>(p.cells[k].second * W + p.cells[k].first)] = p.old[k];
        }
        continue;
      }
      for (const Placed& p : placed) {
        for (const auto& [xx, yy] : p.cells) {
          const size_t i = static_cast<size_t>(yy * W + xx);
          if (m.rg[i] >= 0) m.remove(m.rg[i]);
          m.block[i] = kBlockTerrain;
        }
        content::Value d = content::Value::object();
        d.set("id", content::Value::string(id->text()));
        d.set("tx", content::Value::number(std::to_string(p.tx)));
        d.set("ty", content::Value::number(std::to_string(p.ty)));
        d.set("w", content::Value::number(std::to_string(w)));
        d.set("h", content::Value::number(std::to_string(h)));
        d.set("frame", content::Value::number(std::to_string(static_cast<int>(num(sp.find("frame"), 0)))));
        d.set("ruin", content::Value::boolean(truthy(sp.find("ruin"), false)));
        decor_out.push(std::move(d));
      }
    }
  }
  auto cell = [](C c) {
    content::Value v = content::Value::array();
    v.push(content::Value::number(std::to_string(c.first)));
    v.push(content::Value::number(std::to_string(c.second)));
    return v;
  };
  if (const content::Value* al = gen->find("ambient"); al && al->is_array()) {
    for (const content::Value& sp : al->items()) {
      const content::Value* id = sp.find("id");
      if (!id || !id->is_string()) continue;
      const auto homes = place_rel(sp);
      std::vector<std::vector<P>> paths;
      if (const content::Value* pl = sp.find("path"); pl && pl->is_array()) {
        for (const auto& q : pl->items()) paths.push_back(place_rel(q));
      }
      const content::Value* shv = sp.find("shelter");
      const std::vector<P> shel = shv && shv->is_object() ? place_rel(*shv) : std::vector<P>{};
      const content::Value* mode = sp.find("mode");
      for (size_t i = 0; i < homes.size(); ++i) {
        const auto t = m.nearest_free(ipy(homes[i].first), ipy(homes[i].second), 6);
        if (!t) continue;
        content::Value a = content::Value::object();
        a.set("id", content::Value::string(id->text()));
        a.set("n", content::Value::number(std::to_string(static_cast<int>(num(sp.find("n"), 1)))));
        a.set("at", cell(*t));
        a.set("idle", content::Value::boolean(mode && mode->is_string() && mode->text() == "idle"));
        content::Value pv = content::Value::array();
        for (const auto& pp : paths) {
          if (i >= pp.size()) continue;
          if (const auto q = m.nearest_free(ipy(pp[i].first), ipy(pp[i].second), 6)) pv.push(cell(*q));
        }
        a.set("path", std::move(pv));
        if (i < shel.size()) {
          if (const auto q = m.nearest_free(ipy(shel[i].first), ipy(shel[i].second), 6)) a.set("shelter", cell(*q));
        }
        ambient_out.push(std::move(a));
      }
    }
  }
  // ---- a kimenet: a régi pályaexport formája
  const std::string& ch = chars->text();
  content::Value rows = content::Value::array();
  std::vector<std::string> trees(static_cast<size_t>(H), std::string(static_cast<size_t>(W), '.'));
  content::Value lodes = content::Value::array();
  for (const Res& r : m.res) {
    if (!r.alive) continue;
    if (r.kind == "wood") {
      trees[static_cast<size_t>(r.ty)][static_cast<size_t>(r.tx)] = 'T';
      continue;
    }
    content::Value l = content::Value::object();
    l.set("kind", content::Value::string(r.kind == "stone" ? "gold" : r.kind));
    l.set("x", content::Value::number(std::to_string(r.tx)));
    l.set("y", content::Value::number(std::to_string(r.ty)));
    l.set("size", content::Value::number(std::to_string(r.size)));
    l.set("amount", content::Value::number(std::to_string(r.amount)));
    lodes.push(std::move(l));
  }
  for (int y = 0; y < H; ++y) {
    std::string row(static_cast<size_t>(W), ch[0]);
    for (int x = 0; x < W; ++x) row[static_cast<size_t>(x)] = ch[m.terr[static_cast<size_t>(y * W + x)]];
    rows.push(content::Value::string(std::move(row)));
  }
  content::Value out = content::Value::object();
  for (const auto& [k, v] : doc.members()) {
    if (k == "terrain" || k == "decor" || k == "ambient" || k == "objects" || k == "width" || k == "height" || k == "_forras") continue;
    out.set(k, v);
  }
  out.set("width", content::Value::number(std::to_string(W)));
  out.set("height", content::Value::number(std::to_string(H)));
  content::Value terrain = content::Value::object();
  terrain.set("rows", std::move(rows));
  out.set("terrain", std::move(terrain));
  if (!sides.empty()) {
    content::Value tv = content::Value::array();
    for (const int s : sides) tv.push(content::Value::number(std::to_string(s)));
    out.set("teams", std::move(tv));
  }
  if (!decor_out.items().empty()) out.set("decor", std::move(decor_out));
  if (!ambient_out.items().empty()) out.set("ambient", std::move(ambient_out));
  content::Value objects = content::Value::object();
  content::Value st = content::Value::array();
  for (const C& s : starts) st.push(cell(s));
  objects.set("starts", std::move(st));
  content::Value tr = content::Value::object();
  content::Value trows = content::Value::array();
  for (auto& t : trees) trows.push(content::Value::string(std::move(t)));
  tr.set("rows", std::move(trows));
  objects.set("trees", std::move(tr));
  objects.set("resources", std::move(lodes));
  out.set("objects", std::move(objects));
  return out;
}

}  // namespace totu::map
