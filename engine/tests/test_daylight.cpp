// SPDX-License-Identifier: GPL-3.0-or-later
// A napszak megjelenítési értékeinek tesztje (M5, 4. lépés; D35), grafikus kártya nélkül.
#include <cmath>

#include "check.hpp"
#include "render/daylight.hpp"

namespace tr = totu::render;   // BSD/macOS: a <time.h> globális `daylight` változója miatt minősítve

TEST_CASE("napszak: dél fehér fény és rövid, lefelé mutató árnyék; reggel balra, este jobbra dől") {
  const auto noon = tr::daylight(0.29);
  CHECK(noon.ambient[0] > 0.99f && noon.ambient[1] > 0.97f && noon.ambient[2] > 0.94f);
  CHECK(noon.lights == 0.0f);
  CHECK(std::fabs(noon.shadow_x) < 0.1f && noon.shadow_y > 0.2f && noon.shadow_y < 0.4f);
  CHECK(noon.shadow_alpha > 0.55f);
  const auto morning = tr::daylight(0.10), evening = tr::daylight(0.48);
  CHECK(morning.shadow_x < -0.3f && evening.shadow_x > 0.3f);   // a nap keletről nyugatra halad
  CHECK(std::hypot(morning.shadow_x, morning.shadow_y) > std::hypot(noon.shadow_x, noon.shadow_y));   // reggel hosszabb
  const auto night = tr::daylight(0.75);
  CHECK(night.lights == 1.0f && night.ambient[2] > night.ambient[0] && night.ambient[0] < 0.5f);   // kékes, sötét
  CHECK(night.shadow_alpha < 0.25f);
  const auto dusk = tr::daylight(0.52);
  CHECK(dusk.ambient[0] > dusk.ambient[2]);   // naplemente: meleg tónus
}

TEST_CASE("napszak: a fény folytonos, és a nap végén körbeér") {
  float worst = 0;
  auto prev = tr::daylight(0.0);
  for (int i = 1; i <= 2000; ++i) {
    const auto d = tr::daylight(static_cast<double>(i) / 2000.0);
    for (int c = 0; c < 3; ++c) worst = std::max(worst, std::fabs(d.ambient[c] - prev.ambient[c]));
    prev = d;
  }
  CHECK(worst < 0.03f);   // nincs ugrás (1/2000 napnyi lépésenként)
  const auto a = tr::daylight(0.0), b = tr::daylight(1.0), c = tr::daylight(2.25), d = tr::daylight(0.25);
  CHECK(a.ambient[0] == b.ambient[0] && c.ambient[1] == d.ambient[1]);
}
