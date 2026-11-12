// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/angle.hpp"

namespace totu::core {

namespace {

#include "core/angle_tables.inc"

// 0 .. 16384 egység (negyed kör) szinusza: táblázat + lineáris interpoláció (4 bites tört rész).
int32_t sin_quarter(int32_t q) {
  constexpr int kShift = 4;                                 // 16384 / 1024 = 16 egység szakaszonként
  const int32_t idx = q >> kShift;
  const int32_t frac = q & ((1 << kShift) - 1);
  if (idx >= kSinSegments) return kSinQuarter[kSinSegments];
  const int32_t a = kSinQuarter[idx], b = kSinQuarter[idx + 1];
  // a + (b - a) * frac / 16, kerekítve (a különbség pozitív vagy nulla)
  return a + (((b - a) * frac + (1 << (kShift - 1))) >> kShift);
}

}  // namespace

Fixed sin(Angle a) {
  const int32_t u = a.units();
  const int32_t quad = u >> 14;          // 0..3
  const int32_t q = u & 0x3FFF;
  int32_t v = 0;
  switch (quad) {
    case 0: v = sin_quarter(q); break;
    case 1: v = sin_quarter(16384 - q); break;
    case 2: v = -sin_quarter(q); break;
    default: v = -sin_quarter(16384 - q); break;
  }
  return Fixed::from_raw(v);
}

Fixed cos(Angle a) { return sin(a + Angle::from_units(16384)); }

Angle atan2(Fixed fy, Fixed fx) {
  int64_t x = fx.raw(), y = fy.raw();
  if (x == 0 && y == 0) return Angle{};
  constexpr int64_t kQuarter = int64_t{1} << 22;   // 90° a 2^24-es akkumulátorban
  int64_t acc = 0;
  if (x < 0) {                                     // a bal félsíkot elforgatjuk a jobbra
    const int64_t t = x;
    if (y >= 0) { x = y; y = -t; acc = kQuarter; }
    else { x = -y; y = t; acc = -kQuarter; }
  }
  x <<= 16;                                        // pontosság: |x|, |y| ≤ 2^31 → ≤ 2^47, a CORDIC-erősítéssel is < 2^49
  y <<= 16;
  for (int i = 0; i < kCordicSteps; ++i) {
    const int64_t nx = y > 0 ? x + (y >> i) : x - (y >> i);
    const int64_t ny = y > 0 ? y - (x >> i) : y + (x >> i);
    acc += y > 0 ? kCordicAtan[i] : -kCordicAtan[i];
    x = nx;
    y = ny;
  }
  // 2^24 → 2^16 egység, a legközelebbire kerekítve (az aritmetikai eltolás a negatívot is helyesen kezeli)
  const int64_t units = (acc + 128) >> 8;
  return Angle::from_units(static_cast<int32_t>(units & (Angle::kFullTurn - 1)));
}

int direction8(Angle a) {
  // 8 irány, mindegyik 8192 egység; a középre igazítás miatt +4096
  return ((a.units() + 4096) >> 13) & 7;
}

}  // namespace totu::core
