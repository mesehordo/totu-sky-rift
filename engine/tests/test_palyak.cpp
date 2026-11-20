// SPDX-License-Identifier: GPL-3.0-or-later
// A régi játék 20 csatapályája az új motorban (content/base/maps, a tools/palya_export.py kimenete): betölthető,
// a kezdőhelyek száma a régi játékosszám, a kezdőhelyek járható, építhető, tiszta helyen vannak, egymásból
// elérhetők, mellettük fa és arany van (a régi generátor ígérete: minden bázisnak ugyanaz jut), és a meccs elindul.
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "check.hpp"
#include "match_util.hpp"
#include "map/mapgen.hpp"

using namespace totu;

namespace {

struct Palya {
  std::string id;
  int players = 0;
  std::optional<map::Map> map;
  std::vector<std::string> errors;
};

std::vector<Palya>& palyak() {
  static std::vector<Palya> out = [] {
    std::vector<Palya> v;
    testing::Content& c = testing::content_once();
    std::vector<std::filesystem::path> files;
    for (const auto& e : std::filesystem::directory_iterator(std::filesystem::path(TOTU_CONTENT_DIR) / "base" / "maps")) {
      if (e.path().extension() == ".json") files.push_back(e.path());
    }
    std::sort(files.begin(), files.end());
    for (const auto& f : files) {
      Palya p;
      p.id = f.stem().string();
      const auto doc = content::parse(testing::read_file(f));
      if (!doc) {
        p.errors.push_back("hibás JSON");
      } else {
        if (const content::Value* pl = doc->find("players")) p.players = static_cast<int>(pl->as_int().value_or(0));
        p.map = map::Map::from_json(*doc, c.terrain, p.errors);
      }
      v.push_back(std::move(p));
    }
    return v;
  }();
  return out;
}

int dist2(int ax, int ay, int bx, int by) { return (ax - bx) * (ax - bx) + (ay - by) * (ay - by); }

}  // namespace

TEST_CASE("palyak betoltes: a régi 20 csatapálya betölthető, a kezdőhelyek száma a régi játékosszám") {
  CHECK(palyak().size() == 20);
  for (const Palya& p : palyak()) {
    CHECK(p.errors.empty());
    CHECK(p.map.has_value());
    if (!p.map) continue;
    CHECK(p.players >= 2 && p.players <= 8);
    CHECK(static_cast<int>(p.map->objects().starts.size()) == p.players);
    if (!p.errors.empty() || static_cast<int>(p.map->objects().starts.size()) != p.players) std::printf("  pálya: %s\n", p.id.c_str());
  }
}

TEST_CASE("palyak kezdohely: a kezdőhely járható, építhető és tiszta, mellette fa és arany (minden bázisnak ugyanannyi)") {
  const map::TerrainTable& t = testing::content_once().terrain;
  const uint8_t land = *t.movement_class("land");
  for (const Palya& p : palyak()) {
    if (!p.map) continue;
    const map::Map& m = *p.map;
    const auto& ob = m.objects();
    int gold0 = -1;
    for (const auto& [sx, sy] : ob.starts) {
      bool ok = true;
      for (int y = sy - 3; y <= sy + 3; ++y) {   // a főépület (4×4) és egy sáv körülötte
        for (int x = sx - 3; x <= sx + 3; ++x) {
          ok = ok && m.in_bounds(x, y) && m.passable(t, land, x, y) && t.terrains()[m.terrain(x, y)].buildable;
        }
      }
      for (const auto& [tx, ty] : ob.trees) ok = ok && dist2(tx, ty, sx, sy) >= 7 * 7;
      int gold = 0, meteor = 0, trees = 0;
      for (const map::MapResource& r : ob.resources) {
        ok = ok && (r.x + r.size <= sx - 3 || r.x > sx + 3 || r.y + r.size <= sy - 3 || r.y > sy + 3);
        const int d = dist2(r.x + r.size / 2, r.y + r.size / 2, sx, sy);
        if (r.kind == "gold" && d <= 12 * 12) ++gold;
        if (r.kind == "meteor" && d <= 12 * 12) ++meteor;
      }
      for (const auto& [tx, ty] : ob.trees) trees += dist2(tx, ty, sx, sy) <= 14 * 14 ? 1 : 0;
      ok = ok && gold >= 2 && meteor >= 1 && trees >= 10;
      if (gold0 < 0) gold0 = gold;
      ok = ok && gold == gold0;
      CHECK(ok);
      if (!ok) std::printf("  pálya: %s, kezdőhely (%d, %d): arany %d, meteorit %d, fa %d\n", p.id.c_str(), sx, sy, gold, meteor, trees);
    }
  }
}

TEST_CASE("palyak navigacio: a kezdőhelyek egymásból elérhetők (navigáció a fákkal és a lelőhelyekkel)") {
  const map::TerrainTable& t = testing::content_once().terrain;
  for (const Palya& p : palyak()) {
    if (!p.map) continue;
    map::NavGrid nav(*p.map, t);   // az akadályok előbb a rácsra, a hierarchia egyszer épül (gyors)
    for (const map::MapResource& r : p.map->objects().resources) nav.add_obstacle({r.x, r.y, r.size, r.size});
    for (const auto& [x, y] : p.map->objects().trees) nav.add_obstacle({x, y, 1, 1});
    map::Pathfinder pf(nav);
    const int g = nav.grid_of(*t.movement_class("land"));
    const auto& st = p.map->objects().starts;
    auto cell = [](int x, int y) { return map::Pathfinder::cell_of(core::Fixed::from_int(x), core::Fixed::from_int(y)); };
    for (size_t i = 1; i < st.size(); ++i) {
      const bool ok = pf.reachable(g, 0, cell(st[0].first, st[0].second), cell(st[i].first, st[i].second));
      CHECK(ok);
      if (!ok) std::printf("  pálya: %s, 1. és %zu. kezdőhely\n", p.id.c_str(), i + 1);
    }
  }
}

