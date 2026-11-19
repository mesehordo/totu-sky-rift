// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/sigil.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace totu::client {

namespace {

struct Px {
  float r = 0, g = 0, b = 0, a = 0;   // nem előszorzott
};

void over(Px& d, float r, float g, float b, float a) {   // alfa szerinti ráhelyezés
  const float oa = a + d.a * (1 - a);
  if (oa <= 0) return;
  d.r = (r * a + d.r * d.a * (1 - a)) / oa;
  d.g = (g * a + d.g * d.a * (1 - a)) / oa;
  d.b = (b * a + d.b * d.a * (1 - a)) / oa;
  d.a = oa;
}

bool in_ellipse(float x, float y, float a, float b) { return a > 0 && b > 0 && (x * x) / (a * a) + (y * y) / (b * b) <= 1.0f; }

// pygame.draw.ellipse(…, szélesség): a téglalapba írt ellipszis pereme `t` vastagon befelé
bool in_ring(float x, float y, float a, float b, float t) { return in_ellipse(x, y, a, b) && !in_ellipse(x, y, a - t, b - t); }

bool in_tri(float px, float py, const std::array<float, 6>& t) {
  auto s = [&](float ax, float ay, float bx, float by) { return (px - bx) * (ay - by) - (ax - bx) * (py - by); };
  const float d1 = s(t[0], t[1], t[2], t[3]), d2 = s(t[2], t[3], t[4], t[5]), d3 = s(t[4], t[5], t[0], t[1]);
  const bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
  return !(neg && pos);
}

}  // namespace

render::Image sigil_image(std::array<uint8_t, 3> color, float w, bool inward) {
  constexpr int K = 3;
  const int W = (static_cast<int>(w) + 12) * K, H = (static_cast<int>(w * 0.6f) + 12) * K;
  const float cx = static_cast<float>(W) / 2, cy = static_cast<float>(H) / 2;
  const float rw = w * K / 2, rh = w * 0.3f * K;
  const float cr = color[0] / 255.0f, cg = color[1] / 255.0f, cb = color[2] / 255.0f;
  const float lr = std::min(1.0f, (color[0] + 90) / 255.0f), lg = std::min(1.0f, (color[1] + 90) / 255.0f),
              lb = std::min(1.0f, (color[2] + 90) / 255.0f);
  const float dir = inward ? -1.0f : 1.0f;
  std::vector<std::array<float, 6>> tris;   // a nyílhegyek (a régi pontjai szerint)
  for (int i = 0; i < 4; ++i) {
    const float ang = static_cast<float>(i) * 1.5707963f;
    const float ox = std::cos(ang), oy = std::sin(ang);
    float bx = cx + ox * rw, by = cy + oy * rh;
    const float tipx = bx + ox * 5 * K * dir, tipy = by + oy * 4 * K * dir;
    const float px = -oy, py = ox;
    bx -= ox * 2 * K * dir;
    by -= oy * 2 * K * dir;
    tris.push_back({bx + px * 4 * K, by + py * 3 * K, tipx, tipy, bx - px * 4 * K, by - py * 3 * K});
  }
  std::vector<Px> big(static_cast<size_t>(W) * static_cast<size_t>(H));
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      Px& p = big[static_cast<size_t>(y) * static_cast<size_t>(W) + static_cast<size_t>(x)];
      const float fx = static_cast<float>(x) + 0.5f - cx, fy = static_cast<float>(y) + 0.5f - cy;
      for (int i = 8; i >= 1; --i) {   // belső fény
        const float f = static_cast<float>(i) / 8.0f;
        if (in_ellipse(fx, fy, rw * f, rh * f)) over(p, cr, cg, cb, (8 + 22 * (1 - f)) / 255.0f);
      }
      if (in_ring(fx, fy, rw + K, rh + K, 4 * K)) over(p, 12 / 255.0f, 14 / 255.0f, 10 / 255.0f, 190 / 255.0f);
      if (in_ring(fx, fy, rw, rh, 2 * K)) over(p, cr, cg, cb, 1.0f);
      if (in_ring(fx, fy, rw * 0.8f, rh * 0.8f, K)) over(p, cr, cg, cb, 150 / 255.0f);
      for (const auto& t : tris) {
        if (in_tri(fx + cx - K, fy + cy - K, t)) over(p, 12 / 255.0f, 14 / 255.0f, 10 / 255.0f, 220 / 255.0f);   // árnyék (+K, +K)
        if (in_tri(fx + cx, fy + cy, t)) over(p, lr, lg, lb, 1.0f);
      }
    }
  }
  render::Image img;
  img.width = W / K;
  img.height = H / K;
  img.rgba.assign(static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 4, 0);
  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {   // átlagolás (alfával súlyozva)
      float r = 0, g = 0, b = 0, a = 0;
      for (int j = 0; j < K; ++j) {
        for (int i = 0; i < K; ++i) {
          const Px& p = big[static_cast<size_t>(y * K + j) * static_cast<size_t>(W) + static_cast<size_t>(x * K + i)];
          r += p.r * p.a;
          g += p.g * p.a;
          b += p.b * p.a;
          a += p.a;
        }
      }
      uint8_t* o = img.at(x, y);
      if (a > 0) {
        o[0] = static_cast<uint8_t>(std::lround(std::clamp(r / a, 0.0f, 1.0f) * 255));
        o[1] = static_cast<uint8_t>(std::lround(std::clamp(g / a, 0.0f, 1.0f) * 255));
        o[2] = static_cast<uint8_t>(std::lround(std::clamp(b / a, 0.0f, 1.0f) * 255));
      }
      o[3] = static_cast<uint8_t>(std::lround(std::clamp(a / (K * K), 0.0f, 1.0f) * 255));
    }
  }
  return img;
}

