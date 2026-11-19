// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/hud.hpp"
#include "client/diagnostics.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "render/sprite_batch.hpp"
#include "stb_truetype.h"

namespace totu::client {

namespace {

constexpr float kPi = 3.14159265f;

std::array<uint8_t, 3> shade(std::array<uint8_t, 3> c, float k) {
  for (auto& v : c) v = static_cast<uint8_t>(std::clamp(static_cast<float>(v) * k, 0.0f, 255.0f));
  return c;
}

std::string fmt(const char* f, double v) {
  char b[64];
  std::snprintf(b, sizeof b, f, v);
  return b;
}

std::string mmss(int64_t secs) {
  char b[32];
  std::snprintf(b, sizeof b, "%02lld:%02lld", static_cast<long long>(secs / 60), static_cast<long long>(secs % 60));
  return b;
}

}  // namespace

// ---------------------------------------------------------------- erőforrások
bool Hud::init(render::Device& dev, const std::string& assets_dir, std::string* error) {
  dev_ = &dev;
  assets_ = assets_dir;
  std::ifstream in(std::filesystem::path(assets_dir) / "fonts" / "egszakadas_rovaskonyv_text.ttf", std::ios::binary);
  ttf_.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  stbtt_fontinfo info;
  if (ttf_.empty() || !stbtt_InitFont(&info, ttf_.data(), stbtt_GetFontOffsetForIndex(ttf_.data(), 0))) {
    if (error) *error = "a HUD betűje nem tölthető be (assets/fonts)";
    return false;
  }
  // a régi játék (pygame) a betűméretet em-ben adja; a sütés képpontmagasságot vár
  em_ratio_ = stbtt_ScaleForMappingEmToPixels(&info, 1.0f) / stbtt_ScaleForPixelHeight(&info, 1.0f);
  ready_ = font(15).ok;
  if (!ready_ && error) *error = "a HUD betűje nem süthető";
  return ready_;
}

Hud::Font& Hud::font(int size) {
  size = std::clamp(size, 6, 40);
  auto it = fonts_.find(size);
  if (it != fonts_.end()) return it->second;
  Font& f = fonts_[size];
  if (dev_ && render::bake_font(ttf_, static_cast<float>(size) * em_ratio_, f.bake)) {
    f.tex = dev_->create_array_from(f.bake.atlas, false);
    f.ok = true;
  }
  return f;
}

const Hud::Img* Hud::image(const std::string& rel) {
  auto it = images_.find(rel);
  if (it == images_.end()) {
    Img im;
    const auto p = std::filesystem::path(assets_) / (rel + ".png");
    if (dev_ && std::filesystem::is_regular_file(p)) {
      if (auto png = render::load_png(p.string())) {
        render::premultiply(*png);
        im.tex = dev_->create_array_from(*png, false);
        im.w = png->width;
        im.h = png->height;
        im.ok = true;
      }
    }
    if (!im.ok && dev_) Diagnostics::get().count_once("ASSET_FALLBACK", rel);   // egyszer képenként
    it = images_.emplace(rel, im).first;
  }
  return it->second.ok ? &it->second : nullptr;
}

const Hud::Img* Hud::portrait(const std::string& id) {
  // a tulajdonos döntése (M9): a portré nem színeződik át csapatszínre; a csapatot a sarokjel mutatja (team_badge)
  const std::string key = "#portre/" + id;
  auto it = images_.find(key);
  if (it == images_.end()) {
    Img im;
    const auto p = std::filesystem::path(assets_) / "portraits" / (id + ".png");
    if (dev_ && std::filesystem::is_regular_file(p)) {
      if (auto png = render::load_png(p.string())) {
        render::premultiply(*png);
        im.tex = dev_->create_array_from(*png, false);
        im.w = png->width;
        im.h = png->height;
        im.ok = true;
      }
    }
    it = images_.emplace(key, im).first;
  }
  return it->second.ok ? &it->second : nullptr;
}

void Hud::team_badge(const Rect& r, Col team) {
  // kis, csapatszínű téglalap a portré bal alsó sarkában (sötét szegéllyel)
  const int bw = std::max(6, r.w / 6), bh = std::max(4, r.h / 10);
  const Rect b{r.x + 3, r.bottom() - bh - 3, bw, bh};
  rect(b.inflate(2, 2), {0, 0, 0}, 200);
  rect(b, team);
}

Hud::Theme Hud::theme() const {
  // a régi `faction_ui.THEMES`: alap, keret világos/sötét, kiemelés, kitöltés
  if (fid_ == "elves") return {{150, 128, 92}, {226, 190, 96}, {84, 64, 30}, {40, 170, 140}, {40, 36, 28}};
  if (fid_ == "ursan") return {{92, 62, 40}, {150, 160, 176}, {40, 28, 18}, {120, 180, 255}, {34, 26, 20}};
  if (fid_ == "croftian") return {{66, 38, 80}, {226, 214, 186}, {30, 12, 36}, {150, 255, 120}, {30, 18, 36}};
  if (fid_ == "wullof") return {{140, 74, 46}, {236, 226, 206}, {60, 30, 18}, {214, 150, 72}, {40, 24, 18}};
  if (fid_ == "micelia" || fid_ == "mycel") return {{64, 70, 52}, {226, 214, 186}, {28, 30, 22}, {80, 230, 205}, {26, 30, 24}};
  if (fid_ == "hukucha") return {{54, 96, 84}, {214, 226, 200}, {22, 44, 40}, {120, 230, 210}, {22, 34, 32}};
  if (fid_ == "ooni") return {{118, 40, 36}, {226, 190, 110}, {44, 14, 12}, {80, 200, 140}, {34, 18, 16}};
  if (fid_ == "imperium") return {{120, 116, 110}, {196, 150, 80}, {50, 44, 40}, {120, 255, 190}, {34, 34, 36}};
  return {{90, 86, 78}, {176, 138, 74}, {60, 42, 22}, {230, 190, 90}, {42, 44, 48}};
}

void Hud::sync(World& world, Session& session, int w, int h) {
  Session* ss = &session;
  World* ww = &world;
  held_ = world.snapshot_shared();
  model_.bind(held_.get(), session.rules(), [ss](const std::string& k) { return ss->display_name(k); },
              [ww](const sim::Command& c) { ww->post(c); });
  model_.set_screen(w, h);
  map_w_ = world.map().width();
  map_h_ = world.map().height();
  model_.set_map_size(map_w_, map_h_);
  model_.watch_alerts();   // a régi „Támadás ér!” riasztás
  if (!mini_.ok && dev_) {   // a minimap terepképe (a pálya a betöltés után nem változik)
    const auto cols = HudModel::minimap_terrain(world.map(), session.terrain());
    render::Image im;
    im.width = map_w_;
    im.height = map_h_;
    im.rgba.resize(cols.size() * 4);
    for (size_t i = 0; i < cols.size(); ++i) {
      im.rgba[i * 4] = cols[i][0];
      im.rgba[i * 4 + 1] = cols[i][1];
      im.rgba[i * 4 + 2] = cols[i][2];
      im.rgba[i * 4 + 3] = 255;
    }
    if (!cols.empty()) {
      mini_.tex = dev_->create_array_from(im, false);
      mini_.w = map_w_;
      mini_.h = map_h_;
      mini_.ok = true;
    }
  }
  if (model_.sel_changed) {   // a HUD módosította a kijelölést
    world.set_selection(model_.sel);
    model_.sel_changed = false;
  } else if (!world.selection().empty()) {
    model_.sel = world.selection();
  } else if (world.selected_building()) {
    model_.sel = {world.selected_building()};
  } else {
    model_.sel.clear();
  }
  if (const Snapshot* s = world.snapshot()) fid_ = s->players[std::min<size_t>(model_.player, sim::Sim::kMaxPlayers)].faction;
}

// ---------------------------------------------------------------- rajzoló alapelemek
void Hud::rect(float x, float y, float w, float h, Col c, uint8_t a) {
  if (w <= 0 || h <= 0) return;
  render::SpriteInstance in;
  in.x = x; in.y = y; in.w = w; in.h = h;
  in.tint[0] = c[0]; in.tint[1] = c[1]; in.tint[2] = c[2]; in.tint[3] = a;
  batch_->draw(*white_, nullptr, in);
}

void Hud::outline(const Rect& r, Col c, int t, uint8_t a) {
  const float x = static_cast<float>(r.x), y = static_cast<float>(r.y), w = static_cast<float>(r.w),
              h = static_cast<float>(r.h), tt = static_cast<float>(t);
  rect(x, y, w, tt, c, a);
  rect(x, y + h - tt, w, tt, c, a);
  rect(x, y + tt, tt, h - 2 * tt, c, a);
  rect(x + w - tt, y + tt, tt, h - 2 * tt, c, a);
}

void Hud::line(float x0, float y0, float x1, float y1, Col c, float t) {
  if (std::fabs(y1 - y0) < 0.01f) {
    rect(std::min(x0, x1), y0 - (t > 1 ? t / 2 : 0), std::fabs(x1 - x0) + 1, t, c);
    return;
  }
  if (std::fabs(x1 - x0) < 0.01f) {
    rect(x0 - (t > 1 ? t / 2 : 0), std::min(y0, y1), t, std::fabs(y1 - y0) + 1, c);
    return;
  }
  const float len = std::hypot(x1 - x0, y1 - y0);
  const int n = std::max(1, static_cast<int>(len));
  for (int i = 0; i <= n; ++i) {
    const float k = static_cast<float>(i) / static_cast<float>(n);
    rect(x0 + (x1 - x0) * k - t / 2, y0 + (y1 - y0) * k - t / 2, t, t, c);
  }
}

void Hud::polygon(const std::vector<std::pair<float, float>>& pts, Col c, uint8_t a) {
  if (pts.size() < 3) return;
  float y0 = pts[0].second, y1 = y0;
  for (const auto& p : pts) {
    y0 = std::min(y0, p.second);
    y1 = std::max(y1, p.second);
  }
  std::vector<float> xs;
  for (int y = static_cast<int>(std::floor(y0)); y < static_cast<int>(std::ceil(y1)); ++y) {
    const float sy = static_cast<float>(y) + 0.5f;
    xs.clear();
    for (size_t i = 0; i < pts.size(); ++i) {
      const auto& [ax, ay] = pts[i];
      const auto& [bx, by] = pts[(i + 1) % pts.size()];
      if ((ay <= sy && by > sy) || (by <= sy && ay > sy)) xs.push_back(ax + (sy - ay) / (by - ay) * (bx - ax));
    }
    std::sort(xs.begin(), xs.end());
    for (size_t i = 0; i + 1 < xs.size(); i += 2) rect(std::round(xs[i]), static_cast<float>(y), std::round(xs[i + 1]) - std::round(xs[i]), 1, c, a);
  }
}

void Hud::blit(const Img& im, float x, float y, float w, float h, Col tint, uint8_t a) {
  render::SpriteInstance in;
  in.x = x; in.y = y; in.w = w; in.h = h;
  in.tint[0] = tint[0]; in.tint[1] = tint[1]; in.tint[2] = tint[2]; in.tint[3] = a;
  batch_->draw(im.tex, nullptr, in);
}

void Hud::blit_fit(const Img& im, const Rect& r, bool crop_square) {
  // a régi `fit_icon`: fekvő/álló ikonnál a közepe négyzetre vágva, különben arányosan, középre (2 px peremmel)
  float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
  float w = static_cast<float>(im.w), h = static_cast<float>(im.h);
  if (crop_square && im.w != im.h && std::min(w, h) >= 0.66f * std::max(w, h)) {
    const float m = std::min(w, h);
    u0 = (w - m) / 2 / w; u1 = 1 - u0;
    v0 = (h - m) / 2 / h; v1 = 1 - v0;
    w = h = m;
  }
  const float sc = std::min((static_cast<float>(r.w) - 4) / w, (static_cast<float>(r.h) - 4) / h);
  const float dw = std::max(1.0f, std::floor(w * sc)), dh = std::max(1.0f, std::floor(h * sc));
  render::SpriteInstance in;
  in.x = static_cast<float>(r.x) + std::floor((static_cast<float>(r.w) - dw) / 2);
  in.y = static_cast<float>(r.y) + std::floor((static_cast<float>(r.h) - dh) / 2);
  in.w = dw; in.h = dh;
  in.u0 = u0; in.v0 = v0; in.u1 = u1; in.v1 = v1;
  batch_->draw(im.tex, nullptr, in);
}

void Hud::text(render::SpriteBatch& batch, int size, float x, float y, const std::string& s, uint8_t r, uint8_t g,
               uint8_t b, uint8_t a) {
  Font& f = font(size);
  if (!f.ok) return;
  float pen = std::round(x);
  const float base = std::round(y + f.bake.ascent);
  for (const uint32_t cp : render::utf8_codepoints(s)) {
    const render::Glyph* gl = f.bake.find(cp);
    if (!gl) gl = f.bake.find('?');
    if (!gl) continue;
    if (gl->x1 > gl->x0) {
      render::SpriteInstance in;
      // a toll egész képpontra: a tört helyzetű kis jel (: .) a szűréstől elmosódna
      in.x = std::round(pen) + gl->x0; in.y = base + gl->y0; in.w = gl->x1 - gl->x0; in.h = gl->y1 - gl->y0;
      in.u0 = gl->u0; in.v0 = gl->v0; in.u1 = gl->u1; in.v1 = gl->v1;
      in.tint[0] = r; in.tint[1] = g; in.tint[2] = b; in.tint[3] = a;
      batch.draw(f.tex, nullptr, in);
    }
    pen += gl->advance;
  }
}

float Hud::tw(int size, const std::string& s) { return font(size).bake.width(s); }
float Hud::line_h(int size) { return std::ceil(font(size).bake.line); }

void Hud::stext(int size, float x, float y, const std::string& s, Col c) {
  for (const auto& [ox, oy] : {std::pair{1, 1}, std::pair{2, 2}, std::pair{-1, 1}}) {
    text(*batch_, size, x + static_cast<float>(ox), y + static_cast<float>(oy), s, 0, 0, 0);
  }
  text(*batch_, size, x, y, s, c[0], c[1], c[2]);
}

float Hud::wrap(const std::string& s, float x, float y, float w, int size, Col c, int max_lines) {
  std::string line, word;
  int n = 0;
  auto flush_word = [&]() -> bool {
    if (word.empty()) return true;
    const std::string t = line.empty() ? word : line + " " + word;
    if (tw(size, t) > w && !line.empty()) {
      text(*batch_, size, x, y, line, c[0], c[1], c[2]);
      y += line_h(size);
      if (++n >= max_lines) return false;
      line = word;
    } else {
      line = t;
    }
    word.clear();
    return true;
  };
  for (const char ch : s) {
    if (ch == ' ' || ch == '\n') {
      if (!flush_word()) return y;
    } else {
      word += ch;
    }
  }
  if (!flush_word()) return y;
  if (!line.empty()) {
    text(*batch_, size, x, y, line, c[0], c[1], c[2]);
    y += line_h(size);
  }
  return y;
}

std::string Hud::ellipsize(const std::string& s, int size, float w) {
  if (tw(size, s) <= w) return s;
  std::string t = s;
  while (!t.empty() && tw(size, t + "…") > w) {
    t.pop_back();
    while (!t.empty() && (static_cast<unsigned char>(t.back()) & 0xC0) == 0x80) t.pop_back();   // UTF-8 folytatóbájt
    if (!t.empty() && (static_cast<unsigned char>(t.back()) & 0xC0) == 0xC0) t.pop_back();
  }
  while (!t.empty() && (t.back() == ' ' || t.back() == ',')) t.pop_back();
  return t + "…";
}

void Hud::corner_label(const Rect& r, const std::string& s, Col c, int size, const char* where) {
  const float w = tw(size, s) + 4, h = line_h(size);
  const float x = where[1] == 'r' ? static_cast<float>(r.right()) - w - 1 : static_cast<float>(r.x) + 1;
  const float y = where[0] == 'b' ? static_cast<float>(r.bottom()) - h - 1 : static_cast<float>(r.y) + 1;
  rect(x, y, w, h, {0, 0, 0}, 200);
  text(*batch_, size, x + 2, y, s, c[0], c[1], c[2]);
}

void Hud::well(const Rect& r, Col fill) {
  // süllyesztett mező (a HUD paneljein)
  rect(r, fill);
  const float x = static_cast<float>(r.x), y = static_cast<float>(r.y), w = static_cast<float>(r.w), h = static_cast<float>(r.h);
  rect(x, y, w, 1, {4, 4, 4});
  rect(x, y, 1, h, {4, 4, 4});
  const Col lo = shade(theme().light, 0.75f);
  rect(x, y + h, w + 1, 1, lo);
  rect(x + w, y, 1, h + 1, lo);
}

void Hud::bevel(const Rect& r, Col fill) {
  // vésett, a faj anyagaiból keretezett panel
  const Theme th = theme();
  const Col hi = shade(th.light, 0.85f), lo = shade(th.dark, 0.9f);
  rect(r, fill);
  outline(r, {12, 10, 8});
  const float x = static_cast<float>(r.x), y = static_cast<float>(r.y), w = static_cast<float>(r.w), h = static_cast<float>(r.h);
  rect(x + 1, y + 1, w - 2, 1, hi);
  rect(x + 1, y + 1, 1, h - 2, hi);
  rect(x + 1, y + h - 2, w - 2, 1, lo);
  rect(x + w - 2, y + 1, 1, h - 2, lo);
}

void Hud::section_frame(const Rect& f) {
  // önálló, kiemelt szekció-keret: fajonként megrajzolt 9 szeletes kép, vagy sötét árok, fajszínű perem
  const HudLayout& L = model_.layout();
  const int bd = std::max(8, py_round(16 * L.s));
  if (const Img* art = image("ui/frame_" + fid_)) {
    rect(f, {12, 12, 14});
    const float dx = static_cast<float>(f.x - bd / 2), dy = static_cast<float>(f.y - bd / 2);
    const float dw = static_cast<float>(f.w + 2 * (bd / 2)), dh = static_cast<float>(f.h + 2 * (bd / 2)), b = static_cast<float>(bd);
    if (dw > 2 * b && dh > 2 * b) {
      const float sb = static_cast<float>(art->w / 4), sw = static_cast<float>(art->w), sh = static_cast<float>(art->h);
      const float xs[4][2] = {{0, sb}, {sb, sw - sb}, {sw - sb, sw}, {0, 0}};
      const float ys[4][2] = {{0, sb}, {sb, sh - sb}, {sh - sb, sh}, {0, 0}};
      const float dxs[3][2] = {{dx, dx + b}, {dx + b, dx + dw - b}, {dx + dw - b, dx + dw}};
      const float dys[3][2] = {{dy, dy + b}, {dy + b, dy + dh - b}, {dy + dh - b, dy + dh}};
      for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
          if (i == 1 && j == 1) continue;   // a közepe üres (a szekció tartalma látszik)
          render::SpriteInstance in;
          in.x = dxs[i][0]; in.y = dys[j][0]; in.w = dxs[i][1] - dxs[i][0]; in.h = dys[j][1] - dys[j][0];
          in.u0 = xs[i][0] / sw; in.u1 = xs[i][1] / sw; in.v0 = ys[j][0] / sh; in.v1 = ys[j][1] / sh;
          batch_->draw(art->tex, nullptr, in);
        }
      }
    }
    return;
  }
  const Theme th = theme();
  const Col hi = th.light, lo = shade(hi, 0.45f);
  outline(f.inflate(4, 4), {4, 4, 4}, 2);
  rect(f, {12, 12, 14});
  outline(f, lo, 2);
  rect(static_cast<float>(f.x), static_cast<float>(f.y), static_cast<float>(f.w), 1, hi);
  rect(static_cast<float>(f.x), static_cast<float>(f.y), 1, static_cast<float>(f.h), hi);
  const int k = std::max(3, L.fp + 1);
  for (const auto& [cx, cy] : {std::pair{f.x, f.y}, std::pair{f.right() - 1, f.y}, std::pair{f.x, f.bottom() - 1},
                               std::pair{f.right() - 1, f.bottom() - 1}}) {
    const Rect c{cx - k / 2, cy - k / 2, k, k};
    rect(c, hi);
    outline(c, {4, 4, 4});
  }
}

