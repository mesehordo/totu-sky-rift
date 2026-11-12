// SPDX-License-Identifier: GPL-3.0-or-later
// Részecskék (M5, 5. lépés): tisztán látvány – a kliens futtatja képkockánként, a szimulációt nem érinti (nem kell
// determinisztikusnak lennie a gépek között, de a saját véletlenszám-folyama miatt egy gépen megismételhető, így a
// képpróba azonos). Egy részecske: hely a talajon (x, y), magasság (z, a talaj fölött; a képen y − z), sebesség,
// gravitáció, közegellenállás, élettartam, méret- és színváltozás. Rögzített kapacitás (betelt készletnél az új
// részecske elmarad – a csata közepén sem nő a memória). Grafikus kártya nélkül tesztelhető; a rajzolás a
// `render/effects`-ben.
#pragma once

#include <cstdint>
#include <vector>

namespace totu::render {

enum class ParticleShape : uint8_t { Glow = 0, Puff = 1, Ring = 2, Flake = 3 };   // a részecske-textúra rétegei

struct Particle {
  float x = 0, y = 0, z = 0;          // világ-képpont; z: a talaj fölötti magasság
  float vx = 0, vy = 0, vz = 0;       // képpont / másodperc
  float gravity = 0, drag = 0;        // z irányú gyorsulás lefelé; sebességcsillapítás / s
  float age = 0, life = 1;            // másodperc
  float size0 = 4, size1 = 4;         // átmérő az élet elején és végén
  uint8_t color[4] = {255, 255, 255, 255};   // az alfa az élet végére 0-ra halványul
  ParticleShape shape = ParticleShape::Glow;
  bool additive = false;              // izzó részecske (szikra, parázs): összeadódik, és éjjel is világít
};

// Előre beállított effektek (a tartalom később adatból is adhat ilyet – M7, képességek).
enum class Effect : uint8_t {
  Hit,      // találat: szikrák
  Death,    // halál: porfelhő
  Ember,    // parázs a fáklyából
  Splash,   // esőcsepp a talajon
  Smoke,    // füst (égő épület, tűzvarázslat)
};

class ParticleSystem {
 public:
  explicit ParticleSystem(size_t capacity = 8192, uint64_t seed = 0x70745f7061727469ULL);
  void emit(Effect e, float x, float y, int count = -1);   // count < 0: az effekt alapmennyisége
  bool add(const Particle& p);                            // false: a készlet betelt
  void update(float dt, float wind_x = 0);                // az elöregedett részecskék kikerülnek
  void clear() { live_.clear(); }
  const std::vector<Particle>& particles() const { return live_; }
  size_t capacity() const { return capacity_; }
  size_t dropped() const { return dropped_; }
  size_t emitted() const { return emitted_; }

 private:
  float rnd();                         // [0, 1)
  float range(float a, float b) { return a + (b - a) * rnd(); }
  std::vector<Particle> live_;
  size_t capacity_;
  size_t dropped_ = 0, emitted_ = 0;
  uint64_t state_;
};

}  // namespace totu::render
