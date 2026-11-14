// SPDX-License-Identifier: GPL-3.0-or-later
// Tömeges gép–gép meccsek futtatója (M9 kapu): elakadás-észlelés és játékosonkénti mérőszámok.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "map/terrain.hpp"
#include "sim/rules.hpp"

namespace totu::tools {

struct AiBatchMatch {
  std::string map_path;
  std::vector<std::string> factions;   // játékosonként (mind gép)
  uint64_t seed = 1;
  int64_t ticks = 12 * 60 * 30;
  bool camps = false;
  std::string script_name, script_source;   // a gép szkriptje (minden játékosnak)
};

struct AiBatchResult {
  AiBatchMatch match;
  bool ok = false;
  std::string error;
  uint8_t winner = 0, stalled_player = 0;
  int64_t end_tick = 0, stall_tick = 0;
  size_t script_errors = 0;
  struct Player {
    int64_t gathered = 0;
    int units = 0, buildings = 0, researched = 0;
    bool defeated = false;
  };
  std::vector<Player> players;
};

AiBatchResult run_ai_match(const AiBatchMatch& m, const map::TerrainTable& terrain, const sim::Rules& rules);
std::vector<AiBatchResult> run_ai_batch(const std::vector<AiBatchMatch>& matches, const map::TerrainTable& terrain,
                                        const sim::Rules& rules, int threads);

}  // namespace totu::tools