void Hud::clock_wipe(const Rect& r, float prog) {
  // a még hátralévő rész sötétítve (az óramutató járásával), sárga kerettel
  if (prog < 1.0f) {
    const float cx = static_cast<float>(r.w) / 2, cy = static_cast<float>(r.h) / 2, lim = std::clamp(prog, 0.0f, 1.0f) * 2 * kPi;
    for (int y = 0; y < r.h; ++y) {
      int start = -1;
      for (int x = 0; x <= r.w; ++x) {
        bool dark = false;
        if (x < r.w) {
          float a = std::atan2(static_cast<float>(y) + 0.5f - cy, static_cast<float>(x) + 0.5f - cx) + kPi / 2;
          if (a < 0) a += 2 * kPi;
          dark = a >= lim;
        }
        if (dark && start < 0) start = x;
        if (!dark && start >= 0) {
          rect(static_cast<float>(r.x + start), static_cast<float>(r.y + y), static_cast<float>(x - start), 1, {0, 0, 0}, 150);
          start = -1;
        }
      }
    }
  }
  outline(r, {255, 220, 100});
}

void Hud::hbar(float x, float y, float w, float h, float frac, Col c, const std::string& label) {
  rect(x, y, w, h, {8, 8, 8});
  rect(x + 1, y + 1, (w - 2) * std::clamp(frac, 0.0f, 1.0f), h - 2, shade(c, 0.75f));
  if (!label.empty()) {
    const int fs = model_.layout().font.tiny;
    if (line_h(fs) <= h + 6) text(*batch_, fs, x + w / 2 - tw(fs, label) / 2, y + h / 2 - line_h(fs) / 2, label, 250, 250, 240);
  }
}

void Hud::def_icon(uint16_t def, const Rect& r, Col team) {
  const sim::Rules* rules = model_.rules();
  if (!rules || def >= rules->defs.size()) return;
  const sim::EntityDef& d = rules->defs[def];
  // a faj színével sötétedő háttér (a régi `fit_icon` bg-je)
  const Col fc = model_.faction_color(d.faction, false), top = shade(fc, 0.42f);
  for (int i = 0; i < r.h; i += 2) {
    const float t = static_cast<float>(i) / static_cast<float>(std::max(1, r.h - 1));
    const Col c{static_cast<uint8_t>(top[0] * (1 - t) + 12 * t), static_cast<uint8_t>(top[1] * (1 - t) + 12 * t),
                static_cast<uint8_t>(top[2] * (1 - t) + 12 * t)};
    rect(static_cast<float>(r.x), static_cast<float>(r.y + i), static_cast<float>(r.w), static_cast<float>(std::min(2, r.h - i)), c);
  }
  if (const Img* im = portrait(d.id)) {
    blit_fit(*im, r);
  } else {   // még nincs grafika: a név kezdőbetűi
    const std::string nm = model_.name((d.building ? "building." : "unit.") + d.id + ".name");
    const int fs = model_.layout().font.name;
    const std::string ini = nm.substr(0, std::min<size_t>(nm.size(), (static_cast<unsigned char>(nm[0]) & 0x80) ? 3 : 2));
    text(*batch_, fs, static_cast<float>(r.centerx()) - tw(fs, ini) / 2, static_cast<float>(r.centery()) - line_h(fs) / 2, ini,
         team[0], team[1], team[2]);
  }
}

void Hud::research_icon(int research, const Rect& r) {
  const sim::Rules* rules = model_.rules();
  if (!rules || research < 0 || static_cast<size_t>(research) >= rules->researches.size()) return;
  const std::string& rid = rules->researches[static_cast<size_t>(research)].id;
  const Col top = shade(model_.faction_color(fid_, false), 0.42f);
  rect(r, top);
  rect(static_cast<float>(r.x), static_cast<float>(r.centery()), static_cast<float>(r.w), static_cast<float>(r.h / 2), shade(top, 0.5f));
  const Img* im = image("research/" + rid);
  std::string tier;
  if (!im && rid.size() > 2 && rid[rid.size() - 2] == '_' && (rid.back() == '2' || rid.back() == '3')) {
    im = image("research/" + rid.substr(0, rid.size() - 2));   // a szintes kutatás II. és III. foka
    tier = rid.back() == '2' ? "II" : "III";
  }
  if (!im) im = image("ui/icons/learn");
  if (im) blit_fit(*im, r);
  if (!tier.empty()) corner_label(r, tier, {255, 226, 140}, std::max(10, r.h * 2 / 5), "br");
}

