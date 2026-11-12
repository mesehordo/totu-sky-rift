// SPDX-License-Identifier: GPL-3.0-or-later
// Az útkereső tesztjei (M4, 2. lépés): NAV-DIAG, NAV-UNREACH, NAV-HIGH tesztpályák, a hierarchikus elérhetőség
// egyezése a teljes bejárással (véletlen akadálysorozatban is), indulás akadályból, repülő, rögzített út-hash.
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "check.hpp"
#include "content/loader.hpp"
#include "core/hash.hpp"
#include "core/rng.hpp"
#include "map/pathfinder.hpp"

using namespace totu;
using map::Fixed;
using map::NavCell;

namespace {

const map::TerrainTable& terrain() {
  static map::TerrainTable t = [] {
    content::Vfs vfs;
    vfs.add_layer("totu", std::filesystem::path(TOTU_CONTENT_DIR) / "base");
    content::ContentSet set;
    content::ContentLoader(vfs, std::filesystem::path(TOTU_CONTENT_DIR) / "schema").load(set);
    map::TerrainTable tt;
    std::vector<std::string> errors;
    tt.load(set, errors);
    return tt;
  }();
  return t;
}

map::Map load_map(const char* name) {
  std::ifstream in(std::filesystem::path(TOTU_TEST_DATA) / "maps" / name, std::ios::binary);
  const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  std::vector<std::string> errors;
  auto doc = content::parse(text);
  auto m = doc ? map::Map::from_json(*doc, terrain(), errors) : std::nullopt;
  for (const auto& e : errors) std::printf("  pályahiba (%s): %s\n", name, e.c_str());
  return m ? std::move(*m) : map::Map(1, 1, 0);
}

Fixed T(int whole, int tenths = 0) { return Fixed::from_ratio(whole * 10 + tenths, 10); }

int grid(const map::NavGrid& nav, const char* cls) { return nav.grid_of(*terrain().movement_class(cls)); }

// az út érvényes: a kezdőponttól minden szakasz látóvonalas (nincs átjárás akadályon vagy szirten)
bool path_valid(const map::Pathfinder& pf, int g, int size, Fixed sx, Fixed sy, const map::PathResult& r) {
  NavCell prev = map::Pathfinder::cell_of(sx, sy);
  for (const auto& [x, y] : r.waypoints) {
    const NavCell c = map::Pathfinder::cell_of(x, y);
    if (!pf.line_of_sight(g, size, prev, c)) return false;
    prev = c;
  }
  return !r.waypoints.empty();
}

}  // namespace

TEST_CASE("útkereső: NAV-DIAG – átlós cikcakk mindkét irányban, közel optimális, sarokvágás nélkül") {
  const map::Map m = load_map("NAV-DIAG.json");
  map::NavGrid nav(m, terrain());
  map::Pathfinder pf(nav);
  const int g = grid(nav, "land");
  for (int dir = 0; dir < 2; ++dir) {
    const Fixed sx = dir ? T(45, 5) : T(2, 5), sy = sx;
    const Fixed gx = dir ? T(2, 5) : T(45, 5), gy = gx;
    const auto t0 = std::chrono::steady_clock::now();
    const auto h = pf.find_path(g, 0, sx, sy, gx, gy);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    const auto f = pf.find_path_flat(g, 0, sx, sy, gx, gy);
    CHECK(h.found && h.goal_reachable && f.found);
    CHECK(path_valid(pf, g, 0, sx, sy, h));
    CHECK(h.waypoints.back() == std::make_pair(gx, gy));
    CHECK(f.cost <= h.cost && h.cost * 100 <= f.cost * 115);   // legfeljebb 15%-kal hosszabb az optimálisnál
    CHECK(h.waypoints.size() >= 4);                              // a három résen át cikcakkozik
    std::printf("  NAV-DIAG %s: %zu fordulópont, költség %lld (optimális %lld), kibontva %zu (teljes rácson %zu), %.2f ms\n",
                dir ? "vissza" : "oda", h.waypoints.size(), static_cast<long long>(h.cost),
                static_cast<long long>(f.cost), h.expanded, f.expanded, ms);
  }
}

