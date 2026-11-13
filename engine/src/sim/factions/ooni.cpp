// SPDX-License-Identifier: GPL-3.0-or-later
// Ooni (MECH-LANT-1–3: lámpásfény, harci kedv, Taiko-dobos, Torii-kapupár, őrök; védőfény) – M7, ADR-008; a régi játék és a spec/FAJMECHANIKAK.md szerint.
#include "sim/faction_module.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

class Module final : public FactionModule {
 public:
  const char* name() const override { return "ooni"; }
};

}  // namespace

std::unique_ptr<FactionModule> make_ooni_module() { return std::make_unique<Module>(); }

}  // namespace totu::sim
