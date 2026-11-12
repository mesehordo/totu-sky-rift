// SPDX-License-Identifier: GPL-3.0-or-later
// Sprite-lap (M5): a meglévő grafika leírása (`assets/sprites/<fajta>/<id>.json`) – képkockaméret, horgonypont,
// irányok (sorok), animációk (oszloptartományok), csapatszín-maszk, HD-arány. A betöltő a képet (és a maszkot)
// előszorozza, és ha nagyobb, mint amit a kártya elbír, felezi.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "render/rhi.hpp"

namespace totu::render {

struct Animation {
  int start = 0, frames = 1, fps = 8;
  bool loop = true;
};

struct SpriteSheet {
  std::string id;
  Texture base, mask;           // a maszk lehet üres (id = 0)
  int frame_w = 0, frame_h = 0; // a textúrában (felezés után)
  float anchor_x = 0, anchor_y = 0;
  float scale = 1;              // a sprite-lap felbontása a játékbeli mérethez képest (HD = 2; felezéskor osztva)
  int directions = 1;
  std::map<std::string, Animation> anims;

  // A képkocka textúrakoordinátái (irány: 0 = K, majd az óramutató szerint 45°-onként).
  void uv(int direction, int column, float& u0, float& v0, float& u1, float& v1) const;
  const Animation* anim(const std::string& name) const;
};

// A leírás és a kép(ek) betöltése; a textúrák a megadott eszközön jönnek létre.
std::optional<SpriteSheet> load_sprite_sheet(Device& dev, const std::string& json_path, std::string* error);

}  // namespace totu::render
