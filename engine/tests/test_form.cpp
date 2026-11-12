// SPDX-License-Identifier: GPL-3.0-or-later
// Az alakzatok és a csoportos mozgás tesztjei (M4, 4. lépés): helyek kiszámítása, adat a game.json-ban, NAV-FORM
// (WC3-szerű alakzat szűkületen át), sor alakzat, NAV-BRIDGE (két 50 fős sereg szemből egy hídon).
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "check.hpp"
#include "content/loader.hpp"
#include "map/pathfinder.hpp"
#include "sim/sim.hpp"

using namespace totu;
using core::Fixed;

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

std::string read(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

map::Map load_map(const char* name) {
  std::vector<std::string> errors;
  auto doc = content::parse(read(std::filesystem::path(TOTU_TEST_DATA) / "maps" / name));
  auto m = doc ? map::Map::from_json(*doc, terrain(), errors) : std::nullopt;
  return m ? std::move(*m) : map::Map(1, 1, 0);
}

Fixed F(int num, int den = 1) { return Fixed::from_ratio(num, den); }
double D(Fixed f) { return f.raw() / 65536.0; }

struct Arena {
  map::Map map;
  map::NavGrid nav;
  map::Pathfinder pf;
  sim::Sim sim;
  Arena(const char* name, uint64_t seed) : map(load_map(name)), nav(map, terrain()), pf(nav), sim(seed) {
    sim.attach_navigation(&pf);
  }
  sim::EntityId unit(Fixed x, Fixed y, uint8_t owner, Fixed speed, Fixed range, Fixed radius = F(2, 5)) {
    sim::Command c;
    c.owner = owner;
    c.x = x;
    c.y = y;
    c.speed = speed;
    c.damage = 0;
    c.spread = 0;
    c.range = range;
    c.hp = 100;
    const sim::EntityId e = sim.spawn(c);
    sim.set_body(e, radius, *terrain().movement_class("land"));
    return e;
  }
  const sim::Position& pos(sim::EntityId e) { return *sim.world().get<sim::Position>(e); }
  bool moving(sim::EntityId e) { return sim.world().get<sim::Movement>(e)->moving; }
};

bool no_hard_overlap(Arena& w, const std::vector<sim::EntityId>& ids) {
  for (size_t i = 0; i < ids.size(); ++i) {
    for (size_t j = i + 1; j < ids.size(); ++j) {
      const auto* a = w.sim.world().get<sim::Body>(ids[i]);
      const auto* b = w.sim.world().get<sim::Body>(ids[j]);
      if (!a || !b) continue;
      const Fixed sum = a->radius * sim::NavParams{}.hard_ratio + b->radius * sim::NavParams{}.hard_ratio;
      const int64_t d2 = core::length_sq_raw(w.pos(ids[i]).x - w.pos(ids[j]).x, w.pos(ids[i]).y - w.pos(ids[j]).y);
      if (d2 + 4 < int64_t{sum.raw()} * sum.raw()) return false;
    }
  }
  return true;
}

}  // namespace

TEST_CASE("alakzat: helyek – sor, doboz, WC3-szerű, szétszórt, szárnyas; adat a game.json-ban") {
  const sim::FormationSet def;
  const Fixed sp = F(1);
  auto line = sim::formation_slots(sim::Formation::Line, 10, def[sim::Formation::Line], sp);
  CHECK(line.size() == 10 && line.back().row == 0);
  Fixed sum;
  for (const auto& s : line) sum = sum + s.lateral;
  CHECK(sum.raw() == 0);                                         // középre igazítva
  CHECK(line.back().lateral - line.front().lateral == F(9));
  auto box = sim::formation_slots(sim::Formation::Box, 9, def[sim::Formation::Box], sp);
  CHECK(box.back().row == 2 && box.back().depth == F(2));
  auto aut = sim::formation_slots(sim::Formation::Auto, 8, def[sim::Formation::Auto], sp);
  CHECK(aut.back().row == 1);                                    // széles: 4 × 2
  auto spread = sim::formation_slots(sim::Formation::Spread, 4, def[sim::Formation::Spread], sp);
  CHECK(spread[1].lateral - spread[0].lateral == F(2));
  auto flank = sim::formation_slots(sim::Formation::Flank, 10, def[sim::Formation::Flank], sp);
  Fixed left_max = F(-100), right_min = F(100);
  for (const auto& s : flank) {
    if (s.lateral.raw() < 0) left_max = std::max(left_max, s.lateral);
    else right_min = std::min(right_min, s.lateral);
  }
  CHECK(right_min - left_max >= F(4));                           // rés a két szárny között
  auto big = sim::formation_slots(sim::Formation::Line, 3, def[sim::Formation::Line], F(2));
  CHECK(big[1].lateral - big[0].lateral == F(2));                // nagy egységhez nő a távolság
  // adat
  content::Vfs vfs;
  vfs.add_layer("totu", std::filesystem::path(TOTU_CONTENT_DIR) / "base");
  const auto game = content::parse(*vfs.read_text("game.json"));
  std::vector<std::string> errors;
  const auto set = sim::FormationSet::from_json(game->find("formations"), errors);
  CHECK(errors.empty());
  CHECK(set[sim::Formation::Spread].spacing == F(2) && !set[sim::Formation::None].together);
  CHECK(set[sim::Formation::Flank].gap == F(4));
}

