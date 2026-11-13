// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdio>
#include <cstring>

#include "check.hpp"
#include "core/platform_info.hpp"

// Használat: totu_tests [szűrő] – a szűrő megadásakor csak az a teszt fut, amelynek nevében szerepel (fejlesztéshez);
// a CI szűrő nélkül futtat mindent.
int main(int argc, char** argv) {
  using namespace totu;
  const char* filter = argc > 1 ? argv[1] : nullptr;
  std::setvbuf(stdout, nullptr, _IONBF, 0);   // összeomláskor se vesszen el a kimenet
  std::printf("TOTU-tesztek %s – %s/%s, %s\n", core::version(), core::os_name(), core::cpu_arch(),
              core::compiler().c_str());
  size_t ran = 0;
  for (const auto& c : test::registry()) {
    if (filter && !std::strstr(c.name, filter)) continue;
    const int before = test::failures();
    c.fn();
    ++ran;
    std::printf("%s %s\n", test::failures() == before ? "  OK  " : "  HIBA", c.name);
  }
  std::printf("%zu teszt, %d hiba\n", ran, test::failures());
  return test::failures() == 0 ? 0 : 1;
}