namespace {

// élsimított vászon: K-szoros felbontás, a végén átlagolva
struct Canvas {
  int W, H, K;
  std::vector<Px> px;
  Canvas(int w, int h, int k) : W(w * k), H(h * k), K(k), px(static_cast<size_t>(W) * static_cast<size_t>(H)) {}
  template <typename F>
  void fill(F inside, float r, float g, float b, float a) {   // `inside(x, y)`: kis felbontású koordinátában
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        const float fx = (static_cast<float>(x) + 0.5f) / static_cast<float>(K), fy = (static_cast<float>(y) + 0.5f) / static_cast<float>(K);
        if (inside(fx, fy)) over(px[static_cast<size_t>(y) * static_cast<size_t>(W) + static_cast<size_t>(x)], r, g, b, a);
      }
    }
  }
  render::Image image() const {
    render::Image img;
    img.width = W / K;
    img.height = H / K;
    img.rgba.assign(static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 4, 0);
    for (int y = 0; y < img.height; ++y) {
      for (int x = 0; x < img.width; ++x) {
        float r = 0, g = 0, b = 0, a = 0;
        for (int j = 0; j < K; ++j) {
          for (int i = 0; i < K; ++i) {
            const Px& p = px[static_cast<size_t>(y * K + j) * static_cast<size_t>(W) + static_cast<size_t>(x * K + i)];
            r += p.r * p.a;
            g += p.g * p.a;
            b += p.b * p.a;
            a += p.a;
          }
        }
        uint8_t* o = img.at(x, y);
        if (a > 0) {
          o[0] = static_cast<uint8_t>(std::lround(std::clamp(r / a, 0.0f, 1.0f) * 255));
          o[1] = static_cast<uint8_t>(std::lround(std::clamp(g / a, 0.0f, 1.0f) * 255));
          o[2] = static_cast<uint8_t>(std::lround(std::clamp(b / a, 0.0f, 1.0f) * 255));
        }
        o[3] = static_cast<uint8_t>(std::lround(std::clamp(a / static_cast<float>(K * K), 0.0f, 1.0f) * 255));
      }
    }
    return img;
  }
};

// páros-páratlan szabály (a homorú fecskefarokhoz is)
bool in_poly(float x, float y, const std::vector<std::pair<float, float>>& p) {
  bool in = false;
  for (size_t i = 0, j = p.size() - 1; i < p.size(); j = i++) {
    if ((p[i].second > y) != (p[j].second > y) &&
        x < (p[j].first - p[i].first) * (y - p[i].second) / (p[j].second - p[i].second) + p[i].first) {
      in = !in;
    }
  }
  return in;
}

bool near_seg(float x, float y, float ax, float ay, float bx, float by, float half) {
  const float dx = bx - ax, dy = by - ay;
  const float l2 = dx * dx + dy * dy;
  const float t = l2 > 0 ? std::clamp(((x - ax) * dx + (y - ay) * dy) / l2, 0.0f, 1.0f) : 0.0f;
  const float qx = ax + dx * t - x, qy = ay + dy * t - y;
  return qx * qx + qy * qy <= half * half;
}

