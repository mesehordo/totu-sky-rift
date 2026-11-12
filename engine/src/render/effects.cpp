// SPDX-License-Identifier: GPL-3.0-or-later
#include "render/effects.hpp"

#include <algorithm>
#include <cmath>

namespace totu::render {

namespace {

constexpr int kSize = 32;

// egyszerű, rácsponti értékzaj a füstfelhő foltosságához (rögzített, a képpontból számolt)
float hash2(int x, int y) {
  uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return static_cast<float>((h ^ (h >> 16)) & 0xffff) / 65535.0f;
}

float value_noise(float x, float y) {
  const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
  const float fx = x - static_cast<float>(ix), fy = y - static_cast<float>(iy);
  const float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
  const float a = hash2(ix, iy), b = hash2(ix + 1, iy), c = hash2(ix, iy + 1), d = hash2(ix + 1, iy + 1);
  return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
}

Image shape_image(ParticleShape s) {
  Image img;
  img.width = img.height = kSize;
  img.rgba.assign(static_cast<size_t>(kSize * kSize * 4), 0);
  const float r = kSize / 2.0f;
  for (int y = 0; y < kSize; ++y) {
    for (int x = 0; x < kSize; ++x) {
      const float dx = (static_cast<float>(x) + 0.5f - r) / r, dy = (static_cast<float>(y) + 0.5f - r) / r;
      const float d = std::sqrt(dx * dx + dy * dy);
      float a = 0;
      switch (s) {
        case ParticleShape::Glow: a = std::pow(std::clamp(1.0f - d, 0.0f, 1.0f), 1.6f); break;
        case ParticleShape::Puff: {
          const float n = value_noise(static_cast<float>(x) * 0.22f, static_cast<float>(y) * 0.22f) * 0.6f +
                          value_noise(static_cast<float>(x) * 0.5f + 7, static_cast<float>(y) * 0.5f) * 0.4f;
          a = std::clamp(1.0f - d * (1.15f - 0.4f * n), 0.0f, 1.0f);
          a = a * a * (0.65f + 0.35f * n);
          break;
        }
        case ParticleShape::Ring: a = std::clamp(1.0f - std::fabs(d - 0.75f) * 6.0f, 0.0f, 1.0f); break;
        case ParticleShape::Flake: a = std::clamp((1.0f - d) * 2.5f, 0.0f, 1.0f); break;
      }
      uint8_t* p = img.at(x, y);
      p[0] = p[1] = p[2] = 255;
      p[3] = static_cast<uint8_t>(a * 255.0f + 0.5f);
    }
  }
  premultiply(img);
  return img;
}

}  // namespace

bool Effects::init(Device& dev, std::string* error) {
  dev_ = &dev;
  tex_ = dev.create_array(kSize, kSize, 4, false);
  for (int i = 0; i < 4; ++i) dev.upload_layer(tex_, i, shape_image(static_cast<ParticleShape>(i)));
  dev.finish_array(tex_);
  return dev.check("effektek", error);
}

void Effects::shutdown() {
  if (dev_) dev_->destroy(tex_);
}

void Effects::emit(SpriteBatch& batch, const Particle& p, float grow, float alpha_scale) {
  const float t = std::clamp(p.age / p.life, 0.0f, 1.0f);
  const float size = (p.size0 + (p.size1 - p.size0) * t) * grow;
  // a halványulás: gyors felfutás, majd lassú lecsengés
  const float fade = std::min(1.0f, t * 12.0f) * (1.0f - t * t) * alpha_scale;
  SpriteInstance in;
  in.x = p.x - size * 0.5f;
  in.y = p.y - p.z - size * 0.5f;
  in.w = in.h = size;
  in.tint[0] = p.color[0];
  in.tint[1] = p.color[1];
  in.tint[2] = p.color[2];
  in.tint[3] = static_cast<uint8_t>(std::clamp(static_cast<float>(p.color[3]) * fade, 0.0f, 255.0f));
  in.layer[0] = static_cast<uint8_t>(p.shape);
  batch.draw(tex_, nullptr, in);
}

void Effects::draw_scene(SpriteBatch& batch, const Mat4& projection, const ParticleSystem& ps) {
  if (ps.particles().empty()) return;
  batch.begin(projection);
  for (const Particle& p : ps.particles()) {
    if (!p.additive) emit(batch, p, 1.0f, 1.0f);
  }
  batch.end();
  const gl::Api& gl = dev_->api();
  gl.BlendFuncSeparate(gl::ONE, gl::ONE, gl::ZERO, gl::ONE);   // izzók: összeadódnak, az alfa marad
  batch.begin(projection);
  for (const Particle& p : ps.particles()) {
    if (p.additive) emit(batch, p, 1.0f, 1.0f);
  }
  batch.end();
  gl.BlendFuncSeparate(gl::ONE, gl::ONE_MINUS_SRC_ALPHA, gl::ONE, gl::ONE_MINUS_SRC_ALPHA);
}

void Effects::draw_glow(SpriteBatch& batch, const Mat4& projection, const ParticleSystem& ps, float strength) {
  if (strength <= 0.01f) return;
  batch.begin(projection);
  for (const Particle& p : ps.particles()) {
    if (p.additive) emit(batch, p, 4.0f, 0.5f * strength);
  }
  batch.end();
}

}  // namespace totu::render