TEST_CASE("palyak meccs: rövid meccs indul minden régi pályán (setup_match, teli kezdőhelyek)") {
  testing::Content& c = testing::content_once();
  CHECK(c.rules.has_value());
  if (!c.rules) return;
  for (Palya& p : palyak()) {
    if (!p.map) continue;
    map::Map m = *p.map;
    map::NavGrid nav(m, c.terrain);
    map::Pathfinder pf(nav);
    sim::Sim s(20261115);
    s.attach_navigation(&pf);
    s.attach_rules(&*c.rules);
    sim::MatchSetup setup;
    setup.factions.assign(static_cast<size_t>(p.players), "ursan");
    std::string err;
    const bool ok = s.setup_match(m, setup, &err);
    CHECK(ok);
    if (!ok) {
      std::printf("  pálya: %s: %s\n", p.id.c_str(), err.c_str());
      continue;
    }
    for (int i = 0; i < 40; ++i) s.step();
    int hq = 0;
    s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building&) {
      const sim::Health* h = s.world().get<sim::Health>(e);
      if (h && h->owner >= 1 && h->owner <= p.players) ++hq;
    });
    CHECK(hq >= p.players);   // minden játékosnak áll a főépülete
    // díszletépületek (a régi `Decor`): a pálya listája beolvasva, a helyükre nem lehet építeni
    CHECK(!m.objects().decor.empty());
    const uint16_t house = c.rules->def("urs_hosszuhaz");
    for (const auto& d : m.objects().decor) {
      std::string why;
      CHECK(!s.can_place(1, house, d.tx, d.ty, &why));
    }
  }
}

TEST_CASE("palyak veletlen: a regi generator (map/mapgen) minden palyabol uj maggal betoltheto, elerheto palyat ad") {
  testing::Content& c = testing::content_once();
  CHECK(c.rules.has_value());
  if (!c.rules) return;
  int done = 0;
  for (const auto& e : std::filesystem::directory_iterator(std::filesystem::path(TOTU_CONTENT_DIR) / "base" / "maps")) {
    if (e.path().extension() != ".json") continue;
    const auto doc = content::parse(testing::read_file(e.path()));
    if (!doc || !doc->find("generator")) continue;
    std::string gerr;
    const auto a = map::generate_map(*doc, 1001, &gerr), b = map::generate_map(*doc, 2002, &gerr);
    CHECK(a && b);
    if (!a || !b) {
      std::printf("  %s: %s\n", e.path().stem().string().c_str(), gerr.c_str());
      continue;
    }
    CHECK(content::dump(*a) != content::dump(*b));   // más mag, más pálya
    std::vector<std::string> errs;
    const auto ma = map::Map::from_json(*a, c.terrain, errs);
    CHECK(ma.has_value() && errs.empty());
    if (!ma) {
      for (const auto& er : errs) std::printf("  %s: %s\n", e.path().stem().string().c_str(), er.c_str());
      continue;
    }
    std::vector<std::string> e2;
    const auto orig = map::Map::from_json(*doc, c.terrain, e2);
    CHECK(orig && orig->objects().starts == ma->objects().starts);   // a kezdőhelyek az elrendezésből: változatlanok
    // a kezdőhelyek egymásból elérhetők (fákkal és lelőhelyekkel), és a meccs elindul
    {
      map::NavGrid nav(*ma, c.terrain);
      for (const map::MapResource& r : ma->objects().resources) nav.add_obstacle({r.x, r.y, r.size, r.size});
      for (const auto& [x, y] : ma->objects().trees) nav.add_obstacle({x, y, 1, 1});
      for (const auto& d : ma->objects().decor) nav.add_obstacle({d.tx, d.ty, d.w, d.h});
      map::Pathfinder pf(nav);
      const int g = nav.grid_of(*c.terrain.movement_class("land"));
      const auto& st = ma->objects().starts;
      auto cl = [](int x, int y) { return map::Pathfinder::cell_of(core::Fixed::from_int(x), core::Fixed::from_int(y)); };
      for (size_t i = 1; i < st.size(); ++i) {
        const bool ok = pf.reachable(g, 0, cl(st[0].first, st[0].second), cl(st[i].first, st[i].second));
        CHECK(ok);
        if (!ok) std::printf("  véletlen pálya: %s, 1. és %zu. kezdőhely\n", e.path().stem().string().c_str(), i + 1);
      }
    }
    {
      map::Map mm = *ma;
      map::NavGrid nav(mm, c.terrain);
      map::Pathfinder pf(nav);
      sim::Sim s(7);
      s.attach_navigation(&pf);
      s.attach_rules(&*c.rules);
      sim::MatchSetup setup;
      setup.factions.assign(mm.objects().starts.size(), "ursan");
      std::string err;
      CHECK(s.setup_match(mm, setup, &err));
    }
    ++done;
  }
  CHECK(done == 20);
}