void Hud::cmd_icon(const HudSlot& sl, const Rect& r) {
  if (sl.kind == HudSlot::Research) {
    research_icon(sl.research, r);
    return;
  }
  if (sl.def != sim::kNoDef) {
    def_icon(sl.def, r, model_.player_color(model_.player));
    return;
  }
  if (!sl.ally.empty()) {
    if (const Img* em = image("emblems/" + sl.ally)) blit(*em, static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h));
    return;
  }
  // saját rajz, ha van: assets/abilities/<képesség>.png, majd assets/ui/icons/<fajta>.png
  const Img* im = sl.ab_id.empty() ? nullptr : image("abilities/" + sl.ab_id);
  if (!im && !sl.icon.empty()) im = image("ui/icons/" + sl.icon);
  if (!im) im = image("ui/icons/bolt");
  const Col tint = sl.passive ? Col{120, 120, 120} : Col{255, 255, 255};
  if (im) blit(*im, static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h), tint);
}

void Hud::type_icon(const std::string& key, const Rect& r) {
  if (const Img* im = image("ui/types/" + key)) blit(*im, static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h));
}

// ---------------------------------------------------------------- rajzolás
void Hud::draw(render::SpriteBatch& batch, const render::Texture& white, const HudFrame& f) {
  batch_ = &batch;
  white_ = &white;
  t_ = f.t;
  model_.t = f.t;
  mx_ = f.mx;
  my_ = f.my;
  if (!ready_ || !model_.snap() || !model_.rules()) return;
  model_.mission_choices.clear();
  if (model_.snap()->mission.letterbox) {   // átvezető: csak a sáv és a felirat (a WC3 szerint a felület eltűnik)
    draw_mission(f);
    return;
  }
  draw_bottom(f);
  draw_topbar(f);
  draw_quests();
  draw_clock(f);
  draw_notice(f);
  draw_heroes(f);
  if (model_.help_open) draw_help(f);
  if (model_.path_pending()) draw_path_choice(f);
  else draw_tooltip(f);
  draw_mission(f);
  draw_result(f);
}

void Hud::draw_mission(const HudFrame& f) {
  const SnapMission& m = model_.snap()->mission;
  if (!m.active) return;
  const HudLayout& L = model_.layout();
  const float W = static_cast<float>(f.w), H = static_cast<float>(f.h);
  const int F = L.font.small, B = L.font.res;
  if (m.letterbox) {
    const float bar = std::floor(H * 0.12f);
    rect(0, 0, W, bar, {0, 0, 0});
    rect(0, H - bar, W, bar, {0, 0, 0});
    if (!m.subtitle.empty()) {
      const std::string who = m.sub_speaker.empty() ? std::string() : model_.name("unit." + m.sub_speaker + ".name");
      const std::string line = (who.empty() || who == "unit." + m.sub_speaker + ".name" ? std::string() : who + ": ") + model_.name(m.subtitle);
      stext(B, W / 2 - tw(B, line) / 2, H - bar + (bar - line_h(B)) / 2, line, {240, 236, 214});
    }
    return;
  }
  // üzenetek (felül középen)
  float my = static_cast<float>(L.topbar.y + L.topbar.h) + 8;
  for (const auto& msg : m.messages) {
    const std::string t = model_.name(msg);
    stext(F, W / 2 - tw(F, t) / 2, my, t, {255, 230, 150});
    my += line_h(F) + 2;
  }
  // párbeszéd (az alsó sáv fölött): portré, név, szöveg, válaszok
  if (m.dialogue.empty()) return;
  const float bw = std::min(W - 40, 640.0f), x = W / 2 - bw / 2;
  const float lh = line_h(F) + 2;
  const float bh = 96 + lh * static_cast<float>(m.choices.size());
  const float y = static_cast<float>(L.hud.y) - bh - 10;
  const Rect box{static_cast<int>(x), static_cast<int>(y), static_cast<int>(bw), static_cast<int>(bh)};
  rect(box, {12, 10, 8}, 225);
  outline(box, {200, 170, 110});
  const Rect pr{box.x + 8, box.y + 8, 80, 80};
  if (const Img* p = portrait(m.portrait)) blit_fit(*p, pr);
  else well(pr);
  outline(pr, {90, 80, 60});
  const float tx = static_cast<float>(pr.x + pr.w + 10), tw_ = bw - static_cast<float>(pr.w) - 28;
  const std::string who = model_.name("unit." + m.speaker + ".name");
  stext(B, tx, static_cast<float>(box.y + 6), who, {255, 214, 120});
  wrap(model_.name(m.text), tx, static_cast<float>(box.y + 8) + line_h(B), tw_, F, {240, 236, 214}, 4);
  float cy = static_cast<float>(box.y + 92);
  for (size_t i = 0; i < m.choices.size(); ++i) {
    const Rect cr{static_cast<int>(tx), static_cast<int>(cy), static_cast<int>(tw_), static_cast<int>(lh)};
    const bool hot = cr.contains(mx_, my_);
    if (hot) rect(cr, {60, 50, 30}, 200);
    stext(F, tx + 4, cy, std::to_string(i + 1) + ". " + model_.name(m.choices[i]), hot ? Col{255, 240, 180} : Col{220, 210, 170});
    model_.mission_choices.push_back(cr);
    cy += lh;
  }
}

void Hud::draw_bottom(const HudFrame& f) {
  const HudLayout& L = model_.layout();
  const Theme th = theme();
  // az alsó sáv egybefüggő háttere (assets/ui/hud_<faj>.png, különben a faj színe kerettel)
  if (const Img* bg = image("ui/hud_" + fid_)) {
    blit(*bg, static_cast<float>(L.hud.x), static_cast<float>(L.hud.y), static_cast<float>(L.hud.w), static_cast<float>(L.hud.h));
  } else {
    rect(L.hud, th.base);
    outline(L.hud, {10, 8, 6});
    const float x = static_cast<float>(L.hud.x), y = static_cast<float>(L.hud.y), w = static_cast<float>(L.hud.w), h = static_cast<float>(L.hud.h);
    const Col cols[3] = {th.light, shade(th.light, 0.7f), th.dark};
    for (int k = 1; k <= 3; ++k) {
      const float kk = static_cast<float>(k);
      rect(x + kk, y + kk, w - 2 * kk, 1, cols[k - 1]);
      rect(x + kk, y + kk, 1, h - kk, cols[k - 1]);
      rect(x + w - 1 - kk, y + kk, 1, h - kk, cols[k - 1]);
    }
  }
  // fajbannerek (a jobb oldalon a választott szövetséges népé)
  const std::string ally = model_.snap()->players[std::min<size_t>(model_.player, sim::Sim::kMaxPlayers)].ally;
  for (int side = 0; side < 2; ++side) {
    const Rect& r = side == 0 ? L.banner1 : L.banner2;
    const std::string bf = side == 1 && !ally.empty() ? ally : fid_;
    const Col col = model_.faction_color(bf, false), acc = model_.faction_color(bf, true);
    const float x = static_cast<float>(r.x), y = static_cast<float>(r.y), w = static_cast<float>(r.w), h = static_cast<float>(r.h);
    const float tip = std::max(6.0f, std::floor(w / 2));
    const std::vector<std::pair<float, float>> pts = {{x + 1, y + 3}, {x + w - 2, y + 3}, {x + w - 2, y + h - tip},
                                                      {x + std::floor(w / 2), y + h - 2}, {x + 1, y + h - tip}};
    polygon(pts, shade(col, 0.55f));
    for (float yy = 6; yy < h - tip; yy += 6) rect(x + 3, y + yy, w - 7, 1, shade(col, 0.47f));
    for (size_t i = 0; i < pts.size(); ++i) {
      const auto& a = pts[i];
      const auto& b = pts[(i + 1) % pts.size()];
      line(a.first, a.second, b.first, b.second, acc, 2);
    }
    rect(x, y + 1, w, 2, th.light);   // rúd
    if (const Img* em = image("emblems/" + bf)) {
      blit(*em, x + 2, y + std::floor(h / 2) - std::floor((w - 4) / 2) - std::floor(tip / 2), w - 4, w - 4);
    }
  }
  for (const Rect& fr : L.frames) section_frame(fr);
  rect(static_cast<float>(L.divider), static_cast<float>(L.char_sheet.y), 1, static_cast<float>(L.char_sheet.h), {6, 6, 6});
  rect(static_cast<float>(L.divider + 1), static_cast<float>(L.char_sheet.y), 1, static_cast<float>(L.char_sheet.h), shade(th.light, 0.5f));
  draw_minimap(f);
  draw_char_sheet();
  draw_middle();
  draw_sel_grid();
  draw_cmd_grid();
}

void Hud::draw_minimap(const HudFrame& f) {
  const HudLayout& L = model_.layout();
  const Rect& r = L.minimap;
  const Snapshot* s = model_.snap();
  if (mini_.ok) blit(mini_, static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h));
  else rect(r, {22, 34, 22});
  const float x0 = static_cast<float>(r.x), y0 = static_cast<float>(r.y);
  for (const MiniMark& mk : model_.minimap_marks()) rect(x0 + mk.x, y0 + mk.y, mk.w, mk.h, mk.col);
  if (s->fog && s->fog->size() == static_cast<size_t>(map_w_) * static_cast<size_t>(map_h_)) {   // köd: soronként szakaszok
    const float cw = static_cast<float>(r.w) / static_cast<float>(map_w_), ch = static_cast<float>(r.h) / static_cast<float>(map_h_);
    for (int y = 0; y < map_h_; ++y) {
      int start = 0;
      uint8_t cur = (*s->fog)[static_cast<size_t>(y) * static_cast<size_t>(map_w_)];
      for (int x = 1; x <= map_w_; ++x) {
        const uint8_t v = x < map_w_ ? (*s->fog)[static_cast<size_t>(y) * static_cast<size_t>(map_w_) + static_cast<size_t>(x)] : 1;
        if (x < map_w_ && v == cur) continue;
        if (cur != 255) {
          rect(x0 + static_cast<float>(start) * cw, y0 + static_cast<float>(y) * ch, static_cast<float>(x - start) * cw, ch, {0, 0, 0},
               cur == 0 ? 235 : 120);
        }
        start = x;
        cur = v;
      }
    }
  }
  if (f.cam_w > 0) {   // a látott rész (a minimapre vágva)
    const Rect cr = model_.minimap_view(f.cam_x, f.cam_y, f.cam_w, f.cam_h);
    if (cr.w > 0) outline(cr, {255, 255, 255});
  }
}