constexpr float kDark = 12 / 255.0f;

}  // namespace

render::Image rally_arrow_image(float angle) {
  Canvas c(16, 16, 4);
  const float ux = std::cos(angle), uy = std::sin(angle), px = -uy, py = ux, sz = 4.2f, x = 8, y = 8;
  const std::vector<std::pair<float, float>> tri = {{x + ux * sz, y + uy * sz * 0.6f},
                                                    {x - ux * sz * 0.6f + px * sz, y - uy * sz * 0.4f + py * sz * 0.6f},
                                                    {x - ux * sz * 0.6f - px * sz, y - uy * sz * 0.4f - py * sz * 0.6f}};
  c.fill([&](float fx, float fy) { return in_poly(fx - 1, fy - 1, tri); }, 14 / 255.0f, 16 / 255.0f, kDark, 1.0f);
  c.fill([&](float fx, float fy) { return in_poly(fx, fy, tri); }, 1, 1, 1, 1);
  return c.image();
}

render::Image rally_pole_image() {
  Canvas c(22, 32, 3);
  c.fill([](float x, float y) { return near_seg(x, y, 4, 30, 4, 5, 1.5f); }, 20 / 255.0f, 16 / 255.0f, kDark, 1);
  c.fill([](float x, float y) { return near_seg(x, y, 3, 30, 3, 4, 1.0f); }, 196 / 255.0f, 176 / 255.0f, 136 / 255.0f, 1);
  c.fill([](float x, float y) { return (x - 3) * (x - 3) + (y - 4) * (y - 4) <= 4; }, 240 / 255.0f, 214 / 255.0f, 140 / 255.0f, 1);
  return c.image();
}

render::Image rally_cloth_image(int phase) {
  Canvas c(22, 32, 3);
  const float wave = std::sin(static_cast<float>(phase) / 8.0f * 6.2831853f) * 2.0f;
  const float bx = 3, top = 4;
  const std::vector<std::pair<float, float>> pts = {{bx + 1, top + 2}, {bx + 15, top + 3 + wave * 0.5f}, {bx + 11, top + 7 + wave * 0.3f},
                                                    {bx + 15, top + 11 + wave}, {bx + 1, top + 11}};
  c.fill([&](float x, float y) { return in_poly(x - 1, y - 1, pts); }, 20 / 255.0f, 16 / 255.0f, kDark, 1);   // kontúr
  c.fill([&](float x, float y) { return in_poly(x, y, pts); }, 0.82f, 0.82f, 0.82f, 1);   // a csapatszín (kicsit tompítva)
  c.fill([&](float x, float y) { return near_seg(x, y, bx + 2, top + 4, bx + 12, top + 4 + wave * 0.5f, 0.5f) && in_poly(x, y, pts); },
         1, 1, 1, 1);   // világos csík
  return c.image();
}

