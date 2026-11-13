// SPDX-License-Identifier: GPL-3.0-or-later
// Fényréteg és köd (M5, 4. lépés): a jelenet egy képernyőméretű render-célpontba rajzolódik, a fények egy fél
// felbontású fénycélpontba (a napszak környezeti színe + a fényforrások összeadva: lámpás, fáklya, Égkő-fény …); az
// összetétel a kettőt összeszorozza, és a köd (ismeretlen / felderített / látható, csempénként, simítva) sötétít.
// Utófeldolgozás ugyanebben a menetben (M5, 5. lépés; D35): vándorló felhőárnyék, foltos pára (köd, homok, hamu),
// hőremegés, csapadék (eső- és homokcsík, hó- és hamupehely, a kamerához rögzített, rétegenként kissé eltérő
// mélységgel), villám, színkorrekció (telítettség, kontraszt) és vignetta. Kikapcsolva a kép a 4. lépésével azonos.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "render/rhi.hpp"
#include "render/sprite_batch.hpp"
#include "render/weather_look.hpp"

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
  // 1b. árnyékmaszk (fél felbontás, maximum-keveréssel: az egymást fedő árnyékok – az épület oszlopcsíkjai, a
  //     tömeg – nem sötétednek egymásra, és a kicsinyítés puhít); utána `apply_shadows` sötétíti vele a terepet,
  //     és visszaállítja a jelenetet célpontnak
  void begin_shadow_mask();
  void apply_shadows();
  // 2. a fények: a környezeti szín, majd a fényforrások összeadva (a sprite-köteggel, világ-vetítéssel)
  //    `extra`: további összeadódó fények (pl. izzó részecskék), az összeadó keverés mellett hívva
  void draw_lights(SpriteBatch& batch, const Mat4& projection, const float ambient[3], const std::vector<Light>& lights,
                   float strength, const std::function<void()>& extra = {});
  // 3. a köd csempénként (0 ismeretlen, 128 felderített, 255 látható); üres = nincs köd
  void set_fog(int map_w, int map_h, const std::vector<uint8_t>& fog);
  // 4. összetétel az ablakba: a jelenet × fény, a köddel; a kamera a köd világkoordinátáihoz kell
  //    `post` hamis: utófeldolgozás nélkül; `flash`: a villám felvillanása (0..1)
  void composite(int screen_w, int screen_h, float cam_x, float cam_y, float zoom, float tile_px,
                 const WeatherLook& weather = {}, double time = 0, float flash = 0, bool post = true);

 private:
  Device* dev_ = nullptr;
  Program prog_;
  gl::GLuint vao_ = 0;
  gl::GLint loc_scene_ = -1, loc_light_ = -1, loc_fog_ = -1, loc_use_fog_ = -1, loc_cam_ = -1, loc_screen_ = -1,
            loc_fogsize_ = -1, loc_post_ = -1, loc_grade_ = -1, loc_haze_ = -1, loc_fx_ = -1, loc_precip_ = -1,
            loc_pcolor_ = -1, loc_pdir_ = -1;
  RenderTarget scene_, light_, shadow_;
  Program shadow_prog_;
  gl::GLint loc_shadow_ = -1;
  Texture glow_;         // sugaras fényfolt (a fényforrásokhoz)
  Texture fog_;
  int w_ = 0, h_ = 0, fog_w_ = 0, fog_h_ = 0;
  bool use_fog_ = false;
};

}  // namespace totu::render