TEST_CASE("útkereső: NAV-UNREACH – elérhetetlen cél helyett azonnal a legközelebbi pont") {
  const map::Map m = load_map("NAV-UNREACH.json");
  map::NavGrid nav(m, terrain());
  map::Pathfinder pf(nav);
  const int land = grid(nav, "land"), amph = grid(nav, "amphibious");
  const NavCell start = map::Pathfinder::cell_of(T(3), T(3)), island = map::Pathfinder::cell_of(T(24), T(24));
  CHECK(!pf.reachable(land, 0, start, island));
  CHECK(pf.reachable(amph, 0, start, island));
  const auto r = pf.find_path(land, 0, T(3), T(3), T(24), T(24));
  CHECK(r.found && !r.goal_reachable);
  CHECK(path_valid(pf, land, 0, T(3), T(3), r));
  // a legközelebbi pont a tó partján van (a tó sugara 14 csempe)
  const int64_t dx = r.goal_cell.x - island.x, dy = r.goal_cell.y - island.y, d2 = dx * dx + dy * dy;
  CHECK(d2 >= 26 * 26 && d2 <= 31 * 31);
  CHECK(pf.reachable(land, 0, start, r.goal_cell));
  std::printf("  NAV-UNREACH: legközelebbi pont (%d, %d), kibontva %zu navcella\n", r.goal_cell.x, r.goal_cell.y,
              r.expanded);
  const auto a = pf.find_path(amph, 0, T(3), T(3), T(24), T(24));
  CHECK(a.found && a.goal_reachable && path_valid(pf, amph, 0, T(3), T(3), a));
  // cél egy sziklatömb közepén a parton: a tömb széle
  const auto rock = pf.find_path(land, 0, T(3), T(3), T(7), T(39));
  CHECK(rock.found && !rock.goal_reachable);
  const NavCell rc = map::Pathfinder::cell_of(T(7), T(39));
  CHECK(std::abs(rock.goal_cell.x - rc.x) <= 7 && std::abs(rock.goal_cell.y - rc.y) <= 7);
  CHECK(nav.fits(land, 0, rock.goal_cell.x, rock.goal_cell.y));
}

TEST_CASE("útkereső: NAV-HIGH – rámpán fel a magaslatra, a szirt és a rámpa nélküli mélység zárt") {
  map::Map m = load_map("NAV-HIGH.json");
  map::NavGrid nav(m, terrain());
  map::Pathfinder pf(nav);
  const int land = grid(nav, "land");
  const NavCell start = map::Pathfinder::cell_of(T(3), T(3));
  const NavCell plateau = map::Pathfinder::cell_of(T(40), T(12)), pit = map::Pathfinder::cell_of(T(8), T(35));
  CHECK(pf.reachable(land, 0, start, plateau));
  CHECK(pf.reachable(land, 2, start, plateau));   // a 3 csempés rámpán a nagy egység is felfér
  CHECK(!pf.reachable(land, 0, start, pit));
  const auto r = pf.find_path(land, 0, T(3), T(3), T(40), T(12));
  CHECK(r.found && r.goal_reachable && path_valid(pf, land, 0, T(3), T(3), r));
  bool via_ramp = false;   // az út a rámpa csempéin (x = 26, y = 11–13) megy át
  NavCell prev = start;
  for (const auto& [x, y] : r.waypoints) {
    const NavCell c = map::Pathfinder::cell_of(x, y);
    via_ramp |= (prev.x <= 52 && c.x >= 54) || (c.x >= 52 && c.x <= 53 && c.y >= 22 && c.y <= 27);
    prev = c;
  }
  CHECK(via_ramp);
  // a rámpa helyére fű: a magaslat elérhetetlen; vissza: újra elérhető
  const uint16_t grass = *terrain().terrain("grass"), ramp = *terrain().terrain("ramp");
  for (int y = 11; y < 14; ++y) m.set_terrain(26, y, grass);
  m.compute_cliffs(terrain());
  pf.terrain_changed({26, 11, 1, 3});
  CHECK(!pf.reachable(land, 0, start, plateau));
  const auto blocked = pf.find_path(land, 0, T(3), T(3), T(40), T(12));
  CHECK(blocked.found && !blocked.goal_reachable);
  for (int y = 11; y < 14; ++y) m.set_terrain(26, y, ramp);
  m.compute_cliffs(terrain());
  pf.terrain_changed({26, 11, 1, 3});
  CHECK(pf.reachable(land, 0, start, plateau));
}