namespace {

// A népek lobogói (D51 kiegészítés): a vászon alakja stílusonként, 20 × 56-os képen; a rúd az x = 5-ön, a talp
// y = 54-en. A lengés a vásznat tolja el (függőlegesnél oldalra, lefelé erősödve; a vízszintes zászlónál fel-le).
constexpr float kPi2 = 6.2831853f;

bool shape_in(int style, float x, float y) {
  switch (style) {
    case 1: {   // Ursan: szögletes vászon, alul három fogú cakk
      if (x < 6 || x > 16 || y < 7) return false;
      if (y <= 33) return true;
      const float f = std::fmod((x - 6) / (10.0f / 3.0f), 1.0f);
      return y - 33 <= 5.5f * (1.0f - std::fabs(2 * f - 1));
    }
    case 2: {   // Croftian: szerves, rongyos szélű hártya (csepp alak)
      const float dx = (x - 11.5f) / 5.5f, dy = (y - 21) / 14.0f;
      const float ang = std::atan2(dy, dx);
      const float r = 1.0f + 0.12f * std::sin(ang * 5.0f) + 0.07f * std::sin(ang * 11.0f);
      const float taper = y > 21 ? 1.0f - (y - 21) / 26.0f : 1.0f;   // lefelé elvékonyodik
      return y >= 6 && (dx * dx) / std::max(0.05f, taper * taper) + dy * dy <= r * r;
    }
    case 3: {   // Elves: keskeny, hegyes zászló
      if (y < 6 || y > 46 || x < 6) return false;
      return x - 6 <= 8.0f * (1.0f - (y - 6) / 40.0f);
    }
    case 4: {   // Imperium: nehéz, szögletes hadijelvény, alul rojtos sáv
      if (x < 6 || x > 18 || y < 8) return false;
      if (y <= 30) return true;
      return y <= 33 && std::fmod(x - 6, 2.0f) < 1.1f;
    }
    case 5:   // Ooni: nobori – magas, keskeny vászon
      return x >= 6 && x <= 13 && y >= 6 && y <= 45;
    case 6: {   // Hukucha: tavirózsalevél, alul bevágással
      const float dx = x - 12, dy = y - 19;
      if (x >= 6 && x <= 9 && y >= 11.5f && y <= 13.5f) return true;   // kocsány a rúdhoz
      if (dx * dx + dy * dy > 6.5f * 6.5f) return false;
      return !(dy > 1 && std::fabs(dx) < (dy - 1) * 0.18f);
    }
    case 7: {   // Wullof: vízszintes háromszög a rúd tetején, alatta két szalag
      if (x >= 6 && y >= 6 && y <= 20) {
        const float t = (x - 6) / 13.0f;
        if (t <= 1.0f && std::fabs(y - 13) <= 7.0f * (1.0f - t)) return true;
      }
      return (y >= 20 && y <= 32 && ((x >= 6.5f && x <= 8.3f) || (x >= 10 && x <= 11.8f && y <= 28)));
    }
    case 8: {   // Micélia: a gombakalap alatt fodros függöny
      if (x < 6 || x > 16 || y < 9) return false;
      if (y <= 27) return true;
      return y - 27 <= 2.5f * (0.5f + 0.5f * std::cos((x - 6) / 10.0f * kPi2 * 2.5f));
    }
    default: {   // a többi: fecskefarkú lobogó
      if (x < 6 || x > 15 || y < 6) return false;
      if (y <= 36) return true;
      const float cut = (y - 36) / 6.0f, half = 4.5f;
      return cut <= 1.0f && std::fabs(x - 10.5f) >= cut * half;
    }
  }
}

// a lengés: a pont visszatolása a nyugalmi alakra
bool cloth_in(int style, float x, float y, int phase) {
  const float ph = static_cast<float>(phase) / 8.0f * kPi2;
  if (style == 7) {   // vízszintes zászló: fel-le hullámzik, a rúdtól távolodva erősebben
    const float t = std::clamp((x - 6) / 13.0f, 0.0f, 1.0f);
    return shape_in(style, x, y - std::sin(ph + x * 0.45f) * 1.3f * t);
  }
  const float t = std::clamp((y - 6) / 34.0f, 0.0f, 1.0f);
  return shape_in(style, x - std::sin(ph + y * 0.22f) * 1.3f * t, y);
}

struct PoleStyle {
  float r, g, b;   // a rúd színe
  float top;       // a rúd teteje
  bool crossbar;   // keresztrúd a vászon fölött
};

PoleStyle pole_style(int style) {
  switch (style) {
    case 1: return {112 / 255.0f, 84 / 255.0f, 52 / 255.0f, 4, true};    // fa
    case 2: return {52 / 255.0f, 38 / 255.0f, 58 / 255.0f, 3, false};    // kitin
    case 3: return {196 / 255.0f, 200 / 255.0f, 210 / 255.0f, 3, false}; // ezüst
    case 4: return {74 / 255.0f, 74 / 255.0f, 82 / 255.0f, 4, true};     // vas
    case 5: return {34 / 255.0f, 26 / 255.0f, 26 / 255.0f, 4, true};     // fekete lakk
    case 6: return {96 / 255.0f, 136 / 255.0f, 66 / 255.0f, 6, false};   // nád
    case 7: return {120 / 255.0f, 92 / 255.0f, 58 / 255.0f, 2, false};   // lándzsanyél
    case 8: return {214 / 255.0f, 200 / 255.0f, 176 / 255.0f, 8, false}; // gombatönk
    default: return {112 / 255.0f, 84 / 255.0f, 52 / 255.0f, 4, true};
  }
}

}  // namespace

