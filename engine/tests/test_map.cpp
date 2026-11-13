// SPDX-License-Identifier: GPL-3.0-or-later
// A térkép tesztjei (M4, 1. lépés): terepfajták és mozgásosztályok, pályaformátum, szintek és szirtek, navrács,
// szabad tér méretosztályonként, akadályok helyi újraszámolása (= teljes újraszámolás).
#include <deque>
#include <filesystem>
#include <string>

#include "check.hpp"
#include "content/loader.hpp"
#include "core/rng.hpp"
#include "map/navgrid.hpp"

using namespace totu;
using map::Fixed;

namespace {

const map::TerrainTable& table() {
  static map::TerrainTable t = [] {
    content::Vfs vfs;
    vfs.add_layer("totu", std::filesystem::path(TOTU_CONTENT_DIR) / "base");
    content::ContentSet set;
    content::ContentLoader loader(vfs, std::filesystem::path(TOTU_CONTENT_DIR) / "schema");
    loader.load(set);
    map::TerrainTable tt;
    std::vector<std::string> errors;
    tt.load(set, errors);
    for (const auto& e : errors) std::printf("  terephiba: %s\n", e.c_str());
    return tt;
  }();
  return t;
}

std::optional<map::Map> parse_map(const std::string& json, std::vector<std::string>* errs = nullptr) {
  std::vector<std::string> errors;
  auto doc = content::parse(json);
  if (!doc) return std::nullopt;
  auto m = map::Map::from_json(*doc, table(), errors);
  if (errs) *errs = errors;
  return m;
}

std::string rows_json(const std::vector<std::string>& rows) {
  std::string s = "[";
  for (size_t i = 0; i < rows.size(); ++i) s += (i ? ",\"" : "\"") + rows[i] + "\"";
  return s + "]";
}

// csempés szélességi bejárás a mozgásosztálynak (a szirtek és a terep szerint)
bool reachable(const map::Map& m, uint8_t mc, int sx, int sy, int gx, int gy) {
  std::vector<uint8_t> seen(static_cast<size_t>(m.width() * m.height()), 0);
  std::deque<std::pair<int, int>> q{{sx, sy}};
  seen[static_cast<size_t>(sy * m.width() + sx)] = 1;
  while (!q.empty()) {
    auto [x, y] = q.front();
    q.pop_front();
    if (x == gx && y == gy) return true;
    const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& v : d) {
      const int nx = x + v[0], ny = y + v[1];
      if (!m.passable(table(), mc, nx, ny)) continue;
      auto& s = seen[static_cast<size_t>(ny * m.width() + nx)];
      if (!s) { s = 1; q.emplace_back(nx, ny); }
    }
  }
  return false;
}

}  // namespace

TEST_CASE("terep: terepfajták, járhatósági és mozgásosztályok a tartalomból") {
  const auto& t = table();
  CHECK(t.terrains().size() == 24);
  CHECK(t.movement_classes().size() == 4);
  CHECK(t.pass_classes().size() == 8);   // blocked, bridge, cliff, deep, ice, land, ramp, shallow (D35: jég)
  const uint8_t land = *t.movement_class("land"), amph = *t.movement_class("amphibious");
  const uint8_t water = *t.movement_class("water"), fly = *t.movement_class("flying");
  CHECK(t.speed(land, *t.terrain("grass")) == Fixed::from_int(1));
  CHECK(t.speed(land, *t.terrain("shallow_water")) == Fixed::from_ratio(7, 10));
  CHECK(t.speed(land, *t.terrain("mud")) == Fixed::from_ratio(8, 10));
  CHECK(t.speed(land, *t.terrain("paved")) == Fixed::from_ratio(12, 10));
  CHECK(!t.passable(land, *t.terrain("deep_water")));
  CHECK(t.speed(amph, *t.terrain("deep_water")) == Fixed::from_ratio(8, 10));
  CHECK(!t.passable(water, *t.terrain("grass")));
  CHECK(t.passable(water, *t.terrain("deep_water")));
  CHECK(t.passable(fly, *t.terrain("rock")));
  CHECK(!t.passable(land, *t.terrain("rock")));
  CHECK(!t.passable(land, *t.terrain("cliff")));
  CHECK(t.passable(land, *t.terrain("ramp")) && t.passable(land, *t.terrain("bridge")));
  CHECK(*t.terrain_by_char('~') == *t.terrain("shallow_water"));
  // a megjelenítő adatai (art): táj, sor, rang, árnyalat, part
  const auto& deep = t.terrains()[*t.terrain("deep_water")].art;
  CHECK(deep.set == "tileset" && deep.row == "water" && deep.shore == "sand" && deep.prio == 30);
  const auto& mud = t.terrains()[*t.terrain("mud")].art;
  CHECK(mud.row == "dirt" && mud.tint[0] == 150 && mud.tint[2] == 100);
  CHECK(t.terrains()[*t.terrain("snow")].art.set == "winter");
}

