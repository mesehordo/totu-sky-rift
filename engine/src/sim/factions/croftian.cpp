// SPDX-License-Identifier: GPL-3.0-or-later
// Croftianok (MECH-CREEP-1–8: a mélység terjedése, építés csak mélységre, mozgás és gyógyulás, rothadás, fertőzés, lárvák, tüskék; átváltozás) – M7, ADR-008; a régi játék és a spec/FAJMECHANIKAK.md szerint.
#include "sim/faction_module.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

class Module final : public FactionModule {
 public:
  const char* name() const override { return "croftian"; }
};

}  // namespace

std::unique_ptr<FactionModule> make_croftian_module() { return std::make_unique<Module>(); }

}  // namespace totu::sim
