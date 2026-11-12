// SPDX-License-Identifier: GPL-3.0-or-later
// A determinisztikus matematika tesztjei (ADR-004): fixpontos műveletek, gyök, szögfüggvények, véletlenszám,
// tizedes beolvasás – és egy rögzített hash, amelynek minden rendszeren egyeznie kell (M2 kapu, első rész).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "check.hpp"
#include "core/angle.hpp"
#include "core/decimal.hpp"
#include "core/fixed.hpp"
#include "core/hash.hpp"
#include "core/rng.hpp"

using namespace totu::core;

namespace {
Fixed F(double v) { return Fixed::from_raw(static_cast<int32_t>(std::lround(v * 65536.0))); }   // csak tesztadathoz
bool near(Fixed a, double b, double eps) { return std::fabs(a.to_double() - b) <= eps; }
}  // namespace

TEST_CASE("fixpontos: alapműveletek") {
  CHECK(Fixed::from_int(3).raw() == 3 * 65536);
  CHECK((Fixed::from_int(3) + Fixed::from_int(4)) == Fixed::from_int(7));
  CHECK((Fixed::from_int(3) * Fixed::from_int(-4)) == Fixed::from_int(-12));
  CHECK((Fixed::from_int(7) / Fixed::from_int(2)).raw() == 7 * 32768);
  CHECK((Fixed::from_int(-7) / Fixed::from_int(2)).raw() == -7 * 32768);
  CHECK(Fixed::from_ratio(1, 3).raw() == 21845);       // 0,33333 → 21845,33 → 21845
  CHECK(Fixed::from_ratio(2, 3).raw() == 43691);       // 43690,67 → 43691
  CHECK(Fixed::from_ratio(-1, 2).raw() == -32768);
  CHECK((Fixed::max() + Fixed::from_int(1)) == Fixed::max());   // telítés
  CHECK((Fixed::from_int(1) / Fixed{}) == Fixed::max());        // nullával osztás
}

TEST_CASE("fixpontos: egész gyök pontos") {
  CHECK(isqrt64(0) == 0);
  CHECK(isqrt64(15) == 3);
  CHECK(isqrt64(16) == 4);
  CHECK(isqrt64(0xFFFFFFFFFFFFFFFFull) == 0xFFFFFFFFull);
  CHECK(isqrt64(0xFFFFFFFE00000001ull) == 0xFFFFFFFFull);       // (2^32-1)^2
  CHECK(isqrt64(0xFFFFFFFE00000000ull) == 0xFFFFFFFEull);
  Pcg32 r(7, 1);
  bool ok = true;
  for (int i = 0; i < 20000; ++i) {
    const uint64_t v = (uint64_t{r.next()} << 32) | r.next();
    const uint64_t q = isqrt64(v);
    ok &= q * q <= v && (q == 0xFFFFFFFFull || (q + 1) * (q + 1) > v);
  }
  CHECK(ok);
  CHECK(sqrt(Fixed::from_int(16)) == Fixed::from_int(4));
  CHECK(near(sqrt(F(2.0)), 1.41421356, 1.0 / 65536));
  CHECK(length(Fixed::from_int(3), Fixed::from_int(-4)) == Fixed::from_int(5));
}

TEST_CASE("szögek: szinusz, koszinusz, atan2 pontossága") {
  CHECK(sin(Angle::from_degrees(0)) == Fixed{});
  CHECK(sin(Angle::from_degrees(90)) == Fixed::from_int(1));
  CHECK(sin(Angle::from_degrees(270)) == Fixed::from_int(-1));
  CHECK(cos(Angle::from_degrees(180)) == Fixed::from_int(-1));
  double worst = 0;
  for (int u = 0; u < Angle::kFullTurn; u += 7) {
    const double a = u * 2 * 3.14159265358979323846 / Angle::kFullTurn;
    worst = std::fmax(worst, std::fabs(sin(Angle::from_units(u)).to_double() - std::sin(a)));
    worst = std::fmax(worst, std::fabs(cos(Angle::from_units(u)).to_double() - std::cos(a)));
  }
  CHECK(worst <= 2.0 / 65536);                        // ≤ 2 egység a Q16.16 utolsó helyén
  CHECK(atan2(Fixed{}, Fixed::from_int(1)).units() == 0);
  CHECK(atan2(Fixed::from_int(1), Fixed{}).units() == 16384);
  CHECK(atan2(Fixed{}, Fixed::from_int(-1)).units() == 32768);
  CHECK(atan2(Fixed::from_int(-1), Fixed{}).units() == 49152);
  int worst_units = 0;
  Pcg32 r(11, 2);
  for (int i = 0; i < 20000; ++i) {
    const Fixed y = Fixed::from_raw(r.range(-9000000, 9000000)), x = Fixed::from_raw(r.range(-9000000, 9000000));
    if (x == Fixed{} && y == Fixed{}) continue;
    const double ref = std::atan2(y.to_double(), x.to_double()) / (2 * 3.14159265358979323846) * Angle::kFullTurn;
    int d = static_cast<int>(std::lround(ref)) - atan2(y, x).units();
    d = ((d % 65536) + 65536 + 32768) % 65536 - 32768;
    worst_units = std::max(worst_units, std::abs(d));
  }
  CHECK(worst_units <= 1);                             // ≤ 1 egység (0,0055°)
  CHECK(direction8(Angle::from_degrees(0)) == 0);
  CHECK(direction8(Angle::from_degrees(90)) == 2);
  CHECK(direction8(Angle::from_degrees(350)) == 0);
}

