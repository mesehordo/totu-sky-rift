// SPDX-License-Identifier: GPL-3.0-or-later
// Szürke elfek (MECH-POWER-1–3, MECH-FAITH: energiamező és ellátás, behívás, áhítat; épületképességek) – M7, ADR-008; a régi játék és a spec/FAJMECHANIKAK.md szerint.
#include "sim/faction_module.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

class Module final : public FactionModule {
 public:
  const char* name() const override { return "elves"; }
};

}  // namespace

std::unique_ptr<FactionModule> make_elves_module() { return std::make_unique<Module>(); }

}  // namespace totu::sim