render::Image carry_sack_image() {
  Canvas c(12, 12, 4);
  auto ell = [](float x, float y, float cx, float cy, float rx, float ry) {
    return (x - cx) * (x - cx) / (rx * rx) + (y - cy) * (y - cy) / (ry * ry) <= 1.0f;
  };
  c.fill([&](float x, float y) { return ell(x, y, 6, 6.5f, 5, 4.5f); }, 20 / 255.0f, 16 / 255.0f, kDark, 1);
  c.fill([&](float x, float y) { return ell(x, y, 6, 6.5f, 4, 3.5f); }, 0.8f, 0.8f, 0.8f, 1);
  c.fill([&](float x, float y) { return ell(x, y, 4.8f, 5.6f, 1.6f, 1.0f); }, 1, 1, 1, 1);
  c.fill([&](float x, float y) { return near_seg(x, y, 4, 3.2f, 8, 3.2f, 0.9f); }, 20 / 255.0f, 16 / 255.0f, kDark, 1);
  return c.image();
}

namespace {

// a hős jelvénye (a tulajdonos kérése: kis korona a nép jelképe helyett) a [-0,5; 0,5] egységnégyzetben:
// öt ágú pánt, az ágak hegyén gömb, alul sáv
bool crown_in(float x, float y) {
  if (x < -0.44f || x > 0.44f || y < -0.46f || y > 0.34f) return false;
  if (y >= 0.12f) return std::fabs(x) <= 0.40f;   // a pánt
  const float tipx[3] = {-0.36f, 0.0f, 0.36f}, tipy[3] = {-0.22f, -0.34f, -0.22f};
  for (int i = 0; i < 3; ++i) {   // gömb az ág hegyén
    if ((x - tipx[i]) * (x - tipx[i]) + (y - tipy[i] + 0.04f) * (y - tipy[i] + 0.04f) <= 0.085f * 0.085f) return true;
  }
  // három ág: a hegytől a pántig szélesedő háromszögek, köztük mély völgyek
  for (int i = 0; i < 3; ++i) {
    if (y < tipy[i]) continue;
    const float t = (y - tipy[i]) / (0.12f - tipy[i]);   // 0 a hegynél, 1 a pántnál
    if (std::fabs(x - tipx[i]) <= 0.05f + 0.17f * t) return true;
  }
  return std::fabs(x) <= 0.40f && y >= 0.02f;   // a völgyek alja
}

}  // namespace

render::Image hero_badge_frame() {
  // 21 × 21: a jelkép sötét körvonala (3 képpont) és arany szegélye (1,5 képpont) – a régi `hero_badge`
  Canvas c(21, 21, 4);
  auto sym = [&](float x, float y) { return crown_in((x - 10.5f) / 15.0f, (y - 10.5f) / 15.0f); };
  auto dil = [&](float x, float y, float r) {
    for (int k = 0; k < 12; ++k) {
      const float a = static_cast<float>(k) * 0.5235988f;
      if (sym(x + std::cos(a) * r, y + std::sin(a) * r)) return true;
    }
    return sym(x, y);
  };
  c.fill([&](float x, float y) { return dil(x, y, 2.6f) || dil(x, y, 1.3f); }, 18 / 255.0f, 12 / 255.0f, 6 / 255.0f, 1);
  c.fill([&](float x, float y) { return dil(x, y, 1.3f); }, 1.0f, 214 / 255.0f, 110 / 255.0f, 1);
  return c.image();
}

render::Image hero_badge_symbol() {
  Canvas c(21, 21, 4);
  c.fill([&](float x, float y) { return crown_in((x - 10.5f) / 15.0f, (y - 10.5f) / 15.0f); }, 1, 1, 1, 1);
  c.fill([&](float x, float y) {   // felső fény
    return crown_in((x - 10.5f) / 15.0f, (y - 10.5f) / 15.0f) && !crown_in((x - 10.5f) / 15.0f, (y - 9.7f) / 15.0f);
  }, 1, 1, 1, 0.25f);
  return c.image();
}

render::Image hero_ground_tinted(float w) {
  // a régi `hero_sigil` színezhető része (fehér): fényudvar, a csapatszínű gyűrű és a belső gyűrű
  const int W = static_cast<int>(w) + 8, H = static_cast<int>(w * 0.56f) + 8;
  Canvas c(W, H, 3);
  const float cx = static_cast<float>(W) / 2, cy = static_cast<float>(H) / 2;
  auto ell = [&](float x, float y, float rx, float ry) { return (x - cx) * (x - cx) / (rx * rx) + (y - cy) * (y - cy) / (ry * ry) <= 1; };
  for (int i = 10; i >= 1; --i) {
    const float f = static_cast<float>(i) / 10;
    c.fill([&](float x, float y) { return ell(x, y, w / 2 * f, w * 0.28f * f); }, 1, 1, 1, (10 + 26 * (1 - f)) / 255.0f);
  }
  c.fill([&](float x, float y) { return ell(x, y, w / 2, w * 0.28f) && !ell(x, y, w / 2 - 3, w * 0.28f - 3); }, 1, 1, 1, 245 / 255.0f);
  const float iw = w * 0.78f;
  c.fill([&](float x, float y) { return ell(x, y, iw / 2, iw * 0.28f) && !ell(x, y, iw / 2 - 1, iw * 0.28f - 1); }, 1, 1, 1, 200 / 255.0f);
  return c.image();
}

