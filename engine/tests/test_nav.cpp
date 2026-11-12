// SPDX-License-Identifier: GPL-3.0-or-later
// A navigációs mozgás tesztjei (M4, 3. lépés): NAV-MAMMOTH, NAV-CHOKE, tömeg szűk résen (kemény sugár soha nem
// lapolódik át), félretolás, útkérés-keret, determinizmus, mentés–betöltés–folytatás.
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

map::Map load_map(const char* name) {
  std::ifstream in(std::filesystem::path(TOTU_TEST_DATA) / "maps" / name, std::ios::binary);
  const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  std::vector<std::string> errors;
  auto doc = content::parse(text);
  auto m = doc ? map::Map::from_json(*doc, terrain(), errors) : std::nullopt;
  return m ? std::move(*m) : map::Map(1, 1, 0);
}

Fixed F(int num, int den = 1) { return Fixed::from_ratio(num, den); }

// egy pálya a navigációval: a sorrend számít (a pálya és a rács élettartama a szimulációé felett)
struct World {
  map::Map map;
  map::NavGrid nav;
  map::Pathfinder pf;
  sim::Sim sim;
  World(const char* name, uint64_t seed, sim::NavParams params = {})
      : map(load_map(name)), nav(map, terrain()), pf(nav), sim(seed) {
    sim.attach_navigation(&pf, params);
  }
  sim::EntityId unit(Fixed x, Fixed y, Fixed radius, uint8_t owner = 1, Fixed speed = F(1, 10)) {
    sim::Command c;
    c.owner = owner;
    c.x = x;
    c.y = y;
    c.speed = speed;
    c.damage = 0;
    c.spread = 0;
    c.range = F(1);
    const sim::EntityId e = sim.spawn(c);
    sim.set_body(e, radius, *terrain().movement_class("land"));
    return e;
  }
  const sim::Position& pos(sim::EntityId e) { return *sim.world().get<sim::Position>(e); }
  bool moving(sim::EntityId e) { return sim.world().get<sim::Movement>(e)->moving; }
  Fixed dist(sim::EntityId e, Fixed x, Fixed y) { return core::length(pos(e).x - x, pos(e).y - y); }
};

// a kemény sugarak soha nem lapolódnak át (a mozgás és a kitérés után sem)
bool no_hard_overlap(World& w, const std::vector<sim::EntityId>& ids) {
  for (size_t i = 0; i < ids.size(); ++i) {
    for (size_t j = i + 1; j < ids.size(); ++j) {
      const auto* a = w.sim.world().get<sim::Body>(ids[i]);
      const auto* b = w.sim.world().get<sim::Body>(ids[j]);
      const Fixed sum = a->radius * sim::NavParams{}.hard_ratio + b->radius * sim::NavParams{}.hard_ratio;   // ugyanúgy, mint a szimulációban
      const int64_t d2 = core::length_sq_raw(w.pos(ids[i]).x - w.pos(ids[j]).x, w.pos(ids[i]).y - w.pos(ids[j]).y);
      if (d2 + 4 < int64_t{sum.raw()} * sum.raw()) return false;
    }
  }
  return true;
}

}  // namespace

TEST_CASE("navigáció: NAV-MAMMOTH – a nagy egység csak a széles résen fér át, a kicsi a közelin") {
  World w("NAV-MAMMOTH.json", 1);
  const auto small = w.unit(F(6), F(5), F(38, 100)), medium = w.unit(F(9), F(5), F(60, 100));
  const auto large = w.unit(F(12), F(5), F(95, 100));
  for (const auto e : {small, medium, large}) w.sim.order_move(e, F(9), F(25));
  w.sim.step();
  CHECK(w.sim.world().get<sim::Path>(large)->pending == 0);   // azonnal megvan az út (nincs hosszú próbálkozás)
  bool large_wide = false, small_near = false;
  int large_arrived = -1;
  for (int t = 1; t < 1500; ++t) {
    w.sim.step();
    const auto& lp = w.pos(large);
    if (lp.y > F(14) && lp.y < F(16)) large_wide |= lp.x >= F(29);
    const auto& sp = w.pos(small);
    if (sp.y > F(14) && sp.y < F(16)) small_near |= sp.x <= F(11);
    if (large_arrived < 0 && !w.moving(large)) large_arrived = t;
  }
  CHECK(large_wide && small_near);
  for (const auto e : {small, medium, large}) {
    CHECK(!w.moving(e) && w.dist(e, F(9), F(25)) <= F(3));   // tömeg a célnál: 3 csempén belül
  }
  std::printf("  NAV-MAMMOTH: a nagy egység a %d. tickben ért célba a széles résen át\n", large_arrived);
}

