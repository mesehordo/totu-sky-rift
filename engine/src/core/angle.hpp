// SPDX-License-Identifier: GPL-3.0-or-later
// Szögek és szögfüggvények a szimulációhoz (ADR-004): bináris szög (65 536 egység = teljes kör), szinusz és
// koszinusz rögzített negyedhullám-táblázatból lineáris interpolációval, atan2 egész CORDIC-kal. A táblázatok a
// repóban vannak (tools/gen_fixed_tables.py), így az eredmény minden gépen bitre azonos.
#pragma once

#include <cstdint>

#include "core/fixed.hpp"

namespace totu::core {

class Angle {
 public:
  static constexpr int32_t kFullTurn = 65536;   // 2^16 egység = 360°
  constexpr Angle() = default;
  static constexpr Angle from_units(int32_t u) { Angle a; a.u_ = u & (kFullTurn - 1); return a; }
  static constexpr Angle from_degrees(int32_t deg) {
    return from_units(static_cast<int32_t>((int64_t{deg} * kFullTurn) / 360));
  }
  constexpr int32_t units() const { return u_; }   // 0 .. 65535
  friend constexpr Angle operator+(Angle a, Angle b) { return from_units(a.u_ + b.u_); }
  friend constexpr Angle operator-(Angle a, Angle b) { return from_units(a.u_ - b.u_); }
  friend constexpr bool operator==(Angle a, Angle b) { return a.u_ == b.u_; }

 private:
  int32_t u_ = 0;
};

Fixed sin(Angle a);
Fixed cos(Angle a);
// A (x, y) vektor szöge; (0, 0)-ra 0. A sorrend a szokásos: atan2(y, x).
Angle atan2(Fixed y, Fixed x);
// A szög 8 irányra (0 = kelet, 2 = dél, … – a sprite-irányok sorrendje: K, DK, D, DNy, Ny, ÉNy, É, ÉK).
int direction8(Angle a);

}  // namespace totu::core
