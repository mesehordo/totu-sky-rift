// SPDX-License-Identifier: GPL-3.0-or-later
// Sprite-köteg (M5): példányos rajzolás – egy négyzet = egy példány (hely, méret, textúrakoordináták, árnyalat,
// csapatszín); a csúcsokat a shader a csúcssorszámból képzi, így ezer sprite is egyetlen rajzoló hívás, amíg a
// textúra nem változik. A textúrák tömbtextúrák (a réteg a példányban): egy meccs atlaszlapjai egyetlen tömbben, így a
// népek keverve, Y szerint rendezve is egy hívásban rajzolhatók. A maszk egycsatornás (R8). Csapatszín a maszk szerint („shade” mód, mint a meglévő grafikánál: a maszk alatti, szürkére
// festett rész a csapatszínnel szorzódik).
// A textúrák előszorzott alfájúak (premultiply).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "render/rhi.hpp"

namespace totu::render {

struct SpriteInstance {
  float x = 0, y = 0, w = 0, h = 0;           // képernyő- vagy világkoordináta (a vetítés dönt)
  float u0 = 0, v0 = 0, u1 = 1, v1 = 1;       // textúrakoordináták
  uint8_t tint[4] = {255, 255, 255, 255};      // árnyalat és átlátszóság
  uint8_t team[4] = {0, 0, 0, 0};              // csapatszín; a negyedik bájt az erősség (0 = nincs)
  uint8_t layer[4] = {0, 0, 0, 0};             // a tömbtextúra rétege (az első bájt); a többi tartalék
};
static_assert(sizeof(SpriteInstance) == 44, "a csúcsformátum 44 bájt");

// A kép előszorzása az alfával (a betöltés után, a feltöltés előtt).
void premultiply(Image& img);

class SpriteBatch {
 public:
  bool init(Device& dev, std::string* error);
  void shutdown();
  void begin(const Mat4& projection);
  // A maszk lehet nullptr (csapatszín nélkül). Textúraváltáskor a köteg kiürül (rajzol).
  void draw(const Texture& base, const Texture* mask, const SpriteInstance& s);   // tömbtextúrák
  void end();
  int draw_calls() const { return draw_calls_; }
  int sprites() const { return sprites_; }

 private:
  void flush();

  Device* dev_ = nullptr;
  Program prog_;
  gl::GLuint vao_ = 0, vbo_ = 0;
  gl::GLint loc_proj_ = -1, loc_base_ = -1, loc_mask_ = -1, loc_use_mask_ = -1;
  Mat4 proj_{};
  std::vector<SpriteInstance> pending_;
  gl::GLuint base_ = 0, mask_ = 0;
  int draw_calls_ = 0, sprites_ = 0;
  static constexpr size_t kCapacity = 16384;
};

}  // namespace totu::render
