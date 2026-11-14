// SPDX-License-Identifier: GPL-3.0-or-later
// A totu_sim navigációs parancsai (M4): a golden M4 forgatókönyv és a 8 × 200-as mérés.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "map/terrain.hpp"
#include "sim/replay.hpp"

namespace totu::tools {

// A tartalom (terepfajták, mozgásosztályok) betöltése; hibánál üzenet a stderr-re.
bool load_terrain(const std::string& content_dir, map::TerrainTable& out);
// A meccshez: a terepadat és a szabályadatok (a `game.json`-nal); hibánál false (a hiba a standard hibakimeneten).
bool load_match_content(const std::string& content_dir, map::TerrainTable& terrain, std::optional<sim::Rules>& rules);

// Golden M4: két 60 fős, vegyes méretű, ellenséges sereg a NAV-BRIDGE pályán – alakzatok, csata, kapu, építés.
std::vector<sim::Command> golden_m4_commands(uint64_t seed);

struct BenchResult {
  double p50 = 0, p99 = 0, max = 0, avg = 0;
  int64_t paths = 0, direct = 0, shared = 0, expanded = 0;
  double path_ms = 0, move_ms = 0;
  size_t alive = 0;
};
// 8 × 200 egység folyamatos mozgása egy 256 × 256-os, akadályokkal teli pályán.
BenchResult bench(const map::TerrainTable& terrain, int players, int units, int size, int ticks, uint64_t seed);

}  // namespace totu::tools
