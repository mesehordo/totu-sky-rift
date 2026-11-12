// SPDX-License-Identifier: GPL-3.0-or-later
// Fixpontos szám a szimulációhoz (ADR-004): Q16.16 egy int32-ben, a szorzás és az osztás int64 köztes értékkel.
// Minden művelet egész aritmetika, ezért minden fordítón és processzoron bitre azonos.
//   - szorzás: (a * b) >> 16, aritmetikai eltolás (C++20-ban definiált) – a negatív irányba kerekít;
//   - osztás: (a << 16) / b, nulla felé csonkol; nullával osztás: a típus határértéke (előjeltől függően);
//   - túlcsordulás: telítés a típus határán (a szimuláció értékei ezt nem érik el; a próbák ellenőrzik).
#pragma once

#include <cstdint>
#include <limits>

namespace totu::core {

// Pontos egész négyzetgyök (alsó egészrész). A hardveres gyököt csak kezdőértéknek használja, az egész korrekció
// után az eredmény független a lebegőpontos kerekítéstől.
uint64_t isqrt64(uint64_t v);

class Fixed {
 public:
  static constexpr int kFracBits = 16;
  static constexpr int32_t kOne = int32_t{1} << kFracBits;

  constexpr Fixed() = default;
  static constexpr Fixed from_raw(int32_t raw) { Fixed f; f.raw_ = raw; return f; }
  static constexpr Fixed from_int(int32_t v) { return from_raw(sat(static_cast<int64_t>(v) * kOne)); }
  // num / den pontosan kerekítve (a legközelebbire, félnél a nullától el) – pl. adatállandókhoz.
  static Fixed from_ratio(int64_t num, int64_t den);

  constexpr int32_t raw() const { return raw_; }
  constexpr int32_t floor_int() const { return raw_ >> kFracBits; }
  // Csak megjelenítésre és naplóba – a szimuláció állapotába lebegőpontos szám nem kerülhet vissza.
  constexpr double to_double() const { return static_cast<double>(raw_) / kOne; }

  friend constexpr Fixed operator+(Fixed a, Fixed b) { return from_raw(sat(int64_t{a.raw_} + b.raw_)); }
  friend constexpr Fixed operator-(Fixed a, Fixed b) { return from_raw(sat(int64_t{a.raw_} - b.raw_)); }
  friend constexpr Fixed operator-(Fixed a) { return from_raw(sat(-int64_t{a.raw_})); }
  friend constexpr Fixed operator*(Fixed a, Fixed b) {
    return from_raw(sat((int64_t{a.raw_} * b.raw_) >> kFracBits));
  }
  friend Fixed operator/(Fixed a, Fixed b);
  Fixed& operator+=(Fixed b) { return *this = *this + b; }
  Fixed& operator-=(Fixed b) { return *this = *this - b; }
  Fixed& operator*=(Fixed b) { return *this = *this * b; }
  Fixed& operator/=(Fixed b) { return *this = *this / b; }

  friend constexpr bool operator==(Fixed a, Fixed b) { return a.raw_ == b.raw_; }
  friend constexpr bool operator!=(Fixed a, Fixed b) { return a.raw_ != b.raw_; }
  friend constexpr bool operator<(Fixed a, Fixed b) { return a.raw_ < b.raw_; }
  friend constexpr bool operator<=(Fixed a, Fixed b) { return a.raw_ <= b.raw_; }
  friend constexpr bool operator>(Fixed a, Fixed b) { return a.raw_ > b.raw_; }
  friend constexpr bool operator>=(Fixed a, Fixed b) { return a.raw_ >= b.raw_; }

  static constexpr Fixed max() { return from_raw(std::numeric_limits<int32_t>::max()); }
  static constexpr Fixed min() { return from_raw(std::numeric_limits<int32_t>::min()); }

 private:
  static constexpr int32_t sat(int64_t v) {
    if (v > std::numeric_limits<int32_t>::max()) return std::numeric_limits<int32_t>::max();
    if (v < std::numeric_limits<int32_t>::min()) return std::numeric_limits<int32_t>::min();
    return static_cast<int32_t>(v);
  }
  int32_t raw_ = 0;
};

// Négyzetgyök (negatív bemenetre 0).
Fixed sqrt(Fixed v);
// Vektorhossz sqrt(dx² + dy²) int64 köztes értékkel (nem csordul túl a pálya méretein belül).
Fixed length(Fixed dx, Fixed dy);
// Négyzetes távolság Q32.32-ben (int64) – összevetésekhez, gyök nélkül.
inline int64_t length_sq_raw(Fixed dx, Fixed dy) {
  return int64_t{dx.raw()} * dx.raw() + int64_t{dy.raw()} * dy.raw();
}

}  // namespace totu::core
