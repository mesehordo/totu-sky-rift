// SPDX-License-Identifier: GPL-3.0-or-later
// ADR-004 mérés: lebegőpontos (double) és fixpontos (Q16.16, int64 köztes) szimulációs műveletek sebessége.
// A minta a szimuláció forró ciklusát utánozza: N egység, mindegyik a célja felé lép (távolság, normálás,
// lépés), és megnézi a közeli szomszédot (négyzetes távolság-összevetés). Fordítás:
//   g++ -O2 -std=c++20 -ffp-contract=off bench.cpp -o bench
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

constexpr int N = 2400;
constexpr int TICKS = 2000;

// --- lebegőpontos változat ---
struct UD { double x, y, tx, ty, sp; };

double run_double(std::vector<UD>& u) {
  double acc = 0;
  for (int t = 0; t < TICKS; ++t) {
    for (int i = 0; i < N; ++i) {
      UD& a = u[i];
      double dx = a.tx - a.x, dy = a.ty - a.y;
      double d = std::sqrt(dx * dx + dy * dy);
      if (d > 1e-9) { a.x += dx / d * a.sp; a.y += dy / d * a.sp; }
      const UD& b = u[(i * 7 + t) % N];
      double ex = b.x - a.x, ey = b.y - a.y;
      if (ex * ex + ey * ey < 0.64) acc += 1;
    }
  }
  return acc;
}

// --- fixpontos változat: Q16.16 csempe-egységben, int64 köztes értékkel ---
using fx = int32_t;
constexpr int FB = 16;
inline fx fmul(fx a, fx b) { return static_cast<fx>((static_cast<int64_t>(a) * b) >> FB); }
inline fx fdiv(fx a, fx b) { return static_cast<fx>((static_cast<int64_t>(a) << FB) / b); }
inline uint64_t isqrt64(uint64_t v) {       // egész négyzetgyök (bitenkénti, determinisztikus)
  uint64_t r = 0, bit = uint64_t{1} << 62;
  while (bit > v) bit >>= 2;
  while (bit) {
    if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; } else { r >>= 1; }
    bit >>= 2;
  }
  return r;
}
// Gyors, mégis pontos egész gyök: a hardveres (IEEE, pontosan kerekített) gyök közelítését egész korrekcióval
// a pontos alsó egészrészre igazítjuk – az eredmény a lebegőpontos kerekítéstől függetlenül minden gépen azonos.
inline uint64_t isqrt64_fast(uint64_t v) {
  uint64_t r = static_cast<uint64_t>(std::sqrt(static_cast<double>(v)));
  while (r * r > v) --r;
  while ((r + 1) * (r + 1) <= v) ++r;
  return r;
}
struct UF { fx x, y, tx, ty, sp; };

template <uint64_t (*SQ)(uint64_t)>
double run_fixed(std::vector<UF>& u) {
  double acc = 0;
  const int64_t near2 = static_cast<int64_t>(0.64 * (1 << FB)) << FB;   // 0,64 csempe² (Q32.32)
  for (int t = 0; t < TICKS; ++t) {
    for (int i = 0; i < N; ++i) {
      UF& a = u[i];
      int64_t dx = a.tx - a.x, dy = a.ty - a.y;
      fx d = static_cast<fx>(SQ(static_cast<uint64_t>(dx * dx + dy * dy)));   // Q32.32 → Q16.16
      if (d > 0) { a.x += fmul(fdiv(static_cast<fx>(dx), d), a.sp); a.y += fmul(fdiv(static_cast<fx>(dy), d), a.sp); }
      const UF& b = u[(i * 7 + t) % N];
      int64_t ex = b.x - a.x, ey = b.y - a.y;
      if (ex * ex + ey * ey < near2) acc += 1;
    }
  }
  return acc;
}

template <class F>
double timed(F f, double& out) {
  auto t0 = std::chrono::steady_clock::now();
  out = f();
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace

int main() {
  std::vector<UD> ud(N);
  std::vector<UF> uf(N);
  uint32_t s = 12345;
  auto rnd = [&s] { s = s * 1664525u + 1013904223u; return (s >> 8) / double(1 << 24); };
  for (int i = 0; i < N; ++i) {
    double x = rnd() * 144, y = rnd() * 144, tx = rnd() * 144, ty = rnd() * 144, sp = 0.05 + rnd() * 0.05;
    ud[i] = {x, y, tx, ty, sp};
    uf[i] = {fx(x * 65536), fx(y * 65536), fx(tx * 65536), fx(ty * 65536), fx(sp * 65536)};
  }
  std::vector<UF> uf2 = uf;
  double ad = 0, af = 0, af2 = 0;
  double td = timed([&] { return run_double(ud); }, ad);
  double tf = timed([&] { return run_fixed<isqrt64>(uf); }, af);
  double tf2 = timed([&] { return run_fixed<isqrt64_fast>(uf2); }, af2);
  bool same = true;
  for (int i = 0; i < N; ++i) same &= uf[i].x == uf2[i].x && uf[i].y == uf2[i].y;
  std::printf("{\"units\":%d,\"ticks\":%d,\"double_ms\":%.1f,\"fixed_ms\":%.1f,\"ratio\":%.2f,"
              "\"double_us_per_tick\":%.1f,\"fixed_us_per_tick\":%.1f,\"near_double\":%.0f,\"near_fixed\":%.0f,"
              "\"fixed_fast_ms\":%.1f,\"fast_ratio\":%.2f,\"fast_us_per_tick\":%.1f,\"fast_bit_identical\":%s}\n",
              N, TICKS, td, tf, tf / td, td * 1000 / TICKS, tf * 1000 / TICKS, ad, af, tf2, tf2 / td,
              tf2 * 1000 / TICKS, (same && af == af2) ? "true" : "false");
  return 0;
}
