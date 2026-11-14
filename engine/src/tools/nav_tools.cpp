// SPDX-License-Identifier: GPL-3.0-or-later
#include "tools/nav_tools.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>

#include "content/loader.hpp"
#include "core/rng.hpp"
#include "map/pathfinder.hpp"

namespace totu::tools {

using core::Fixed;

bool load_terrain(const std::string& content_dir, map::TerrainTable& out) {
  content::Vfs vfs;
  vfs.add_layer("totu", std::filesystem::path(content_dir) / "base");
  content::ContentSet set;
  content::ContentLoader loader(vfs, std::filesystem::path(content_dir) / "schema");
  const auto rep = loader.load(set);
  std::vector<std::string> errors = rep.errors;
  if (!out.load(set, errors) || !errors.empty()) {
    for (const auto& e : errors) std::fprintf(stderr, "tartalomhiba: %s\n", e.c_str());
    return false;
  }
  return true;
}

bool load_match_content(const std::string& content_dir, map::TerrainTable& terrain, std::optional<sim::Rules>& rules) {
  content::Vfs vfs;
  vfs.add_layer("totu", std::filesystem::path(content_dir) / "base");
  content::ContentSet set;
  content::ContentLoader loader(vfs, std::filesystem::path(content_dir) / "schema");
  const auto rep = loader.load(set);
  std::vector<std::string> errors = rep.errors;
  terrain.load(set, errors);
  std::ifstream in(std::filesystem::path(content_dir) / "base" / "game.json", std::ios::binary);
  const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  const auto game = content::parse(text);
  if (game) rules = sim::Rules::load(*game, set, errors);
  if (!game || !rules || !errors.empty()) {
    for (const auto& e : errors) std::fprintf(stderr, "tartalomhiba: %s\n", e.c_str());
    return false;
  }
  return true;
}

std::vector<sim::Command> golden_m4_commands(uint64_t seed) {
  core::Pcg32 rng(seed, 98);
  std::vector<sim::Command> cmds;
  std::vector<sim::EntityId> army[2];
  sim::EntityId next = 1;
  for (int side = 0; side < 2; ++side) {
    for (int i = 0; i < 60; ++i) {
      sim::Command c;
      c.kind = sim::Command::Kind::Spawn;
      c.owner = static_cast<uint8_t>(side + 1);
      c.x = Fixed::from_ratio(1800 + (i % 12) * 200 + rng.range(-40, 40), 100);
      c.y = Fixed::from_ratio((side == 0 ? 300 : 2800) + (i / 12) * 180 + rng.range(-30, 30), 100);
      const int kind = i % 12 == 0 ? 2 : (i % 4 == 0 ? 1 : 0);   // 0 közelharcos, 1 távolsági, 2 nagy
      c.radius = Fixed::from_ratio(kind == 2 ? 90 : (kind == 1 ? 40 : 38), 100);
      c.speed = Fixed::from_ratio(kind == 2 ? 6 : (kind == 1 ? 8 : 9), 100);
      c.range = Fixed::from_ratio(kind == 1 ? 500 : 150, 100);
      c.hp = kind == 2 ? 400 : 120;
      c.damage = kind == 2 ? 14 : 7;
      c.spread = 4;
      c.cooldown = kind == 1 ? 40 : 30;
      cmds.push_back(c);
      army[side].push_back(next++);
    }
  }
  auto group = [&](int64_t t, int side, int x, int y, sim::Formation f) {
    sim::Command c;
    c.kind = sim::Command::Kind::GroupMove;
    c.tick = t;
    c.units = army[side];
    c.x = Fixed::from_int(x);
    c.y = Fixed::from_int(y);
    c.formation = static_cast<uint8_t>(f);
    cmds.push_back(c);
  };
  auto rect = [&](int64_t t, sim::Command::Kind k, int x, int y, int w, int h, uint8_t flag) {
    sim::Command c;
    c.kind = k;
    c.tick = t;
    c.rx = x; c.ry = y; c.rw = w; c.rh = h;
    c.flag = flag;
    cmds.push_back(c);
  };
  group(30, 0, 30, 32, sim::Formation::Auto);
  group(30, 1, 30, 8, sim::Formation::Line);
  rect(200, sim::Command::Kind::Gate, 28, 19, 4, 2, 0b10);    // kapu a hídon: csak a 2. játékosnak nyitott
  rect(600, sim::Command::Kind::Gate, 28, 19, 4, 2, 0b11);    // mindkettőnek
  rect(800, sim::Command::Kind::Block, 10, 28, 6, 3, 1);      // építés a déli mezőn
  group(1000, 0, 12, 6, sim::Formation::Flank);
  group(1000, 1, 45, 34, sim::Formation::Box);
  rect(1400, sim::Command::Kind::Block, 10, 28, 6, 3, 0);     // bontás
  group(1500, 0, 30, 30, sim::Formation::Spread);
  group(1500, 1, 30, 10, sim::Formation::Auto);
  std::stable_sort(cmds.begin(), cmds.end(), [](const sim::Command& a, const sim::Command& b) { return a.tick < b.tick; });
  return cmds;
}

BenchResult bench(const map::TerrainTable& terrain, int players, int units, int size, int ticks, uint64_t seed) {
  core::Pcg32 rng(seed, 97);
  map::Map m(size, size, *terrain.terrain("grass"));
  const uint16_t rock = *terrain.terrain("rock"), water = *terrain.terrain("deep_water");
  for (int k = 0; k < size * size / 160; ++k) {   // sziklatömbök és tavak (~10% a pályából)
    const int cx = rng.range(0, size - 1), cy = rng.range(0, size - 1), r = rng.range(1, 5);
    const uint16_t t = rng.chance(1, 3) ? water : rock;
    for (int y = cy - r; y <= cy + r; ++y) {
      for (int x = cx - r; x <= cx + r; ++x) {
        if (x >= 0 && y >= 0 && x < size && y < size && (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) m.set_terrain(x, y, t);
      }
    }
  }
  map::NavGrid nav(m, terrain);
  map::Pathfinder pf(nav);
  sim::Sim s(seed);
  s.attach_navigation(&pf);
  std::vector<std::vector<sim::EntityId>> army(static_cast<size_t>(players));
  const uint8_t land = terrain.movement_class("land").value_or(0);
  for (int p = 0; p < players; ++p) {
    // a játékosok a pálya széle mentén, körben
    const int bx = (p % 4) * (size / 4) + size / 8, by = (p / 4) * (size / 2) + size / 4;
    for (int i = 0; i < units; ++i) {
      sim::Command c;
      c.owner = static_cast<uint8_t>(p + 1);
      const int side = 15;
      c.x = Fixed::from_int(std::clamp(bx - side + (i % side) * 2, 1, size - 2));
      c.y = Fixed::from_int(std::clamp(by - side + (i / side) * 2, 1, size - 2));
      c.speed = Fixed::from_ratio(8, 100);
      c.damage = 0;
      c.spread = 0;
      c.range = Fixed::from_int(1);
      const sim::EntityId e = s.spawn(c);
      s.set_body(e, Fixed::from_ratio(i % 20 == 0 ? 80 : 40, 100), land);
      army[static_cast<size_t>(p)].push_back(e);
    }
  }
  std::vector<double> times;
  times.reserve(static_cast<size_t>(ticks));
  for (int t = 0; t < ticks; ++t) {
    if (t % 300 == 0) {   // 10 másodpercenként minden sereg új célt kap, változó alakzatban
      for (int p = 0; p < players; ++p) {
        const auto f = static_cast<sim::Formation>(1 + (t / 300 + p) % 5);
        s.order_group_move(army[static_cast<size_t>(p)], Fixed::from_int(rng.range(8, size - 8)),
                           Fixed::from_int(rng.range(8, size - 8)), f);
      }
    }
    const auto a = std::chrono::steady_clock::now();
    s.step();
    times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count());
  }
  BenchResult r;
  std::vector<double> sorted = times;
  std::sort(sorted.begin(), sorted.end());
  r.p50 = sorted[sorted.size() / 2];
  r.p99 = sorted[std::min(sorted.size() - 1, sorted.size() * 99 / 100)];
  r.max = sorted.back();
  for (const double x : times) r.avg += x;
  r.avg /= static_cast<double>(times.size());
  r.paths = s.nav_stats().paths_total;
  r.direct = s.nav_stats().direct_total;
  r.shared = s.nav_stats().shared_total;
  r.expanded = s.nav_stats().expanded_total;
  r.path_ms = s.nav_stats().path_ms;
  r.move_ms = s.nav_stats().move_ms;
  r.alive = s.world().entity_count();
  return r;
}

}  // namespace totu::tools