void Hud::draw_char_sheet() {
  const HudLayout& L = model_.layout();
  const Snapshot* s = model_.snap();
  const sim::Rules* rules = model_.rules();
  const Rect& pr = L.portrait;
  const Rect& st = L.stats;
  well(pr);
  well(L.hpbar, {8, 8, 8});
  well(L.mpbar, {8, 8, 8});
  const SnapUnit* e = model_.primary();
  const int P = pr.w;
  float x = static_cast<float>(st.x + 2), y = static_cast<float>(st.y + 2);
  const float w = static_cast<float>(st.w - 4);
  const int fname = L.font.name, fsmall = L.font.small, ftiny = L.font.tiny;
  if (!e) {   // nincs kijelölés: faj-összefoglaló
    if (const Img* em = image("emblems/" + fid_)) {
      blit(*em, static_cast<float>(pr.x + 4), static_cast<float>(pr.y + 4), static_cast<float>(P - 8), static_cast<float>(P - 8));
    }
    text(*batch_, fname, x, y, model_.faction_name(fid_), 240, 220, 160);
    y += line_h(fname) + 2;
    const std::string tag = model_.name("faction." + fid_ + ".tagline");
    if (tag != "faction." + fid_ + ".tagline") y = wrap(tag, x, y, w, ftiny, model_.faction_color(fid_, true), 2) + 4;
    wrap(model_.name("faction." + fid_ + ".desc"), x, y, w, ftiny, {170, 170, 160},
         std::max(1, static_cast<int>((static_cast<float>(st.bottom()) - y) / line_h(ftiny))));
    return;
  }
  if (e->kind == SnapUnit::kResource) {
    static const char* kIcon[] = {"wood", "stone", "meteor", "faith"};
    if (const Img* im = image(std::string("ui/res/") + kIcon[std::min<int>(e->res, 3)])) blit_fit(*im, pr, false);
    const Col rc = model_.res_color(e->res);
    text(*batch_, fname, x, y, model_.res_name(e->res), rc[0], rc[1], rc[2]);
    y += line_h(fname) + 4;
    text(*batch_, fsmall, x, y, model_.tr("stock", {std::to_string(e->amount)}), 220, 220, 210);
    if (e->res == sim::kGold) {
      y += line_h(fsmall) + 2;
      wrap(model_.tr("res.gold_note"), x, y, w, ftiny, {170, 170, 160}, 3);
    }
    return;
  }
  const sim::EntityDef* d = model_.def_of(*e);
  if (!d) return;
  const bool own = e->owner == model_.player;
  const Col team = model_.player_color(e->owner);
  if (const Img* im = portrait(d->id)) blit_fit(*im, pr);
  else def_icon(e->def, pr, team);
  team_badge(pr, team);   // melyik csapat tagja
  outline(pr, own ? Col{200, 170, 100} : Col{220, 90, 80});
  if (d->hero) corner_label(pr, std::to_string(e->hero_level), {255, 230, 150}, fname, "tr");
  if (e->life_until > 0) {
    corner_label(pr, model_.tr("seconds", {std::to_string(std::max<int64_t>(0, (e->life_until - s->tick) / 30))}),
                 {255, 220, 130}, fsmall, "br");
  }
  const std::string tkey = model_.unit_type(*e);
  if (!tkey.empty()) {   // az egység típusa
    const int isz = std::max(16, P / 5);
    type_icon(tkey, {pr.x + 3, pr.y + 3, isz, isz});
  }
  // név és típus: ami nem fér el, kisebb betűvel; a hősök mellékneve külön sorban
  std::string nm = model_.unit_name(*e), epithet;
  int fn = fname;
  if (tw(fn, nm) > w && nm.find(", ") != std::string::npos) {
    epithet = nm.substr(nm.find(", ") + 2);
    nm = nm.substr(0, nm.find(", "));
  }
  if (tw(fn, nm) > w && tw(fsmall, nm) <= w) fn = fsmall;
  const Col namec = own ? Col{240, 220, 160} : Col{255, 140, 120};
  text(*batch_, fn, x, y, ellipsize(nm, fn, w), namec[0], namec[1], namec[2]);
  y += line_h(fn);
  if (!epithet.empty()) {
    text(*batch_, ftiny, x, y, ellipsize(epithet, ftiny, w), 214, 196, 150);
    y += line_h(ftiny);
  }
  std::string typ;
  const std::string ofid = s->players[std::min<size_t>(e->owner, sim::Sim::kMaxPlayers)].faction;
  if (!own) typ = model_.tr("owner", {std::to_string(e->owner), model_.faction_name(ofid)});
  else if (e->kind == SnapUnit::kBuilding) typ = e->complete ? model_.tr("building_kind") : model_.tr("building_progress", {std::to_string(e->progress / 10)});
  else if (d->hero) typ = model_.tr("hero_level", {std::to_string(e->hero_level)});
  else typ = d->worker ? model_.tr("worker") : (!tkey.empty() ? model_.type_info(tkey).first : model_.tr("unit"));
  if (const std::string rk = model_.rank_name(*e); !rk.empty()) {   // előléptetés (sim::Veteran)
    typ += " • " + rk;
    for (int k = 0; k < e->rank && k < 3; ++k) {   // csíkok a portré bal alsó sarkában
      const float cx = static_cast<float>(pr.x + 4), cy = static_cast<float>(pr.bottom() - 6 - std::max(4, pr.h / 10) - 6 - 5 * k), cw = static_cast<float>(std::max(10, P / 6));
      polygon({{cx, cy}, {cx + cw / 2, cy - 4}, {cx + cw, cy}, {cx + cw, cy + 2}, {cx + cw / 2, cy - 2}, {cx, cy + 2}}, {255, 214, 110});
    }
  }
  size_t n_sel = 0;
  for (const sim::EntityId id : model_.sel) n_sel += model_.unit(id) ? 1u : 0u;
  if (n_sel > 1 && tw(ftiny, typ + "  (" + std::to_string(n_sel) + ")") <= w) typ += "  (" + std::to_string(n_sel) + ")";
  const Col acc = model_.faction_color(ofid, true);
  text(*batch_, ftiny, x, y, typ, acc[0], acc[1], acc[2]);
  y += line_h(ftiny) + 3;
  // a portré alatt: életerő, alatta energia (vagy pajzs) – csak ha az egység használja
  const Rect& hb = L.hpbar;
  const Rect& mb = L.mpbar;
  hbar(static_cast<float>(hb.x), static_cast<float>(hb.y), static_cast<float>(hb.w), static_cast<float>(hb.h),
       static_cast<float>(e->hp) / static_cast<float>(std::max(1, e->max_hp)), {80, 210, 80},
       std::to_string(e->hp) + "/" + std::to_string(e->max_hp));
  const std::vector<CharBar> second = model_.char_bars(*e);   // energia, pajzs, harci kedv
  if (!second.empty()) hbar(static_cast<float>(mb.x), static_cast<float>(mb.y), static_cast<float>(mb.w), static_cast<float>(mb.h),
                            second[0].frac, second[0].col, second[0].label);
  const float bh = static_cast<float>(std::max(6, mb.h / 2 + 1));
  for (size_t i = 1; i < second.size(); ++i) {   // a további sávok az adatoszlopba kerülnek
    hbar(x, y, w, bh + 4, second[i].frac, second[i].col, second[i].label);
    y += bh + 7;
  }
  if (d->hero && e->kind == SnapUnit::kUnit) {   // tapasztalat
    const auto& lv = rules->heroes.levels;
    const size_t l = e->hero_level;
    float frac = 1.0f;
    if (l < lv.size() && l >= 1) {
      frac = static_cast<float>(e->xp - lv[l - 1]) / static_cast<float>(std::max(1, lv[l] - lv[l - 1]));
    }
    hbar(x, y, w, bh, frac, {230, 190, 70}, "");
    y += bh + 5;
  }
  // alapadatok: támadás, páncél, mozgás, látótáv
  std::vector<std::pair<std::string, std::string>> stats;
  if (e->damage > 0) stats.push_back({model_.tr("stat.attack"), std::to_string(e->damage)});
  stats.push_back({model_.tr("stat.armor"), std::to_string(e->armor)});
  if (e->kind == SnapUnit::kUnit) stats.push_back({model_.tr("stat.speed"), fmt("%.3g", e->speed)});
  if (d->sight > 0) stats.push_back({model_.tr("stat.sight"), std::to_string(d->sight)});
  const float row_h = draw_counter_row(*e, tkey, x, static_cast<float>(st.bottom()), w);
  const float bottom = static_cast<float>(st.bottom()) - row_h;
  float need = 0;
  for (const auto& [n, v] : stats) need = std::max(need, tw(ftiny, n) + tw(ftiny, v) + 18);
  const int ncol = need <= w / 2 ? 2 : 1;   // széles betűnél egy oszlop
  const float cw = std::floor(w / static_cast<float>(ncol)), lh = line_h(ftiny);
  for (size_t i = 0; i < stats.size(); ++i) {
    const float cx = x + static_cast<float>(i % static_cast<size_t>(ncol)) * cw, cy = y + static_cast<float>(i / static_cast<size_t>(ncol)) * lh;
    if (cy + lh > bottom - 2) break;
    text(*batch_, ftiny, cx, cy, stats[i].first, 160, 160, 150);
    text(*batch_, ftiny, cx + cw - tw(ftiny, stats[i].second) - 8, cy, stats[i].second, 240, 236, 210);
  }
  y += lh * static_cast<float>((stats.size() + static_cast<size_t>(ncol) - 1) / static_cast<size_t>(ncol)) + 2;
  if (e->kind == SnapUnit::kUnit) draw_effects_row(*e, x, y, w, bottom);   // mi hat rá éppen
  const std::string info = model_.status_text(*e);   // állapotsor: mit csinál
  if (!info.empty() && y + lh <= bottom) wrap(info, x, y, w, ftiny, {170, 210, 170}, std::max(1, static_cast<int>((bottom - y) / lh)));
}

void Hud::draw_effects_row(const SnapUnit& e, float x, float& y, float w, float bottom) {
  const auto eff = model_.active_effects(e);
  if (eff.empty()) return;
  const int ft = model_.layout().font.tiny;
  const float lh = line_h(ft);
  if (y + lh > bottom) return;
  const std::string head = model_.tr("effects");
  text(*batch_, ft, x, y, head, 160, 160, 150);
  float cx = x + tw(ft, head) + 6;
  for (size_t i = 0; i < eff.size(); ++i) {
    std::string lab = eff[i].left < 600 ? model_.tr("effect_left", {eff[i].name, fmt("%.0f", eff[i].left)}) : eff[i].name;
    lab = ellipsize(lab, ft, std::max(1.0f, w - 8));
    const float ew = std::min(w, tw(ft, lab) + 8);
    if (cx + ew > x + w) {
      if (y + 2 * lh + 2 > bottom) {   // nincs több sor: „+N” címke
        const std::string more = "+" + std::to_string(eff.size() - i);
        const float mw = tw(ft, more) + 8, mx = std::min(cx, x + w - mw);
        const Rect r{static_cast<int>(mx), static_cast<int>(y), static_cast<int>(mw), static_cast<int>(lh)};
        rect(r, {24, 20, 16});
        outline(r, {220, 210, 180});
        text(*batch_, ft, mx + 4, y, more, 230, 220, 190);
        break;
      }
      cx = x;
      y += lh + 2;
    }
    const Rect r{static_cast<int>(cx), static_cast<int>(y), static_cast<int>(ew), static_cast<int>(lh)};
    rect(r, {24, 20, 16});
    outline(r, eff[i].col);
    text(*batch_, ft, cx + 4, y, lab, eff[i].col[0], eff[i].col[1], eff[i].col[2]);
    cx += ew + 4;
  }
  y += lh + 3;
}

float Hud::draw_counter_row(const SnapUnit& e, const std::string& key, float x, float bottom, float w) {
  // a karakterlap alján: ▲ (zöld) ami ellen erős, ▼ (piros) ami ellen gyenge
  if (key.empty()) return 0;
  const auto [strong, weak] = model_.counters(e, key);
  if (strong.empty() && weak.empty()) return 0;
  const int ft = model_.layout().font.tiny;
  float isz = std::clamp(line_h(ft) + 8, 16.0f, 34.0f);
  const float n = static_cast<float>(strong.size() + weak.size()), gap = 2;
  const float avail = w - 2 * (std::floor(isz / 2) + 4);
  if (n * (isz + gap) > avail) isz = std::max(10.0f, std::floor(avail / std::max(1.0f, n)) - gap);
  const float y = bottom - isz - 2;
  float cx = x;
  auto tri = [&](bool up, Col c) {
    const float h = isz * 0.6f, my = y + isz / 2;
    if (up) polygon({{cx, my + h / 2}, {cx + h, my + h / 2}, {cx + h / 2, my - h / 2}}, c);
    else polygon({{cx, my - h / 2}, {cx + h, my - h / 2}, {cx + h / 2, my + h / 2}}, c);
    cx += std::floor(h) + 3;
  };
  const int is = static_cast<int>(isz);
  if (!strong.empty()) {
    tri(true, {110, 220, 110});
    for (const auto& kv : strong) {
      type_icon(kv.first, {static_cast<int>(cx), static_cast<int>(y), is, is});
      cx += isz + gap;
    }
    cx += 4;
  }
  if (!weak.empty()) {
    tri(false, {235, 100, 90});
    for (const auto& kv : weak) {
      type_icon(kv.first, {static_cast<int>(cx), static_cast<int>(y), is, is});
      cx += isz + gap;
    }
  }
  return isz + 4;
}

