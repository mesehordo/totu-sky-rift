// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/decimal.hpp"

#include <string>

namespace totu::core {

namespace {

constexpr int kMaxFracDigits = 12;
constexpr uint64_t kPow10[kMaxFracDigits + 1] = {
    1ull, 10ull, 100ull, 1000ull, 10000ull, 100000ull, 1000000ull, 10000000ull,
    100000000ull, 1000000000ull, 10000000000ull, 100000000000ull, 1000000000000ull};

bool is_digit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

std::optional<Fixed> parse_fixed(std::string_view s) {
  size_t i = 0;
  bool neg = false;
  if (i < s.size() && (s[i] == '-' || s[i] == '+')) { neg = s[i] == '-'; ++i; }
  std::string digits;          // az összes számjegy, a tizedesvessző nélkül
  int point = -1;              // hány számjegy áll a tizedesvessző előtt
  bool any = false;
  for (; i < s.size(); ++i) {
    const char c = s[i];
    if (is_digit(c)) { digits.push_back(c); any = true; }
    else if (c == '.' && point < 0) { point = static_cast<int>(digits.size()); }
    else break;
  }
  if (!any) return std::nullopt;
  if (point < 0) point = static_cast<int>(digits.size());
  if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
    ++i;
    bool eneg = false;
    if (i < s.size() && (s[i] == '-' || s[i] == '+')) { eneg = s[i] == '-'; ++i; }
    if (i >= s.size() || !is_digit(s[i])) return std::nullopt;
    int e = 0;
    for (; i < s.size() && is_digit(s[i]); ++i) {
      e = e * 10 + (s[i] - '0');
      if (e > 40) return std::nullopt;           // a Q16.16 tartományon messze kívül
    }
    point += eneg ? -e : e;
  }
  if (i != s.size()) return std::nullopt;
  // egész és tört rész szétválasztása (a tizedesvessző a kitevővel eltolva)
  uint64_t whole = 0;
  for (int k = 0; k < point && k < static_cast<int>(digits.size()); ++k) {
    whole = whole * 10 + static_cast<uint64_t>(digits[static_cast<size_t>(k)] - '0');
    if (whole > 32768) return std::nullopt;
  }
  for (int k = static_cast<int>(digits.size()); k < point; ++k) {   // a kitevő miatti záró nullák
    whole *= 10;
    if (whole > 32768) return std::nullopt;
  }
  uint64_t frac = 0;
  int fdigits = 0;
  for (int k = point; k < static_cast<int>(digits.size()) && fdigits < kMaxFracDigits; ++k) {
    const char c = k < 0 ? '0' : digits[static_cast<size_t>(k)];   // a negatív kitevő miatti vezető nullák
    frac = frac * 10 + static_cast<uint64_t>(c - '0');
    ++fdigits;
  }
  // raw = whole * 2^16 + round(frac * 2^16 / 10^fdigits)
  const uint64_t den = kPow10[fdigits];
  const uint64_t num = frac << Fixed::kFracBits;                 // frac < 10^12 → < 2^57
  uint64_t raw = (whole << Fixed::kFracBits) + num / den;
  if ((num % den) * 2 >= den && den > 1) ++raw;
  const uint64_t limit = neg ? 2147483648ull : 2147483647ull;
  if (raw > limit) return std::nullopt;
  const int64_t sr = neg ? -static_cast<int64_t>(raw) : static_cast<int64_t>(raw);
  return Fixed::from_raw(static_cast<int32_t>(sr));
}

}  // namespace totu::core