TEST_CASE("útkereső: a hierarchikus elérhetőség = teljes bejárás, akadályváltozások közben is") {
  const map::Map m(40, 40, *terrain().terrain("grass"));
  map::NavGrid nav(m, terrain());
  map::Pathfinder pf(nav);
  const int g = grid(nav, "land");
  core::Pcg32 rng(2026, 11);
  std::vector<map::TileRect> placed;
  bool same = true;
  int checks = 0;
  for (int step = 0; step < 40; ++step) {
    if (!placed.empty() && rng.chance(1, 4)) {
      const auto k = rng.below(static_cast<uint32_t>(placed.size()));
      pf.remove_obstacle(placed[k]);
      placed.erase(placed.begin() + k);
    } else {
      // hosszú falak is, hogy a pálya részekre essen
      const bool wall = rng.chance(1, 2);
      map::TileRect r{rng.range(0, 37), rng.range(0, 37), wall ? rng.range(6, 30) : rng.range(1, 4),
                      wall ? 1 : rng.range(1, 4)};
      if (wall && rng.chance(1, 2)) std::swap(r.w, r.h);
      pf.add_obstacle(r);
      placed.push_back(r);
    }
    for (int size = 0; size < 3; ++size) {
      // teljes bejárás (4-szomszédság) az első elférő navcellából
      std::vector<int> comp(static_cast<size_t>(nav.width() * nav.height()), 0);
      int id = 0;
      for (int y = 0; y < nav.height(); ++y) {
        for (int x = 0; x < nav.width(); ++x) {
          if (comp[static_cast<size_t>(y * nav.width() + x)] || !nav.fits(g, size, x, y)) continue;
          ++id;
          std::deque<NavCell> q{{x, y}};
          comp[static_cast<size_t>(y * nav.width() + x)] = id;
          while (!q.empty()) {
            const NavCell c = q.front();
            q.pop_front();
            const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& v : d) {
              const int nx = c.x + v[0], ny = c.y + v[1];
              if (!nav.fits(g, size, nx, ny)) continue;
              int& cc = comp[static_cast<size_t>(ny * nav.width() + nx)];
              if (!cc) { cc = id; q.push_back({nx, ny}); }
            }
          }
        }
      }
      for (int k = 0; k < 30; ++k) {
        const NavCell a{rng.range(0, 79), rng.range(0, 79)}, b{rng.range(0, 79), rng.range(0, 79)};
        const int ca = comp[static_cast<size_t>(a.y * nav.width() + a.x)];
        const bool truth = ca != 0 && ca == comp[static_cast<size_t>(b.y * nav.width() + b.x)];
        same &= pf.reachable(g, size, a, b) == truth;
        ++checks;
      }
    }
  }
  CHECK(same);
  CHECK(checks == 40 * 3 * 30);
  // a frissített hierarchia azonos egy nulláról felépítettel
  map::Pathfinder fresh(nav);
  bool eq = true;
  for (int size = 0; size < 3; ++size) {
    eq &= fresh.region_count(g, size) == pf.region_count(g, size);
    for (int y = 0; y < nav.height(); y += 3) {
      for (int x = 0; x < nav.width(); x += 3) eq &= fresh.component(g, size, {x, y}) == pf.component(g, size, {x, y});
    }
  }
  CHECK(eq);
}

TEST_CASE("útkereső: indulás akadályból, repülő, rögzített út-hash minden rendszeren") {
  const map::Map m = load_map("NAV-DIAG.json");
  map::NavGrid nav(m, terrain());
  map::Pathfinder pf(nav);
  const int g = grid(nav, "land");
  const auto r = pf.find_path(g, 0, T(30, 5), T(0, 5), T(2, 5), T(2, 5));   // a (30, 0) csempe szikla
  CHECK(r.found && r.goal_reachable);
  const auto fly = pf.find_path(-1, 0, T(1), T(1), T(40), T(7));
  CHECK(fly.found && fly.waypoints.size() == 1 && fly.waypoints[0] == std::make_pair(T(40), T(7)));
  // több út hash-e: ugyanazt kell adnia mind az öt rendszeren
  core::StateHash h;
  const int pts[][4] = {{2, 2, 45, 45}, {45, 45, 2, 2}, {2, 45, 45, 2}, {10, 3, 38, 44}, {44, 20, 3, 30}};
  for (int size = 0; size < 3; ++size) {
    for (const auto& p : pts) {
      const auto x = pf.find_path(g, size, T(p[0], 5), T(p[1], 5), T(p[2], 5), T(p[3], 5));
      h.add_u64(x.goal_reachable);
      h.add_i64(x.cost);
      for (const auto& [wx, wy] : x.waypoints) { h.add_i64(wx.raw()); h.add_i64(wy.raw()); }
    }
  }
  std::printf("  út-hash: 0x%016llx\n", static_cast<unsigned long long>(h.value()));
  CHECK(h.value() == 0x0f4f54236284369aull);
}