TEST_CASE("pálya: sorok jelmagyarázattal, futáshossz-kódolás, hibák") {
  const auto a = parse_map(R"({"width": 4, "height": 2, "terrain": {"rows": ["..~~", "#.HW"]}})");
  CHECK(a.has_value());
  const auto b = parse_map(R"({"width": 4, "height": 2,
    "terrain": {"rle": "grass*2 shallow_water*2 rock grass bridge deep_water"}})");
  CHECK(b.has_value() && a->hash() == b->hash());
  const auto c = parse_map(R"({"width": 4, "height": 2, "terrain": {"legend": {"x": "grass", "y": "shallow_water",
    "z": "rock", "h": "bridge", "v": "deep_water"}, "rows": ["xxyy", "zxhv"]}})");
  CHECK(c.has_value() && a->hash() == c->hash());
  std::vector<std::string> errs;
  CHECK(!parse_map(R"({"width": 4, "height": 2, "terrain": {"rows": ["..~~", "#.H"]}})", &errs));
  CHECK(errs.size() == 1 && errs[0].find("2. sor") != std::string::npos);
  CHECK(!parse_map(R"({"width": 4, "height": 2, "terrain": {"rows": ["..~~", "#.H?"]}})", &errs));
  CHECK(errs.size() == 1 && errs[0].find("`?`") != std::string::npos);
  CHECK(!parse_map(R"({"width": 4, "height": 2, "terrain": {"rle": "grass*7"}})"));
  CHECK(!parse_map(R"({"width": 4, "height": 2, "terrain": {"rle": "lava*8"}})"));
  CHECK(!parse_map(R"({"width": 600, "height": 2, "terrain": {"rle": "grass*1200"}})"));
  CHECK(!parse_map(R"({"width": 2, "height": 1, "terrain": {"rle": "grass*2"}, "levels": {"rows": ["03"]}})"));
}

TEST_CASE("pálya: magaslat, mélység, szirtperem és rámpa") {
  // 10 × 10: a felső öt sor magaslat (1), a 4–5. oszlopban rámpa (a 4–5. sorban); jobb alul egy mélység (−1)
  const std::vector<std::string> terrain = {"..........", "..........", "..........", "..........", "..../.....",
                                            "..../.....", "..........", "..........", "..........", ".........."};
  std::vector<std::string> t2 = terrain;
  t2[4][5] = '/';
  t2[5][5] = '/';
  const std::vector<std::string> levels = {"1111111111", "1111111111", "1111111111", "1111111111", "1111111111",
                                           "0000110000", "0000000000", "0000000---", "0000000---", "0000000---"};
  const auto m = parse_map(R"({"width": 10, "height": 10, "terrain": {"rows": )" + rows_json(t2) +
                           R"(}, "levels": {"rows": )" + rows_json(levels) + "}}");
  CHECK(m.has_value());
  CHECK(m->level(0, 0) == 1 && m->level(8, 8) == -1 && m->level(0, 9) == 0);
  CHECK(m->cliff(0, 4));                 // a magaslat pereme
  CHECK(!m->cliff(0, 3));
  CHECK(!m->cliff(4, 4) && !m->cliff(4, 5));   // rámpa
  CHECK(m->cliff(6, 7) && m->cliff(6, 8));     // a mélység körüli alapszint pereme
  CHECK(!m->cliff(8, 8));
  const uint8_t land = *table().movement_class("land"), fly = *table().movement_class("flying");
  CHECK(reachable(*m, land, 4, 9, 4, 1));      // fel a rámpán
  CHECK(!reachable(*m, land, 0, 9, 8, 8));     // a mélységbe nincs rámpa
  CHECK(m->passable(table(), fly, 8, 8));
  // rámpa nélkül a magaslat elérhetetlen
  const auto no_ramp = parse_map(R"({"width": 10, "height": 10, "terrain": {"rle": "grass*100"}, "levels": {"rows": )" +
                                 rows_json(levels) + "}}");
  CHECK(no_ramp.has_value() && !reachable(*no_ramp, land, 4, 9, 4, 1));
  // szirt kikapcsolva: a szint csak adat
  const auto flat = parse_map(R"({"width": 10, "height": 10, "terrain": {"rle": "grass*100"}, "auto_cliffs": false,
    "levels": {"rows": )" + rows_json(levels) + "}}");
  CHECK(flat.has_value() && reachable(*flat, land, 4, 9, 4, 1));
}

TEST_CASE("navrács: szabad tér és méretosztály – 1, 2 és 3 csempés rés") {
  // a 3. sor sziklafal, rések: 1 csempe (x=3), 2 csempe (x=9–10), 3 csempe (x=16–18)
  std::vector<std::string> rows(7, std::string(24, '.'));
  rows[3] = "###.#####..#####...#####";
  const auto m = parse_map(R"({"width": 24, "height": 7, "terrain": {"rows": )" + rows_json(rows) + "}}");
  CHECK(m.has_value());
  map::NavGrid nav(*m, table());
  CHECK(nav.width() == 48 && nav.height() == 14);
  const int g = nav.grid_of(*table().movement_class("land"));
  CHECK(g >= 0);
  CHECK(nav.grid_of(*table().movement_class("flying")) == -1);
  auto best = [&](int tx0, int tx1) {
    int b = 0;
    for (int y = 6; y < 8; ++y) {
      for (int x = tx0 * 2; x < tx1 * 2; ++x) b = std::max(b, static_cast<int>(nav.clearance(g, x, y)));
    }
    return b;
  };
  CHECK(best(3, 4) == 1);
  CHECK(best(9, 11) == 2);
  CHECK(best(16, 19) == 3);
  const int small = nav.size_class(Fixed::from_ratio(38, 100));
  const int medium = nav.size_class(Fixed::from_ratio(60, 100));
  const int large = nav.size_class(Fixed::from_ratio(95, 100));
  CHECK(small == 0 && medium == 1 && large == 2);
  CHECK(nav.fits(g, small, 7, 6));                 // a kis egység az 1 csempés résen is átfér
  CHECK(!nav.fits(g, medium, 7, 6));
  CHECK(nav.fits(g, medium, 19, 6) && !nav.fits(g, large, 19, 6));
  CHECK(nav.fits(g, large, 34, 6));                // a nagy csak a 3 csempés résen
  CHECK(nav.clearance(g, 0, 0) == 1);              // a pálya széle akadály
  CHECK(nav.clearance(g, 20, 0) == 1 && nav.clearance(g, 20, 2) == 3);
}

TEST_CASE("navrács: akadályok – helyi újraszámolás = teljes, eltávolítás visszaállít") {
  const auto m = parse_map(R"({"width": 40, "height": 40, "terrain": {"rle": "grass*1600"}})");
  CHECK(m.has_value());
  map::NavGrid nav(*m, table());
  const int g = nav.grid_of(*table().movement_class("land"));
  const uint64_t empty = nav.hash();
  nav.add_obstacle({10, 10, 3, 3});
  CHECK(nav.clearance(g, 22, 22) == 0);
  CHECK(nav.clearance(g, 19, 22) == 1 && nav.clearance(g, 16, 22) == 4);
  const uint64_t one = nav.hash();
  nav.rebuild_all();
  CHECK(nav.hash() == one);
  nav.add_obstacle({11, 11, 3, 3});                // átfedő akadály
  nav.remove_obstacle({10, 10, 3, 3});
  CHECK(nav.clearance(g, 23, 23) == 0);            // az átfedésben még akadály van
  nav.remove_obstacle({11, 11, 3, 3});
  CHECK(nav.hash() == empty);
  // véletlen sorozat (szélekkel is): a helyi frissítés minden lépés után egyezik a teljes újraszámolással
  core::Pcg32 rng(42, 7);
  std::vector<map::TileRect> placed;
  bool same = true;
  for (int i = 0; i < 60; ++i) {
    if (!placed.empty() && rng.chance(1, 3)) {
      const auto k = rng.below(static_cast<uint32_t>(placed.size()));
      nav.remove_obstacle(placed[k]);
      placed.erase(placed.begin() + k);
    } else {
      map::TileRect r{rng.range(-1, 38), rng.range(-1, 38), rng.range(1, 4), rng.range(1, 4)};
      nav.add_obstacle(r);
      placed.push_back(r);
    }
    const uint64_t local = nav.hash();
    map::NavGrid fresh(*m, table());
    for (const auto& r : placed) fresh.add_obstacle(r);
    fresh.rebuild_all();
    same &= fresh.hash() == local;
  }
  CHECK(same);
}

TEST_CASE("navrács: mozgásosztályok – szárazföldi, kétéltű, vízi; terepváltozás") {
  std::vector<std::string> rows(8, std::string(8, '.'));
  for (int y = 2; y < 6; ++y) rows[static_cast<size_t>(y)] = "..WWWW..";
  rows[1] = "..~~~~..";
  auto m = parse_map(R"({"width": 8, "height": 8, "terrain": {"rows": )" + rows_json(rows) + "}}");
  CHECK(m.has_value());
  map::NavGrid nav(*m, table());
  const int land = nav.grid_of(*table().movement_class("land"));
  const int amph = nav.grid_of(*table().movement_class("amphibious"));
  const int water = nav.grid_of(*table().movement_class("water"));
  CHECK(!nav.passable(land, 8, 8) && nav.passable(amph, 8, 8) && nav.passable(water, 8, 8));
  CHECK(nav.passable(land, 4, 2) && nav.speed(land, 4, 2) == Fixed::from_ratio(7, 10));   // gázló
  CHECK(!nav.passable(water, 0, 0));
  // terepváltozás (szerkesztő): egy magaslat a pálya bal felső sarkában
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 2; ++x) m->set_level(x, y, 1);
  }
  m->compute_cliffs(table());
  nav.terrain_changed({0, 0, 2, 2});
  const uint64_t local = nav.hash();
  nav.rebuild_all();
  CHECK(nav.hash() == local);
  CHECK(!nav.passable(land, 3, 3));                // a (1, 1) csempe a magaslat pereme
  CHECK(nav.passable(land, 0, 0));                 // a sarokcsempének nincs alacsonyabb szomszédja
}

TEST_CASE("pálya: méretek és navigációs beállítás a game.json-ból (D34)") {
  content::Vfs vfs;
  vfs.add_layer("totu", std::filesystem::path(TOTU_CONTENT_DIR) / "base");
  const auto text = vfs.read_text("game.json");
  CHECK(text.has_value());
  const auto game = content::parse(*text);
  CHECK(game.has_value());
  const content::Value* sizes = game->find("map_sizes");
  CHECK(sizes && sizes->find("small")->find("size")->as_int() == 96);
  CHECK(sizes->find("medium")->find("size")->as_int() == 160);
  CHECK(sizes->find("large")->find("size")->as_int() == 256);
  CHECK(sizes->find("max_custom")->as_int() == map::kMaxMapSize);
  std::vector<std::string> errors;
  const auto cfg = map::NavConfig::from_json(game->find("navigation"), errors);
  CHECK(errors.empty() && cfg.sizes.size() == 3 && cfg.sizes[2].clearance == 3);
  CHECK(game->find("elevation_rules") != nullptr);
  // a legnagyobb méretű pálya navrácsa is gyorsan felépül
  map::Map big(256, 256, *table().terrain("grass"));
  map::NavGrid nav(big, table(), cfg);
  CHECK(nav.width() == 512);
}
