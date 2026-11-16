// SPDX-License-Identifier: GPL-3.0-or-later
// A szerkesztő felületének logikája (M11, a WC3 World Editor mintájára), GL nélkül: eszközök (terep, erdő, egység,
// régió, pont, kamera, kijelölés), a bal oldali paletta (eszközgombok, ecsetméret, terepfajták, játékos, egységlista),
// alsó állapotsor, és az egér/billentyű kezelése. A műveleteket az EditorDoc-on végzi; a rajzolás külön (main).
#pragma once

#include <string>
#include <vector>

#include "client/editor_doc.hpp"
#include "client/hud_layout.hpp"

namespace totu::map {
class TerrainTable;
}
namespace totu::sim {
struct Rules;
}

namespace totu::client {

class EditorUi {
 public:
  enum class Tool : uint8_t { Select = 0, Terrain, Trees, Units, Regions, Points, Cameras, Count };
  static const char* tool_key(Tool t);   // a fordítási kulcs utótagja (editor.tool.<…>)

  void init(const map::TerrainTable& terrain, const sim::Rules& rules);
  // az elrendezés a képernyő méretéhez (minden képkockában)
  void layout(int w, int h);
  void sync(const EditorDoc& d);   // a játékosok népe a dokumentumból (az egységpalettához)

  // bemenet: képernyő-koordináta (sx, sy) és a hozzá tartozó világhely csempében (wx, wy); true: elfogyasztotta
  bool mouse_down(EditorDoc& d, float sx, float sy, double wx, double wy, bool right);
  void mouse_move(EditorDoc& d, float sx, float sy, double wx, double wy);
  void mouse_up(EditorDoc& d, double wx, double wy);
  // billentyű: ctrl+Z/Y visszavonás/újra, Del törlés, 1–7 eszköz, +/- ecset; true: elfogyasztotta
  bool key(EditorDoc& d, int key, bool ctrl);
  bool in_panel(float sx, float sy) const { return panel.contains(sx, sy); }

  // állapot (a rajzolónak)
  Tool tool = Tool::Terrain;
  int brush = 2;
  size_t terrain_index = 0;
  int owner = 1;
  size_t unit_index = 0;
  int selected = -1;                       // a kijelölt egység sorszáma
  bool dragging = false;                   // régió húzása vagy egység mozgatása
  double drag_x0 = 0, drag_y0 = 0, drag_x1 = 0, drag_y1 = 0;
  std::string status;                      // az utolsó művelet / hiba
  int revision = 0;                        // a dokumentum változásainak száma (az előnézet újraépítéséhez)
  float hover_sx = -1, hover_sy = -1;
  double hover_wx = -1, hover_wy = -1;

  struct Item {
    Rect r;
    std::string label;   // fordítási kulcs vagy név
    bool active = false;
  };
  Rect panel, status_bar;
  std::vector<Item> tool_buttons, palette;   // a paletta az eszköztől függ (terepek / egységek)
  std::vector<Item> owner_buttons;
  Rect brush_minus, brush_plus;
  std::vector<std::pair<char, std::string>> terrains;   // jel, név
  std::vector<std::pair<std::string, std::string>> units_of_owner() const;   // az aktuális játékos népének sablonjai

 private:
  void apply_brush(EditorDoc& d, double wx, double wy);
  void changed() { ++revision; }
  const sim::Rules* rules_ = nullptr;
  std::vector<std::string> owner_faction_;   // helyenként a nép (a dokumentumból, a layout frissíti)
  const EditorDoc* last_doc_ = nullptr;
  int scroll_ = 0;
};

}  // namespace totu::client
