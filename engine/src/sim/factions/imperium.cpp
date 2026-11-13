// SPDX-License-Identifier: GPL-3.0-or-later
// Aurethi Birodalom (MECH-IMP-1–3: alapozás, Reaktorpajzs, Lélekhorgony; túlhajtás, horgonymentés; csak az Arató gyűjt aranyat és meteoritot) – M7, ADR-008; a régi játék és a spec/FAJMECHANIKAK.md szerint.
#include "sim/faction_module.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

class Module final : public FactionModule {
 public:
  const char* name() const override { return "imperium"; }
};

}  // namespace

std::unique_ptr<FactionModule> make_imperium_module() { return std::make_unique<Module>(); }

}  // namespace totu::sim
