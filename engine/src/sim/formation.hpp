// SPDX-License-Identifier: GPL-3.0-or-later
// Alakzatok (ADR-006 7. pont, D32): alapból WC3-szerű automatikus alakzat (elöl a közelharcosok, hátul a távolsági
// egységek; a csoport a leglassabb tag sebességével halad), emellett választható AoE-alakzatok: sor, doboz,
// szétszórt, szárnyas. Az alakzat a csoport célpontja köré számolt helyek listája (oldalirány, mélység); a paraméterek
// adatban vannak (`game.json` → `formations`), így modolhatók. Fixpontos, determinisztikus.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "content/json.hpp"
#include "core/fixed.hpp"

namespace totu::sim {

using core::Fixed;

enum class Formation : uint8_t { None = 0, Auto = 1, Line = 2, Box = 3, Spread = 4, Flank = 5 };
constexpr int kFormationCount = 6;

struct FormationSpec {
  Fixed spacing = Fixed::from_int(1);   // a helyek távolsága (csempe); a legnagyobb egységhez igazodva nő
  int max_row = 20;                     // egy sor legfeljebb ennyi helyből áll
  Fixed gap = Fixed::from_int(4);       // szárnyas: a két szárny közötti rés
  bool together = true;                 // a leglassabb tag sebességével halad (a None-nál nem)
};

struct FormationSet {
  std::array<FormationSpec, kFormationCount> spec{};
  FormationSet();
  const FormationSpec& operator[](Formation f) const { return spec[static_cast<size_t>(f)]; }
  // a `game.json` `formations` részéből (a hiányzó mezők alapértéke marad)
  static FormationSet from_json(const content::Value* doc, std::vector<std::string>& errors);
};

struct Slot {
  Fixed lateral;   // jobbra + (a haladási irányhoz képest)
  Fixed depth;     // az első sortól hátrafelé
  int row = 0;
};

// n hely a megadott alakzatban; a sorok elölről hátra, a soron belül balról jobbra. min_spacing: a legnagyobb egység
// átmérője alapján (a hely legalább ekkora).
std::vector<Slot> formation_slots(Formation f, int n, const FormationSpec& spec, Fixed min_spacing);
Formation formation_from_name(const std::string& name, bool* ok = nullptr);

}  // namespace totu::sim
