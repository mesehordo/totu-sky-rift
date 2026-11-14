// SPDX-License-Identifier: GPL-3.0-or-later
// A játék közbeni HUD rajzolása (M8; D42, D44): a régi `rts/hud.py` kinézete a motor saját rajzolójával – alsó sáv
// (minimap | fajbanner | karakterlap | tárgyak 2×3 / fajcímer | kijelölés 4×3 | fajbanner | parancsrács 4×3), felső
// sáv középen ([szövetségesek] [meteorit | fa | arany | létszám] [fogaskerék]), küldetéslista, óra, tétlen munkások,
// irányválasztó ablak, súgó, a meccs vége. A betű a régi játéké (Rováskönyv Text). Az állapot, a nézetmodell és a
// bemenet a GL nélküli `HudModel`-ben van (client/hud_model.hpp); ez a réteg csak rajzol.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "client/hud_model.hpp"
#include "client/world.hpp"
#include "render/font.hpp"

namespace totu::client {

// a rajzoláshoz kellő képkocka-adatok
struct HudFrame {
  int w = 0, h = 0;                 // képernyő (képpont)
  float mx = -1, my = -1;           // egér
  double t = 0;                     // a felület órája (másodperc)
  float cam_x = 0, cam_y = 0, cam_w = 0, cam_h = 0;   // a látott világrész (világ-képpont) a minimaphez
};

class Hud {
 public:
  bool init(render::Device& dev, const std::string& assets_dir, std::string* error);
  HudModel& model() { return model_; }
  // képkockánként a bemenet és a rajzolás előtt: a pillanatkép, a szabályok és a kijelölés átvétele
  void sync(World& world, Session& session, int w, int h);
  // képernyő-koordinátában (a hívó a képernyő vetítésével kezdi a köteget)
  void draw(render::SpriteBatch& batch, const render::Texture& white, const HudFrame& f);
  // szöveg (a hívónak is): a bal felső sarok (x, y), betűméret (képpont), szín
  void text(render::SpriteBatch& batch, int size, float x, float y, const std::string& s, uint8_t r, uint8_t g,
            uint8_t b, uint8_t a = 255);

 private:
  struct Font {
    render::FontBake bake;
    render::Texture tex;
    bool ok = false;
  };
  struct Img {
    render::Texture tex;
    int w = 0, h = 0;
    bool ok = false;
  };
  using Col = std::array<uint8_t, 3>;
  Font& font(int size);
  const Img* image(const std::string& rel);                         // assets/<rel>.png (nincs: nullptr)
  const Img* portrait(const std::string& id, Col team);             // portré a csapatszín-maszkkal
  // rajzoló alapelemek
  void rect(float x, float y, float w, float h, Col c, uint8_t a = 255);
  void rect(const Rect& r, Col c, uint8_t a = 255) { rect(float(r.x), float(r.y), float(r.w), float(r.h), c, a); }
  void outline(const Rect& r, Col c, int t = 1, uint8_t a = 255);
  void line(float x0, float y0, float x1, float y1, Col c, float t = 1);
  void polygon(const std::vector<std::pair<float, float>>& pts, Col c, uint8_t a = 255);
  void blit(const Img& im, float x, float y, float w, float h, Col tint = {255, 255, 255}, uint8_t a = 255);
  void blit_fit(const Img& im, const Rect& r, bool crop_square = true);
  float tw(int size, const std::string& s);
  float line_h(int size);
  void stext(int size, float x, float y, const std::string& s, Col c);   // árnyékolt, háttér nélküli
  float wrap(const std::string& s, float x, float y, float w, int size, Col c, int max_lines);
  std::string ellipsize(const std::string& s, int size, float w);
  void corner_label(const Rect& r, const std::string& s, Col c, int size, const char* where = "br");
  void well(const Rect& r, Col fill = {14, 14, 16});
  void bevel(const Rect& r, Col fill);
  void section_frame(const Rect& f);
  void clock_wipe(const Rect& r, float prog);
  void hbar(float x, float y, float w, float h, float frac, Col c, const std::string& label);
  void def_icon(uint16_t def, const Rect& r, Col team);
  void research_icon(int research, const Rect& r);
  void cmd_icon(const HudSlot& sl, const Rect& r);
  void type_icon(const std::string& key, const Rect& r);
  // részek
  void draw_bottom(const HudFrame& f);
  void draw_minimap(const HudFrame& f);
  void draw_char_sheet();
  void draw_effects_row(const SnapUnit& e, float x, float& y, float w, float bottom);
  float draw_counter_row(const SnapUnit& e, const std::string& key, float x, float bottom, float w);
  void draw_middle();
  void draw_sel_grid();
  void draw_cmd_grid();
  void draw_topbar(const HudFrame& f);
  void draw_allies_panel();
  void draw_quests();
  void draw_clock(const HudFrame& f);
  void draw_tooltip(const HudFrame& f);
  void draw_path_choice(const HudFrame& f);
  void draw_result(const HudFrame& f);
  struct Theme { Col base, light, dark, accent, fill; };
  Theme theme() const;

  HudModel model_;
  render::Device* dev_ = nullptr;
  std::string assets_;
  std::vector<uint8_t> ttf_;
  float em_ratio_ = 1.0f;           // pygame-méret (em) → stb képpontmagasság
  std::map<int, Font> fonts_;
  std::map<std::string, Img> images_;
  render::SpriteBatch* batch_ = nullptr;
  const render::Texture* white_ = nullptr;
  double t_ = 0;
  float mx_ = -1, my_ = -1;
  std::string fid_;                 // a játékos népe (a kinézethez)
  int map_w_ = 1, map_h_ = 1;
  bool ready_ = false;
};

}  // namespace totu::client
