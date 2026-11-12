// SPDX-License-Identifier: GPL-3.0-or-later
// Fényréteg és köd (M5, 4. lépés): a jelenet egy képernyőméretű render-célpontba rajzolódik, a fények egy fél
// felbontású fénycélpontba (a napszak környezeti színe + a fényforrások összeadva: lámpás, fáklya, Égkő-fény …); az
// összetétel a kettőt összeszorozza, és a köd (ismeretlen / felderített / látható, csempénként, simítva) sötétít.
#pragma once

#include <string>
#include <vector>

#include "render/rhi.hpp"
#include "render/sprite_batch.hpp"

namespace totu::render {

struct Light {
  float x = 0, y = 0, radius = 96;   // világ-képpontban
  uint8_t r = 255, g = 200, b = 120;
  float intensity = 1;
};

class Lighting {
 public:
  bool init(Device& dev, std::string* error);
  void shutdown();
  void resize(int w, int h);           // a képernyő mérete változott

  // 1. a jelenet: ide rajzol a terep, az árnyék és a sprite-ok
  void begin_scene(float r, float g, float b);
  // 2. a fények: a környezeti szín, majd a fényforrások összeadva (a sprite-köteggel, világ-vetítéssel)
  void draw_lights(SpriteBatch& batch, const Mat4& projection, const float ambient[3], const std::vector<Light>& lights,
                   float strength);
  // 3. a köd csempénként (0 ismeretlen, 128 felderített, 255 látható); üres = nincs köd
  void set_fog(int map_w, int map_h, const std::vector<uint8_t>& fog);
  // 4. összetétel az ablakba: a jelenet × fény, a köddel; a kamera a köd világkoordinátáihoz kell
  void composite(int screen_w, int screen_h, float cam_x, float cam_y, float zoom, float tile_px);

 private:
  Device* dev_ = nullptr;
  Program prog_;
  gl::GLuint vao_ = 0;
  gl::GLint loc_scene_ = -1, loc_light_ = -1, loc_fog_ = -1, loc_use_fog_ = -1, loc_cam_ = -1, loc_screen_ = -1,
            loc_fogsize_ = -1;
  RenderTarget scene_, light_;
  Texture glow_;         // sugaras fényfolt (a fényforrásokhoz)
  Texture fog_;
  int w_ = 0, h_ = 0, fog_w_ = 0, fog_h_ = 0;
  bool use_fog_ = false;
};

}  // namespace totu::render
