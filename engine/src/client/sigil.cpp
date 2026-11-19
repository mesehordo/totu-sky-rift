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

}  // namespace totu::client
