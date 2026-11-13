// SPDX-License-Identifier: GPL-3.0-or-later
// Huk’ucha (MECH-POND-1–5: tavacska ásása, építés saját tóra, mozgás tavon, építésgyorsítás, áradás, Gyöngybója; ikrarakás) – M7, ADR-008; a régi játék és a spec/FAJMECHANIKAK.md szerint.
#include "sim/faction_module.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

class Module final : public FactionModule {
 public:
  const char* name() const override { return "hukucha"; }
};

}  // namespace

std::unique_ptr<FactionModule> make_hukucha_module() { return std::make_unique<Module>(); }

}  // namespace totu::sim
