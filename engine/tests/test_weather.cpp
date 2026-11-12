// SPDX-License-Identifier: GPL-3.0-or-later
// Az időjárás látványa és a részecskék (M5, 5. lépés; D35) – grafikus kártya nélkül.
#include <cmath>

#include "check.hpp"
#include "render/particles.hpp"
#include "render/weather_look.hpp"

namespace tr = totu::render;

TEST_CASE("időjárás: nevek oda-vissza; a derült ég és a nulla erősség semleges") {
  for (int i = 0; i < static_cast<int>(tr::Weather::Count); ++i) {
    tr::Weather w{};
    CHECK(tr::weather_from_name(tr::weather_name(static_cast<tr::Weather>(i)), &w));
    CHECK(static_cast<int>(w) == i);
  }
  tr::Weather w{};
  CHECK(!tr::weather_from_name("napfogyatkozas", &w));
  for (int i = 0; i < static_cast<int>(tr::Weather::Count); ++i) {
    const auto l = tr::weather_look(static_cast<tr::Weather>(i), 0.0f);
    CHECK(l.ambient[0] == 1.0f && l.ambient[1] == 1.0f && l.ambient[2] == 1.0f);
    CHECK(l.density == 0.0f && l.haze_amount == 0.0f && l.shadow == 1.0f && l.saturation == 1.0f);
  }
  const auto clear = tr::weather_look(tr::Weather::Clear, 1.0f);
  CHECK(clear.precip == tr::Precip::None && clear.lightning == 0.0f && clear.ambient[0] == 1.0f);
}

TEST_CASE("időjárás: a fajták jellege (eső sötétít és esik, vihar villámlik, köd párás, homok vízszintes)") {
  const auto rain = tr::weather_look(tr::Weather::Rain, 1.0f);
  CHECK(rain.precip == tr::Precip::Rain && rain.density > 0.3f && rain.length > 0 && rain.splashes > 0);
  CHECK(rain.ambient[0] < 0.9f && rain.saturation < 1.0f && rain.shadow < 0.5f);
  const auto storm = tr::weather_look(tr::Weather::Storm, 1.0f);
  CHECK(storm.lightning > 0 && storm.ambient[0] < rain.ambient[0] && storm.density > rain.density);
  CHECK(tr::weather_look(tr::Weather::Fog, 1.0f).haze_amount > 0.4f);
  const auto sand = tr::weather_look(tr::Weather::Sandstorm, 1.0f);
  CHECK(sand.precip == tr::Precip::Sand && std::fabs(sand.fall_x) > std::fabs(sand.fall_y));
  CHECK(sand.haze[0] > sand.haze[2]);   // meleg, sárgás pára
  const auto snow = tr::weather_look(tr::Weather::Snow, 1.0f);
  CHECK(snow.precip == tr::Precip::Snow && snow.length == 0.0f && snow.speed < rain.speed);
  CHECK(tr::weather_look(tr::Weather::Heat, 1.0f).heat > 0.5f);
  CHECK(tr::weather_look(tr::Weather::Ashfall, 1.0f).precip == tr::Precip::Ash);
  // az irány mindig egységvektor; enyhe = félúton
  for (int i = 0; i < static_cast<int>(tr::Weather::Count); ++i) {
    const auto l = tr::weather_look(static_cast<tr::Weather>(i), 0.5f);
    CHECK(std::fabs(std::hypot(l.fall_x, l.fall_y) - 1.0f) < 1e-4f);
  }
  const auto mild = tr::weather_look(tr::Weather::Rain, 0.5f);
  CHECK(std::fabs(mild.density - rain.density * 0.5f) < 1e-4f && mild.precip == tr::Precip::Rain);
}

TEST_CASE("időjárás: az átmenet folytonos, a csapadék fajtaváltáskor a nullán megy át") {
  const auto a = tr::weather_look(tr::Weather::Snow, 1.0f), b = tr::weather_look(tr::Weather::Storm, 1.0f);
  float worst = 0;
  auto prev = tr::mix(a, b, 0.0f);
  CHECK(prev.precip == tr::Precip::Snow && prev.density == a.density);
  for (int i = 1; i <= 1000; ++i) {
    const auto m = tr::mix(a, b, static_cast<float>(i) / 1000.0f);
    for (int c = 0; c < 3; ++c) worst = std::max(worst, std::fabs(m.ambient[c] - prev.ambient[c]));
    worst = std::max(worst, std::fabs(m.density - prev.density));
    prev = m;
  }
  CHECK(worst < 0.01f);
  CHECK(prev.precip == tr::Precip::Rain && prev.density == b.density);
  CHECK(tr::mix(a, b, 0.5f).density < 0.01f);
}

TEST_CASE("villám: csak viharban, rövid, megismételhető, és a gyakoriság nagyjából stimmel") {
  CHECK(tr::lightning_flash(12.3, 0.0f) == 0.0f);
  int strikes = 0;
  float peak = 0;
  bool was = false;
  for (int i = 0; i < 600 * 60; ++i) {   // 10 perc, 60 minta / s
    const double t = static_cast<double>(i) / 60.0;
    const float f = tr::lightning_flash(t, 5.0f);
    CHECK(f >= 0.0f && f <= 1.0f);
    CHECK(f == tr::lightning_flash(t, 5.0f));
    peak = std::max(peak, f);
    const bool on = f > 0.75f;   // az utóvillanás csúcsa ~0,63: villámonként egyszer számol
    if (on && !was) ++strikes;
    was = on;
  }
  CHECK(peak > 0.9f);
  CHECK(strikes >= 30 && strikes <= 70);   // 5 / perc × 10 perc ≈ 50
}

TEST_CASE("részecskék: kibocsátás, öregedés, kapacitás, megismételhetőség") {
  tr::ParticleSystem ps(64);
  ps.emit(tr::Effect::Hit, 100, 100);
  CHECK(ps.particles().size() == 4);
  for (const auto& p : ps.particles()) CHECK(p.additive && p.z > 0);
  ps.emit(tr::Effect::Death, 200, 100);
  CHECK(ps.particles().size() == 11);
  // a szikrák lehullanak, és fél másodperc alatt elfogynak; a porfelhő tovább él
  for (int i = 0; i < 30; ++i) ps.update(1.0f / 60.0f);
  int sparks = 0;
  for (const auto& p : ps.particles()) {
    CHECK(p.z >= 0.0f);
    if (p.additive) ++sparks;
  }
  CHECK(sparks == 0);
  CHECK(ps.particles().size() == 7);
  for (int i = 0; i < 120; ++i) ps.update(1.0f / 60.0f);
  CHECK(ps.particles().empty());
  // betelt készlet: az új részecske elmarad
  for (int i = 0; i < 20; ++i) ps.emit(tr::Effect::Hit, 0, 0);
  CHECK(ps.particles().size() == 64 && ps.dropped() == 16);
  // ugyanazzal a maggal ugyanaz a látvány
  tr::ParticleSystem a(256, 7), b(256, 7);
  a.emit(tr::Effect::Ember, 5, 5, 10);
  b.emit(tr::Effect::Ember, 5, 5, 10);
  a.update(0.1f, 20);
  b.update(0.1f, 20);
  CHECK(a.particles().size() == b.particles().size());
  for (size_t i = 0; i < a.particles().size(); ++i) {
    CHECK(a.particles()[i].x == b.particles()[i].x && a.particles()[i].z == b.particles()[i].z);
  }
  // a parázs fölfelé száll
  for (const auto& p : a.particles()) CHECK(p.z > 0.0f);
}