void Hud::draw_middle() {
  // 2×3: hősnél a 6 tárgyhely, minden más esetben a fajcímer
  const HudLayout& L = model_.layout();
  const auto regs = model_.middle_regions();
  if (!regs.empty() && regs[0].kind == "emblem") {
    const Rect& r = L.inv;
    well(r, {16, 16, 18});
    const SnapUnit* e = model_.primary();
    std::string fid = fid_;
    if (e && e->kind != SnapUnit::kResource && e->owner > 0) fid = model_.snap()->players[std::min<size_t>(e->owner, sim::Sim::kMaxPlayers)].faction;
    if (const Img* em = image("emblems/" + fid)) {
      const int sz = std::min(r.w, r.h) - 8;
      blit(*em, static_cast<float>(r.centerx() - sz / 2), static_cast<float>(r.centery() - sz / 2), static_cast<float>(sz), static_cast<float>(sz));
    }
    return;
  }
  for (const Rect& c : L.inv_cells) well(c);
  for (const HudRegion& rg : regs) {
    if (const Img* im = image("items/" + rg.data)) blit(*im, static_cast<float>(rg.r.x), static_cast<float>(rg.r.y), static_cast<float>(rg.r.w), static_cast<float>(rg.r.h));
    else text(*batch_, L.font.tiny, static_cast<float>(rg.r.x + 3), static_cast<float>(rg.r.y + 3), rg.tip_title.substr(0, 4), 230, 220, 190);
  }
}

void Hud::draw_sel_grid() {
  const HudLayout& L = model_.layout();
  const Snapshot* s = model_.snap();
  const sim::Rules* rules = model_.rules();
  const auto cells = model_.sel_cells();
  for (size_t i = 0; i < L.sel_cells.size(); ++i) {
    const Rect& r = L.sel_cells[i];
    well(r);
    if (i >= cells.size()) continue;
    const SelCell& c = cells[i];
    const SnapUnit* e0 = model_.unit(c.ents[0]);
    if (!e0) continue;
    if (c.kind == SelCell::Queue) {   // gyártósor
      const size_t qi = static_cast<size_t>(c.idx);
      const bool res = (e0->queue_research >> c.idx) & 1;
      const uint16_t q = e0->queue_def[qi];
      if (res) research_icon(q, r);
      else def_icon(q, r, model_.player_color(model_.player));
      const float prog = c.idx == 0 ? static_cast<float>(e0->queue_progress) / 1000.0f : 0.0f;
      clock_wipe(r, prog);
      if (c.idx == 0) {
        const int32_t time = res ? (q < rules->researches.size() ? rules->researches[q].time_ticks : 0)
                                 : (q < rules->defs.size() ? rules->defs[q].time_ticks : 0);
        corner_label(r, fmt("%.0f", std::max(0.0, time * (1.0 - prog) / 30.0)), {255, 230, 150}, L.font.tiny);
      }
      continue;
    }
    if (c.kind == SelCell::More) {
      const std::string t = "+" + std::to_string(c.ents.size());
      text(*batch_, L.font.name, static_cast<float>(r.centerx()) - tw(L.font.name, t) / 2,
           static_cast<float>(r.centery()) - line_h(L.font.name) / 2, t, 220, 220, 220);
      continue;
    }
    def_icon(e0->def, r, model_.player_color(e0->owner));
    float avg = 0;
    int alive = 0;
    for (const sim::EntityId id : c.ents) {
      if (const SnapUnit* u = model_.unit(id); u && u->hp > 0) {
        avg += static_cast<float>(u->hp) / static_cast<float>(std::max(1, u->max_hp));
        ++alive;
      }
    }
    avg /= static_cast<float>(std::max(1, alive));
    const Col col = avg > 0.5f ? Col{80, 210, 80} : avg > 0.25f ? Col{235, 170, 50} : Col{230, 60, 50};
    const int hb = std::max(3, py_round(4 * L.s));
    rect(static_cast<float>(r.x + 2), static_cast<float>(r.bottom() - hb - 2), static_cast<float>(r.w - 4), static_cast<float>(hb), {0, 0, 0});
    rect(static_cast<float>(r.x + 2), static_cast<float>(r.bottom() - hb - 2), static_cast<float>(r.w - 4) * avg, static_cast<float>(hb), col);
    team_badge({r.x, r.y, r.w, r.h - hb - 2}, model_.player_color(e0->owner));   // a csapat jele (az életsáv fölött)
    if (avg <= 0.35f) outline(r, {200, 70, 50});   // veszélyben: visszafogott piros keret
    if (c.kind == SelCell::Group && alive > 1) {
      corner_label({r.x, r.y, r.w, r.h - hb - 2}, std::to_string(alive) + "×", {255, 255, 255}, L.font.key);
    }
    std::vector<int64_t> left;
    for (const sim::EntityId id : c.ents) {
      if (const SnapUnit* u = model_.unit(id); u && u->life_until > 0) left.push_back((u->life_until - s->tick) / 30);
    }
    if (!left.empty()) {
      const int64_t m = *std::min_element(left.begin(), left.end());
      corner_label(r, model_.tr("seconds", {std::to_string(std::max<int64_t>(0, m))}), m <= 5 ? Col{255, 150, 100} : Col{255, 220, 130},
                   L.font.tiny, "tr");
    }
  }
}

void Hud::draw_cmd_grid() {
  const HudLayout& L = model_.layout();
  const CommandCard slots = model_.command_card();
  for (size_t i = 0; i < L.cmd_cells.size(); ++i) {
    const Rect& r = L.cmd_cells[i];
    well(r);
    const auto& sl = slots[i];
    if (!sl) continue;   // nincs akció: üres hely, nincs betű
    cmd_icon(*sl, r);
    if (!sl->enabled || sl->passive) rect(r, {0, 0, 0}, sl->passive ? 120 : 150);
    if (sl->prog >= 0) clock_wipe(r, sl->prog);
    if (sl->cd > 0) clock_wipe(r, 1 - sl->cd);
    if (model_.slot_flash == static_cast<int>(i) && t_ - model_.slot_flash_t < 0.18) {   // ikonvillanás
      outline(r, {255, 255, 255}, 2);
    } else if (sl->active) {
      outline(r, {255, 214, 110}, 2);
    } else {
      outline(r, sl->passive ? Col{90, 90, 96} : Col{150, 130, 90});
    }
    if (!sl->cost.empty()) corner_label(r, sl->cost, sl->cost_col, L.font.tiny, "tr");
    if (sl->count > 1) corner_label(r, std::to_string(sl->count), {255, 255, 255}, L.font.tiny, "tl");
    corner_label(r, std::string(1, kSlotKeys[i]), {255, 244, 210}, L.font.key);   // a billentyű mindig látszik
  }
}

void Hud::draw_topbar(const HudFrame& f) {
  const HudLayout& L = model_.layout();
  const Snapshot* s = model_.snap();
  const SnapPlayer& me = s->players[std::min<size_t>(model_.player, sim::Sim::kMaxPlayers)];
  // tétlen munkások: jobbra, közvetlenül a HUD fölött
  {
    const Rect& r = L.idle;
    const size_t n = model_.idle_workers().size();
    bevel(r, r.contains(f.mx, f.my) ? Col{60, 52, 36} : Col{26, 24, 20});
    type_icon("munkas", {r.x + 3, r.y + 3, r.h - 6, r.h - 6});
    const std::string t = std::to_string(n);
    const Col c = n ? Col{255, 210, 90} : Col{150, 146, 136};
    text(*batch_, L.font.res, static_cast<float>(r.right() - std::max(5, r.h / 5)) - tw(L.font.res, t),
         static_cast<float>(r.centery()) - line_h(L.font.res) / 2, t, c[0], c[1], c[2]);
  }
  for (const char* key : {"allies", "gear"}) {
    const Rect& r = std::string(key) == "allies" ? L.allies : L.gear;
    bevel(r, r.contains(f.mx, f.my) ? Col{60, 52, 36} : Col{26, 24, 20});
    if (const Img* art = image(std::string("ui/icons/") + key)) {
      const float sz = std::round(static_cast<float>(r.h) * 0.92f);
      blit(*art, static_cast<float>(r.centerx()) - sz / 2, static_cast<float>(r.centery()) - sz / 2, sz, sz);
    }
  }
  const Rect& rb = L.resbar;
  bevel(rb, {20, 20, 22});
  rect(rb.inflate(-6, -6), {6, 6, 6});   // sötét, süllyesztett sáv: a számok jól olvashatók
  const int fr = L.font.res;
  static const char* kResImg[] = {"wood", "stone", "meteor", "faith"};
  for (size_t i = 0; i < 4; ++i) {
    const Rect& r = L.res[i];
    if (i) rect(static_cast<float>(r.x), static_cast<float>(r.y + 6), 1, static_cast<float>(r.h - 13), {70, 64, 52});
    std::string txt;
    Col col{250, 240, 170};
    std::string icon;
    if (i < 3) {
      const int k = kResOrder[i];
      txt = std::to_string(me.res[static_cast<size_t>(k)]);
      const double fl = t_ - model_.res_flash[static_cast<size_t>(k)];
      if (fl < 1.6) {   // hiány: diszkrét, elhalványuló piros jelzés
        rect(static_cast<float>(r.x + 4), static_cast<float>(r.y + 4), static_cast<float>(r.w - 8), static_cast<float>(r.h - 8), {200, 40, 30},
             static_cast<uint8_t>(100 * (1 - fl / 1.6)));
        col = {255, 140, 110};
      }
      icon = std::string("ui/res/") + kResImg[k];
    } else {
      const bool full = me.supply_used >= me.supply_cap;
      const float uf = static_cast<float>(model_.rules()->upkeep_permille(me.supply_used)) / 1000.0f;   // Hadtáp
      col = full ? Col{255, 120, 90} : uf >= 1 ? Col{250, 240, 170} : uf >= 0.7f ? Col{255, 210, 90} : Col{255, 150, 70};
      txt = std::to_string(me.supply_used) + "/" + std::to_string(me.supply_cap);
      icon = "ui/res/supply_" + (fid_ == "micelia" ? std::string("mycel") : fid_);
    }
    const int sz = r.h - std::max(8, r.h / 3);
    if (const Img* im = image(icon)) blit(*im, static_cast<float>(r.x + std::max(6, r.h / 4)), static_cast<float>(r.centery() - sz / 2), static_cast<float>(sz), static_cast<float>(sz));
    text(*batch_, fr, static_cast<float>(r.right() - std::max(6, r.h / 4)) - tw(fr, txt), static_cast<float>(r.centery()) - line_h(fr) / 2,
         txt, col[0], col[1], col[2]);
  }
  if (model_.has_faith()) {   // Áhítat / Tóének
    const Rect& r = L.faith;
    bevel(r, {20, 20, 22});
    rect(r.inflate(-6, -6), {6, 6, 6});
    const int sz = r.h - std::max(8, r.h / 3);
    if (const Img* im = image("ui/res/faith")) blit(*im, static_cast<float>(r.x + std::max(6, r.h / 4)), static_cast<float>(r.centery() - sz / 2), static_cast<float>(sz), static_cast<float>(sz));
    const std::string v = std::to_string(me.res[sim::kFaith]);
    text(*batch_, fr, static_cast<float>(r.right() - std::max(6, r.h / 4)) - tw(fr, v), static_cast<float>(r.centery()) - line_h(fr) / 2, v, 255, 226, 150);
  }
  if (model_.show_allies) draw_allies_panel();
}

void Hud::draw_allies_panel() {
  const HudLayout& L = model_.layout();
  const Snapshot* s = model_.snap();
  const int F = L.font.small, ft = L.font.tiny;
  std::vector<int> rows;
  for (int i = 1; i <= sim::Sim::kMaxPlayers; ++i) {
    if (!s->players[static_cast<size_t>(i)].faction.empty()) rows.push_back(i);
  }
  const float lh = line_h(F) + 4;
  const int w = py_round(360 * L.s) + 60;
  const Rect r{L.topbar.centerx() - w / 2, L.topbar.bottom() + 6, w, static_cast<int>(lh * static_cast<float>(rows.size())) + 16};
  bevel(r, {16, 16, 18});
  float y = static_cast<float>(r.y + 8);
  for (const int q : rows) {
    const SnapPlayer& p = s->players[static_cast<size_t>(q)];
    const bool me = q == model_.player;
    const std::string rel = me ? model_.tr("rel.you") : model_.tr("rel.enemy");
    const Col col = me ? Col{150, 230, 150} : Col{240, 140, 120};
    rect(static_cast<float>(r.x + 10), y + 3, 12, lh - 10, model_.player_color(static_cast<uint8_t>(q)));
    std::string nm = model_.tr("player", {std::to_string(q)});
    if (p.defeated) nm += model_.tr("defeated_mark");
    const Col nc = p.defeated ? Col{130, 130, 130} : Col{230, 226, 210};
    text(*batch_, F, static_cast<float>(r.x + 30), y, nm, nc[0], nc[1], nc[2]);
    text(*batch_, ft, static_cast<float>(r.x + 30 + w / 2 - 40), y + 2, model_.faction_name(p.faction), 180, 176, 160);
    text(*batch_, ft, static_cast<float>(r.right() - 10) - tw(ft, rel), y + 2, rel, col[0], col[1], col[2]);
    y += lh;
  }
}