render::Image hero_ground_fixed(float w) {
  // a nem színezett rész: sötét külső perem és nyolc arany rovátka
  const int W = static_cast<int>(w) + 8, H = static_cast<int>(w * 0.56f) + 8;
  Canvas c(W, H, 3);
  const float cx = static_cast<float>(W) / 2, cy = static_cast<float>(H) / 2;
  auto ell = [&](float x, float y, float rx, float ry) { return (x - cx) * (x - cx) / (rx * rx) + (y - cy) * (y - cy) / (ry * ry) <= 1; };
  c.fill([&](float x, float y) { return ell(x, y, w / 2 + 0.5f, w * 0.28f + 0.5f) && !ell(x, y, w / 2 - 3.5f, w * 0.28f - 3.5f) &&
                                        !(ell(x, y, w / 2, w * 0.28f) && !ell(x, y, w / 2 - 3, w * 0.28f - 3)); },
         20 / 255.0f, 14 / 255.0f, 8 / 255.0f, 200 / 255.0f);
  for (int k = 0; k < 8; ++k) {
    const float a = static_cast<float>(k) * 0.785398f + 0.392699f;
    const float x0 = cx + std::cos(a) * w * 0.40f, y0 = cy + std::sin(a) * w * 0.22f;
    const float x1 = cx + std::cos(a) * w * 0.50f, y1 = cy + std::sin(a) * w * 0.28f;
    c.fill([&](float x, float y) { return near_seg(x, y, x0, y0, x1, y1, 1.0f); }, 1.0f, 230 / 255.0f, 150 / 255.0f, 230 / 255.0f);
  }
  return c.image();
}

render::Image chant_mark_image(float w, int phase) {
  // a régi „chant” talajjel: szaggatott, forgó külső gyűrű és körbejáró rúnák (kék)
  const int W = static_cast<int>(w) + 14, H = static_cast<int>(w * 0.6f) + 14;
  Canvas c(W, H, 3);
  const float cx = static_cast<float>(W) / 2, cy = static_cast<float>(H) / 2, rw = w / 2, rh = w * 0.3f;
  const float cr = 150 / 255.0f, cg = 205 / 255.0f, cb = 1.0f;
  auto ell = [&](float x, float y, float f) { return (x - cx) * (x - cx) / (rw * rw * f * f) + (y - cy) * (y - cy) / (rh * rh * f * f) <= 1; };
  c.fill([&](float x, float y) { return ell(x, y, 1.06f) && !ell(x, y, 1.06f - 3 / rh); }, 10 / 255.0f, 16 / 255.0f, 28 / 255.0f, 150 / 255.0f);
  c.fill([&](float x, float y) {
    if (!(ell(x, y, 1.0f) && !ell(x, y, 1.0f - 2 / rh))) return false;
    float a = std::atan2((y - cy) / rh, (x - cx) / rw) - static_cast<float>(phase) * 0.13f;
    a = std::fmod(a + 62.83185f, 6.2831853f);
    const int i = static_cast<int>(a / (6.2831853f / 24));
    const float within = a - static_cast<float>(i) * (6.2831853f / 24);
    return i % 3 != 2 && within < 0.2f;
  }, cr, cg, cb, 230 / 255.0f);
  c.fill([&](float x, float y) { return ell(x, y, 0.78f) && !ell(x, y, 0.78f - 1 / rh); }, cr, cg, cb, 90 / 255.0f);
  for (int i = 0; i < 6; ++i) {
    const float a = 6.2831853f * static_cast<float>(i) / 6 + static_cast<float>(phase) * 0.26f;
    const float px = cx + std::cos(a) * rw * 0.89f, py = cy + std::sin(a) * rh * 0.89f, s = 2.6f;
    const std::vector<std::pair<float, float>> outer = {{px, py - s - 1}, {px + s * 0.7f + 1, py}, {px, py + s + 1}, {px - s * 0.7f - 1, py}};
    const std::vector<std::pair<float, float>> inner = {{px, py - s}, {px + s * 0.7f, py}, {px, py + s}, {px - s * 0.7f, py}};
    c.fill([&](float x, float y) { return in_poly(x, y, outer); }, 10 / 255.0f, 16 / 255.0f, 28 / 255.0f, 220 / 255.0f);
    c.fill([&](float x, float y) { return in_poly(x, y, inner); }, 225 / 255.0f, 240 / 255.0f, 1.0f, 1);
  }
  return c.image();
}

