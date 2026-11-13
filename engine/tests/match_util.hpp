// SPDX-License-Identifier: GPL-3.0-or-later
// Közös meccs-segéd a játékmag-tesztekhez (M6): tartalom egyszer betöltve, pálya, navigáció, szimuláció két ursan
// játékossal.
#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "content/loader.hpp"
#include "map/map.hpp"
#include "map/pathfinder.hpp"
#include "sim/rules.hpp"
#include "sim/sim.hpp"

namespace totu::testing {

using core::Fixed;

inline std::string read_file(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

struct Content {
  content::ContentSet set;
  map::TerrainTable terrain;
  std::optional<sim::Rules> rules;
  Content() {
    content::Vfs vfs;
    vfs.add_layer("totu", std::filesystem::path(TOTU_CONTENT_DIR) / "base");
    content::ContentLoader(vfs, std::filesystem::path(TOTU_CONTENT_DIR) / "schema").load(set);
    std::vector<std::string> errors;
    terrain.load(set, errors);
    const auto game = content::parse(read_file(std::filesystem::path(TOTU_CONTENT_DIR) / "base" / "game.json"));
    if (game) rules = sim::Rules::load(*game, set, errors);
  }
};

inline Content& content_once() {
  static Content c;
  return c;
}

// egy meccs: pálya, navigáció, szimuláció két ursan játékossal
struct Match {
  std::optional<map::Map> map;
  std::unique_ptr<map::NavGrid> nav;
  std::unique_ptr<map::Pathfinder> pf;
  std::unique_ptr<sim::Sim> sim;
  bool ok = false;
  explicit Match(const std::string& name = "ECO-ALAP", bool upkeep = true, bool start = true,
                 std::vector<std::string> factions = {"ursan", "ursan"}) {
    Content& c = content_once();
    std::vector<std::string> errors;
    const auto doc = content::parse(read_file(std::filesystem::path(TOTU_TEST_DATA) / "maps" / (name + ".json")));
    if (!doc || !c.rules) return;
    map = map::Map::from_json(*doc, c.terrain, errors);
    if (!map) return;
    nav = std::make_unique<map::NavGrid>(*map, c.terrain);
    pf = std::make_unique<map::Pathfinder>(*nav);
    sim = std::make_unique<sim::Sim>(20261113);
    sim->attach_navigation(pf.get());
    sim->attach_rules(&*c.rules);
    sim::MatchSetup setup;
    setup.factions = std::move(factions);
    setup.upkeep = upkeep;
    std::string err;
    ok = !start || sim->setup_match(*map, setup, &err);
  }
  void run(int ticks) {
    for (int i = 0; i < ticks; ++i) sim->step();
  }
  int64_t total_node_amount() {
    int64_t s = 0;
    sim->world().pool<sim::ResourceNode>().each([&](sim::EntityId, const sim::ResourceNode& n) { s += n.amount; });
    return s;
  }
  int64_t carried() {
    int64_t s = 0;
    sim->world().pool<sim::Worker>().each([&](sim::EntityId, const sim::Worker& w) { s += w.carry; });
    return s;
  }
  std::vector<sim::EntityId> workers(uint8_t owner) {
    std::vector<sim::EntityId> out;
    sim->world().pool<sim::Worker>().each([&](sim::EntityId e, const sim::Worker&) {
      if (sim->world().get<sim::Health>(e)->owner == owner) out.push_back(e);
    });
    return out;
  }
};


}  // namespace totu::testing
