// SPDX-License-Identifier: GPL-3.0-or-later
// Determinisztikus véletlenszám-generátor a szimulációhoz (ADR-004): PCG32 (M. E. O'Neill, „PCG XSH RR 64/32”).
// Csak egész műveletek; ugyanaz a mag és folyam minden gépen ugyanazt a sorozatot adja. Rendszerenként külön
// folyam (pl. harc, gépi ellenfél, pályagenerálás), hogy egy új hívás az egyikben ne tolja el a többit.
#pragma once

#include <cstdint>

namespace totu::core {

class Pcg32 {
 public:
  Pcg32(uint64_t seed, uint64_t stream) { reseed(seed, stream); }

  void reseed(uint64_t seed, uint64_t stream) {
    state_ = 0;
    inc_ = (stream << 1u) | 1u;
    next();
    state_ += seed;
    next();
  }

  uint32_t next() {
    const uint64_t old = state_;
    state_ = old * 6364136223846793005ull + inc_;
    const auto xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
    const auto rot = static_cast<uint32_t>(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
  }

  // Egyenletes egész a [0, bound) tartományban, torzítás nélkül (elutasításos módszer); bound = 0 → 0.
  uint32_t below(uint32_t bound) {
    if (bound == 0) return 0;
    const uint32_t threshold = (0u - bound) % bound;
    for (;;) {
      const uint32_t r = next();
      if (r >= threshold) return r % bound;
    }
  }

  // Egyenletes egész a [lo, hi] zárt tartományban.
  int32_t range(int32_t lo, int32_t hi) {
    if (hi <= lo) return lo;
    const auto span = static_cast<uint32_t>(static_cast<int64_t>(hi) - lo + 1);
    return static_cast<int32_t>(lo + static_cast<int64_t>(below(span)));
  }

  // Esély: true, ha egy [0, 1) egyenletes szám kisebb, mint num/den.
  bool chance(uint32_t num, uint32_t den) { return den != 0 && below(den) < num; }

  uint64_t state() const { return state_; }
  uint64_t increment() const { return inc_; }
  void restore(uint64_t state, uint64_t inc) { state_ = state; inc_ = inc; }

 private:
  uint64_t state_ = 0;
  uint64_t inc_ = 1;
};

}  // namespace totu::core