render::Image hidden_mark_image(float w) {
  // a régi „hidden” talajjel: pöttyözött gyűrű
  const int W = static_cast<int>(w) + 14, H = static_cast<int>(w * 0.6f) + 14;
  Canvas c(W, H, 3);
  const float cx = static_cast<float>(W) / 2, cy = static_cast<float>(H) / 2, rw = w / 2, rh = w * 0.3f;
  for (int i = 0; i < 28; ++i) {
    const float a = 6.2831853f * static_cast<float>(i) / 28;
    const float px = cx + std::cos(a) * rw, py = cy + std::sin(a) * rh;
    c.fill([&](float x, float y) { return (x - px) * (x - px) + (y - py) * (y - py) <= 1.6f * 1.6f; }, 175 / 255.0f, 180 / 255.0f, 215 / 255.0f,
           170 / 255.0f);
  }
  return c.image();
}

int banner_style(const std::string& faction) {
  static const std::pair<const char*, int> kStyles[] = {{"ursan", 1}, {"croftian", 2}, {"elves", 3}, {"imperium", 4},
                                                        {"ooni", 5},  {"hukucha", 6},  {"wullof", 7}, {"mycel", 8}};
  for (const auto& [id, s] : kStyles) {
    if (faction == id) return s;
  }
  return 0;
}

render::Image banner_pole_image(int style) {
  Canvas c(20, 56, 3);
  const PoleStyle p = pole_style(style);
  c.fill([&](float x, float y) { return near_seg(x, y, 6, 54, 6, p.top + 1, 1.6f); }, 20 / 255.0f, 16 / 255.0f, kDark, 1);
  c.fill([&](float x, float y) { return near_seg(x, y, 5, 54, 5, p.top, 1.1f); }, p.r, p.g, p.b, 1);
  if (style == 6) {   // nád: ízek
    for (float yy : {16.0f, 28.0f, 40.0f}) {
      c.fill([&](float x, float y) { return near_seg(x, y, 3.8f, yy, 6.2f, yy, 0.5f); }, 60 / 255.0f, 90 / 255.0f, 40 / 255.0f, 1);
    }
  }
  if (p.crossbar) {
    const float right = style == 4 ? 19.0f : style == 5 ? 14.0f : 17.0f;
    c.fill([&](float x, float y) { return near_seg(x, y, 4, p.top + 3, right, p.top + 3, 0.9f); }, p.r * 0.8f, p.g * 0.8f, p.b * 0.8f, 1);
  }
  return c.image();
}

