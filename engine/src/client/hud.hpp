// SPDX-License-Identifier: GPL-3.0-or-later
// Ideiglenes HUD (M6, ADR-007 9. lépés; D37: az M8 RmlUi-felülete váltja le, ezért kinézeti munka nélkül): felül a
// nyersanyagok, a létszám, a napszak és az időjárás; alul a kijelölés (név, életerő, helyőrség), a termelési sor és a
// parancsgombok (képzés, kutatás, építés, képességek, kiszállás, főépület-képesség, fejlődési irány); a meccs végén a
// győzelem vagy a vereség. A betű a játék saját betűje (Rováskönyv Text, mint a régi játékban).
#pragma once

#include <string>
#include <vector>

#include "client/world.hpp"
#include "render/font.hpp"

namespace totu::client {

struct HudAction {
  enum Kind { None, Train, Research, Build, Ability, TargetAbility, Unload, Power, ChoosePath, Learn, UseItem } kind = None;
  std::string id;        // sablon, kutatás, képesség vagy irány
  uint16_t def = sim::kNoDef;
  sim::EntityId building = 0;
  int slot = 0;          // UseItem: a leltárhely
};

class Hud {
 public:
  bool init(render::Device& dev, const std::string& assets_dir, std::string* error);
  // képernyő-koordinátában (a hívó a képernyő vetítésével kezdi a köteget)
  void draw(render::SpriteBatch& batch, const render::Texture& white, World& world, Session& session, int w, int h);
  bool contains(float x, float y, int h) const;   // a HUD sávjai (a kattintás nem megy a világba)
  HudAction click(float x, float y) const;        // a legutóbb rajzolt gombok szerint
  // szöveg (a képpróbákhoz és a hívónak): a bal felső sarok (x, y), szín
  void text(render::SpriteBatch& batch, bool big, float x, float y, const std::string& s, uint8_t r, uint8_t g, uint8_t b,
            uint8_t a = 255) const;

  static constexpr float kTop = 26.0f, kBottom = 132.0f;

 private:
  struct Button {
    float x, y, w, h;
    std::string label;
    HudAction action;
  };
  render::FontBake small_, big_;
  render::Texture small_tex_, big_tex_;
  bool ready_ = false;
  std::vector<Button> buttons_;
};

}  // namespace totu::client
