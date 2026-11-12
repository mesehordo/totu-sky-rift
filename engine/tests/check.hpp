// SPDX-License-Identifier: GPL-3.0-or-later
// Minimális tesztkeret külső függőség nélkül (minden rendszeren fordul).
#pragma once

#include <cstdio>
#include <functional>
#include <vector>

namespace totu::test {

struct Case {
  const char* name;
  std::function<void()> fn;
};

inline std::vector<Case>& registry() {
  static std::vector<Case> r;
  return r;
}

inline int& failures() {
  static int f = 0;
  return f;
}

struct Register {
  Register(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

}  // namespace totu::test

#define TOTU_CAT2(a, b) a##b
#define TOTU_CAT(a, b) TOTU_CAT2(a, b)
#define TEST_CASE(name)                                                                  \
  static void TOTU_CAT(test_fn_, __LINE__)();                                            \
  static ::totu::test::Register TOTU_CAT(test_reg_, __LINE__)(name, TOTU_CAT(test_fn_, __LINE__)); \
  static void TOTU_CAT(test_fn_, __LINE__)()

#define CHECK(expr)                                                                       \
  do {                                                                                    \
    if (!(expr)) {                                                                        \
      std::printf("  HIBA %s:%d: %s\n", __FILE__, __LINE__, #expr);                       \
      ++::totu::test::failures();                                                         \
    }                                                                                     \
  } while (0)
