// SPDX-License-Identifier: GPL-3.0-or-later
#include "render/weather_look.hpp"

#include <algorithm>
#include <cmath>

namespace totu::render {

namespace {

constexpr const char* kNames[] = {"derult", "felhos", "eso", "zivatar", "kod",
                                  "ho",     "hovihar", "homokvihar", "hoseg", "hamueso"};

float lerp(float a, float b, float t) { return a + (b - a) * t; }

void set3(float* d, float a, float b, float c) {
  d[0] = a;
  d[1] = b;
  d[2] = c;
}

// a teljes erősségű látvány (intensity = 1)
WeatherLook full(Weather w) {
  WeatherLook l;
  switch (w) {
    case Weather::Clear:
    case Weather::Count:
      break;
    case Weather::Cloudy:
      set3(l.ambient, 0.86f, 0.88f, 0.92f);
      l.shadow = 0.45f;
      l.saturation = 0.9f;
      l.clouds = 0.22f;
      l.wind = 6;
      break;
    case Weather::Rain:
      set3(l.ambient, 0.70f, 0.74f, 0.82f);
      l.shadow = 0.25f;
      l.saturation = 0.78f;
      l.contrast = 0.95f;
      set3(l.haze, 0.55f, 0.6f, 0.68f);
      l.haze_amount = 0.12f;
      l.clouds = 0.15f;
      l.precip = Precip::Rain;
      l.density = 0.55f;
      l.speed = 900;
      l.length = 22;
      l.size = 1.0f;
      set3(l.color, 0.78f, 0.84f, 0.95f);
      l.color[3] = 0.32f;
      l.fall_x = 0.18f;
      l.splashes = 60;
      l.wind = 10;
      break;
    case Weather::Storm:
      set3(l.ambient, 0.52f, 0.56f, 0.68f);
      l.shadow = 0.1f;
      l.lights = 0.45f;
      l.saturation = 0.7f;
      l.contrast = 1.05f;
      set3(l.haze, 0.45f, 0.5f, 0.6f);
      l.haze_amount = 0.16f;
      l.clouds = 0.2f;
      l.precip = Precip::Rain;
      l.density = 0.8f;
      l.speed = 1200;
      l.length = 30;
      l.size = 1.2f;
      set3(l.color, 0.75f, 0.8f, 0.92f);
      l.color[3] = 0.4f;
      l.fall_x = 0.32f;
      l.lightning = 5;
      l.splashes = 110;
      l.wind = 24;
      break;
    case Weather::Fog:
      set3(l.ambient, 0.86f, 0.88f, 0.9f);
      l.shadow = 0.2f;
      l.saturation = 0.72f;
      l.contrast = 0.85f;
      set3(l.haze, 0.74f, 0.77f, 0.8f);
      l.haze_amount = 0.55f;
      l.wind = 3;
      break;
    case Weather::Snow:
      set3(l.ambient, 0.9f, 0.94f, 1.02f);
      l.shadow = 0.4f;
      l.saturation = 0.8f;
      set3(l.haze, 0.86f, 0.9f, 0.96f);
      l.haze_amount = 0.12f;
      l.precip = Precip::Snow;
      l.density = 0.5f;
      l.speed = 70;
      l.size = 1.8f;
      set3(l.color, 1.0f, 1.0f, 1.0f);
      l.color[3] = 0.85f;
      l.fall_x = 0.2f;
      l.wind = 6;
      break;
    case Weather::Blizzard:
      set3(l.ambient, 0.8f, 0.86f, 0.98f);
      l.shadow = 0.1f;
      l.saturation = 0.6f;
      l.contrast = 0.9f;
      set3(l.haze, 0.86f, 0.9f, 0.97f);
      l.haze_amount = 0.42f;
      l.precip = Precip::Snow;
      l.density = 0.85f;
      l.speed = 260;
      l.size = 1.6f;
      set3(l.color, 1.0f, 1.0f, 1.0f);
      l.color[3] = 0.9f;
      l.fall_x = 0.85f;
      l.wind = 40;
      break;
    case Weather::Sandstorm:
      set3(l.ambient, 0.98f, 0.86f, 0.68f);
      l.shadow = 0.2f;
      l.saturation = 0.75f;
      l.contrast = 0.92f;
      set3(l.haze, 0.82f, 0.64f, 0.4f);
      l.haze_amount = 0.5f;
      l.precip = Precip::Sand;
      l.density = 0.6f;
      l.speed = 700;
      l.length = 46;
      l.size = 1.0f;
      set3(l.color, 0.9f, 0.74f, 0.5f);
      l.color[3] = 0.28f;
      l.fall_x = 0.97f;
      l.fall_y = 0.24f;
      l.wind = 50;
      break;
    case Weather::Heat:
      set3(l.ambient, 1.06f, 1.0f, 0.9f);
      l.saturation = 1.08f;
      l.contrast = 1.04f;
      set3(l.haze, 0.95f, 0.88f, 0.72f);
      l.haze_amount = 0.08f;
      l.heat = 1.0f;
      break;
    case Weather::Ashfall:
      set3(l.ambient, 0.72f, 0.68f, 0.66f);
      l.shadow = 0.3f;
      l.saturation = 0.55f;
      l.contrast = 0.95f;
      set3(l.haze, 0.42f, 0.38f, 0.36f);
      l.haze_amount = 0.28f;
      l.precip = Precip::Ash;
      l.density = 0.45f;
      l.speed = 45;
      l.size = 1.5f;
      set3(l.color, 0.36f, 0.34f, 0.33f);
      l.color[3] = 0.8f;
      l.fall_x = 0.3f;
      l.wind = 5;
      break;
  }
  // az irány egységvektor
  const float n = std::hypot(l.fall_x, l.fall_y);
  l.fall_x /= n;
  l.fall_y /= n;
  return l;
}

}  // namespace

const char* weather_name(Weather w) {
  const auto i = static_cast<size_t>(w);
  return i < static_cast<size_t>(Weather::Count) ? kNames[i] : "?";
}

bool weather_from_name(std::string_view name, Weather* out) {
  for (size_t i = 0; i < static_cast<size_t>(Weather::Count); ++i) {
    if (name == kNames[i]) {
      *out = static_cast<Weather>(i);
      return true;
    }
  }
  return false;
}

WeatherLook mix(const WeatherLook& a, const WeatherLook& b, float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  WeatherLook r;
  for (int c = 0; c < 3; ++c) {
    r.ambient[c] = lerp(a.ambient[c], b.ambient[c], t);
    r.haze[c] = lerp(a.haze[c], b.haze[c], t);
  }
  r.shadow = lerp(a.shadow, b.shadow, t);
  r.lights = lerp(a.lights, b.lights, t);
  r.saturation = lerp(a.saturation, b.saturation, t);
  r.contrast = lerp(a.contrast, b.contrast, t);
  r.haze_amount = lerp(a.haze_amount, b.haze_amount, t);
  r.clouds = lerp(a.clouds, b.clouds, t);
  r.heat = lerp(a.heat, b.heat, t);
  r.lightning = lerp(a.lightning, b.lightning, t);
  r.splashes = lerp(a.splashes, b.splashes, t);
  r.wind = lerp(a.wind, b.wind, t);
  if (a.precip == b.precip) {
    const WeatherLook& s = t < 0.5f ? a : b;
    r.precip = s.precip;
    r.density = lerp(a.density, b.density, t);
    r.speed = lerp(a.speed, b.speed, t);
    r.length = lerp(a.length, b.length, t);
    r.size = lerp(a.size, b.size, t);
    for (int c = 0; c < 4; ++c) r.color[c] = lerp(a.color[c], b.color[c], t);
    r.fall_x = lerp(a.fall_x, b.fall_x, t);
    r.fall_y = lerp(a.fall_y, b.fall_y, t);
  } else {
    // a régi csapadék elfogy, majd az új indul: a sűrűség a felénél nulla
    const WeatherLook& s = t < 0.5f ? a : b;
    const float k = t < 0.5f ? 1.0f - 2.0f * t : 2.0f * t - 1.0f;
    r.precip = s.precip;
    r.density = s.density * k;
    r.speed = s.speed;
    r.length = s.length;
    r.size = s.size;
    for (int c = 0; c < 4; ++c) r.color[c] = s.color[c];
    r.fall_x = s.fall_x;
    r.fall_y = s.fall_y;
  }
  const float n = std::hypot(r.fall_x, r.fall_y);
  if (n > 1e-4f) {
    r.fall_x /= n;
    r.fall_y /= n;
  } else {
    r.fall_x = 0;
    r.fall_y = 1;
  }
  return r;
}

WeatherLook weather_look(Weather w, float intensity) {
  const float k = std::clamp(intensity, 0.0f, 1.0f);
  const WeatherLook f = full(w);
  if (f.precip == Precip::None) return mix(WeatherLook{}, f, k);
  // csapadéknál a fajta ne váltson a felénél: a derült ég „ugyanilyen, nulla sűrűségű” csapadék
  WeatherLook clear = WeatherLook{};
  clear.precip = f.precip;
  clear.speed = f.speed;
  clear.length = f.length;
  clear.size = f.size;
  for (int c = 0; c < 4; ++c) clear.color[c] = f.color[c];
  clear.fall_x = f.fall_x;
  clear.fall_y = f.fall_y;
  return mix(clear, f, k);
}

float lightning_flash(double time, float per_minute) {
  if (per_minute <= 0 || time < 0) return 0;
  // az időt résekre osztjuk; minden résben legfeljebb egy villám, véletlen időpontban, kettős felvillanással
  constexpr double kSlot = 2.0;   // másodperc
  const double slot = std::floor(time / kSlot);
  auto hash = [](uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return static_cast<double>(x >> 11) / 9007199254740992.0;
  };
  const auto id = static_cast<uint64_t>(static_cast<int64_t>(slot));
  const double chance = std::min(1.0, static_cast<double>(per_minute) * kSlot / 60.0);
  if (hash(id * 2 + 1) >= chance) return 0;
  const double at = slot * kSlot + hash(id * 2 + 2) * (kSlot - 0.6);
  const double d = time - at;
  if (d < 0 || d > 0.55) return 0;
  // két villanás: erős, majd egy gyengébb utóvillanás
  const double f1 = std::exp(-d * 22.0);
  const double f2 = d > 0.16 ? 0.6 * std::exp(-(d - 0.16) * 14.0) : 0.0;
  return static_cast<float>(std::min(1.0, f1 + f2));
}

}  // namespace totu::render
