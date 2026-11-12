// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstring>

#include "check.hpp"
#include "core/hash.hpp"
#include "core/platform_info.hpp"

using totu::core::StateHash;

TEST_CASE("platform: a rendszer és az architektúra ismert") {
  CHECK(std::strcmp(totu::core::os_name(), "ismeretlen") != 0);
  CHECK(std::strcmp(totu::core::cpu_arch(), "ismeretlen") != 0);
  CHECK(std::strlen(totu::core::version()) > 0);
}

TEST_CASE("hash: üres állapot = FNV-1a alapérték") {
  StateHash h;
  CHECK(h.value() == 1469598103934665603ULL);
}

// Rögzített referenciaértékek: ha bármelyik rendszeren eltér, a hash nem platformfüggetlen.
TEST_CASE("hash: rögzített referencia minden rendszeren") {
  StateHash h;
  h.add_u64(0x0123456789ABCDEFULL);
  h.add_i64(-42);
  h.add_bytes("TOTU: Sky Rift – Égszakadás");
  CHECK(h.value() == 0x1fac0121db25f6a4ULL);
}

TEST_CASE("hash: a sorrend számít") {
  StateHash a, b;
  a.add_u64(1);
  a.add_u64(2);
  b.add_u64(2);
  b.add_u64(1);
  CHECK(a.value() != b.value());
}
