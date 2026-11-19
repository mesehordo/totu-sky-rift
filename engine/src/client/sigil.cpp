// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/sigil.hpp"

#include <algorithm>
#include <cmath>
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

// a lobogó alakja: a keresztrúd alatt lógó, alul fecskefarkú vászon, a széllel lefelé erősödően hullámozva
bool banner_in(float x, float y, int phase, float shrink) {
  const float top = 6, bottom = 36, left = 6, right = 15;
  if (y < top + shrink || y > bottom + 6) return false;
  const float t = (y - top) / (bottom - top);
  const float ph = static_cast<float>(phase) / 8.0f * 6.2831853f;
  const float sway = std::sin(ph + y * 0.22f) * 1.3f * t;
  const float l = left + sway + shrink, r = right + sway - shrink;
  if (x < l || x > r) return false;
  if (y <= bottom - shrink) return true;
  const float mid = (l + r) / 2, half = (r - l) / 2;   // a fecskefarok: középen V alakú bevágás
  const float depth = 6.0f - shrink;
  const float cut = (y - (bottom - shrink)) / depth;   // 0..1
  return cut <= 1.0f && std::fabs(x - mid) >= cut * half;
}

}  // namespace

render::Image banner_pole_image() {
  Canvas c(16, 56, 3);
  c.fill([](float x, float y) { return near_seg(x, y, 6, 54, 6, 5, 1.6f); }, 20 / 255.0f, 16 / 255.0f, kDark, 1);
  c.fill([](float x, float y) { return near_seg(x, y, 5, 54, 5, 4, 1.1f); }, 112 / 255.0f, 84 / 255.0f, 52 / 255.0f, 1);
  c.fill([](float x, float y) { return near_seg(x, y, 5, 6, 16, 6, 0.9f); }, 90 / 255.0f, 66 / 255.0f, 40 / 255.0f, 1);
  return c.image();
}

render::Image banner_knob_image() {
  Canvas c(16, 56, 3);
  c.fill([](float x, float y) { return (x - 5) * (x - 5) + (y - 3.5f) * (y - 3.5f) <= 6.5f; }, kDark, kDark, kDark, 1);
  c.fill([](float x, float y) { return (x - 5) * (x - 5) + (y - 3.5f) * (y - 3.5f) <= 4.0f; }, 1, 1, 1, 1);
  return c.image();
}

render::Image banner_cloth_image(int phase) {
  Canvas c(16, 56, 3);
  c.fill([&](float x, float y) { return banner_in(x - 1, y - 1, phase, 0); }, 20 / 255.0f, 16 / 255.0f, kDark, 0.85f);
  c.fill([&](float x, float y) { return banner_in(x, y, phase, 0); }, 0.86f, 0.86f, 0.86f, 1);
  c.fill([&](float x, float y) { return banner_in(x, y, phase, 0) && std::fmod(y + static_cast<float>(phase) * 1.5f, 9.0f) < 2.0f; },
         1, 1, 1, 1);
  return c.image();
}

render::Image banner_trim_image(int phase) {
  Canvas c(16, 56, 3);
  c.fill([&](float x, float y) { return banner_in(x, y, phase, 0) && !banner_in(x, y, phase, 1.4f); }, 1, 1, 1, 1);
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
