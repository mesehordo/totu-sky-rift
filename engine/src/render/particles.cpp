// SPDX-License-Identifier: GPL-3.0-or-later
#include "render/particles.hpp"

#include <algorithm>
#include <cmath>

namespace totu::render {

ParticleSystem::ParticleSystem(size_t capacity, uint64_t seed) : capacity_(capacity), state_(seed ? seed : 1) {
  live_.reserve(capacity);
}

float ParticleSystem::rnd() {
  // xorshift64* – a látványhoz elég, és gépenként megismételhető
  state_ ^= state_ >> 12;
  state_ ^= state_ << 25;
  state_ ^= state_ >> 27;
  return static_cast<float>((state_ * 0x2545F4914F6CDD1DULL) >> 40) / 16777216.0f;
}

bool ParticleSystem::add(const Particle& p) {
  if (live_.size() >= capacity_) {
    ++dropped_;
    return false;
  }
  live_.push_back(p);
  ++emitted_;
  return true;
}

void ParticleSystem::emit(Effect e, float x, float y, int count) {
  constexpr float kTau = 6.2831853f;
  switch (e) {
    case Effect::Hit: {
      const int n = count < 0 ? 4 : count;
      for (int i = 0; i < n; ++i) {
        Particle p;
        const float a = range(0, kTau), s = range(40, 110);
        p.x = x + range(-3, 3);
        p.y = y + range(-2, 2);
        p.z = range(10, 22);
        p.vx = std::cos(a) * s;
        p.vy = std::sin(a) * s * 0.5f;
        p.vz = range(30, 90);
        p.gravity = 260;
        p.drag = 2.5f;
        p.life = range(0.22f, 0.45f);
        p.size0 = range(3.0f, 5.0f);
        p.size1 = 1.0f;
        p.color[0] = 255; p.color[1] = static_cast<uint8_t>(range(170, 230)); p.color[2] = 90; p.color[3] = 255;
        p.shape = ParticleShape::Glow;
        p.additive = true;
        add(p);
      }
      break;
    }
    case Effect::Death: {
      const int n = count < 0 ? 7 : count;
      for (int i = 0; i < n; ++i) {
        Particle p;
        const float a = range(0, kTau), s = range(8, 28);
        p.x = x + std::cos(a) * range(0, 8);
        p.y = y + std::sin(a) * range(0, 5);
        p.z = range(0, 6);
        p.vx = std::cos(a) * s;
        p.vy = std::sin(a) * s * 0.6f;
        p.vz = range(6, 20);
        p.drag = 1.6f;
        p.life = range(0.8f, 1.4f);
        p.size0 = range(10, 16);
        p.size1 = range(26, 38);
        const auto g = static_cast<uint8_t>(range(120, 160));
        p.color[0] = static_cast<uint8_t>(g + 12); p.color[1] = g; p.color[2] = static_cast<uint8_t>(g - 22);
        p.color[3] = 150;
        p.shape = ParticleShape::Puff;
        add(p);
      }
      break;
    }
    case Effect::Ember: {
      const int n = count < 0 ? 1 : count;
      for (int i = 0; i < n; ++i) {
        Particle p;
        p.x = x + range(-3, 3);
        p.y = y;
        p.z = range(0, 4);
        p.vx = range(-8, 8);
        p.vz = range(18, 40);
        p.gravity = -6;   // a meleg levegő viszi fölfelé
        p.drag = 0.6f;
        p.life = range(0.7f, 1.6f);
        p.size0 = range(2.5f, 4.0f);
        p.size1 = 1.0f;
        p.color[0] = 255; p.color[1] = static_cast<uint8_t>(range(120, 190)); p.color[2] = 50; p.color[3] = 255;
        p.shape = ParticleShape::Glow;
        p.additive = true;
        add(p);
      }
      break;
    }
    case Effect::Splash: {
      const int n = count < 0 ? 1 : count;
      for (int i = 0; i < n; ++i) {
        Particle p;
        p.x = x;
        p.y = y;
        p.life = range(0.22f, 0.36f);
        p.size0 = 2;
        p.size1 = range(9, 14);
        p.color[0] = 200; p.color[1] = 215; p.color[2] = 235; p.color[3] = 150;
        p.shape = ParticleShape::Ring;
        add(p);
      }
      break;
    }
    case Effect::Smoke: {
      const int n = count < 0 ? 1 : count;
      for (int i = 0; i < n; ++i) {
        Particle p;
        p.x = x + range(-4, 4);
        p.y = y;
        p.z = range(0, 6);
        p.vx = range(-5, 5);
        p.vz = range(14, 26);
        p.drag = 0.3f;
        p.life = range(1.8f, 3.0f);
        p.size0 = range(8, 12);
        p.size1 = range(30, 44);
        const auto g = static_cast<uint8_t>(range(55, 80));
        p.color[0] = g; p.color[1] = g; p.color[2] = static_cast<uint8_t>(g + 6); p.color[3] = 140;
        p.shape = ParticleShape::Puff;
        add(p);
      }
      break;
    }
  }
}

void ParticleSystem::update(float dt, float wind_x) {
  if (dt <= 0) return;
  size_t out = 0;
  for (size_t i = 0; i < live_.size(); ++i) {
    Particle p = live_[i];
    p.age += dt;
    if (p.age >= p.life) continue;
    const float k = std::max(0.0f, 1.0f - p.drag * dt);
    p.vx = p.vx * k + wind_x * dt * (p.shape == ParticleShape::Puff ? 0.6f : 0.15f);
    p.vy *= k;
    p.vz = p.vz * k - p.gravity * dt;
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    p.z += p.vz * dt;
    if (p.z < 0) {   // a talajon: megáll (a szikra nem pattog vissza – rövid életű)
      p.z = 0;
      p.vz = 0;
      p.vx *= 0.5f;
      p.vy *= 0.5f;
    }
    live_[out++] = p;
  }
  live_.resize(out);
}

}  // namespace totu::render
