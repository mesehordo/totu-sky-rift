// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/menu_view.hpp"
#include "client/diagnostics.hpp"
#include "client/savegame.hpp"

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

#include "content/loader.hpp"
#include "content/vfs.hpp"
#include "net/lan_discovery.hpp"
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

// a tippek száma (`ui.tip.1` …, a régi `tips.py`): az első hiányzóig
int tip_count(const Locale& loc) {
  int n = 0;
  while (n < 200 && loc.tr("ui.tip." + std::to_string(n + 1)) != "ui.tip." + std::to_string(n + 1)) ++n;
  return n;
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

  std::string loading_bg_;
  std::vector<std::array<float, 4>> meteors_;
  double meteor_t_ = 0;
  std::mt19937 meteor_rng_{std::random_device{}()};
  double blink_t_ = 0;
  float logo_aspect() const { return logo_.height > 0 ? static_cast<float>(logo_.width) / static_cast<float>(logo_.height) : 0; }

  // a régi `Loader`: a csata előtt véletlenszerűen az egyik tusrajzos térkép (assets/ui/bg/loading), alul sötét sáv a
  // felirattal, forgó rúnakörrel és egy tippel
  void draw_hint(render::SpriteBatch& batch, int w, int h, const std::string& s) {   // egy középre igazított sor alul (a nyitóképen)
    bake(std::max(1.0f, static_cast<float>(h) / 720.0f));
    W_ = static_cast<float>(w);
    H_ = static_cast<float>(h);
    u_ = std::max(1.0f, H_ / 720.0f);
    batch.begin(render::ortho(0, W_, H_, 0));
    shadowed(batch, MenuText::Med, (W_ - width(MenuText::Med, s)) / 2, H_ - line(MenuText::Med) * 3, s, Rgba{230, 220, 200, 255});
    batch.end();
  }

  void draw_lobby(render::SpriteBatch& batch, int w, int h, float u, const Locale& loc, const std::vector<LobbyLine>& seats,
                  const std::vector<std::string>& chat, const std::string& status, double t, const std::string* typing) {
    draw_loading(batch, w, h, u, status, loc.tr("ui.lobby.hint"), t);   // a háttér, a forgó rúnakör és az állapot
    batch.begin(render::ortho(0, W_, H_, 0));
    const float pw = std::min(k(720), W_ - k(80)), px = (W_ - pw) / 2, py = k(80);
    const float rh = k(34);
    const float ph = k(70) + static_cast<float>(seats.size()) * rh + (chat.empty() ? 0 : k(30) + static_cast<float>(chat.size()) * k(24)) +
                     (typing ? k(40) : 0);
    panel(batch, {px, py, pw, ph}, 225, true);
    shadowed(batch, MenuText::Big, px + k(20), py + k(14), loc.tr("ui.lobby.title"), kGold);
    float y = py + k(64);
    for (size_t i = 0; i < seats.size(); ++i) {
      const LobbyLine& s = seats[i];
      const std::string who = s.taken ? s.name + (s.you ? "  " + loc.tr("ui.lobby.you") : std::string()) : loc.tr("ui.lobby.free");
      shadowed(batch, MenuText::Med, px + k(20), y, std::to_string(i + 1) + ".  " + who, s.taken ? kText : kDim);
      if (s.taken) {   // a saját helyen a nép nyilakkal váltható (← →)
        shadowed(batch, MenuText::Small, px + pw * 0.55f, y + k(3), s.you ? "< " + s.faction + " >" : s.faction, s.you ? kGold : kText);
        const std::string rd = loc.tr(s.ready ? "ui.lobby.ready" : "ui.lobby.waiting");
        shadowed(batch, MenuText::Small, px + pw - k(20) - width(MenuText::Small, rd), y + k(3), rd, s.ready ? Rgba{150, 230, 150, 255} : kDim);
      }
      y += rh;
    }
    if (!chat.empty()) {
      y += k(10);
      for (const auto& c : chat) {
        shadowed(batch, MenuText::Small, px + k(20), y, c, kText);
        y += k(24);
      }
    }
    if (typing) {   // az írt üzenet a panel alján, villogó kurzorral
      y += k(8);
      const std::string line = loc.tr("ui.lobby.say") + " " + *typing + (static_cast<int>(t * 2) % 2 == 0 ? "_" : "");
      shadowed(batch, MenuText::Small, px + k(20), y, line, kGold);
    }
    batch.end();
  }

  void draw_loading(render::SpriteBatch& batch, int w, int h, float u, const std::string& title, const std::string& tip, double t) {
    bake(u);
    W_ = static_cast<float>(w);
    H_ = static_cast<float>(h);
    u_ = u;
    if (loading_bg_.empty()) {
      std::vector<std::filesystem::path> pics;
      std::error_code ec;
      for (const auto& e : std::filesystem::directory_iterator(std::filesystem::path(assets_) / "ui" / "bg" / "loading", ec)) {
        if (e.path().extension() == ".png") pics.push_back(e.path());
      }
      std::sort(pics.begin(), pics.end());
      loading_bg_ = pics.empty() ? std::string("-") : pics[std::random_device{}() % pics.size()].string();
    }
    batch.begin(render::ortho(0, W_, H_, 0));
    auto it = bg_.find(loading_bg_);
    if (it == bg_.end()) {
      render::Texture tex;
      if (auto img = loading_bg_ == "-" ? std::nullopt : render::load_png(loading_bg_)) {
        if (img->width <= dev_.max_texture_size() && img->height <= dev_.max_texture_size()) tex = dev_.create_array_from(*img, false);
      }
      it = bg_.emplace(loading_bg_, tex).first;
    }
    rect(batch, 0, 0, W_, H_, Rgba{12, 10, 14, 255});
    if (it->second.id) {   // kitöltve, középre
      const auto& tex = it->second;
      const float sc = std::max(W_ / static_cast<float>(tex.width), H_ / static_cast<float>(tex.height));
      render::SpriteInstance in;
      in.w = static_cast<float>(tex.width) * sc;
      in.h = static_cast<float>(tex.height) * sc;
      in.x = (W_ - in.w) / 2;
      in.y = (H_ - in.h) / 2;
      batch.draw(tex, nullptr, in);
    }
    const float bh = k(120);
    rect(batch, 0, H_ - bh, W_, bh, Rgba{8, 6, 10, 215});
    rect(batch, 0, H_ - bh, W_, k(2), kGold);
    // forgó rúnakör (12 kis rombusz körbe)
    const float rx = W_ - k(80), ry = H_ - bh / 2, rr = k(30);
    for (int i = 0; i < 12; ++i) {
      const float a = static_cast<float>(t) * 1.6f + static_cast<float>(i) * 0.5236f;
      const uint8_t al = static_cast<uint8_t>(90 + 165 * ((i + static_cast<int>(t * 8)) % 12) / 11);
      diamond(batch, rx + std::cos(a) * rr, ry + std::sin(a) * rr, k(4), Rgba{kGold[0], kGold[1], kGold[2], al});
    }
    const auto [tf, ts] = fit(MenuText::Big, MenuText::Med, title, W_ - k(200));
    shadowed(batch, tf, k(40), H_ - bh + k(18), ts, kGold);
    const auto [pf, ps] = fit(MenuText::Med, MenuText::Small, tip, W_ - k(200));
    shadowed(batch, pf, k(40), H_ - bh + k(66), ps, color(MenuText::Text));
    batch.end();
  }

  void draw(render::SpriteBatch& batch, const Menu& m, int w, int h, float mx, float my, double t) {
    bake(m.scale());
    W_ = static_cast<float>(w);
    H_ = static_cast<float>(h);
    u_ = m.scale();
    blink_t_ = t;
    batch.begin(render::ortho(0, W_, H_, 0));
    background(batch, m.screen(), t);
    if (m.screen() == MenuScreen::Main) meteors(batch, t);
    if (m.sidebar_width() > 0) sidebar(batch, m, mx, my);
    for (const MenuPanel& p : m.panels()) panel(batch, p.r, p.alpha, p.gold_top);
    for (const MenuImage& im : m.images()) image(batch, im);
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
    const std::string path = s == MenuScreen::Main || s == MenuScreen::Settings || s == MenuScreen::Sound || s == MenuScreen::Keys ||
                                     s == MenuScreen::Mods || s == MenuScreen::Load || s == MenuScreen::Credits || s == MenuScreen::Lan
                                 ? main_bg_   // az oldalsáv melletti képernyők: a főmenü háttere
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

  void image(render::SpriteBatch& b, const MenuImage& im) {   // kép az assets mappából (a betöltött a gyorsítótárban)
    const std::string path = (std::filesystem::path(assets_) / im.path).string();   // abszolút útvonal is lehet (a mentés kis képe)
    auto it = bg_.find(path);
    if (it == bg_.end()) {
      render::Texture tex;
      if (auto img = render::load_png(path)) {
        render::premultiply(*img);
        tex = dev_.create_array_from(*img, false);
      }
      it = bg_.emplace(path, tex).first;
    }
    if (!it->second.id) return;
    render::SpriteInstance in;
    in.x = im.r.x; in.y = im.r.y; in.w = im.r.w; in.h = im.r.h;
    b.draw(it->second, nullptr, in);
  }

  // a régi `Background._meteors`: hulló meteorok kék-fehér csóvával a főmenü háttere fölött
  void meteors(render::SpriteBatch& b, double t) {
    const float dt = static_cast<float>(std::clamp(t - meteor_t_, 0.0, 0.1));
    meteor_t_ = t;
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    if (u(meteor_rng_) < 0.012f) meteors_.push_back({-200 + u(meteor_rng_) * (W_ + 200), -20, 5 + u(meteor_rng_) * 4, 3.5f + u(meteor_rng_) * 2.5f});
    for (auto& m : meteors_) {
      m[0] += m[2] * dt * 60;
      m[1] += m[3] * dt * 60;
      for (int k2 = 0; k2 < 14; ++k2) {
        const float a = 1.0f - static_cast<float>(k2) / 14.0f, rr = std::max(1.0f, 3.0f * a) * u_;
        rect(b, m[0] - m[2] * static_cast<float>(k2) * 1.6f - rr, m[1] - m[3] * static_cast<float>(k2) * 1.6f - rr, rr * 2, rr * 2,
             Rgba{static_cast<uint8_t>(90 + 160 * a), static_cast<uint8_t>(150 + 90 * a), 255, static_cast<uint8_t>(255 * a)});
      }
    }
    meteors_.erase(std::remove_if(meteors_.begin(), meteors_.end(), [&](const auto& m) { return m[1] > H_ * 0.6f; }), meteors_.end());
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
      case MenuWidget::Kind::TextField: {   // szövegmező: sötét doboz, a szöveg, fókuszban villogó kurzor
        rect(b, r.x, r.y, r.w, r.h, Rgba{24, 20, 16, 255});
        frame(b, r, w.selected ? kGold : hot ? Rgba{190, 150, 80, 255} : Rgba{130, 104, 62, 255});
        const auto [f, s] = fit(MenuText::Med, MenuText::Small, w.label, r.w - k(16));
        put(b, f, r.x + k(8), r.cy() - line(f) / 2, s, kText);
        if (w.selected && static_cast<int>(blink_t_ * 2) % 2 == 0) rect(b, r.x + k(9) + width(f, s), r.y + k(6), std::max(1.0f, k(2)), r.h - k(12), kGold);
        break;
      }
      case MenuWidget::Kind::MapRow: {
        if (w.label.empty()) {   // felirat nélküli kattintófelület (pl. a népkártya): csak keret egérre
          if (hot) frame(b, r, kGold);
          break;
        }
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
  if (!save_settings(o.settings_file, st, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    Diagnostics::get().count("SAVE_FAILED");
  }
}

}  // namespace

void apply_display(SDL_Window* win, const Settings& st) {
  SDL_SetWindowFullscreen(win, st.fullscreen);
  if (!st.fullscreen) SDL_SetWindowSize(win, st.width, st.height);
}

std::filesystem::path saves_dir() {
  std::filesystem::path dir;
  if (char* pref = SDL_GetPrefPath("TOTU", "SkyRift")) {
    dir = std::filesystem::path(pref) / "mentesek";
    SDL_free(pref);
  }
  return dir;
}

void loading_frame(SDL_Window* win, render::Device& dev, render::SpriteBatch& batch, const Locale& loc, const std::string& assets,
                   float ui_scale, const std::string& status, double t) {
  static std::unique_ptr<MenuView> view;
  if (!view) {
    view = std::make_unique<MenuView>(dev);
    std::string err;
    if (!view->init(assets, &err)) {
      view.reset();
      return;
    }
  }
  int w = 0, h = 0;
  SDL_GetWindowSizeInPixels(win, &w, &h);
  dev.bind_target(nullptr, w, h);
  dev.clear(0.05f, 0.04f, 0.06f, 1.0f);
  view->draw_loading(batch, w, h, ui_scale, status, loc.tr("ui.atlas.wait"), t);
  SDL_GL_SwapWindow(win);
}

void lobby_frame(SDL_Window* win, render::Device& dev, render::SpriteBatch& batch, const Locale& loc, const std::string& assets,
                 float ui_scale, const std::vector<LobbyLine>& seats, const std::vector<std::string>& chat, const std::string& status,
                 double t, const std::string* typing) {
  static std::unique_ptr<MenuView> view;
  if (!view) {
    view = std::make_unique<MenuView>(dev);
    std::string err;
    if (!view->init(assets, &err)) {
      view.reset();
      return;
    }
  }
  int w = 0, h = 0;
  SDL_GetWindowSizeInPixels(win, &w, &h);
  dev.bind_target(nullptr, w, h);
  dev.clear(0.05f, 0.04f, 0.06f, 1.0f);
  view->draw_lobby(batch, w, h, ui_scale, loc, seats, chat, status, t, typing);
  SDL_GL_SwapWindow(win);
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
  map::TerrainTable terrain;   // H-65: a pályák teljes ellenőrzéséhez (betöltési hibánál csak a szerkezeti szűrés)
  bool terrain_ok = false;
  {
    content::Vfs vfs;
    content::add_mod_layers(vfs, o.content, o.mods);
    content::ContentSet set;
    content::ContentLoader loader(vfs, std::filesystem::path(o.content) / "schema");
    const auto rep = loader.load(set);
    std::vector<std::string> errs = rep.errors;
    terrain_ok = terrain.load(set, errs) && errs.empty();
  }
  std::vector<MapInfo> maps = scan_maps(o.maps_dir, terrain_ok ? &terrain : nullptr);
  for (const auto& m : o.mods) {   // M13: a modok pályái is a listába (azonos névnél a később betöltött mod nyer)
    if (m.id == "totu") continue;
    for (auto& mi : scan_maps((m.root / "maps").string(), terrain_ok ? &terrain : nullptr)) {
      const auto same = std::find_if(maps.begin(), maps.end(), [&](const MapInfo& x) { return x.name == mi.name; });
      if (same != maps.end()) *same = std::move(mi);
      else maps.push_back(std::move(mi));
    }
  }
  Menu menu(st, loc, std::move(maps), scan_factions(o.content));
  menu.set_mods(o.available_mods);
  menu.set_allies(scan_allies(o.content));
  {
    static bool intro_done = false;
    const auto intro_path = std::filesystem::path(o.assets) / "ui" / "bg" / "intro.png";
    if (!intro_done && o.start == MenuScreen::Main && o.screenshot.empty() && o.frames < 0) {
      intro_done = true;
      if (auto img = render::load_png(intro_path.string())) {
        render::Texture tex = dev.create_array_from(*img, false);
        const auto t0 = std::chrono::steady_clock::now();
        double leave = -1;
        bool run = true;
        while (run) {
          const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
          SDL_Event ev;
          while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) { dev.destroy(tex); return false; }
            if ((ev.type == SDL_EVENT_KEY_DOWN || ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) && leave < 0 && t > 0.3) leave = t;
          }
          if (leave < 0 && t > 7.0) leave = t;
          float a = static_cast<float>(std::min(1.0, t / 1.2));
          if (leave >= 0) {
            a = std::min(a, static_cast<float>(1.0 - (t - leave) / 0.6));
            if (a <= 0) break;
          }
          int w = 0, h = 0;
          SDL_GetWindowSizeInPixels(win, &w, &h);
          dev.bind_target(nullptr, w, h);
          dev.clear(0, 0, 0, 1);
          batch.begin(render::ortho(0, static_cast<float>(w), static_cast<float>(h), 0));
          const float sc = std::max(static_cast<float>(w) / static_cast<float>(tex.width), static_cast<float>(h) / static_cast<float>(tex.height));
          render::SpriteInstance in;
          in.w = static_cast<float>(tex.width) * sc;
          in.h = static_cast<float>(tex.height) * sc;
          in.x = (static_cast<float>(w) - in.w) / 2;
          in.y = (static_cast<float>(h) - in.h) / 2;
          for (int c = 0; c < 3; ++c) in.tint[c] = static_cast<uint8_t>(255 * a);   // elhalványulás feketébe
          batch.draw(tex, nullptr, in);
          batch.end();
          if (leave < 0 && t > 1.5 && static_cast<int>(t * 1.6) % 2 == 0) {   // „Nyomj meg egy gombot”
            view.draw_hint(batch, w, h, loc.tr("ui.intro.press"));
          }
          SDL_GL_SwapWindow(win);
          SDL_Delay(15);
        }
        dev.destroy(tex);
      }
    }
  }
  auto refresh_saves = [&] {   // a régi `LoadMenu` listája: a mentések, legújabb elöl
    std::vector<Menu::SaveRow> rows;
    for (const auto& e : list_saves(saves_dir())) {
      Menu::SaveRow r;
      r.path = e.path.string();
      r.title = e.path.stem().string();
      if (const auto v = content::parse(e.meta)) {
        if (const auto* t = v->find("title"); t && t->is_string()) r.title = t->text();
        if (const auto* d = v->find("date"); d && d->is_string()) r.detail = d->text();
      }
      if (auto th = std::filesystem::path(e.path).replace_extension(".png"); std::filesystem::exists(th)) r.thumb = th.string();
      rows.push_back(std::move(r));
    }
    menu.set_saves(std::move(rows));
  };
  refresh_saves();
  {   // M13: a modok (és az alap) győzelmi szkriptjei
    content::Vfs vfs;
    content::add_mod_layers(vfs, o.content, o.mods);
    menu.set_victories(script::list_victories(vfs));
  }
  menu.set_logo_aspect(view.logo_aspect());
  {   // a régi `StoriesMenu` könyvtára (stories/konyvtar.json, az assets mellett); a novellák kötete nélkül
    std::ifstream in(std::filesystem::path(o.assets).parent_path() / "stories" / "konyvtar.json", std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto plain = [](std::string s) {   // a **kiemelés** és a *dőlt* jelei nélkül
      s.erase(std::remove(s.begin(), s.end(), '*'), s.end());
      return s;
    };
    std::vector<Menu::StoryBook> books;
    if (const auto doc = content::parse(text); doc && doc->find("books") && doc->find("books")->is_array()) {
      for (const auto& b : doc->find("books")->items()) {
        const content::Value* id = b.find("id");
        if (id && id->is_string() && id->text() == "novellak") continue;
        Menu::StoryBook book;
        if (const auto* t = b.find("title"); t && t->is_string()) book.title = t->text();
        const content::Value* chs = b.find("chapters");
        if (!chs || !chs->is_array()) continue;
        for (const auto& c : chs->items()) {
          Menu::StoryChapter ch;
          if (const auto* t = c.find("title"); t && t->is_string()) ch.title = plain(t->text());
          if (const auto* t = c.find("sub"); t && t->is_string()) ch.sub = plain(t->text());
          const content::Value* bl = c.find("blocks");
          for (const auto& x : bl && bl->is_array() ? bl->items() : std::vector<content::Value>{}) {
            const content::Value* tv = x.find("t");
            if (!tv || !tv->is_string()) continue;
            Menu::StoryBlock sb;
            sb.type = tv->text();
            if (sb.type == "img") {
              if (const auto* cap = x.find("cap"); cap && cap->is_string()) sb.text = plain(cap->text());
            } else if (sb.type == "table") {   // soronként: cella | cella
              if (const auto* rows = x.find("rows"); rows && rows->is_array()) {
                for (const auto& r : rows->items()) {
                  Menu::StoryBlock row{"li", ""};
                  for (const auto& cell : r.is_array() ? r.items() : std::vector<content::Value>{}) {
                    if (!row.text.empty()) row.text += "  |  ";
                    row.text += plain(cell.is_string() ? cell.text() : std::string());
                  }
                  ch.blocks.push_back(std::move(row));
                }
              }
              continue;
            } else if (const auto* t = x.find("x"); t && t->is_string()) {
              sb.text = plain(t->text());
            }
            ch.blocks.push_back(std::move(sb));
          }
          book.chapters.push_back(std::move(ch));
        }
        books.push_back(std::move(book));
      }
    }
    menu.set_stories(std::move(books));
  }
  ProfileStore profiles(saves_dir().parent_path() / "profilok");   // a régi `ProfileMenu` (a felhasználói mappában)
  menu.set_profiles(&profiles);
  menu.set_tracks(o.tracks, o.current_track);
  if (menu.take_changes() & Menu::kSave) save(o, st);   // az első profil kiválasztása
  if (o.start != MenuScreen::Main) menu.go(o.start);
  view.colorblind_ = st.colorblind;
  const bool fixed = !o.screenshot.empty();
  const auto start = std::chrono::steady_clock::now();
  for (long frame = 0; o.frames < 0 || frame < o.frames; ++frame) {
    int w = 0, h = 0, ww = 0, wh = 0;
    SDL_GetWindowSizeInPixels(win, &w, &h);
    SDL_GetWindowSize(win, &ww, &wh);
    const float px = ww > 0 ? static_cast<float>(w) / static_cast<float>(ww) : 1.0f;   // képpont / ablakpont
    menu.set_time(fixed ? static_cast<double>(frame) / 60.0
                        : std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
    menu.set_status(o.status ? o.status() : std::string());
    if (o.tick) o.tick();
    menu.layout(w, h);
    SDL_Event ev;
    Menu::Result res = Menu::Result::None;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_EVENT_QUIT) return false;
      if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && (ev.button.button == SDL_BUTTON_LEFT || ev.button.button == SDL_BUTTON_RIGHT)) {
        res = menu.click(ev.button.x * px, ev.button.y * px, ev.button.button == SDL_BUTTON_RIGHT ? 3 : 1);
      } else if (ev.type == SDL_EVENT_MOUSE_WHEEL) {
        menu.wheel(ev.wheel.y > 0 ? 1 : ev.wheel.y < 0 ? -1 : 0);
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
        else if (kc == SDLK_BACKSPACE) key = 8;
        if (key) res = menu.key(key);
      } else if (ev.type == SDL_EVENT_TEXT_INPUT && menu.text_focus() >= 0) {
        menu.text_input(ev.text.text);
      }
      const uint32_t ch = menu.take_changes();
      if (ch & Menu::kSave) save(o, st);
      if (ch & Menu::kDisplay) apply_display(win, st);
      if (ch & Menu::kDisplay) save(o, st);
      if (ch & (Menu::kLanguage | Menu::kColors)) save(o, st);
      view.colorblind_ = st.colorblind;
      if (res == Menu::Result::Quit) return false;
      if (res == Menu::Result::Track && o.play_track) o.play_track(menu.track_choice());
      if (o.single && menu.screen() == MenuScreen::Main) {   // a szünetmenüből nyitott képernyő: vissza a játékba
        if (SDL_TextInputActive(win)) SDL_StopTextInput(win);
        return false;
      }
      if (res == Menu::Result::Restart) {   // M13: a modkészlet megváltozott – a hívó újraindítja a játékot
        if (rc) *rc = kMenuRestart;
        return false;
      }
      if (const std::string del = menu.take_delete(); !del.empty()) {   // a mentés törlése (a kis képével)
        std::error_code ec;
        std::filesystem::remove(del, ec);
        std::filesystem::remove(std::filesystem::path(del).replace_extension(".png"), ec);
        refresh_saves();
      }
      auto loading_screen = [&] {   // a régi `Loader`: a meccs betöltése alatt látszik
        const int n = tip_count(loc);
        const std::string tip = n > 0 ? loc.tr("ui.tip." + std::to_string(1 + static_cast<int>(std::random_device{}() % static_cast<unsigned>(n)))) : std::string();
        dev.bind_target(nullptr, w, h);
        dev.clear(0.05f, 0.04f, 0.06f, 1.0f);
        view.draw_loading(batch, w, h, menu.scale(), loc.tr("ui.loading"), tip, 0.0);
        SDL_GL_SwapWindow(win);
      };
      if (res == Menu::Result::Load && out) {   // mentett játék betöltése
        *out = MatchConfig{};
        out->load_path = menu.load_choice();
        loading_screen();
        return true;
      }
      if (menu.text_focus() >= 0 && !SDL_TextInputActive(win)) SDL_StartTextInput(win);
      if (menu.text_focus() < 0 && SDL_TextInputActive(win)) SDL_StopTextInput(win);
      if (res == Menu::Result::Scan) {   // a régi `LanMenu.scan`: a helyi hálózaton futó játékok
        std::vector<Menu::LanRow> rows;
        for (const auto& g : net::lan_scan(800)) {
          Menu::LanRow r;
          r.address = g.address;
          r.port = g.port;
          r.joinable = g.taken < g.seats && g.content == o.content_hash;
          r.title = g.name + "  –  " + g.map + "  (" + std::to_string(g.taken) + "/" + std::to_string(g.seats) + ")  " + g.address +
                    (g.content != o.content_hash ? "  " + loc.tr("ui.lan.other_mods") : std::string());
          rows.push_back(std::move(r));
        }
        menu.set_lan_games(std::move(rows));
      }
      if ((res == Menu::Result::Host || res == Menu::Result::Join) && out) {
        const uint64_t seed = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        if (menu.net_config(seed, out)) {
          if (SDL_TextInputActive(win)) SDL_StopTextInput(win);
          save(o, st);
          return true;
        }
      }
      if (res == Menu::Result::Start) {
        const uint64_t seed = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        if (menu.match_config(seed, out)) {
          loading_screen();
          return true;
        }
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
