// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/hash.hpp"

namespace totu::core {

void StateHash::byte(std::uint8_t b) {
  h_ ^= b;
  h_ *= 1099511628211ULL;
}

void StateHash::add_u64(std::uint64_t v) {
  for (int i = 0; i < 8; ++i) {
    byte(static_cast<std::uint8_t>(v >> (8 * i)));
  }
}

void StateHash::add_bytes(std::string_view s) {
  add_u64(s.size());
  for (char c : s) {
    byte(static_cast<std::uint8_t>(c));
  }
}

}  // namespace totu::core