TEST_CASE("navigáció: NAV-CHOKE – lezárt folyosó: azonnal kerül; mindkettő zárva: megáll, nem köröz") {
  for (int both = 0; both < 2; ++both) {
    World w("NAV-CHOKE.json", 3);
    std::vector<sim::EntityId> ids;
    for (int i = 0; i < 12; ++i) ids.push_back(w.unit(F(3 + (i % 4) * 12, 10), F(2 + (i / 4) * 12, 10), F(40, 100)));
    for (const auto e : ids) w.sim.order_move(e, F(10), F(26));
    bool overlap_ok = true, went_right = false;
    std::vector<sim::Position> before;
    for (int t = 0; t < 1600; ++t) {
      if (t == 40) {   // a bal folyosó lezárása (pl. egy épület), majd mindkettőé
        w.pf.add_obstacle({9, 14, 3, 2});
        if (both) w.pf.add_obstacle({28, 14, 3, 2});
        w.sim.navigation_changed();
      }
      w.sim.step();
      overlap_ok &= no_hard_overlap(w, ids);
      for (const auto e : ids) went_right |= w.pos(e).x >= F(28) && w.pos(e).y > F(8) && w.pos(e).y < F(22);
      if (t == 1399) {
        before.clear();
        for (const auto e : ids) before.push_back(w.pos(e));
      }
    }
    CHECK(overlap_ok);
    int stopped = 0, still = 0;
    for (size_t i = 0; i < ids.size(); ++i) {
      stopped += !w.moving(ids[i]);
      still += w.pos(ids[i]).x == before[i].x && w.pos(ids[i]).y == before[i].y;
    }
    CHECK(stopped == 12 && still == 12);   // az utolsó 200 tickben senki sem mozdult (nincs körözés)
    if (!both) {
      CHECK(went_right);
      int arrived = 0;
      for (const auto e : ids) {
        arrived += w.dist(e, F(10), F(26)) <= F(4);
      }
      CHECK(arrived == 12);
    } else {
      CHECK(!went_right);   // nincs út: nem indulnak el a hosszú, haszontalan kerülőre
      for (const auto e : ids) CHECK(w.pos(e).y < F(14));
    }
    std::printf("  NAV-CHOKE (%s): %lld útkeresés, %lld közvetlen út\n", both ? "mindkettő zárva" : "bal zárva",
                static_cast<long long>(w.sim.nav_stats().paths_total), static_cast<long long>(w.sim.nav_stats().direct_total));
  }
}

TEST_CASE("navigáció: 20 egység egy 2 csempés résen át – átfedés nélkül, mind átér") {
  World w("NAV-MAMMOTH.json", 5);
  std::vector<sim::EntityId> ids;
  for (int i = 0; i < 20; ++i) ids.push_back(w.unit(F(4 + (i % 5) * 11, 10), F(3 + (i / 5) * 11, 10), F(38, 100)));
  for (const auto e : ids) w.sim.order_move(e, F(8, 1) + F(1, 2), F(22));
  bool overlap_ok = true;
  for (int t = 0; t < 1200; ++t) {
    w.sim.step();
    overlap_ok &= no_hard_overlap(w, ids);
  }
  CHECK(overlap_ok);
  int through = 0, stopped = 0;
  for (const auto e : ids) {
    through += w.pos(e).y > F(16);
    stopped += !w.moving(e);
  }
  CHECK(through == 20 && stopped == 20);
}

TEST_CASE("navigáció: az álló saját egység félreáll, az idegen nem") {
  World w("NAV-MAMMOTH.json", 7);
  const auto idle = w.unit(F(20), F(8), F(40, 100));
  const auto mover = w.unit(F(20), F(4), F(40, 100));
  w.sim.order_move(mover, F(20), F(12));
  for (int t = 0; t < 200; ++t) w.sim.step();
  CHECK(!w.moving(mover) && w.dist(mover, F(20), F(12)) <= F(1, 10));
  CHECK(w.pos(idle).x != F(20));   // félreállt
  CHECK(!w.moving(idle));
  // idegen egységet nem tol félre: kikerüli
  World v("NAV-MAMMOTH.json", 7);
  const auto enemy = v.unit(F(20), F(8), F(40, 100), 2);
  const auto m2 = v.unit(F(20), F(4), F(40, 100), 1);
  v.sim.order_move(m2, F(20), F(12));
  for (int t = 0; t < 200; ++t) v.sim.step();
  CHECK(v.pos(enemy).x == F(20) && v.pos(enemy).y == F(8));
  CHECK(v.dist(m2, F(20), F(12)) <= F(1, 10));
}