render::Image banner_knob_image(int style) {
  Canvas c(20, 56, 3);
  const float t = pole_style(style).top;
  auto dark = [&](auto f) { c.fill(f, kDark, kDark, kDark, 1); };
  auto white = [&](auto f) { c.fill(f, 1, 1, 1, 1); };
  switch (style) {
    case 2: case 7: {   // tüske / lándzsahegy
      const std::vector<std::pair<float, float>> tri = {{5, t - 4}, {3, t + 2}, {7, t + 2}};
      dark([&](float x, float y) { return in_poly(x - 0.6f, y - 0.6f, tri); });
      white([&](float x, float y) { return in_poly(x, y, tri); });
      break;
    }
    case 3: {   // kristály
      const std::vector<std::pair<float, float>> dia = {{5, t - 4}, {7, t}, {5, t + 3}, {3, t}};
      dark([&](float x, float y) { return in_poly(x - 0.6f, y - 0.6f, dia); });
      white([&](float x, float y) { return in_poly(x, y, dia); });
      break;
    }
    case 4:   // kereszt
      dark([&](float x, float y) { return near_seg(x, y, 5, t - 4, 5, t + 1, 1.4f) || near_seg(x, y, 2.5f, t - 2, 7.5f, t - 2, 1.4f); });
      white([&](float x, float y) { return near_seg(x, y, 5, t - 4, 5, t + 1, 0.8f) || near_seg(x, y, 2.5f, t - 2, 7.5f, t - 2, 0.8f); });
      break;
    case 6:   // bimbó
      dark([&](float x, float y) { return (x - 5) * (x - 5) / 4.4f + (y - (t - 1)) * (y - (t - 1)) / 9.0f <= 1; });
      white([&](float x, float y) { return (x - 5) * (x - 5) / 2.6f + (y - (t - 1)) * (y - (t - 1)) / 6.0f <= 1; });
      break;
    case 8: {   // gombakalap a rúd tetején
      dark([&](float x, float y) { return y <= t + 1.5f && (x - 10) * (x - 10) / 64.0f + (y - (t + 1.5f)) * (y - (t + 1.5f)) / 30.0f <= 1; });
      white([&](float x, float y) { return y <= t + 1 && (x - 10) * (x - 10) / 50.0f + (y - (t + 1)) * (y - (t + 1)) / 24.0f <= 1; });
      break;
    }
    default:   // gömb
      dark([&](float x, float y) { return (x - 5) * (x - 5) + (y - (t - 0.5f)) * (y - (t - 0.5f)) <= 6.5f; });
      white([&](float x, float y) { return (x - 5) * (x - 5) + (y - (t - 0.5f)) * (y - (t - 0.5f)) <= 4.0f; });
  }
  return c.image();
}

render::Image banner_cloth_image(int style, int phase) {
  Canvas c(20, 56, 3);
  c.fill([&](float x, float y) { return cloth_in(style, x - 1, y - 1, phase); }, 20 / 255.0f, 16 / 255.0f, kDark, 0.85f);   // kontúr
  c.fill([&](float x, float y) { return cloth_in(style, x, y, phase); }, 0.86f, 0.86f, 0.86f, 1);
  c.fill([&](float x, float y) {   // a hajlat világosabb csíkjai
    return cloth_in(style, x, y, phase) && std::fmod((style == 7 ? x : y) + static_cast<float>(phase) * 1.5f, 9.0f) < 2.0f;
  }, 1, 1, 1, 1);
  return c.image();
}

render::Image banner_trim_image(int style, int phase) {
  Canvas c(20, 56, 3);
  const float s = 1.3f;   // a szegély: a vászon pereme (a négy irányban elkoptatva)
  c.fill([&](float x, float y) {
    return cloth_in(style, x, y, phase) &&
           !(cloth_in(style, x - s, y, phase) && cloth_in(style, x + s, y, phase) && cloth_in(style, x, y - s, phase) &&
             cloth_in(style, x, y + s, phase));
  }, 1, 1, 1, 1);
  return c.image();
}

render::Image rally_shadow_image(int phase) {
  Canvas c(40, 20, 3);
  const float wave = std::sin(static_cast<float>(phase) / 8.0f * 6.2831853f) * 2.0f;
  auto gp = [](float dx, float h) { return std::pair<float, float>{3 + dx + h * 0.62f, 3 + h * 0.44f}; };
  const auto a = gp(0, 0), b = gp(0, 26);
  c.fill([&](float x, float y) { return near_seg(x, y, a.first, a.second, b.first, b.second, 1.0f); }, 8 / 255.0f, 10 / 255.0f, 14 / 255.0f,
         150 / 255.0f);
  const std::vector<std::pair<float, float>> pts = {gp(1, 24), gp(15, 23 - wave * 0.5f), gp(11, 19 - wave * 0.3f), gp(15, 15 - wave), gp(1, 15)};
  c.fill([&](float x, float y) { return in_poly(x, y, pts); }, 8 / 255.0f, 10 / 255.0f, 14 / 255.0f, 140 / 255.0f);
  render::Image img = c.image();
  render::Image soft = img;   // puha szél (a régi kétszeres kicsinyítés helyett 3 × 3-as átlag)
  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      int acc = 0, n = 0;
      for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
          const int xx = x + i, yy = y + j;
          if (xx < 0 || yy < 0 || xx >= img.width || yy >= img.height) continue;
          acc += img.at(xx, yy)[3];
          ++n;
        }
      }
      soft.at(x, y)[0] = 8;
      soft.at(x, y)[1] = 10;
      soft.at(x, y)[2] = 14;
      soft.at(x, y)[3] = static_cast<uint8_t>(acc / std::max(1, n));
    }
  }
  return soft;
}

}  // namespace totu::client
