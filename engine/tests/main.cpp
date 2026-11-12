// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdio>

#include "check.hpp"
#include "core/platform_info.hpp"

int main() {
  using namespace totu;
  std::printf("TOTU-tesztek %s – %s/%s, %s\n", core::version(), core::os_name(), core::cpu_arch(),
              core::compiler().c_str());
  for (const auto& c : test::registry()) {
    const int before = test::failures();
    c.fn();
    std::printf("%s %s\n", test::failures() == before ? "  OK  " : "  HIBA", c.name);
  }
  std::printf("%zu teszt, %d hiba\n", test::registry().size(), test::failures());
  return test::failures() == 0 ? 0 : 1;
}
