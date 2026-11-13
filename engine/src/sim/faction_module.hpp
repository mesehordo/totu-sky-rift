// SPDX-License-Identifier: GPL-3.0-or-later
// Népmodul (M7, ADR-008): egy faj (vagy kisnép) saját mechanikája egy fájlban (`src/sim/factions/<nép>.cpp`), horgokon
// át kapcsolódva a szimulációhoz. A modul a nép saját adatkulcsait a nyers JSON-ból olvassa (`FactionDef::raw`,
// `EntityDef::raw`, `AbilityDef::spec`), így a közös fájlokhoz (rules, sim) nem kell nyúlnia. A modulok sorrendje
// rögzített (registry.cpp) – a determinizmus miatt. Minden horog alapértéke semleges.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/fixed.hpp"
#include "sim/world.hpp"

namespace totu::sim {

class Sim;
using core::Fixed;

class FactionModule {
 public:
  virtual ~FactionModule() = default;
  virtual const char* name() const = 0;
  // a modul saját komponensei (az attach_rules-ban; a regisztráció sorrendje a hash és a mentés része)
  virtual void register_components(World&) {}
  virtual void on_match_start(Sim&) {}                     // a setup_match végén
  virtual void tick(Sim&) {}                               // tickenként, a képességek után
  // szorzók (1 = semleges): mozgási sebesség, a támadó sebzése a célpont ellen, az építés haladása, a gyűjtés üteme
  virtual Fixed speed_mult(Sim&, EntityId) { return Fixed::from_int(1); }
  virtual Fixed damage_mult(Sim&, EntityId /*attacker*/, EntityId /*target*/) { return Fixed::from_int(1); }
  virtual Fixed build_rate_mult(Sim&, EntityId /*building*/) { return Fixed::from_int(1); }
  virtual Fixed gather_mult(Sim&, EntityId /*worker*/) { return Fixed::from_int(1); }
  virtual Fixed regen_add(Sim&, EntityId) { return Fixed{}; }       // életerő / s
  virtual int32_t supply_bonus(Sim&, uint8_t /*owner*/) { return 0; }
  // engedélyek: lerakás (hibánál az ok az `error`-ba), gyűjtés, pajzstöltés
  virtual bool can_place(Sim&, uint8_t /*owner*/, uint16_t /*def*/, int /*tx*/, int /*ty*/, std::string* /*error*/) { return true; }
  virtual bool can_gather(Sim&, EntityId /*worker*/, int /*kind*/) { return true; }
  virtual bool shield_regen_allowed(Sim&, EntityId) { return true; }
  virtual bool hidden(Sim&, EntityId) { return false; }   // rejtett (a felfedés a közös szabály szerint)
  // események
  virtual void on_unit_spawned(Sim&, EntityId) {}
  virtual void on_building_placed(Sim&, EntityId) {}
  virtual void on_building_complete(Sim&, EntityId) {}
  virtual void on_death(Sim&, EntityId /*victim*/, EntityId /*killer*/, uint8_t /*killer_owner*/) {}
  // a keret által nem ismert képességtípus (`AbilityDef::Type::Unknown`, a `spec` szerint); true: kezelte
  virtual bool use_ability(Sim&, const std::vector<EntityId>& /*units*/, uint16_t /*ability*/, Fixed /*x*/, Fixed /*y*/,
                           EntityId /*target*/) {
    return false;
  }
};

// a modulok rögzített sorrendben (src/sim/factions/registry.cpp)
std::vector<std::unique_ptr<FactionModule>> make_faction_modules();

}  // namespace totu::sim
