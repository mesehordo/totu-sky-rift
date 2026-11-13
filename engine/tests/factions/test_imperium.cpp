// SPDX-License-Identifier: GPL-3.0-or-later
// Aurethi Birodalom (M7, ADR-008): a népmodul viselkedési próbái (src/sim/factions/imperium.cpp).
#include "check.hpp"
#include "match_util.hpp"

using namespace totu;
using namespace totu::testing;

TEST_CASE("Aurethi Birodalom: a népmodul betöltve") {
  Match m("BUILD-ALAP");
  bool found = false;
  for (const auto& mod : m.sim->modules_) found = found || std::string(mod->name()) == "imperium";
  CHECK(found);
}
