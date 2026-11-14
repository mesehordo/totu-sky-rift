// SPDX-License-Identifier: GPL-3.0-or-later
// A játék közbeni HUD geometriája (M8; a régi `rts/hud.py` `_layout`/`hud_layout` pontos átvétele). Tiszta függvény,
// grafikus kártya nélkül számolható és tesztelhető.
//
// Alsó sáv balról jobbra (kötött sorrend):
//     minimap | fajbanner | karakterlap (1:1 portré + adatok) | tárgyak 2×3 (nem hősnél: fajcímer)
//     | kijelölt egységek 4×3 | fajbanner | parancsrács 4×3 (W E R T / S D F G / X C V B)
// Felül középen: [szövetségesek] [meteorit | fa | arany | létszám] [fogaskerék = menü]
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace totu::client {

// képernyő-téglalap egész képpontban (a pygame.Rect szerint: a jobb és az alsó él már kívül van)
struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
  int right() const { return x + w; }
  int bottom() const { return y + h; }
  int centerx() const { return x + w / 2; }
  int centery() const { return y + h / 2; }
  bool contains(float px, float py) const {
    return px >= static_cast<float>(x) && px < static_cast<float>(x + w) && py >= static_cast<float>(y) &&
           py < static_cast<float>(y + h);
  }
  Rect inflate(int dx, int dy) const { return {x - dx / 2, y - dy / 2, w + dx, h + dy}; }
  bool operator==(const Rect&) const = default;
};

struct HudLayout {
  double s = 1.0;                     // méretarány (1,0 = a referencia)
  int pad = 0, gap = 0, fp = 0, cell = 0, cg = 0;
  Rect hud;                           // az alsó sáv
  std::vector<Rect> frames;           // a keretezett szekciók külső kerete
  Rect minimap, banner1, char_sheet, inv, sel, banner2, cmd;
  Rect portrait, hpbar, mpbar, stats;
  int divider = 0;                    // a portré és az adatoszlop közti elválasztó x-e
  std::array<Rect, 6> inv_cells{};    // 2 × 3
  std::array<Rect, 12> sel_cells{};   // 4 × 3
  std::array<Rect, 12> cmd_cells{};   // 4 × 3
  Rect allies, resbar, gear, faith, topbar, idle;
  std::array<Rect, 4> res{};          // meteorit, fa, arany, létszám
  int quest_x = 0, quest_y = 0, title_x = 0, title_y = 0;
  struct Fonts { int name = 19, small = 14, tiny = 12, key = 13, res = 16; } font;
};

// A HUD összes téglalapja az adott képernyőméretre (16:9 a referencia; keskenyebb ablakon kicsinyít).
HudLayout hud_layout(int sw, int sh);

// a parancsrács billentyűi (4 oszlop × 3 sor), és a betű → hely (-1: nem parancsbillentyű)
inline constexpr char kSlotKeys[] = "WERTSDFGXCVB";
int key_slot(int key);
// a felső sáv nyersanyag-sorrendje: meteorit, fa, arany (a negyedik mező a létszám)
inline constexpr std::array<int, 3> kResOrder = {2, 0, 1};   // sim::kMeteor, sim::kWood, sim::kGold

// a Python 3 `round()`-ja (a felezőnél páros felé) – a régi geometria pontos átvételéhez
int py_round(double v);

}  // namespace totu::client
