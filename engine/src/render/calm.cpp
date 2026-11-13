// SPDX-License-Identifier: GPL-3.0-or-later
#include "render/calm.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace totu::render {

namespace {

constexpr float kLum[3] = {0.299f, 0.587f, 0.114f};
constexpr float kInk[3] = {34.0f, 23.0f, 36.0f};

struct Grid {   // w × h-s lebegőpontos rács, a peremen kívül állandó értékkel
  int w = 0, h = 0;
  std::vector<float> v;
  Grid(int w_, int h_, float fill = 0) : w(w_), h(h_), v(static_cast<size_t>(w_) * static_cast<size_t>(h_), fill) {}
  float& at(int x, int y) { return v[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]; }
  float at(int x, int y) const { return v[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]; }
  float get(int x, int y, float pad) const {
    return x < 0 || y < 0 || x >= w || y >= h ? pad : v[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
  }
  float clamp_get(int x, int y) const {   // a peremen a szélső érték (np.pad mode="edge")
    x = std::clamp(x, 0, w - 1);
    y = std::clamp(y, 0, h - 1);
    return v[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
  }
};

Grid filter(const Grid& g, int r, bool max, float pad) {
  Grid out(g.w, g.h);
  for (int y = 0; y < g.h; ++y) {
    for (int x = 0; x < g.w; ++x) {
      float m = g.get(x, y, pad);
      for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
          const float s = g.get(x + dx, y + dy, pad);
          m = max ? std::max(m, s) : std::min(m, s);
        }
      }
      out.at(x, y) = m;
    }
  }
  return out;
}

Grid gauss065(const Grid& g) {   // 0,65 képpontos Gauss-elmosás (3 × 3-as, szétválasztható; a peremen a szélső érték)
  const float k0 = 0.306f / 1.612f, k1 = 1.0f / 1.612f;
  Grid t(g.w, g.h), out(g.w, g.h);
  for (int y = 0; y < g.h; ++y) {
    for (int x = 0; x < g.w; ++x) t.at(x, y) = g.clamp_get(x - 1, y) * k0 + g.clamp_get(x, y) * k1 + g.clamp_get(x + 1, y) * k0;
  }
  for (int y = 0; y < g.h; ++y) {
    for (int x = 0; x < g.w; ++x) out.at(x, y) = t.clamp_get(x, y - 1) * k0 + t.clamp_get(x, y) * k1 + t.clamp_get(x, y + 1) * k0;
  }
  return out;
}

}  // namespace

bool calm_applies(const std::string& t) { return t == "unit" || t == "building" || t == "resource" || t == "prop"; }

