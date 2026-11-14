// SPDX-License-Identifier: GPL-3.0-or-later
// Szövetséges kisnép: mhonyo (D26, KIS-1–2) – a nép saját mechanikája ide kerül (ADR-008 horgai). Új kisnép: ennek a mintájára
// egy fájl itt, egy sor a registry.cpp-ben és a CMakeLists.txt-ben, az adat a content/base/templates/*/mhonyo.json-okban.
#include "sim/faction_module.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

class Module final : public FactionModule {
 public:
  const char* name() const override { return "mhonyo"; }
};

}  // namespace

std::unique_ptr<FactionModule> make_mhonyo_module() { return std::make_unique<Module>(); }

}  // namespace totu::sim
