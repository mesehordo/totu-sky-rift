// SPDX-License-Identifier: GPL-3.0-or-later
#include "render/daylight.hpp"

#include <algorithm>
#include <cmath>

namespace totu::render {

namespace {

struct Key {
  double t;
  float r, g, b, lights;
};

// a környezeti fény kulcsai (lineárisan, körbe)
const Key kKeys[] = {
    {0.00, 0.42f, 0.46f, 0.72f, 1.0f},   // a hajnal kezdete: még éjszakai kék
    {0.03, 0.95f, 0.72f, 0.66f, 0.4f},   // pirkadat
    {0.08, 1.00f, 1.00f, 1.00f, 0.0f},   // nappal
    {0.47, 1.00f, 0.98f, 0.95f, 0.0f},
    {0.52, 1.00f, 0.72f, 0.52f, 0.3f},   // naplemente
    {0.58, 0.40f, 0.44f, 0.70f, 1.0f},   // éjszaka (holdfény)
    {1.00, 0.42f, 0.46f, 0.72f, 1.0f},
};

constexpr double kPi = 3.14159265358979323846;

}  // namespace

DayLight daylight(double t) {
  t -= std::floor(t);
  DayLight d;
  for (size_t i = 0; i + 1 < sizeof kKeys / sizeof kKeys[0]; ++i) {
    const Key& a = kKeys[i];
    const Key& b = kKeys[i + 1];
    if (t < a.t || t > b.t) continue;
    const float f = static_cast<float>((t - a.t) / (b.t - a.t));
    d.ambient[0] = a.r + (b.r - a.r) * f;
    d.ambient[1] = a.g + (b.g - a.g) * f;
    d.ambient[2] = a.b + (b.b - a.b) * f;
    d.lights = a.lights + (b.lights - a.lights) * f;
    break;
  }
  // a nap: 0,02-től 0,56-ig az égen; délelőtt balra le, délben lefelé, délután jobbra le vet árnyékot
  const double rise = 0.02, set = 0.56;
  if (t > rise && t < set) {
    const double p = (t - rise) / (set - rise);                   // 0 napkelte, 1 napnyugta
    const double theta = (150.0 - 120.0 * p) * kPi / 180.0;       // 150° → 30° (a képernyőn, y lefelé)
    const double len = 1.5 - 0.95 * std::sin(kPi * p);            // délben rövid, reggel és este hosszú
    d.shadow_x = static_cast<float>(std::cos(theta) * len);
    d.shadow_y = static_cast<float>(std::sin(theta) * len * 0.6);
    const double edge = std::min(p, 1.0 - p);                     // napkeltekor és napnyugtakor halványuló
    d.shadow_alpha = static_cast<float>(0.59 * std::min(1.0, edge / 0.08));
  } else {   // holdfény: halvány, állandó irányú
    d.shadow_x = 0.3f;
    d.shadow_y = 0.25f;
    d.shadow_alpha = 0.18f;
  }
  return d;
}

}  // namespace totu::render