void Hud::draw_quests() {
  // bal felül: háttér nélküli, árnyékolt célkitűzés-lista jelölőnégyzetekkel
  const HudLayout& L = model_.layout();
  const float x = static_cast<float>(L.quest_x);
  float y = static_cast<float>(L.quest_y);
  const int fr = L.font.res, F = L.font.small;
  const float lh = line_h(F) + 3, box = std::max(8.0f, lh - 9);
  const std::string head = model_.tr("quests");
  stext(fr, x, y, head, {255, 214, 120});
  const float k = std::max(4.0f, std::floor(line_h(fr) / 4)), cx = x + tw(fr, head) + 6 + k, cy = y + std::floor(line_h(fr) / 2);
  polygon({{cx - k + 1, cy - std::floor(k / 2) + 1}, {cx + k + 1, cy - std::floor(k / 2) + 1}, {cx + 1, cy + k + 1}}, {0, 0, 0});
  polygon({{cx - k, cy - std::floor(k / 2)}, {cx + k, cy - std::floor(k / 2)}, {cx, cy + k}}, {220, 210, 170});
  y += line_h(fr) + 2;
  if (model_.snap()->mission.active) {   // M11: a küldetés céljai (kész: zöld pipa, elbukott: piros)
    for (const auto& o : model_.snap()->mission.objectives) {
      const Col col = o.state == 2 ? Col{150, 235, 150} : o.state == 3 ? Col{235, 110, 90} : Col{240, 236, 214};
      const Rect r{static_cast<int>(x), static_cast<int>(y + (lh - box) / 2 - 1), static_cast<int>(box), static_cast<int>(box)};
      outline({r.x + 1, r.y + 1, r.w, r.h}, {0, 0, 0});
      outline(r, col);
      if (o.state == 2) rect(static_cast<float>(r.x + 2), static_cast<float>(r.y + 2), static_cast<float>(r.w - 4), static_cast<float>(r.h - 4), col);
      const std::string t = model_.name(o.text) + (o.kind == 1 ? " " + model_.tr("quest.optional") : std::string());
      stext(F, x + box + 8, y, t, col);
      y += lh;
    }
    return;
  }
  const bool done = model_.snap()->winner == model_.player;
  const Col col = done ? Col{150, 235, 150} : Col{240, 236, 214};
  const Rect r{static_cast<int>(x), static_cast<int>(y + (lh - box) / 2 - 1), static_cast<int>(box), static_cast<int>(box)};
  outline({r.x + 1, r.y + 1, r.w, r.h}, {0, 0, 0});
  outline(r, col);
  stext(F, x + box + 8, y, model_.tr("quest.defeat_all"), col);
}

void Hud::draw_clock(const HudFrame& f) {
  // jobb felül, visszafogottan: a játékidő
  const HudLayout& L = model_.layout();
  const int F = L.font.small;
  std::string t = mmss(model_.snap()->tick / 30);
  // M12: a mód visszajelzése (béke hátralévő ideje, a domb állása, a túlélés hulláma és hátralévő ideje)
  const sim::MatchMode& md = model_.snap()->mode;
  const int64_t now = model_.snap()->tick;
  if (md.peace_until > now) t = model_.tr("mode.peace", {mmss((md.peace_until - now) / 30)}) + "   " + t;
  if (md.victory == 2) {
    const int64_t mine = md.hill[std::min<size_t>(model_.player, 8)] / 30;
    t = model_.tr("mode.hill", {mmss(mine), mmss(int64_t{md.minutes} * 60)}) + "   " + t;
  }
  if (md.victory == 3) t = model_.tr("mode.timed", {mmss(std::max<int64_t>(0, int64_t{md.minutes} * 60 - now / 30))}) + "   " + t;
  if (md.tempo == 3) {
    t = model_.tr("mode.survival", {std::to_string(md.wave), mmss(std::max<int64_t>(0, int64_t{md.minutes} * 60 - now / 30))}) + "   " + t;
  }
  // a régi `draw_clock`: jobbról balra [+] ×1 [−] óra; alatta apró betűvel „N fps  •  F1: súgó”
  const float th = static_cast<float>(L.topbar.h), ty = static_cast<float>(L.topbar.y);
  float x = static_cast<float>(f.w - L.pad * 2);
  const float bs = th - 6;
  char sp[16];
  std::snprintf(sp, sizeof sp, "×%g", static_cast<double>(model_.speed_pct) / 100.0);
  for (int i = 1; i >= 0; --i) {   // [+], a felirat, [−]
    if (i == 0) {
      x -= tw(F, sp);
      stext(F, x, ty + (th - line_h(F)) / 2, sp, model_.speed_locked ? Col{150, 150, 150} : Col{200, 220, 255});
      x -= 6;
    }
    const Rect r{static_cast<int>(x - bs), static_cast<int>(ty + 3), static_cast<int>(bs), static_cast<int>(bs)};
    model_.speed_btn[static_cast<size_t>(i)] = r;
    const bool hover = r.contains(mx_, my_) && !model_.speed_locked;
    rect(r, hover ? Col{70, 60, 36} : Col{20, 18, 14});
    outline(r, {160, 136, 80});
    const std::string lab = i == 1 ? "+" : "−";
    stext(F, static_cast<float>(r.x) + (bs - tw(F, lab)) / 2, static_cast<float>(r.y) + (bs - line_h(F)) / 2, lab,
          model_.speed_locked ? Col{120, 120, 120} : Col{255, 230, 160});
    x = static_cast<float>(r.x) - 6;
  }
  x -= tw(F, t) + 4;
  stext(F, x, ty + (th - line_h(F)) / 2, t, {240, 235, 200});
  std::string sub = f.fps >= 0 ? std::to_string(f.fps) + " fps" : std::string();
  sub += (sub.empty() ? "" : "  •  ") + model_.tr("help.hint");
  const int FT = L.font.tiny;
  stext(FT, static_cast<float>(f.w - L.pad * 2) - tw(FT, sub), ty + th + 2, sub, {170, 170, 160});
}

std::vector<Rect> Hud::draw_pause(render::SpriteBatch& batch, const render::Texture& white, const HudFrame& f, float slide,
                                  const std::string& title, const std::vector<std::string>& items) {
  batch_ = &batch;
  white_ = &white;
  std::vector<Rect> out;
  const float s = static_cast<float>(model_.layout().s);
  const float H = static_cast<float>(f.h);
  const float sw = std::round(std::max(260.0f, 330.0f * s));
  const float e = 1.0f - (1.0f - slide) * (1.0f - slide) * (1.0f - slide);   // lassuló becsúszás
  const float x0 = -sw * (1.0f - e);
  for (int i = 0; i < 48; ++i) {   // a főmenü oldalsávja: függőleges átmenet, fent kicsit világosabb
    const float g = 1 - static_cast<float>(i) / 48.0f;
    const float y0 = std::floor(H * static_cast<float>(i) / 48.0f), y1 = std::floor(H * static_cast<float>(i + 1) / 48.0f);
    rect(x0, y0, sw, y1 - y0, {static_cast<uint8_t>(16 + 14 * g), static_cast<uint8_t>(12 + 9 * g), static_cast<uint8_t>(18 + 10 * g)}, 232);
  }
  const Col frame{176, 138, 74}, dark{60, 42, 22}, gold{230, 190, 90};
  rect(x0 + sw - 5, 0, 1, H, dark);
  rect(x0 + sw - 4, 0, 2, H, frame);
  rect(x0 + sw - 1, 0, 1, H, dark);
  auto ornament = [&](float cy) {   // középről kifelé halványuló vonal, közepén arany rombusz
    const float cx = x0 + sw / 2, half = (sw - 60 * s) / 2;
    for (int i = 0; i < 12; ++i) {
      const float x1 = half * static_cast<float>(i) / 12.0f, x2 = half * static_cast<float>(i + 1) / 12.0f;
      const auto a = static_cast<uint8_t>(255 * (1 - static_cast<float>(i) / 12.0f));
      rect(cx + x1, cy, x2 - x1, 1, frame, a);
      rect(cx - x2, cy, x2 - x1, 1, frame, a);
    }
    rect(cx - 4 * s, cy - 4 * s, 8 * s, 8 * s, gold);
  };
  const int FT = model_.layout().font.name;
  const int TT = std::max(FT + 8, static_cast<int>(std::lround(34 * s)));
  const float ty = std::floor(H * 0.18f);
  stext(TT, x0 + sw / 2 - tw(TT, title) / 2, ty, title, {240, 220, 160});
  ornament(ty + line_h(TT) + 14 * s);
  const int FI = std::max(FT, static_cast<int>(std::lround(24 * s)));
  const float ih = line_h(FI) + 22 * s;
  float y = ty + line_h(TT) + 40 * s;
  for (const std::string& it : items) {
    const Rect r{static_cast<int>(x0 + 20 * s), static_cast<int>(y), static_cast<int>(sw - 40 * s), static_cast<int>(ih)};
    const bool hot = slide >= 1.0f && r.contains(mx_, my_);
    if (hot) rect(r, {70, 56, 30}, 160);
    stext(FI, static_cast<float>(r.centerx()) - tw(FI, it) / 2, static_cast<float>(r.y) + (ih - line_h(FI)) / 2, it,
          hot ? Col{255, 236, 170} : Col{232, 226, 210});
    out.push_back(r);
    y += ih;
  }
  ornament(y + 10 * s);
  return out;
}

void Hud::draw_heroes(const HudFrame& f) {
  // a régi `draw_portraits`: jobb felül, a felső sáv alatt egymás alatt legfeljebb 4 hős; arany keret (kijelölve
  // világosabb), portré, alul életerő (zöld) és energia (lila) csík, jobb felül szintjelvény, bal felül „+”, ha van
  // elkölthető képességpont
  const HudLayout& L = model_.layout();
  const float k = static_cast<float>(L.s);
  const int size = std::max(42, static_cast<int>(std::lround(54 * k)));
  const int pw = size + 4, ph = size + 13;
  const int x = f.w - pw - std::max(8, static_cast<int>(std::lround(12 * k)));
  int y = L.topbar.bottom() + std::max(20, static_cast<int>(std::lround(24 * k))) + static_cast<int>(line_h(L.font.tiny)) + 4;
  model_.hero_ids.fill(0);
  const auto hs = model_.my_heroes();
  const int Ft = L.font.tiny;
  for (size_t i = 0; i < model_.hero_btn.size(); ++i) {
    model_.hero_btn[i] = {};
    if (i >= hs.size()) continue;
    const SnapUnit& h = *hs[i];
    const Rect r{x, y, pw, ph};
    model_.hero_btn[i] = r;
    model_.hero_ids[i] = h.id;
    const bool sel = model_.sel.size() == 1 && model_.sel[0] == h.id;
    rect(r, sel ? Col{70, 58, 24} : Col{30, 26, 18});
    outline(r, {230, 190, 90}, 1);
    def_icon(h.def, Rect{r.x + 2, r.y + 2, size, size}, model_.player_color(h.owner));
    rect(static_cast<float>(r.x + 2), static_cast<float>(r.bottom() - 9), static_cast<float>(r.w - 4), 7, {10, 10, 10});
    rect(static_cast<float>(r.x + 2), static_cast<float>(r.bottom() - 9),
         static_cast<float>(r.w - 4) * static_cast<float>(h.hp) / static_cast<float>(std::max(1, h.max_hp)), 3, {80, 210, 80});
    if (h.energy_max > 0) {
      rect(static_cast<float>(r.x + 2), static_cast<float>(r.bottom() - 5),
           static_cast<float>(r.w - 4) * static_cast<float>(h.energy) / static_cast<float>(h.energy_max), 3, {150, 100, 255});
    }
    const Rect badge{r.right() - 16, r.y - 4, 20, 16};
    rect(badge, {20, 16, 8});
    outline(badge, {230, 190, 90}, 1);
    const std::string lv = std::to_string(std::max<int>(1, h.hero_level));
    stext(Ft, static_cast<float>(badge.centerx()) - tw(Ft, lv) / 2, static_cast<float>(badge.y) + 1, lv, {255, 230, 150});
    if (h.hero_points > 0) {
      const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(t_) * 5.0f);
      const Rect pr{r.x - 3, r.y - 4, 16, 16};
      rect(pr, {static_cast<uint8_t>(120 + 120 * pulse), 200, 80});
      stext(Ft, static_cast<float>(pr.x) + 4, static_cast<float>(pr.y), "+", {10, 20, 0});
    }
    y += ph + 8;
  }
}