TEST_CASE("alakzat: NAV-FORM – WC3-szerű alakzat a résen át, elöl a közelharcosok, közös sebesség és út") {
  Arena w("NAV-FORM.json", 21);
  std::vector<sim::EntityId> melee, ranged, all;
  for (int i = 0; i < 12; ++i) {
    melee.push_back(w.unit(F(10 + (i % 6) * 3, 1), F(3 + (i / 6) * 3, 1), 1, F(10, 100), F(1)));
    ranged.push_back(w.unit(F(12 + (i % 6) * 3, 1), F(9 + (i / 6) * 3, 1), 1, F(8, 100), F(5)));
  }
  all = melee;
  all.insert(all.end(), ranged.begin(), ranged.end());
  w.sim.order_group_move(all, F(25), F(32), sim::Formation::Auto);
  CHECK(w.sim.world().get<sim::Path>(melee[0])->cap == F(8, 100));   // a leglassabb tag sebessége
  CHECK(w.sim.nav_stats().shared_total >= 12);
  bool overlap_ok = true;
  int last = -1;
  for (int t = 0; t < 1500; ++t) {
    w.sim.step();
    overlap_ok &= no_hard_overlap(w, all);
    bool any = false;
    for (const auto e : all) any |= w.moving(e);
    if (any) last = t;
  }
  CHECK(overlap_ok);
  CHECK(last < 1499);   // mindenki megérkezett
  // elöl (dél felé, nagyobb y) a közelharcosok
  Fixed my, ry;
  for (const auto e : melee) my = my + w.pos(e).y;
  for (const auto e : ranged) ry = ry + w.pos(e).y;
  CHECK(my > ry);
  int near = 0;
  for (const auto e : all) near += core::length(w.pos(e).x - F(25), w.pos(e).y - F(32)) <= F(7);
  CHECK(near == 24);    // összeállt az alakzat a cél körül
  std::printf("  NAV-FORM: 24 egység a %d. tickre áll össze; közelharcos átlag y %.2f, távolsági %.2f; %lld útkeresés, "
              "%lld közös út\n", last, D(my) / 12, D(ry) / 12, static_cast<long long>(w.sim.nav_stats().paths_total),
              static_cast<long long>(w.sim.nav_stats().shared_total));
}

TEST_CASE("alakzat: sor alakzat nyílt terepen – egy sorban állnak meg") {
  Arena w("NAV-FORM.json", 22);
  std::vector<sim::EntityId> ids;
  for (int i = 0; i < 10; ++i) ids.push_back(w.unit(F(5 + (i % 5) * 2), F(25 + (i / 5) * 2), 1, F(1, 10), F(1)));
  w.sim.order_group_move(ids, F(30), F(30), sim::Formation::Line);
  for (int t = 0; t < 800; ++t) w.sim.step();
  Fixed min_x = F(100), max_x = F(-100), min_y = F(100), max_y = F(-100);
  for (const auto e : ids) {
    CHECK(!w.moving(e));
    min_x = std::min(min_x, w.pos(e).x); max_x = std::max(max_x, w.pos(e).x);
    min_y = std::min(min_y, w.pos(e).y); max_y = std::max(max_y, w.pos(e).y);
  }
  // kelet felé haladtak: a sor észak–déli irányú, kb. 9 csempe hosszú, keskeny
  CHECK(max_y - min_y >= F(7) && max_x - min_x <= F(2));
}

TEST_CASE("alakzat: NAV-BRIDGE – két 50 fős (szövetséges) sereg szemből egy hídon, átfedés és beragadás nélkül") {
  Arena w("NAV-BRIDGE.json", 23);
  std::vector<sim::EntityId> north, south, all;
  for (int i = 0; i < 50; ++i) {
    north.push_back(w.unit(F(20 + (i % 10) * 2, 1), F(3 + (i / 10) * 2, 1), 1, F(9, 100), F(1)));
    south.push_back(w.unit(F(21 + (i % 10) * 2, 1), F(28 + (i / 10) * 2, 1), 1, F(9, 100), F(1)));
  }
  all = north;
  all.insert(all.end(), south.begin(), south.end());
  w.sim.order_group_move(north, F(30), F(32), sim::Formation::Auto);
  w.sim.order_group_move(south, F(30), F(8), sim::Formation::Auto);
  bool overlap_ok = true;
  int last = -1;
  double worst = 0, total = 0;
  for (int t = 0; t < 4000; ++t) {
    const auto a = std::chrono::steady_clock::now();
    w.sim.step();
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count();
    worst = std::max(worst, ms);
    total += ms;
    if (t % 10 == 0) overlap_ok &= no_hard_overlap(w, all);
    bool any = false;
    for (const auto e : all) any |= w.moving(e);
    if (any) last = t;
  }
  CHECK(overlap_ok);
  int crossed = 0;
  for (const auto e : north) crossed += w.pos(e).y > F(23);
  for (const auto e : south) crossed += w.pos(e).y < F(17);
  std::printf("  NAV-BRIDGE: %d / 100 átkelt, az utolsó mozgás a %d. tickben; tick átlag %.3f ms, legrosszabb %.2f ms; "
              "%lld útkeresés, %lld közös út\n", crossed, last, total / 4000, worst,
              static_cast<long long>(w.sim.nav_stats().paths_total), static_cast<long long>(w.sim.nav_stats().shared_total));
  CHECK(crossed == 100);
  CHECK(last < 3999);
}
