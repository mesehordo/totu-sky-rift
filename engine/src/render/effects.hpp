// SPDX-License-Identifier: GPL-3.0-or-later
// Effektek rajzolása (M5, 5. lépés): a részecskék a sprite-köteggel, egyetlen, a kódból előállított négyrétegű
// textúrával (izzó pont, füstfelhő, gyűrű, pehely). A sima részecskék (por, füst, csepp) előszorzott alfával a
// jelenetbe kerülnek, az izzók (szikra, parázs) összeadódva – és a fényrétegbe is, így éjjel világítanak.
#pragma once

#include <string>

#include "render/particles.hpp"
#include "render/rhi.hpp"
#include "render/sprite_batch.hpp"

namespace totu::render {

class Effects {
 public:
  bool init(Device& dev, std::string* error);
  void shutdown();
  // a jelenetbe: előbb a sima, majd az izzó részecskék
  void draw_scene(SpriteBatch& batch, const Mat4& projection, const ParticleSystem& ps);
  // a fényrétegbe (összeadó keverés mellett hívandó): az izzó részecskék nagyobb, halvány fényfoltként
  void draw_glow(SpriteBatch& batch, const Mat4& projection, const ParticleSystem& ps, float strength);
  const Texture& texture() const { return tex_; }

 private:
  void emit(SpriteBatch& batch, const Particle& p, float grow, float alpha_scale);
  Device* dev_ = nullptr;
  Texture tex_;
};

}  // namespace totu::render