void Hud::draw_help(const HudFrame& f) {
  // a régi `draw_help`: a felső sáv alatt, középen, legfeljebb 1000 (·s) széles sötét panel arany kerettel; cím, sorok
  const HudLayout& L = model_.layout();
  const float s = static_cast<float>(L.s);
  const float pad = 16 * s;
  const float w = std::min(static_cast<float>(f.w) - 40, 1000 * s);
  const float y = static_cast<float>(L.topbar.bottom()) + 10 * s;
  const float avail = static_cast<float>(L.hud.y) - y - 10 * s;
  const int FT = L.font.name, FB = L.font.small;
  std::vector<std::string> lines;
  for (int i = 1; i <= 16; ++i) {
    const std::string key = "help." + std::to_string(i);
    const std::string v = model_.tr(key);
    if (v.empty() || v == "hud." + key) break;
    lines.push_back(v);
  }
  const float h = std::min(avail, 2 * pad + line_h(FT) + 6 * s + static_cast<float>(lines.size()) * line_h(FB));
  const float x = static_cast<float>(f.w) / 2 - w / 2;
  rect(x, y, w, h, {8, 10, 12}, 235);
  outline(Rect{static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)}, {170, 140, 70}, 2);
  stext(FT, x + pad, y + pad, model_.tr("help.title"), {255, 230, 150});
  float yy = y + pad + line_h(FT) + 6 * s;
  for (const auto& ln : lines) {
    if (yy + line_h(FB) > y + h - pad / 2) break;
    const bool indent = !ln.empty() && ln[0] == ' ';
    stext(FB, x + pad + (indent ? 20 * s : 0), yy, indent ? ln.substr(ln.find_first_not_of(' ')) : ln, {220, 220, 210});
    yy += line_h(FB);
  }
}

void Hud::draw_notice(const HudFrame& f) {
  // a régi játék szerint (`ui.py` `draw_hud`): bal oldalt, az alsó sáv fölött, a legújabb alul, kis betű, soronként
  // félig áttetsző fekete sáv (alfa 140), 8 mp-ig, legfeljebb 6 sor; a hibák narancsszínűek (255, 160, 90)
  (void)f;
  const HudLayout& L = model_.layout();
  const int F = L.font.small;
  const float s = static_cast<float>(L.s);
  const float lh = line_h(F) + 4 * s;
  float y = static_cast<float>(L.hud.y) - lh - 6 * s;
  for (auto it = model_.notice_log.rbegin(); it != model_.notice_log.rend(); ++it) {
    if (t_ < it->t || t_ - it->t > 8.0) continue;
    rect(8 * s, y - 1, tw(F, it->text) + 10 * s, line_h(F) + 2, {0, 0, 0}, 140);
    stext(F, 13 * s, y, it->text, it->col);
    y -= lh;
  }
}

void Hud::draw_tooltip(const HudFrame& f) {
  const HudLayout& L = model_.layout();
  std::string title, body, key;
  for (size_t i = 0; i < L.cmd_cells.size(); ++i) {
    if (!L.cmd_cells[i].contains(f.mx, f.my)) continue;
    const CommandCard slots = model_.command_card();
    if (!slots[i]) return;
    title = slots[i]->tip_title.empty() ? slots[i]->label : slots[i]->tip_title;
    body = slots[i]->tip_text;
    key = std::string(1, kSlotKeys[i]);
  }
  if (title.empty()) {
    for (const HudRegion& r : model_.middle_regions()) {
      if (r.r.contains(f.mx, f.my)) {
        title = r.tip_title;
        body = r.tip_text;
      }
    }
  }
  if (title.empty()) {
    for (size_t i = 0; i < 4; ++i) {
      if (!L.res[i].contains(f.mx, f.my)) continue;
      title = i < 3 ? model_.res_name(kResOrder[i]) : model_.tr("supply");
    }
  }
  if (title.empty()) return;
  const int F = L.font.small, ft = L.font.tiny;
  const float w = std::max(220.0f, std::round(300.0f * static_cast<float>(L.s)));
  // magasság: becslés a tördelt sorok számával
  int lines = 0;
  {
    std::string line, word;
    auto add = [&] {
      if (word.empty()) return;
      const std::string t = line.empty() ? word : line + " " + word;
      if (tw(ft, t) > w - 16 && !line.empty()) {
        ++lines;
        line = word;
      } else {
        line = t;
      }
      word.clear();
    };
    for (const char c : body) {
      if (c == ' ') add();
      else word += c;
    }
    add();
    if (!line.empty()) ++lines;
  }
  const float h = 8 + line_h(F) + static_cast<float>(lines) * line_h(ft) + (key.empty() ? 0 : line_h(F) + 2) + 8;
  const float x = std::clamp(f.mx - w / 2, 4.0f, static_cast<float>(f.w) - w - 4);
  const float y = static_cast<float>(L.hud.y) - h - 4;
  const Rect r{static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)};
  rect(r, {16, 14, 12}, 235);
  outline(r, {150, 130, 90});
  text(*batch_, F, x + 8, y + 6, title, 255, 236, 190);
  float yy = wrap(body, x + 8, y + 6 + line_h(F), w - 16, ft, {220, 216, 200}, 20);
  if (!key.empty()) text(*batch_, F, x + 8, yy + 2, model_.tr("key_hint", {key}), 255, 230, 150);
}

void Hud::draw_path_choice(const HudFrame& f) {
  // felemelkedés: 3 irány-kártya egymás mellett (kattintás vagy 1/2/3; Esc = később)
  const HudLayout& L = model_.layout();
  rect(0, 0, static_cast<float>(f.w), static_cast<float>(f.h), {0, 0, 0}, 150);
  const float u = static_cast<float>(f.h) / 720.0f;
  const int big = std::min(32, py_round(24 * u)), med = py_round(17 * u), small = py_round(15 * u), tiny = py_round(14 * u);
  const auto cards = model_.path_cards();
  if (cards.empty()) return;
  const float y0 = static_cast<float>(cards[0].first.y);
  const float head = line_h(big) + line_h(tiny) + 16 * u;
  const std::string title = model_.tr("path.title"), sub = model_.tr("path.sub");
  text(*batch_, big, static_cast<float>(f.w) / 2 - tw(big, title) / 2, y0 - head, title, 255, 220, 130);
  text(*batch_, tiny, static_cast<float>(f.w) / 2 - tw(tiny, sub) / 2, y0 - head + line_h(big) + 4 * u, sub, 230, 220, 200);
  const Col acc = model_.faction_color(fid_, true);
  for (size_t i = 0; i < cards.size(); ++i) {
    const Rect& r = cards[i].first;
    const std::string& pid = cards[i].second;
    const bool hover = r.contains(f.mx, f.my);
    rect(r, hover ? Col{40, 30, 20} : Col{16, 16, 20}, hover ? 245 : 240);
    outline(r, hover ? Col{255, 214, 120} : acc, hover ? 3 : 2);
    const float pad = std::round(12 * u), iw = static_cast<float>(r.w) - 2 * pad;
    float yy = static_cast<float>(r.y) + pad;
    const std::string key = "path." + fid_ + "." + pid;
    text(*batch_, med, static_cast<float>(r.x) + pad, yy, ellipsize(std::to_string(i + 1) + ". " + model_.name(key + ".name"), med, iw), 255, 236, 190);
    yy += line_h(med) + 6 * u;
    yy = wrap(model_.name(key + ".tagline"), static_cast<float>(r.x) + pad, yy, iw, small, {255, 200, 120}, 2) + 8 * u;
    const int room = std::max(1, static_cast<int>((static_cast<float>(r.bottom()) - yy - 20 * u) / line_h(tiny)));
    wrap(model_.name(key + ".desc"), static_cast<float>(r.x) + pad, yy, iw, tiny, {220, 218, 206}, std::min(9, room));
  }
  const std::string hint = model_.tr("path.hint");
  const float hw = tw(tiny, hint) + 24 * u, hh = line_h(tiny) + 10 * u;
  const float hx = static_cast<float>(f.w) / 2 - hw / 2, hy = static_cast<float>(cards[0].first.bottom()) + 12 * u;
  rect(hx, hy, hw, hh, {10, 10, 14}, 230);
  text(*batch_, tiny, hx + 12 * u, hy + 5 * u, hint, 210, 206, 196);
  (void)L;
}

