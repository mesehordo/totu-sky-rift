// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/fixed.hpp"

#include <cmath>

namespace totu::core {

uint64_t isqrt64(uint64_t v) {
  constexpr uint64_t kMaxRoot = 0xFFFFFFFFull;   // floor(sqrt(2^64 - 1))
  auto r = static_cast<uint64_t>(std::sqrt(static_cast<double>(v)));
  if (r > kMaxRoot) r = kMaxRoot;
  while (r * r > v) --r;
  while (r < kMaxRoot && (r + 1) * (r + 1) <= v) ++r;
  return r;
}

Fixed Fixed::from_ratio(int64_t num, int64_t den) {
  if (den == 0) return num >= 0 ? max() : min();
  const bool neg = (num < 0) != (den < 0);
  const uint64_t n = num < 0 ? 0 - static_cast<uint64_t>(num) : static_cast<uint64_t>(num);
  const uint64_t d = den < 0 ? 0 - static_cast<uint64_t>(den) : static_cast<uint64_t>(den);
  // q = round(n * 2^16 / d) – az egész és a tört részt külön számolva, hogy ne csorduljon túl
  const uint64_t whole = n / d, rem = n % d;
  uint64_t q = whole << kFracBits;
  const uint64_t frac_num = rem << kFracBits;   // rem < d; a szimulációs állandóknál d << 2^47
  q += frac_num / d;
  if ((frac_num % d) * 2 >= d) ++q;             // félnél a nullától el
  const int64_t s = q > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
                        ? std::numeric_limits<int64_t>::max()
                        : static_cast<int64_t>(q);
  return from_raw(sat(neg ? -s : s));
}

Fixed operator/(Fixed a, Fixed b) {
  if (b.raw_ == 0) return a.raw_ >= 0 ? Fixed::max() : Fixed::min();
  return Fixed::from_raw(Fixed::sat((int64_t{a.raw_} * Fixed::kOne) / b.raw_));
}

Fixed sqrt(Fixed v) {
  if (v.raw() <= 0) return Fixed{};
  // sqrt(raw / 2^16) * 2^16 = sqrt(raw * 2^16)
  return Fixed::from_raw(static_cast<int32_t>(isqrt64(static_cast<uint64_t>(v.raw()) << Fixed::kFracBits)));
}

Fixed length(Fixed dx, Fixed dy) {
  const uint64_t sq = static_cast<uint64_t>(length_sq_raw(dx, dy));   // Q32.32
  const uint64_t r = isqrt64(sq);                                     // Q16.16
  return Fixed::from_raw(r > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())
                             ? std::numeric_limits<int32_t>::max()
                             : static_cast<int32_t>(r));
}

}  // namespace totu::core