TEST_CASE("véletlenszám: PCG32 referenciasorozat és torzítatlanság") {
  // a PCG referencia-implementáció (pcg32_srandom(42, 54)) első kimenetei
  Pcg32 r(42, 54);
  const uint32_t expect[6] = {0xa15c02b7u, 0x7b47f409u, 0xba1d3330u, 0x83d2f293u, 0xbfa4784bu, 0xcbed606eu};
  bool ok = true;
  for (uint32_t e : expect) ok &= r.next() == e;
  CHECK(ok);
  Pcg32 s(1, 1);
  int hist[6] = {0, 0, 0, 0, 0, 0};
  for (int i = 0; i < 60000; ++i) ++hist[s.below(6)];
  bool fair = true;
  for (int h : hist) fair &= h > 9500 && h < 10500;
  CHECK(fair);
  CHECK(s.range(5, 5) == 5);
}

TEST_CASE("tizedes beolvasás: pontos és determinisztikus") {
  CHECK(parse_fixed("2.7")->raw() == 176947);          // 176947,2
  CHECK(parse_fixed("0.5")->raw() == 32768);
  CHECK(parse_fixed("-0.35")->raw() == -22938);        // 22937,6 → 22938
  CHECK(parse_fixed("1e-3")->raw() == 66);             // 65,536
  CHECK(parse_fixed("2.5E2") == Fixed::from_int(250));
  CHECK(parse_fixed("32767.99999")->raw() == 2147483647);
  CHECK(parse_fixed("-32768") == Fixed::min());
  CHECK(parse_fixed("0.30000000000000004")->raw() == 19661);   // 12 jegyig számít
  CHECK(!parse_fixed("32768"));
  CHECK(!parse_fixed(""));
  CHECK(!parse_fixed("1.2.3"));
  CHECK(!parse_fixed("abc"));
  CHECK(!parse_fixed("1e"));
}

// M2 kapu (első rész): egy rögzített számítássorozat hash-e. Ha bármelyik rendszeren eltér, a matematika nem
// platformfüggetlen. Az értéket a Windows-os első futás adta; a CI mind az öt rendszeren ellenőrzi.
TEST_CASE("determinizmus: rögzített hash minden rendszeren") {
  StateHash h;
  Pcg32 r(20261112, 7);
  for (int i = 0; i < 50000; ++i) {
    const Fixed a = Fixed::from_raw(r.range(-2000000000, 2000000000));
    const Fixed b = Fixed::from_raw(r.range(-3000000, 3000000));
    h.add_i64((a * b).raw());
    h.add_i64((a / (b == Fixed{} ? Fixed::from_int(1) : b)).raw());
    h.add_i64(sqrt(a).raw());
    h.add_i64(length(a, b).raw());
    const Angle ang = Angle::from_units(static_cast<int32_t>(r.next() & 0xFFFF));
    h.add_i64(sin(ang).raw());
    h.add_i64(cos(ang).raw());
    h.add_i64(atan2(b, Fixed::from_raw(r.range(-3000000, 3000000))).units());
    h.add_u64(r.below(1000));
  }
  const char* nums[] = {"2.7", "-0.35", "1e-3", "123.456789", "0.1", "-32767.5", "0.999999999999"};
  for (const char* n : nums) h.add_i64(parse_fixed(n)->raw());
  std::printf("  determinizmus-hash: 0x%016llx\n", static_cast<unsigned long long>(h.value()));
  CHECK(h.value() == 0xf77842dd87d0cb56ull);
}