void Hud::draw_result(const HudFrame& f) {
  // a meccs utáni összesítő (a régi `draw_summary`): fülek – áttekintés, gazdaság, katonaság, grafikon; játékosonként
  // egy sor, az oszlop legjobb értéke kiemelve
  model_.sum_tab_btn.clear();
  model_.sum_menu_btn = {};
  model_.sum_diag_btn = {};
  if (!model_.match_over()) return;
  const Snapshot* s = model_.snap();
  const bool mission = s->mission.active;
  const bool won = mission ? s->mission.result == 1 : s->winner == model_.player;
  const HudLayout& L = model_.layout();
  const float u = static_cast<float>(L.s);
  auto k = [&](float v) { return std::round(v * u); };
  const float W = static_cast<float>(f.w), H = static_cast<float>(f.h);
  rect(0, 0, W, H, {0, 0, 0}, 175);
  const float pw = std::min(W - k(40), k(1180)), ph = std::min(H - k(40), k(680));
  const Rect P{static_cast<int>(W / 2 - pw / 2), static_cast<int>(H / 2 - ph / 2), static_cast<int>(pw), static_cast<int>(ph)};
  rect(P, {14, 12, 10}, 236);
  const Col frame{170, 140, 80};
  rect(static_cast<float>(P.x), static_cast<float>(P.y), pw, 2, frame);
  rect(static_cast<float>(P.x), static_cast<float>(P.bottom() - 2), pw, 2, frame);
  rect(static_cast<float>(P.x), static_cast<float>(P.y), 2, ph, frame);
  rect(static_cast<float>(P.right() - 2), static_cast<float>(P.y), 2, ph, frame);
  const int FB = std::max(L.font.name + 6, static_cast<int>(k(30))), FM = std::max(L.font.name, static_cast<int>(k(20)));
  const int FS = L.font.small;
  const std::string head = model_.tr("sum.title");
  text(*batch_, FB, static_cast<float>(P.x) + k(20), static_cast<float>(P.y) + k(14), head, 255, 220, 130);
  const int64_t secs = s->tick / 30;
  char clock[32];
  std::snprintf(clock, sizeof clock, "%lld:%02lld", static_cast<long long>(secs / 60), static_cast<long long>(secs % 60));
  const std::string sub = model_.tr(won ? "sum.won" : "sum.lost", {clock});
  text(*batch_, FS, static_cast<float>(P.right()) - k(20) - tw(FS, sub), static_cast<float>(P.y) + k(22), sub, 220, 210, 190);
  // fülek
  const char* tabs[] = {"sum.tab.overview", "sum.tab.economy", "sum.tab.military", "sum.tab.graph"};
  float tx = static_cast<float>(P.x) + k(20);
  const float ty = static_cast<float>(P.y) + k(14) + line_h(FB) + k(10);
  for (int i = 0; i < 4; ++i) {
    const std::string nm = model_.tr(tabs[i]);
    const Rect r{static_cast<int>(tx), static_cast<int>(ty), static_cast<int>(tw(FM, nm) + k(28)), static_cast<int>(line_h(FM) + k(12))};
    const bool on = i == model_.sum_tab, hot = r.contains(mx_, my_);
    rect(r, on ? Col{60, 46, 26} : hot ? Col{40, 34, 26} : Col{24, 20, 16});
    const Col bc = on ? Col{200, 166, 90} : Col{110, 92, 60};
    rect(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), 1, bc);
    rect(static_cast<float>(r.x), static_cast<float>(r.bottom() - 1), static_cast<float>(r.w), 1, bc);
    rect(static_cast<float>(r.x), static_cast<float>(r.y), 1, static_cast<float>(r.h), bc);
    rect(static_cast<float>(r.right() - 1), static_cast<float>(r.y), 1, static_cast<float>(r.h), bc);
    if (on) text(*batch_, FM, static_cast<float>(r.x) + k(14), static_cast<float>(r.y) + k(6), nm, 255, 236, 190);
    else text(*batch_, FM, static_cast<float>(r.x) + k(14), static_cast<float>(r.y) + k(6), nm, 200, 190, 170);
    model_.sum_tab_btn.push_back(r);
    tx = static_cast<float>(r.right()) + k(6);
  }
  const float by = ty + line_h(FM) + k(12) + k(20);
  const Rect body{P.x + static_cast<int>(k(20)), static_cast<int>(by), P.w - static_cast<int>(k(40)),
                  P.bottom() - static_cast<int>(by) - static_cast<int>(k(76))};
  rect(static_cast<float>(P.x) + k(12), static_cast<float>(body.y) - k(8), pw - k(24), 1, {110, 92, 60});
  std::vector<int> players;
  for (int i = 1; i <= sim::Sim::kMaxPlayers; ++i) {
    if (!s->players[static_cast<size_t>(i)].faction.empty()) players.push_back(i);
  }
  auto outcome = [&](int q) {
    const SnapPlayer& p = s->players[static_cast<size_t>(q)];
    if (p.defeated) return model_.tr("sum.out.defeated");
    if (mission ? (q == model_.player && won) : s->winner == q) return model_.tr("sum.out.winner");
    return model_.tr("sum.out.alive");
  };
  if (model_.sum_tab == 3) {
    summary_graphs(body, players);
  } else {
    std::vector<std::string> heads;
    std::vector<std::vector<std::string>> vals;
    std::vector<std::vector<int64_t>> nums;   // a kiemeléshez (−1: nem szám)
    for (const int q : players) {
      const SnapPlayer& p = s->players[static_cast<size_t>(q)];
      const sim::PlayerStats& st = p.stats;
      std::vector<int64_t> row;
      if (model_.sum_tab == 0) {
        const auto sc = HudModel::score(p);
        row = {sc[0], sc[1], sc[2], sc[3]};
      } else if (model_.sum_tab == 1) {
        int64_t all = 0, spent = 0;
        for (const int r : kResOrder) row.push_back(p.gathered[static_cast<size_t>(r)]);
        for (int r = 0; r < sim::kResources; ++r) all += p.gathered[static_cast<size_t>(r)];
        for (int r = 0; r < sim::kResources; ++r) if (r != sim::kFaith) spent += st.spent[static_cast<size_t>(r)] / 1000;
        row.push_back(p.gathered[sim::kFaith]);
        row.push_back(all);
        row.push_back(spent);
      } else {
        row = {st.units_trained, st.units_killed, st.units_lost, st.buildings_built, st.buildings_killed, st.buildings_lost};
      }
      std::vector<std::string> sv;
      for (const int64_t v : row) sv.push_back(std::to_string(v));
      if (model_.sum_tab == 0) {
        sv.push_back(outcome(q));
        row.push_back(-1);
      }
      vals.push_back(sv);
      nums.push_back(row);
    }
    if (model_.sum_tab == 0) {
      heads = {model_.tr("sum.col.score"), model_.tr("sum.col.military"), model_.tr("sum.col.economy"),
               model_.tr("sum.col.building"), model_.tr("sum.col.outcome")};
    } else if (model_.sum_tab == 1) {
      for (const int r : kResOrder) heads.push_back(model_.res_name(r));
      heads.push_back(model_.res_name(sim::kFaith));
      heads.push_back(model_.tr("sum.col.gathered"));
      heads.push_back(model_.tr("sum.col.spent"));
    } else {
      heads = {model_.tr("sum.col.trained"), model_.tr("sum.col.killed"), model_.tr("sum.col.lost"),
               model_.tr("sum.col.built"), model_.tr("sum.col.razed"), model_.tr("sum.col.blost")};
    }
    summary_table(body, players, heads, vals, nums);
  }
  // gomb
  const Rect br{P.right() - static_cast<int>(k(220)), P.bottom() - static_cast<int>(k(62)), static_cast<int>(k(200)), static_cast<int>(k(46))};
  const bool hot = br.contains(mx_, my_);
  rect(br, hot ? Col{92, 70, 34} : Col{64, 48, 24});
  rect(static_cast<float>(br.x), static_cast<float>(br.y), static_cast<float>(br.w), 2, {200, 166, 90});
  rect(static_cast<float>(br.x), static_cast<float>(br.bottom() - 2), static_cast<float>(br.w), 2, {200, 166, 90});
  const std::string bl = model_.tr("sum.menu");
  text(*batch_, FM, static_cast<float>(br.centerx()) - tw(FM, bl) / 2, static_cast<float>(br.centery()) - line_h(FM) / 2, bl, 255, 236,
       190);
  model_.sum_menu_btn = br;
  // a diagnosztikai jelentés mentése (helyi fájl; semmi nem megy hálózatra)
  const std::string dl = model_.tr("diag.save");
  const Rect dr{br.x - static_cast<int>(tw(FS, dl) + k(40)) - static_cast<int>(k(12)), br.y, static_cast<int>(tw(FS, dl) + k(40)), br.h};
  const bool dhot = dr.contains(mx_, my_);
  rect(dr, dhot ? Col{50, 44, 34} : Col{30, 26, 20});
  rect(static_cast<float>(dr.x), static_cast<float>(dr.y), static_cast<float>(dr.w), 1, {110, 92, 60});
  rect(static_cast<float>(dr.x), static_cast<float>(dr.bottom() - 1), static_cast<float>(dr.w), 1, {110, 92, 60});
  text(*batch_, FS, static_cast<float>(dr.centerx()) - tw(FS, dl) / 2, static_cast<float>(dr.centery()) - line_h(FS) / 2, dl, 220, 210, 190);
  model_.sum_diag_btn = dr;
}

void Hud::summary_table(const Rect& body, const std::vector<int>& players, const std::vector<std::string>& heads,
                        const std::vector<std::vector<std::string>>& vals, const std::vector<std::vector<int64_t>>& nums) {
  const Snapshot* s = model_.snap();
  const float u = static_cast<float>(model_.layout().s);
  auto k = [&](float v) { return std::round(v * u); };
  const int F = model_.layout().font.small;
  const float name_w = std::floor(static_cast<float>(body.w) * 0.34f);
  const float cw = std::floor((static_cast<float>(body.w) - name_w) / static_cast<float>(std::max<size_t>(1, heads.size())));
  float y = static_cast<float>(body.y);
  for (size_t i = 0; i < heads.size(); ++i) {
    const float x = static_cast<float>(body.x) + name_w + static_cast<float>(i) * cw + cw - k(8);
    text(*batch_, F, x - tw(F, heads[i]), y, heads[i], 230, 200, 130);
  }
  y += k(34);
  std::vector<int64_t> best(heads.size(), -1);
  for (const auto& row : nums) {
    for (size_t c = 0; c < row.size() && c < best.size(); ++c) best[c] = std::max(best[c], row[c]);
  }
  for (size_t pi = 0; pi < players.size(); ++pi) {
    const int q = players[pi];
    const SnapPlayer& p = s->players[static_cast<size_t>(q)];
    rect(static_cast<float>(body.x), y - k(4), static_cast<float>(body.w), k(36), q == model_.player ? Col{40, 34, 26} : Col{24, 22, 18},
         q == model_.player ? 150 : 120);
    rect(static_cast<float>(body.x) + k(6), y + k(4), k(18), k(18), model_.player_color(static_cast<uint8_t>(q)));
    const std::string nm = model_.tr("player", {std::to_string(q)}) + " – " + model_.faction_name(p.faction);
    text(*batch_, F, static_cast<float>(body.x) + k(32), y + k(2), nm, 236, 228, 210);
    for (size_t c = 0; c < vals[pi].size() && c < heads.size(); ++c) {
      const int64_t v = c < nums[pi].size() ? nums[pi][c] : -1;
      const bool hi = v > 0 && v == best[c];
      const float x = static_cast<float>(body.x) + name_w + static_cast<float>(c) * cw + cw - k(8);
      const Col col = hi ? Col{255, 220, 120} : Col{220, 214, 200};
      text(*batch_, F, x - tw(F, vals[pi][c]), y + k(2), vals[pi][c], col[0], col[1], col[2]);
    }
    y += k(40);
  }
}

void Hud::summary_graphs(const Rect& body, const std::vector<int>& players) {
  // két grafikon (a régi szerint): a sereg értéke és az összes gyűjtött nyersanyag az idő függvényében
  const Snapshot* s = model_.snap();
  const float u = static_cast<float>(model_.layout().s);
  auto k = [&](float v) { return std::round(v * u); };
  const int F = model_.layout().font.small, ft = model_.layout().font.tiny;
  const float gw = std::floor((static_cast<float>(body.w) - k(24)) / 2);
  for (int gi = 0; gi < 2; ++gi) {
    const std::string title = model_.tr(gi == 0 ? "sum.graph.army" : "sum.graph.gathered");
    const float rx = static_cast<float>(body.x) + static_cast<float>(gi) * (gw + k(24)), ry = static_cast<float>(body.y) + k(30);
    const float rw = gw, rh = static_cast<float>(body.h) - k(40);
    text(*batch_, F, rx, static_cast<float>(body.y), title, 230, 200, 130);
    rect(rx, ry, rw, rh, {20, 18, 15});
    const Col fc{110, 92, 60};
    rect(rx, ry, rw, 1, fc);
    rect(rx, ry + rh - 1, rw, 1, fc);
    rect(rx, ry, 1, rh, fc);
    rect(rx + rw - 1, ry, 1, rh, fc);
    auto val = [&](const sim::PlayerStats::Sample& x) { return gi == 0 ? x.army : x.gathered; };
    int32_t tmax = 1, vmax = 1;
    for (const int q : players) {
      for (const auto& x : s->players[static_cast<size_t>(q)].stats.timeline) {
        tmax = std::max(tmax, x.t);
        vmax = std::max(vmax, val(x));
      }
    }
    for (const float fr : {0.25f, 0.5f, 0.75f}) rect(rx + 1, std::floor(ry + rh - rh * fr), rw - 2, 1, {40, 36, 30});
    const std::string vl = std::to_string(vmax);
    text(*batch_, ft, rx + k(4), ry + k(4), vl, 170, 160, 140);
    for (const int q : players) {
      const auto& tl = s->players[static_cast<size_t>(q)].stats.timeline;
      if (tl.size() < 2) continue;
      const Col c = model_.player_color(static_cast<uint8_t>(q));
      auto pt = [&](const sim::PlayerStats::Sample& x) {
        return std::pair<float, float>{rx + k(4) + (rw - k(8)) * static_cast<float>(x.t) / static_cast<float>(tmax),
                                       ry + rh - k(4) - (rh - k(12)) * static_cast<float>(val(x)) / static_cast<float>(vmax)};
      };
      for (size_t i = 1; i < tl.size(); ++i) {
        const auto a = pt(tl[i - 1]), b = pt(tl[i]);
        line(a.first, a.second, b.first, b.second, c, std::max(2.0f, k(2)));
      }
    }
    char mins[32];   // a vízszintes tengely vége (perc:másodperc)
    std::snprintf(mins, sizeof mins, "%d:%02d", static_cast<int>(tmax / 60), static_cast<int>(tmax % 60));
    text(*batch_, ft, rx + rw - tw(ft, mins) - k(4), ry + rh - line_h(ft) - k(4), mins, 170, 160, 140);
  }
}

}  // namespace totu::client
