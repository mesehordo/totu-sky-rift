// SPDX-License-Identifier: GPL-3.0-or-later
// Állapothash a determinizmus-ellenőrzéshez: FNV-1a 64 bit, bájtsorrendtől független (mindig little-endian
// szerint fűzi be az egész számokat), így minden rendszeren ugyanazt adja.
#pragma once

#include <cstdint>
#include <string_view>

namespace totu::core {

class StateHash {
 public:
  void add_u64(std::uint64_t v);
  void add_i64(std::int64_t v) { add_u64(static_cast<std::uint64_t>(v)); }
  void add_bytes(std::string_view s);
  std::uint64_t value() const { return h_; }

 private:
  void byte(std::uint8_t b);
  std::uint64_t h_ = 1469598103934665603ULL;
};

}  // namespace totu::core
