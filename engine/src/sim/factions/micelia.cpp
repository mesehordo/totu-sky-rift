// SPDX-License-Identifier: GPL-3.0-or-later
// Micéliák (MECH-NET-1–2, D28: fonalgráf, spórák, osztódás; összeolvadás) – M7, ADR-008; a régi játék és a spec/FAJMECHANIKAK.md szerint.
#include "sim/faction_module.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

class Module final : public FactionModule {
 public:
  const char* name() const override { return "micelia"; }
};

}  // namespace

std::unique_ptr<FactionModule> make_micelia_module() { return std::make_unique<Module>(); }

}  // namespace totu::sim