void calm_frame(Image& img, int fx, int fy, int fw, int fh) {
  // a nem átlátszó rész befoglalója, 4 képpontos munkaperemmel (a körvonalnak és a szűrőknek)
  int x0 = fw, y0 = fh, x1 = -1, y1 = -1;
  for (int y = 0; y < fh; ++y) {
    for (int x = 0; x < fw; ++x) {
      if (img.at(fx + x, fy + y)[3] >= 1) {
        x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y);
      }
    }
  }
  if (x1 - x0 < 1 || y1 - y0 < 1) return;
  x0 = std::max(0, x0 - 4); y0 = std::max(0, y0 - 4); x1 = std::min(fw - 1, x1 + 4); y1 = std::min(fh - 1, y1 + 4);
  const int w = x1 - x0 + 1, h = y1 - y0 + 1;
  auto px = [&](int x, int y) { return img.at(fx + x0 + x, fy + y0 + y); };
  std::array<Grid, 3> rgb{Grid(w, h), Grid(w, h), Grid(w, h)};
  Grid a(w, h), L(w, h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const uint8_t* p = px(x, y);
      for (int c = 0; c < 3; ++c) rgb[static_cast<size_t>(c)].at(x, y) = p[c];
      a.at(x, y) = p[3];
      L.at(x, y) = p[0] * kLum[0] + p[1] * kLum[1] + p[2] * kLum[2];
    }
  }
  const Grid amin2 = filter(a, 2, false, 0);
  const Grid lmax = filter(L, 1, true, 0), lmin = filter(L, 1, false, 0);
  // 1. felület: medián az élek kivételével
  std::array<Grid, 3> smooth{Grid(w, h), Grid(w, h), Grid(w, h)};
  Grid inner(w, h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      inner.at(x, y) = amin2.get(x, y, 0) > 220 ? 1.0f : 0.0f;
      const float edge = std::clamp((lmax.get(x, y, 0) - lmin.get(x, y, 0) - 8.0f) / 26.0f, 0.0f, 1.0f);
      const float wgt = 0.90f * (1.0f - edge) * inner.at(x, y);
      for (int c = 0; c < 3; ++c) {
        std::array<float, 9> n{};
        int k = 0;
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) n[static_cast<size_t>(k++)] = rgb[static_cast<size_t>(c)].get(x + dx, y + dy, 0);
        }
        std::nth_element(n.begin(), n.begin() + 4, n.end());
        smooth[static_cast<size_t>(c)].at(x, y) = rgb[static_cast<size_t>(c)].at(x, y) * (1.0f - wgt) + n[4] * wgt;
      }
    }
  }
  // 2. belső tinta
  Grid ls(w, h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      ls.at(x, y) = smooth[0].at(x, y) * kLum[0] + smooth[1].at(x, y) * kLum[1] + smooth[2].at(x, y) * kLum[2];
    }
  }
  const Grid lg = gauss065(ls);
  const Grid lgmax = filter(lg, 1, true, 0);
  std::array<Grid, 3> out{Grid(w, h), Grid(w, h), Grid(w, h)};
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const float ink = 0.35f * std::clamp((lgmax.get(x, y, 0) - lg.at(x, y) - 8.0f) / 24.0f, 0.0f, 0.76f) * inner.at(x, y);
      for (int c = 0; c < 3; ++c) {
        out[static_cast<size_t>(c)].at(x, y) = smooth[static_cast<size_t>(c)].at(x, y) * (1.0f - ink) + kInk[c] * ink;
      }
    }
  }
  // 3. külső körvonal a sziluett köré, a helyi belső színből
  const Grid amin1 = filter(a, 1, false, 0);
  const Grid a0 = a;   // a körvonal előtti alfa (a „tömör” és a „belső” mintapontokhoz; a régi kód szerint)
  Grid sil(w, h);
  for (size_t i = 0; i < sil.v.size(); ++i) sil.v[i] = a.v[i] >= 128 ? 1.0f : 0.0f;
  const Grid silmax = filter(sil, 1, true, 0);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      if (sil.at(x, y) > 0 || silmax.at(x, y) <= 0) continue;   // csak a gyűrű
      float acc[3] = {0, 0, 0}, wsum = 0;
      for (int dy = -3; dy <= 3; ++dy) {
        for (int dx = -3; dx <= 3; ++dx) {
          if (dx * dx + dy * dy > 10) continue;
          const int sx = x + dx, sy = y + dy;
          if (sx < 0 || sy < 0 || sx >= w || sy >= h || a0.at(sx, sy) <= 190) continue;
          const float ww = (amin1.at(sx, sy) > 190 ? 3.0f : 1.0f) / static_cast<float>(1 + dx * dx + dy * dy);
          for (int c = 0; c < 3; ++c) acc[c] += smooth[static_cast<size_t>(c)].at(sx, sy) * ww;
          wsum += ww;
        }
      }
      if (wsum <= 0) continue;
      const float af = a.at(x, y) / 255.0f;
      const float add[3] = {8.0f, 6.0f, 8.0f};
      for (int c = 0; c < 3; ++c) {
        const float line = std::clamp(0.40f * acc[c] / wsum + add[c], 0.0f, 255.0f);
        out[static_cast<size_t>(c)].at(x, y) = out[static_cast<size_t>(c)].at(x, y) * af + line * (1.0f - af);
      }
      a.at(x, y) = 255.0f;
    }
  }
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      uint8_t* p = px(x, y);
      for (int c = 0; c < 3; ++c) p[c] = static_cast<uint8_t>(std::clamp(out[static_cast<size_t>(c)].at(x, y), 0.0f, 255.0f));
      p[3] = static_cast<uint8_t>(std::clamp(a.at(x, y), 0.0f, 255.0f));
    }
  }
}

}  // namespace totu::render