TEST_CASE("útkereső: mérés nagy (256 × 256) pályán") {
  map::Map m(256, 256, *terrain().terrain("grass"));
  core::Pcg32 rng(7, 3);
  const uint16_t rock = *terrain().terrain("rock"), water = *terrain().terrain("deep_water");
  for (int k = 0; k < 400; ++k) {   // sziklatömbök és tavak
    const int cx = rng.range(0, 255), cy = rng.range(0, 255), r = rng.range(1, 5);
    const uint16_t t = rng.chance(1, 3) ? water : rock;
    for (int y = cy - r; y <= cy + r; ++y) {
      for (int x = cx - r; x <= cx + r; ++x) {
        if (x >= 0 && y >= 0 && x < 256 && y < 256 && (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) m.set_terrain(x, y, t);
      }
    }
  }
  const auto t0 = std::chrono::steady_clock::now();
  map::NavGrid nav(m, terrain());
  const auto t1 = std::chrono::steady_clock::now();
  map::Pathfinder pf(nav);
  const auto t2 = std::chrono::steady_clock::now();
  const int g = grid(nav, "land");
  double worst = 0, total = 0;
  size_t found = 0;
  for (int k = 0; k < 20; ++k) {
    const Fixed sx = T(rng.range(0, 30)), sy = T(rng.range(0, 255)), gx = T(rng.range(225, 255)), gy = T(rng.range(0, 255));
    const auto a = std::chrono::steady_clock::now();
    const auto r = pf.find_path(g, 0, sx, sy, gx, gy);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count();
    worst = std::max(worst, ms);
    total += ms;
    found += r.found;
  }
  const auto b = std::chrono::steady_clock::now();
  pf.add_obstacle({128, 128, 4, 4});
  const double upd = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - b).count();
  auto ms = [](auto x, auto y) { return std::chrono::duration<double, std::milli>(y - x).count(); };
  std::printf("  256 × 256: navrács %.1f ms, hierarchia %.1f ms (%zu régió), 20 hosszú út átlag %.2f ms, "
              "legrosszabb %.2f ms, épület lerakása %.2f ms\n",
              ms(t0, t1), ms(t1, t2), pf.region_count(g, 0), total / 20, worst, upd);
  CHECK(found == 20);
}

TEST_CASE("útkereső: kapuk – csapatonkénti elérhetőség = teljes bejárás, véletlen kapukkal és falakkal") {
  const map::Map m(40, 40, *terrain().terrain("grass"));
  map::NavGrid nav(m, terrain());
  map::Pathfinder pf(nav);
  const int g = grid(nav, "land");
  core::Pcg32 rng(77, 5);
  bool same = true;
  for (int step = 0; step < 25; ++step) {
    if (rng.chance(1, 2)) {
      map::TileRect r{rng.range(0, 37), rng.range(0, 37), rng.range(6, 30), 1};
      if (rng.chance(1, 2)) std::swap(r.w, r.h);
      pf.add_obstacle(r);
    } else {
      pf.set_gate({rng.range(0, 37), rng.range(0, 37), rng.range(1, 3), rng.range(1, 3)},
                  static_cast<uint8_t>(rng.range(0, 255)));
    }
    for (int team = -1; team < 8; team += 3) {
      // teljes bejárás a csapat szemével (4-szomszédság), a kis méretosztállyal
      std::vector<int> comp(static_cast<size_t>(nav.width() * nav.height()), 0);
      int id = 0;
      for (int y = 0; y < nav.height(); ++y) {
        for (int x = 0; x < nav.width(); ++x) {
          if (comp[static_cast<size_t>(y * nav.width() + x)] || !pf.passable(g, 0, x, y, team)) continue;
          ++id;
          std::deque<NavCell> q{{x, y}};
          comp[static_cast<size_t>(y * nav.width() + x)] = id;
          while (!q.empty()) {
            const NavCell c = q.front();
            q.pop_front();
            const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& v : d) {
              const int nx = c.x + v[0], ny = c.y + v[1];
              if (!pf.passable(g, 0, nx, ny, team)) continue;
              int& cc = comp[static_cast<size_t>(ny * nav.width() + nx)];
              if (!cc) { cc = id; q.push_back({nx, ny}); }
            }
          }
        }
      }
      for (int k = 0; k < 40; ++k) {
        const NavCell a{rng.range(0, 79), rng.range(0, 79)}, b{rng.range(0, 79), rng.range(0, 79)};
        const int ca = comp[static_cast<size_t>(a.y * nav.width() + a.x)];
        const bool truth = ca != 0 && ca == comp[static_cast<size_t>(b.y * nav.width() + b.x)];
        same &= pf.reachable(g, 0, a, b, team) == truth;
      }
    }
  }
  CHECK(same);
}
