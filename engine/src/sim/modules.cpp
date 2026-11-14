// SPDX-License-Identifier: GPL-3.0-or-later
// A népmodulok horgainak összesítése (M7, ADR-008): a szorzók szorzata, az engedélyek ÉS-e, a bónuszok összege – a
// modulok rögzített sorrendjében (registry.cpp).
#include "sim/sim.hpp"

namespace totu::sim {

Fixed Sim::mod_speed(EntityId e) {
  Fixed m = Fixed::from_int(1);
  for (auto& x : modules_) m = m * x->speed_mult(*this, e);
  return m;
}

Fixed Sim::mod_damage(EntityId e, EntityId t) {
  Fixed m = Fixed::from_int(1);
  for (auto& x : modules_) m = m * x->damage_mult(*this, e, t);
  return m;
}

Fixed Sim::mod_build(EntityId b) {
  Fixed m = Fixed::from_int(1);
  for (auto& x : modules_) m = m * x->build_rate_mult(*this, b);
  return m;
}

Fixed Sim::mod_gather(EntityId w) {
  Fixed m = Fixed::from_int(1);
  for (auto& x : modules_) m = m * x->gather_mult(*this, w);
  return m;
}

Fixed Sim::mod_regen(EntityId e) {
  Fixed r;
  for (auto& x : modules_) r = r + x->regen_add(*this, e);
  return r;
}

int32_t Sim::mod_supply(uint8_t owner) {
  int32_t s = 0;
  for (auto& x : modules_) s += x->supply_bonus(*this, owner);
  return s;
}

bool Sim::mod_can_place(uint8_t owner, uint16_t def, int tx, int ty, std::string* error) {
  for (auto& x : modules_) {
    if (!x->can_place(*this, owner, def, tx, ty, error)) return false;
  }
  return true;
}

bool Sim::mod_can_gather(EntityId w, int kind) {
  for (auto& x : modules_) {
    if (!x->can_gather(*this, w, kind)) return false;
  }
  return true;
}

bool Sim::mod_shield_ok(EntityId e) {
  for (auto& x : modules_) {
    if (!x->shield_regen_allowed(*this, e)) return false;
  }
  return true;
}

bool Sim::mod_hidden(EntityId e) {
  for (auto& x : modules_) {
    if (x->hidden(*this, e)) return true;
  }
  return false;
}

Fixed Sim::mod_damage_taken(EntityId src, EntityId t, bool ranged, bool magic) {
  Fixed m = Fixed::from_int(1);
  for (auto& x : modules_) m = m * x->damage_taken_mult(*this, src, t, ranged, magic);
  return m;
}

int32_t Sim::mod_armor(EntityId t) {
  int32_t a = 0;
  for (auto& x : modules_) a += x->armor_add(*this, t);
  return a;
}

bool Sim::mod_ability_affordable(EntityId e, const AbilityDef& a) {
  for (auto& m : modules_) {
    if (!m->ability_affordable(*this, e, a)) return false;
  }
  return true;
}

void Sim::mod_ability_paid(EntityId e, const AbilityDef& a) {
  for (auto& m : modules_) m->ability_paid(*this, e, a);
}

bool Sim::mod_ability_costs() const {
  for (const auto& m : modules_) {
    if (m->has_ability_costs()) return true;
  }
  return false;
}

bool Sim::mod_use_power(uint8_t owner, EntityId b, uint16_t ability, Fixed x, Fixed y) {
  for (auto& m : modules_) {
    if (m->use_power(*this, owner, b, ability, x, y)) return true;
  }
  return false;
}

bool Sim::mod_command(const Command& c) {
  for (auto& m : modules_) {
    if (m->on_command(*this, c)) return true;
  }
  return false;
}

}  // namespace totu::sim