TEST_CASE("navigáció: útkérés-keret, determinizmus, mentés–betöltés–folytatás") {
  sim::NavParams np;
  np.path_budget = 8;
  World a("NAV-CHOKE.json", 11, np), b("NAV-CHOKE.json", 11, np);
  std::vector<sim::EntityId> ids;
  for (int i = 0; i < 30; ++i) {
    const Fixed x = F(2 + (i % 10) * 3), y = F(2 + (i / 10));
    ids.push_back(a.unit(x, y, F(40, 100)));
    b.unit(x, y, F(40, 100));
  }
  for (const auto e : ids) { a.sim.order_move(e, F(20), F(27)); b.sim.order_move(e, F(20), F(27)); }
  a.sim.step();
  b.sim.step();
  CHECK(a.sim.nav_stats().paths_this_tick <= 8);
  CHECK(a.sim.nav_stats().queued > 0);
  for (int t = 0; t < 10; ++t) { a.sim.step(); b.sim.step(); }
  CHECK(a.sim.nav_stats().queued == 0);
  for (int t = 0; t < 300; ++t) { a.sim.step(); b.sim.step(); }
  CHECK(a.sim.hash() == b.sim.hash());
  const auto data = a.sim.save();
  World c("NAV-CHOKE.json", 999, np);
  CHECK(c.sim.load(data));
  CHECK(c.sim.hash() == a.sim.hash());
  for (int t = 0; t < 500; ++t) { a.sim.step(); c.sim.step(); }
  CHECK(c.sim.hash() == a.sim.hash());
  CHECK(c.sim.save() == a.sim.save());
  std::printf("  navigációs állapothash: 0x%016llx\n", static_cast<unsigned long long>(a.sim.hash()));
  CHECK(a.sim.hash() == 0x6daebed53e7984a7ull);   // minden rendszeren ugyanaz (mozgás, ütközés, kitérés)
  bool body = false;
  for (const auto& [n, h] : a.sim.component_hashes()) body |= n == "Path";
  CHECK(body);
}

TEST_CASE("navigáció: NAV-GATE – a kapu a saját csapatnak nyitott, az ellenfélnek fal") {
  World w("NAV-GATE.json", 31);
  const int g = w.nav.grid_of(*terrain().movement_class("land"));
  const map::NavCell north = map::Pathfinder::cell_of(F(10), F(5)), south = map::Pathfinder::cell_of(F(10), F(25));
  CHECK(w.pf.reachable(g, 0, north, south, 0) && w.pf.reachable(g, 0, north, south, 1));
  w.sim.nav_gate(18, 14, 2, 2, 0b01);   // az 1. játékos (0. csapat) kapuja
  CHECK(w.pf.reachable(g, 0, north, south, 0));
  CHECK(!w.pf.reachable(g, 0, north, south, 1));
  CHECK(w.pf.reachable(g, 0, north, south));   // csapat nélkül (minden kapu nyitva)
  const auto own = w.unit(F(10), F(5), F(40, 100), 1), foe = w.unit(F(12), F(5), F(40, 100), 2);
  w.sim.order_move(own, F(10), F(25));
  w.sim.order_move(foe, F(12), F(25));
  for (int t = 0; t < 600; ++t) w.sim.step();
  CHECK(!w.moving(own) && w.dist(own, F(10), F(25)) <= F(1, 10));
  CHECK(!w.moving(foe) && w.pos(foe).y < F(14));   // a falnál, a kapu előtt megáll
  // a kapu megnyitása mindkét csapatnak: az ellenfél is átjut
  w.sim.nav_gate(18, 14, 2, 2, 0b11);
  CHECK(w.pf.reachable(g, 0, north, south, 1));
  w.sim.order_move(foe, F(12), F(25));
  for (int t = 0; t < 600; ++t) w.sim.step();
  CHECK(w.dist(foe, F(12), F(25)) <= F(1, 10));
  // eltávolítás: közönséges átjáró
  w.sim.nav_gate(18, 14, 2, 2, 0);
  CHECK(w.nav.gates().empty() && w.pf.reachable(g, 0, north, south, 5));
}
