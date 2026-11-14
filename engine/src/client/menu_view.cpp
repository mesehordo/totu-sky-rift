// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/menu_view.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <random>
#include <vector>

#include "render/font.hpp"
#include "render/image.hpp"

namespace totu::client {

namespace {

using Rgba = std::array<uint8_t, 4>;
// a régi `rts/menu.py` színei
constexpr Rgba kText{232, 228, 214, 255}, kGold{255, 214, 120, 255}, kDim{168, 164, 154, 255}, kFrame{150, 120, 70, 255},
    kFrameDark{58, 44, 28, 255}, kWarn{255, 150, 110, 255}, kLight{255, 240, 200, 255};

Rgba shade(Rgba c, float k) {
  for (int i = 0; i < 3; ++i) c[static_cast<size_t>(i)] = static_cast<uint8_t>(std::clamp(c[static_cast<size_t>(i)] * k, 0.0f, 255.0f));
  return c;
}
Rgba alpha(Rgba c, int a) {
  c[3] = static_cast<uint8_t>(std::clamp(a, 0, 255));
  return c;
}

std::vector<uint8_t> read_bytes(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

class MenuView {
 public:
  explicit MenuView(render::Device& dev) : dev_(dev) {}
  ~MenuView() {
    for (auto& f : fonts_) dev_.destroy(f.tex);
    for (auto& [k, t] : bg_) dev_.destroy(t);
    dev_.destroy(logo_);
    dev_.destroy(white_);
  }

  bool init(const std::string& assets, std::string* error) {
    assets_ = assets;
    const auto fonts = std::filesystem::path(assets) / "fonts";
    text_ttf_ = read_bytes(fonts / "egszakadas_rovaskonyv_text.ttf");
    display_ttf_ = read_bytes(fonts / "egszakadas_rovaskonyv.ttf");
    if (display_ttf_.empty()) display_ttf_ = text_ttf_;
    if (text_ttf_.empty()) {
      if (error) *error = "a menü betűje nem tölthető be (assets/fonts)";
      return false;
    }
    render::Image px;
    px.width = px.height = 1;
    px.rgba = {255, 255, 255, 255};
    white_ = dev_.create_array_from(px, false);
    if (auto img = render::load_png((std::filesystem::path(assets) / "ui" / "logo.png").string())) {
      render::premultiply(*img);
      logo_ = dev_.create_array_from(*img, false);
    }
    // a főmenü háttere: véletlenszerűen az egyik festett kép (assets/ui/bg/menu/*.png), mint a régi játékban
    std::vector<std::filesystem::path> pics;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(std::filesystem::path(assets) / "ui" / "bg" / "menu", ec)) {
      if (e.path().extension() == ".png") pics.push_back(e.path());
    }
    std::sort(pics.begin(), pics.end());
    if (!pics.empty()) main_bg_ = pics[std::random_device{}() % pics.size()].string();
    return true;
  }

  float logo_aspect() const { return logo_.height > 0 ? static_cast<float>(logo_.width) / static_cast<float>(logo_.height) : 0; }

  void draw(render::SpriteBatch& batch, const Menu& m, int w, int h, float mx, float my, double t) {
    bake(m.scale());
    W_ = static_cast<float>(w);
    H_ = static_cast<float>(h);
    u_ = m.scale();
    batch.begin(render::ortho(0, W_, H_, 0));
    background(batch, m.screen(), t);
    if (m.screen() == MenuScreen::Main) sidebar(batch, m, mx, my);
    for (const MenuPanel& p : m.panels()) panel(batch, p.r, p.alpha, p.gold_top);
    for (const MenuText& tx : m.texts()) label(batch, tx);
    const MenuWidget* open = m.open_dropdown();
    for (const MenuWidget& wd : m.widgets()) widget(batch, wd, mx, my, open == &wd);
    if (open) dropdown_list(batch, *open, m.dropdown_items(), mx, my);
    batch.end();
  }

 private:
  struct Font {
    render::FontBake bake;
    render::Texture tex;
  };

  float k(float v) const { return std::round(v * u_); }

  void bake(float u) {
    if (std::abs(u - baked_u_) < 0.01f) return;
    baked_u_ = u;
    static const float sizes[5] = {14, 16, 18, 26, 54};   // a régi MENU betűméretek 1280×720-on
    for (size_t i = 0; i < 5; ++i) {
      dev_.destroy(fonts_[i].tex);
      fonts_[i].bake = {};
      const auto& ttf = i >= 3 ? display_ttf_ : text_ttf_;   // a nagyobb címek a díszesebb betűvel
      if (render::bake_font(ttf, std::max(8.0f, std::round(sizes[i] * u)), fonts_[i].bake)) {
        fonts_[i].tex = dev_.create_array_from(fonts_[i].bake.atlas, false);
      }
    }
  }

  void rect(render::SpriteBatch& b, float x, float y, float w, float h, Rgba c) {
    if (w <= 0 || h <= 0) return;
    render::SpriteInstance in;
    in.x = x; in.y = y; in.w = w; in.h = h;
    for (size_t i = 0; i < 4; ++i) in.tint[i] = c[i];
    b.draw(white_, nullptr, in);
  }
  void frame(render::SpriteBatch& b, const MenuRect& r, Rgba c, float t = 1) {
    rect(b, r.x, r.y, r.w, t, c);
    rect(b, r.x, r.y + r.h - t, r.w, t, c);
    rect(b, r.x, r.y, t, r.h, c);
    rect(b, r.x + r.w - t, r.y, t, r.h, c);
  }
  void diamond(render::SpriteBatch& b, float cx, float cy, float d, Rgba c) {
    for (float dy = -d; dy <= d; dy += 1) {
      const float hw = d - std::abs(dy);
      rect(b, cx - hw, cy + dy, 2 * hw + 1, 1, c);
    }
  }
  // háromszög (lefelé vagy jobbra mutató), soronként
  void triangle(render::SpriteBatch& b, float cx, float cy, float d, bool right, Rgba c) {
    for (float i = 0; i <= d; i += 1) {
      const float len = 2 * (d - i) + 1;
      if (right) rect(b, cx - d / 2 + i, cy - len / 2, 1, len, c);
      else rect(b, cx - len / 2, cy - d / 2 + i, len, 1, c);
    }
  }

  // --- szöveg ---
  float width(int f, const std::string& s) const { return fonts_[static_cast<size_t>(f)].bake.width(s); }
  float line(int f) const { return fonts_[static_cast<size_t>(f)].bake.line; }
  void put(render::SpriteBatch& b, int f, float x, float y, const std::string& s, Rgba c) {
    const Font& ft = fonts_[static_cast<size_t>(f)];
    if (!ft.tex.id) return;
    float pen = x;
    const float base = y + ft.bake.ascent;
    for (const uint32_t cp : render::utf8_codepoints(s)) {
      const render::Glyph* gl = ft.bake.find(cp);
      if (!gl) gl = ft.bake.find('?');
      if (!gl) continue;
      if (gl->x1 > gl->x0) {
        render::SpriteInstance in;
        in.x = std::round(pen) + gl->x0; in.y = std::round(base) + gl->y0; in.w = gl->x1 - gl->x0; in.h = gl->y1 - gl->y0;
        in.u0 = gl->u0; in.v0 = gl->v0; in.u1 = gl->u1; in.v1 = gl->v1;
        for (size_t i = 0; i < 4; ++i) in.tint[i] = c[i];
        b.draw(ft.tex, nullptr, in);
      }
      pen += gl->advance;
    }
  }
  // árnyékolt felirat (a régi `label`)
  void shadowed(render::SpriteBatch& b, int f, float x, float y, const std::string& s, Rgba c, bool shadow = true) {
    if (shadow) put(b, f, x + std::max(1.0f, k(2)), y + std::max(1.0f, k(2)), s, Rgba{8, 6, 10, c[3]});
    put(b, f, x, y, s, c);
  }
  // a szélességre: a hosszabb szöveg kisebb betűre vált (legfeljebb `lowest`-ig), végül „...”-ra rövidül
  std::pair<int, std::string> fit(int f, int lowest, const std::string& s, float maxw) const {
    for (int i = f; i >= lowest; --i) {
      if (width(i, s) <= maxw) return {i, s};
    }
    std::string t = s;
    while (!t.empty() && width(lowest, t + "...") > maxw) {
      t.pop_back();
      while (!t.empty() && (static_cast<uint8_t>(t.back()) & 0xC0) == 0x80) t.pop_back();   // UTF-8 folytatóbájt
      if (!t.empty() && (static_cast<uint8_t>(t.back()) & 0xC0) == 0xC0) t.pop_back();
    }
    return {lowest, t + "..."};
  }
  std::vector<std::string> wrap(int f, const std::string& s, float w) const {
    std::vector<std::string> lines;
    std::string cur, word;
    auto push_word = [&] {
      if (word.empty()) return;
      const std::string c = cur.empty() ? word : cur + " " + word;
      if (!cur.empty() && width(f, c) > w) {
        lines.push_back(cur);
        cur = word;
      } else {
        cur = c;
      }
      word.clear();
    };
    for (const char ch : s) {
      if (ch == ' ' || ch == '\n') {
        push_word();
        if (ch == '\n') {
          lines.push_back(cur);
          cur.clear();
        }
      } else {
        word += ch;
      }
    }
    push_word();
    if (!cur.empty()) lines.push_back(cur);
    return lines;
  }

  Rgba color(MenuText::Color c) const {
    switch (c) {
      case MenuText::Gold: return kGold;
      case MenuText::Dim: return kDim;
      case MenuText::Warn: return kWarn;
      case MenuText::Light: return kLight;
      default: return kText;
    }
  }

  void label(render::SpriteBatch& b, const MenuText& t) {
    if (t.title) {   // a régi `Screen.title`: két árnyék, arany betű
      const auto [f, s] = fit(MenuText::Huge, MenuText::Big, t.text, t.w);
      const float x = t.x - width(f, s) / 2;
      put(b, f, x + k(4), t.y + k(4), s, Rgba{40, 20, 10, 255});
      put(b, f, x + k(2), t.y + k(2), s, Rgba{120, 70, 30, 255});
      put(b, f, x, t.y, s, kGold);
      return;
    }
    if (t.max_lines > 1) {
      auto lines = wrap(t.font, t.text, t.w);
      if (static_cast<int>(lines.size()) > t.max_lines) {
        lines.resize(static_cast<size_t>(t.max_lines));
        lines.back() = fit(t.font, t.font, lines.back() + " ...", t.w).second;
      }
      float y = t.y;
      for (const auto& ln : lines) {
        const float x = t.center ? t.x - width(t.font, ln) / 2 : t.x;
        shadowed(b, t.font, x, y, ln, color(t.color));
        y += line(t.font);
      }
      return;
    }
    const auto [f, s] = fit(t.font, std::max(0, t.font - 2), t.text, t.w);
    const float x = t.center ? t.x - width(f, s) / 2 : t.x;
    shadowed(b, f, x, t.y, s, color(t.color));
  }

  // --- háttér, oldalsáv, panel ---
  void background(render::SpriteBatch& b, MenuScreen s, double t) {
    const std::string path = s == MenuScreen::Main       ? main_bg_
                             : s == MenuScreen::Skirmish ? (std::filesystem::path(assets_) / "ui" / "bg" / "setup.png").string()
                                                         : (std::filesystem::path(assets_) / "ui" / "bg" / "options.png").string();
    auto it = bg_.find(path);
    if (it == bg_.end()) {
      render::Texture tex;
      if (auto img = path.empty() ? std::nullopt : render::load_png(path)) {
        if (img->width <= dev_.max_texture_size() && img->height <= dev_.max_texture_size()) tex = dev_.create_array_from(*img, false);
      }
      it = bg_.emplace(path, tex).first;
    }
    const render::Texture& tex = it->second;
    if (!tex.id) {   // kép nélkül: éjszakai égbolt-átmenet
      for (int i = 0; i < 32; ++i) {
        const float g = static_cast<float>(i) / 31.0f;
        rect(b, 0, H_ * g, W_, H_ / 32 + 1,
             Rgba{static_cast<uint8_t>(24 + 20 * g), static_cast<uint8_t>(20 + 12 * g), static_cast<uint8_t>(36 - 10 * g), 255});
      }
      return;
    }
    // „kitöltve”, 8%-kal nagyobban, lassan úszva (a régi `Background`)
    constexpr float kPan = 1.08f;
    const float sc = std::max(W_ * kPan / static_cast<float>(tex.width), H_ * kPan / static_cast<float>(tex.height));
    const float iw = static_cast<float>(tex.width) * sc, ih = static_cast<float>(tex.height) * sc;
    const float ph = static_cast<float>(t) * 0.05f;
    render::SpriteInstance in;
    in.x = -(iw - W_) * (0.5f + 0.5f * std::sin(ph));
    in.y = -(ih - H_) * (0.5f + 0.5f * std::sin(ph * 0.7f + 1.0f));
    in.w = iw;
    in.h = ih;
    b.draw(tex, nullptr, in);
  }

  void ornament(render::SpriteBatch& b, float cx, float y, float w) {
    const float half = w / 2;
    for (int i = 0; i < 12; ++i) {   // középről kifelé halványuló vonal
      const float a = 1 - static_cast<float>(i) / 12.0f;
      const float x0 = half * static_cast<float>(i) / 12.0f, x1 = half * static_cast<float>(i + 1) / 12.0f;
      const Rgba c = alpha(kFrame, static_cast<int>(255 * a));
      rect(b, cx + x0, y, x1 - x0, 1, c);
      rect(b, cx - x1, y, x1 - x0, 1, c);
    }
    const float d = std::max(3.0f, std::round(5 * u_));
    diamond(b, cx, y, d + 1, kFrameDark);
    diamond(b, cx, y, d, kGold);
  }

  void sidebar(render::SpriteBatch& b, const Menu& m, float mx, float my) {
    const float sw = static_cast<float>(m.sidebar_width());
    for (int i = 0; i < 48; ++i) {   // függőleges átmenet: fent kicsit világosabb (átfedés nélküli sávok)
      const float g = 1 - static_cast<float>(i) / 48.0f;
      const float y0 = std::floor(H_ * static_cast<float>(i) / 48.0f), y1 = std::floor(H_ * static_cast<float>(i + 1) / 48.0f);
      rect(b, 0, y0, sw, y1 - y0,
           Rgba{static_cast<uint8_t>(16 + 14 * g), static_cast<uint8_t>(12 + 9 * g), static_cast<uint8_t>(18 + 10 * g), 222});
    }
    rect(b, sw - 5, 0, 1, H_, kFrameDark);
    rect(b, sw - 4, 0, 2, H_, kFrame);
    rect(b, sw - 1, 0, 1, H_, kFrameDark);
    for (float y = std::floor(H_ * 0.08f); y < H_; y += std::max(40.0f, std::floor(H_ / 9))) diamond(b, sw - 3, y, k(4), kGold);
    const MenuRect lg = m.logo_rect();
    float ly = k(14) + k(90);
    if (logo_.id && lg.w > 0) {
      render::SpriteInstance in;
      in.x = lg.x; in.y = lg.y; in.w = lg.w; in.h = lg.h;
      b.draw(logo_, nullptr, in);
      ly = lg.y + lg.h;
    }
    ornament(b, sw / 2, ly + k(32), sw - k(60));
    const float foot = H_ - k(70);
    ornament(b, sw / 2, foot - k(70), sw - k(60));
    for (const MenuWidget& w : m.widgets()) {   // súgó a kijelölt menüponthoz (az oldalsáv alján)
      if (w.kind == MenuWidget::Kind::Item && w.r.contains(mx, my) && !w.hint.empty()) {
        MenuText t;
        t.x = k(22);
        t.y = foot - k(58);
        t.w = sw - k(44);
        t.text = w.hint;
        t.font = MenuText::Tiny;
        t.max_lines = 3;
        t.color = MenuText::Light;
        label(b, t);
      }
    }
  }

  void panel(render::SpriteBatch& b, const MenuRect& r, int a, bool gold_top) {
    rect(b, r.x, r.y, r.w, r.h, Rgba{10, 9, 14, static_cast<uint8_t>(std::clamp(a, 0, 255))});
    frame(b, {r.x - 1, r.y - 1, r.w + 2, r.h + 2}, kFrameDark);
    frame(b, r, kFrame);
    if (gold_top) rect(b, r.x + 1, r.y + 1, r.w - 3, 1, kGold);
  }

  // --- widgetek ---
  void button(render::SpriteBatch& b, const MenuWidget& w, bool hot, int big) {
    const MenuRect& r = w.r;
    Rgba base = w.enabled ? Rgba{64, 52, 34, 255} : Rgba{38, 36, 40, 255};
    if (hot || w.selected) base = Rgba{124, 92, 44, 255};
    const float d = std::max(2.0f, std::floor(r.h / 16));
    rect(b, r.x + d, r.y + d, r.w, r.h, shade(base, 0.6f));
    rect(b, r.x, r.y, r.w, r.h, base);
    rect(b, r.x, r.y, r.w, 2, shade(base, 1.6f));
    rect(b, r.x, r.y, 2, r.h, shade(base, 1.6f));
    frame(b, r, hot || w.selected ? kGold : kFrame);
    int top = big;
    while (top > 0 && line(top) > r.h - 2) --top;
    const auto [f, s] = fit(top, std::max(0, top - 2), w.label, r.w - std::max(12.0f, r.h / 2));
    const Rgba c = w.enabled ? Rgba{255, 244, 214, 255} : Rgba{136, 132, 124, 255};
    shadowed(b, f, r.cx() - width(f, s) / 2, r.cy() - line(f) / 2, s, c, w.enabled);
  }

  void widget(render::SpriteBatch& b, const MenuWidget& w, float mx, float my, bool is_open) {
    const MenuRect& r = w.r;
    const bool hot = w.enabled && r.contains(mx, my);
    switch (w.kind) {
      case MenuWidget::Kind::Item: {   // a főmenü pontja: keret nélküli felirat, egérre aranyló sáv
        if (hot || w.selected) {
          for (int i = 0; i < 24; ++i) {
            const float x0 = r.w * static_cast<float>(i) / 24.0f, x1 = r.w * static_cast<float>(i + 1) / 24.0f;
            const float a = std::pow(1 - std::abs((x0 + x1) / r.w - 1), 0.6f);
            rect(b, r.x + x0, r.y + 1, x1 - x0, r.h - 2, Rgba{190, 140, 60, static_cast<uint8_t>(120 * a)});
            rect(b, r.x + x0, r.y, x1 - x0, 1, alpha(kGold, static_cast<int>(220 * a)));
            rect(b, r.x + x0, r.y + r.h - 1, x1 - x0, 1, alpha(kGold, static_cast<int>(220 * a)));
          }
        }
        const Rgba c = hot || w.selected ? kLight : w.enabled ? kText : Rgba{120, 116, 108, 255};
        const auto [f, s] = fit(MenuText::Big, MenuText::Small, w.label, r.w - r.h * 1.4f);
        shadowed(b, f, r.cx() - width(f, s) / 2, r.cy() - line(f) / 2, s, c);
        if (w.sub) triangle(b, r.x + r.w - r.h * 0.45f, r.cy(), std::max(4.0f, std::floor(r.h / 7)), true, hot || w.selected ? kGold : kFrame);
        if (!w.tag.empty()) put(b, MenuText::Tiny, r.x + r.w - width(MenuText::Tiny, w.tag) - k(6), r.y + r.h - line(MenuText::Tiny), w.tag, kDim);
        break;
      }
      case MenuWidget::Kind::Button: button(b, w, hot, w.small ? MenuText::Med : MenuText::Big); break;
      case MenuWidget::Kind::KeyCell: button(b, w, hot, MenuText::Huge); break;
      case MenuWidget::Kind::MapRow: {
        if (w.selected || hot) rect(b, r.x, r.y, r.w, r.h, w.selected ? Rgba{90, 70, 36, 255} : Rgba{50, 40, 26, 255});
        const auto [f, s] = fit(MenuText::Small, MenuText::Tiny, w.label, r.w - k(12));
        put(b, f, r.x + k(6), r.cy() - line(f) / 2, s, w.selected ? kGold : kText);
        break;
      }
      case MenuWidget::Kind::Dropdown: {
        rect(b, r.x, r.y, r.w, r.h, w.enabled ? Rgba{40, 34, 24, 255} : Rgba{30, 30, 32, 255});
        frame(b, r, hot || is_open ? kGold : Rgba{130, 104, 62, 255});
        const float pad = std::max(4.0f, std::round(6 * u_));
        float tx = r.x + pad;
        if (w.team >= 0) {   // a csapatszín négyzete
          const float sq = r.h - 2 * pad;
          const auto& c = team_colors(colorblind_)[static_cast<size_t>(w.team % 8)];
          rect(b, r.x + pad, r.y + pad, sq, sq, Rgba{c[0], c[1], c[2], 255});
          tx += sq + pad;
        }
        const float arrow = w.enabled ? std::round(16 * u_) : 0;
        const std::string lab = w.current >= 0 && static_cast<size_t>(w.current) < w.options.size() ? w.options[static_cast<size_t>(w.current)] : "";
        const auto [f, s] = fit(MenuText::Small, MenuText::Tiny, lab, r.x + r.w - arrow - tx - 2);
        put(b, f, tx, r.cy() - line(f) / 2, s, w.enabled ? kText : kDim);
        if (w.enabled) triangle(b, r.x + r.w - arrow / 2 - 1, r.cy(), std::max(3.0f, std::round(4 * u_)) * 2, false, kGold);
        break;
      }
      case MenuWidget::Kind::Slider: {
        const Rgba c = w.selected ? kGold : kDim;
        const float th = std::max(2.0f, k(6));
        rect(b, r.x, r.cy() - th / 2, r.w, th, kFrame);
        const float thumb = r.x + std::round((r.w - 1) * static_cast<float>(w.current) / 100.0f);
        const float lh = std::max(2.0f, k(4));
        rect(b, r.x, r.cy() - lh / 2, thumb - r.x, lh, c);
        const float d = std::max(3.0f, k(7));
        rect(b, thumb - d, r.cy() - d, 2 * d, 2 * d, c);
        break;
      }
    }
  }

  void dropdown_list(render::SpriteBatch& b, const MenuWidget& d, const std::vector<MenuRect>& items, float mx, float my) {
    for (size_t i = 0; i < items.size(); ++i) {
      const MenuRect& r = items[i];
      const bool hot = r.contains(mx, my);
      rect(b, r.x, r.y, r.w, r.h, hot ? Rgba{90, 70, 36, 255} : Rgba{28, 24, 18, 245});
      frame(b, r, Rgba{130, 104, 62, 255});
      const auto [f, s] = fit(MenuText::Small, MenuText::Tiny, d.options[i], r.w - k(12));
      put(b, f, r.x + k(6), r.cy() - line(f) / 2, s, static_cast<int>(i) == d.current ? kGold : kText);
    }
  }

 public:
  bool colorblind_ = false;

 private:
  render::Device& dev_;
  std::string assets_, main_bg_;
  std::vector<uint8_t> text_ttf_, display_ttf_;
  std::array<Font, 5> fonts_{};
  float baked_u_ = -1;
  float W_ = 1280, H_ = 720, u_ = 1;
  render::Texture white_, logo_;
  std::map<std::string, render::Texture> bg_;
};

void save(const MenuRunOptions& o, const Settings& st) {
  if (o.settings_file.empty()) return;
  std::string err;
  if (!save_settings(o.settings_file, st, &err)) std::fprintf(stderr, "%s\n", err.c_str());
}

}  // namespace

void apply_display(SDL_Window* win, const Settings& st) {
  SDL_SetWindowFullscreen(win, st.fullscreen);
  if (!st.fullscreen) SDL_SetWindowSize(win, st.width, st.height);
}

bool run_menu(SDL_Window* win, render::Device& dev, render::SpriteBatch& batch, Settings& st, Locale& loc,
              const MenuRunOptions& o, MatchConfig* out, int* rc) {
  MenuView view(dev);
  std::string err;
  if (!view.init(o.assets, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    if (rc) *rc = 1;
    return false;
  }
  Menu menu(st, loc, scan_maps(o.maps_dir), scan_factions(o.content));
  menu.set_logo_aspect(view.logo_aspect());
  if (o.start != MenuScreen::Main) menu.go(o.start);
  view.colorblind_ = st.colorblind;
  const bool fixed = !o.screenshot.empty();
  const auto start = std::chrono::steady_clock::now();
  for (long frame = 0; o.frames < 0 || frame < o.frames; ++frame) {
    int w = 0, h = 0, ww = 0, wh = 0;
    SDL_GetWindowSizeInPixels(win, &w, &h);
    SDL_GetWindowSize(win, &ww, &wh);
    const float px = ww > 0 ? static_cast<float>(w) / static_cast<float>(ww) : 1.0f;   // képpont / ablakpont
    menu.layout(w, h);
    SDL_Event ev;
    Menu::Result res = Menu::Result::None;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_EVENT_QUIT) return false;
      if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && (ev.button.button == SDL_BUTTON_LEFT || ev.button.button == SDL_BUTTON_RIGHT)) {
        res = menu.click(ev.button.x * px, ev.button.y * px, ev.button.button == SDL_BUTTON_RIGHT ? 3 : 1);
      } else if (ev.type == SDL_EVENT_MOUSE_MOTION && (ev.motion.state & SDL_BUTTON_LMASK)) {
        menu.drag(ev.motion.x * px, ev.motion.y * px);
      } else if (ev.type == SDL_EVENT_MOUSE_BUTTON_UP && ev.button.button == SDL_BUTTON_LEFT) {
        menu.release();
      } else if (ev.type == SDL_EVENT_KEY_DOWN) {
        const SDL_Keycode kc = ev.key.key;
        int key = 0;
        if (kc == SDLK_ESCAPE) key = 27;
        else if (kc == SDLK_RETURN || kc == SDLK_KP_ENTER) key = 13;
        else if (kc >= SDLK_A && kc <= SDLK_Z) key = 'A' + static_cast<int>(kc - SDLK_A);
        else if (kc >= SDLK_0 && kc <= SDLK_9) key = '0' + static_cast<int>(kc - SDLK_0);
        if (key) res = menu.key(key);
      }
      const uint32_t ch = menu.take_changes();
      if (ch & Menu::kSave) save(o, st);
      if (ch & Menu::kDisplay) apply_display(win, st);
      if (ch & Menu::kDisplay) save(o, st);
      if (ch & (Menu::kLanguage | Menu::kColors)) save(o, st);
      view.colorblind_ = st.colorblind;
      if (res == Menu::Result::Quit) return false;
      if (res == Menu::Result::Start) {
        const uint64_t seed = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        if (menu.match_config(seed, out)) return true;
      }
      menu.layout(w, h);
    }
    float mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);
    const double t = fixed ? static_cast<double>(frame) / 60.0
                           : std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    dev.bind_target(nullptr, w, h);
    dev.clear(0.05f, 0.04f, 0.06f, 1.0f);
    view.draw(batch, menu, w, h, mx * px, my * px, t);
    if (o.frames >= 0 && frame + 1 >= o.frames && !o.screenshot.empty()) {
      const render::Image shot = dev.read_pixels(w, h);
      if (!render::save_png(o.screenshot, shot)) {
        std::fprintf(stderr, "nem írható: %s\n", o.screenshot.c_str());
        if (rc) *rc = 1;
      } else {
        std::printf("képernyőkép: %s (%d × %d)\n", o.screenshot.c_str(), w, h);
      }
    }
    SDL_GL_SwapWindow(win);
  }
  return false;
}

}  // namespace totu::client
