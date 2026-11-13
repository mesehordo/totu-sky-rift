// SPDX-License-Identifier: GPL-3.0-or-later
// Ooni (M7, ADR-008): a népmodul viselkedési próbái (src/sim/factions/ooni.cpp).
#include "check.hpp"
#include "match_util.hpp"

using namespace totu;
using namespace totu::testing;

TEST_CASE("Ooni: a népmodul betöltve") {
  Match m("BUILD-ALAP");
  bool found = false;
  for (const auto& mod : m.sim->modules_) found = found || std::string(mod->name()) == "ooni";
  CHECK(found);
}
