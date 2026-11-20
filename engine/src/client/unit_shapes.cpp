// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/unit_shapes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace totu::client {

namespace {

constexpr const char* kNames[kShapeKinds] = {"warrior", "worker", "shieldbearer", "archer", "rider", "caster", "beast",
                                             "flyer", "bug", "spitter", "artillery", "horror", "golem", "cocoon"};

// rétegfelosztó raszterező: 3 × 3 részképpont képpontonként, mindegyik a legfelső elemé (réteg + szín)
struct Raster {
  static constexpr int K = 3;
  struct Sub {
    int8_t layer = -1;
    uint8_t r = 0, g = 0, b = 0;
  };
  std::vector<Sub> sub = std::vector<Sub>(static_cast<size_t>(kShapeSize * K * kShapeSize * K));
  float ang = 0, ca = 1, sa = 0;
  const float cx = kShapeSize / 2.0f, cy = kShapeSize / 2.0f;

  // a helyi (előre = +x) koordináta a képre forgatva
  std::array<float, 2> rot(float px, float py) const { return {cx + px * ca - py * sa, cy + px * sa + py * ca}; }

  template <typename F>
  void fill(F inside, int layer, uint8_t r, uint8_t g, uint8_t b) {
    const int W = kShapeSize * K;
    for (int y = 0; y < W; ++y) {
      for (int x = 0; x < W; ++x) {
        const float fx = (static_cast<float>(x) + 0.5f) / K, fy = (static_cast<float>(y) + 0.5f) / K;
        if (inside(fx, fy)) sub[static_cast<size_t>(y * W + x)] = {static_cast<int8_t>(layer), r, g, b};
      }
    }
  }
  void gray(int layer, float g, auto inside) {   // színezett réteg: szürkeárnyalat (a szorzó)
    const uint8_t v = static_cast<uint8_t>(std::clamp(g * 255.0f, 0.0f, 255.0f));
    fill(inside, layer, v, v, v);
  }
  static auto circle(std::array<float, 2> c, float r) {
    return [=](float x, float y) { return (x - c[0]) * (x - c[0]) + (y - c[1]) * (y - c[1]) <= r * r; };
  }
  static auto ring(std::array<float, 2> c, float r, float w) {
    return [=](float x, float y) {
      const float d = std::sqrt((x - c[0]) * (x - c[0]) + (y - c[1]) * (y - c[1]));
      return std::abs(d - r) <= w / 2;
    };
  }
  auto ellipse(float a, float b, float ox = 0, float oy = 0) const {
    const auto c = rot(ox, oy);
    const float c0 = ca, s0 = sa;
    return [=](float x, float y) {
      const float dx = x - c[0], dy = y - c[1];
      const float u = dx * c0 + dy * s0, v = -dx * s0 + dy * c0;
      return (u * u) / (a * a) + (v * v) / (b * b) <= 1.0f;
    };
  }
  static auto segment(std::array<float, 2> p, std::array<float, 2> q, float w) {
    return [=](float x, float y) {
      const float vx = q[0] - p[0], vy = q[1] - p[1];
      const float l2 = std::max(1e-6f, vx * vx + vy * vy);
      const float t = std::clamp(((x - p[0]) * vx + (y - p[1]) * vy) / l2, 0.0f, 1.0f);
      const float dx = x - (p[0] + vx * t), dy = y - (p[1] + vy * t);
      return dx * dx + dy * dy <= w * w / 4;
    };
  }
  static auto polygon(std::vector<std::array<float, 2>> pts) {
    return [pts = std::move(pts)](float x, float y) {
      bool in = false;
      for (size_t i = 0, j = pts.size() - 1; i < pts.size(); j = i++) {
        if (((pts[i][1] > y) != (pts[j][1] > y)) &&
            (x < (pts[j][0] - pts[i][0]) * (y - pts[i][1]) / (pts[j][1] - pts[i][1]) + pts[i][0])) in = !in;
      }
      return in;
    };
  }
  void polyline(const std::vector<std::array<float, 2>>& pts, float w, int layer, float g) {
    for (size_t i = 0; i + 1 < pts.size(); ++i) gray(layer, g, segment(pts[i], pts[i + 1], w));
  }
  void polyline_rgb(const std::vector<std::array<float, 2>>& pts, float w, uint8_t r, uint8_t g, uint8_t b) {
    for (size_t i = 0; i + 1 < pts.size(); ++i) fill(segment(pts[i], pts[i + 1], w), 3, r, g, b);
  }
};

enum L { Team = 0, Base = 1, Accent = 2, Fixed = 3 };
// a régi árnyalatok a nép alapszínéhez (a színezés szorozza): világos 1,3, alap 1, sötét 0,55
constexpr float kLight = 1.0f, kBase = 0.77f, kDark = 0.42f, kMid = 0.58f;

void draw(Raster& s, int shape) {
  const float r = kShapeRadius;
  const std::array<float, 2> c{s.cx, s.cy};
  switch (shape) {
    case 1:   // dolgozó: szerszámmal
      s.gray(Team, 1, Raster::circle(c, r));
      s.gray(Base, kBase, Raster::circle(c, r * 0.78f));
      s.fill(Raster::segment(s.rot(r * 0.9f, r * 0.6f), s.rot(r * 1.6f, r * 0.9f), std::max(1.0f, r * 0.25f)), Fixed, 120, 90, 50);
      s.fill(Raster::circle(s.rot(r * 1.6f, r * 0.9f), std::max(2.0f, r * 0.3f)), Fixed, 160, 160, 165);
      break;
    case 2: {   // pajzsos
      s.gray(Team, 1, Raster::circle(c, r));
      s.gray(Base, kBase, Raster::circle(c, r * 0.8f));
      s.gray(Base, kLight, Raster::circle(s.rot(r * 0.2f, 0), r * 0.35f));
      const std::vector<std::array<float, 2>> p = {s.rot(r * 0.9f, -r * 1.0f), s.rot(r * 1.25f, -r * 0.4f), s.rot(r * 1.25f, r * 0.4f),
                                                   s.rot(r * 0.9f, r * 1.0f)};
      s.polyline(p, std::max(3.0f, r * 0.45f), Team, 0.8f);
      s.polyline(p, std::max(1.0f, r * 0.15f), Accent, 1);
      break;
    }
    case 3: {   // íjász
      s.gray(Team, 1, Raster::circle(c, r));
      s.gray(Base, kBase, Raster::circle(c, r * 0.8f));
      std::vector<std::array<float, 2>> arc;
      for (int i = 0; i <= 6; ++i) {
        const float a = static_cast<float>(i) / 6.0f * 3.14159f - 1.5708f;
        arc.push_back(s.rot(r * 0.9f + std::cos(a) * r * 0.5f, std::sin(a) * r * 1.1f));
      }
      s.polyline_rgb(arc, 2, 130, 90, 40);
      s.gray(Base, kLight, Raster::segment(s.rot(r * 0.3f, 0), s.rot(r * 1.8f, 0), 1));
      break;
    }
    case 4: {   // lovas
      s.gray(Base, kDark, s.ellipse(r * 1.15f, r * 0.6f));
      for (const auto& [sx, sy] : {std::pair{0.6f, 0.6f}, {-0.6f, 0.6f}, {0.6f, -0.6f}, {-0.6f, -0.6f}}) {
        s.gray(Base, kDark, Raster::segment(s.rot(sx * r * 0.8f, sy * r * 0.5f), s.rot(sx * r, sy * r * 0.85f), std::max(2.0f, r * 0.2f)));
      }
      s.gray(Base, kDark, Raster::circle(s.rot(r * 1.35f, 0), r * 0.38f));
      s.gray(Accent, 1, Raster::circle(s.rot(r * 1.5f, -r * 0.15f), std::max(1.0f, r * 0.1f)));
      for (int i = 0; i < 3; ++i) {
        s.gray(Accent, 1, Raster::segment(s.rot(-r * 0.2f - static_cast<float>(i) * r * 0.3f, 0),
                                          s.rot(-r * 0.5f - static_cast<float>(i) * r * 0.3f, -r * 0.45f), 2));
      }
      s.gray(Team, 1, Raster::circle(c, r * 0.48f));
      s.gray(Base, kBase, Raster::circle(c, r * 0.34f));
      break;
    }
    case 5:   // varázsló: rombusz, fénylő kézzel
      s.gray(Accent, 0.5f, Raster::ring(c, r * 1.25f, 1));
      s.gray(Team, 1, Raster::polygon({s.rot(r * 1.1f, 0), s.rot(0, r * 0.85f), s.rot(-r * 0.9f, 0), s.rot(0, -r * 0.85f)}));
      s.gray(Base, kBase, Raster::polygon({s.rot(r * 0.8f, 0), s.rot(0, r * 0.6f), s.rot(-r * 0.6f, 0), s.rot(0, -r * 0.6f)}));
      s.gray(Accent, 1, Raster::circle(s.rot(r * 0.9f, r * 0.5f), r * 0.3f));
      break;
    case 6:   // vadállat: agyarakkal, nyereggel
      s.gray(Base, kDark, s.ellipse(r * 1.1f, r * 0.8f));
      s.gray(Base, kMid, s.ellipse(r * 0.9f, r * 0.62f));
      s.gray(Base, kDark, Raster::circle(s.rot(r * 1.15f, 0), r * 0.45f));
      for (const float sy : {-1.0f, 1.0f}) {
        s.polyline_rgb({s.rot(r * 1.3f, sy * r * 0.25f), s.rot(r * 1.75f, sy * r * 0.45f), s.rot(r * 1.8f, sy * r * 0.15f)},
                       std::max(2.0f, r * 0.12f), 235, 230, 210);
      }
      s.gray(Team, 1, Raster::polygon({s.rot(-r * 0.35f, -r * 0.4f), s.rot(r * 0.35f, -r * 0.4f), s.rot(r * 0.35f, r * 0.4f), s.rot(-r * 0.35f, r * 0.4f)}));
      break;
    case 7:   // repülő: kiterjesztett szárnyak
      for (const float sy : {-1.0f, 1.0f}) {
        s.gray(Base, kDark, Raster::polygon({s.rot(r * 0.3f, 0), s.rot(-r * 0.3f, sy * r * 1.6f), s.rot(-r * 0.7f, sy * r * 1.2f), s.rot(-r * 0.4f, 0)}));
      }
      s.gray(Team, 1, s.ellipse(r * 0.9f, r * 0.35f));
      s.gray(Base, kBase, s.ellipse(r * 0.65f, r * 0.22f));
      s.gray(Base, kLight, Raster::circle(s.rot(r * 0.9f, 0), r * 0.22f));
      break;
    case 8:   // bogár: hat láb, csápok
      for (const int i : {-1, 0, 1}) {
        for (const float sy : {-1.0f, 1.0f}) {
          s.gray(Base, kDark, Raster::segment(s.rot(static_cast<float>(i) * r * 0.4f, 0), s.rot(static_cast<float>(i) * r * 0.5f, sy * r * 1.15f), 1));
        }
      }
      s.gray(Team, 1, s.ellipse(r * 1.1f, r * 0.7f));
      s.gray(Base, kBase, s.ellipse(r * 0.85f, r * 0.5f));
      for (const float sy : {-1.0f, 1.0f}) s.gray(Base, kLight, Raster::segment(s.rot(r * 0.9f, sy * r * 0.2f), s.rot(r * 1.4f, sy * r * 0.5f), 2));
      break;
    case 9:   // köpködő
      s.gray(Team, 1, s.ellipse(r * 1.1f, r * 0.85f));
      s.gray(Base, kBase, s.ellipse(r * 0.9f, r * 0.65f));
      for (const float sy : {-1.0f, 1.0f}) s.gray(Accent, 1, Raster::circle(s.rot(-r * 0.3f, sy * r * 0.4f), r * 0.25f));
      s.gray(Base, kDark, Raster::segment(s.rot(r * 0.5f, 0), s.rot(r * 1.5f, 0), std::max(2.0f, r * 0.35f)));
      break;
    case 10:   // ostromgép
      s.gray(Base, kDark, s.ellipse(r * 1.1f, r * 0.9f));
      s.gray(Accent, 1, Raster::circle(s.rot(-r * 0.3f, 0), r * 0.55f));
      s.gray(Team, 1, Raster::ring(s.rot(-r * 0.3f, 0), r * 0.55f, 2));
      s.gray(Base, kBase, Raster::segment(s.rot(r * 0.3f, 0), s.rot(r * 1.7f, 0), std::max(3.0f, r * 0.4f)));
      break;
    case 11:   // iszonyat: tekergő csápok
      for (int i = 0; i < 7; ++i) {
        const float a = static_cast<float>(i) / 7.0f * 6.283f;
        std::vector<std::array<float, 2>> pts;
        for (int k = 0; k < 5; ++k) {
          const float rr = r * (0.7f + static_cast<float>(k) * 0.22f), aa = a + 0.3f * static_cast<float>(k) * 0.15f;
          pts.push_back({c[0] + std::cos(aa) * rr, c[1] + std::sin(aa) * rr});
        }
        s.polyline(pts, std::max(2.0f, r * 0.18f), Base, kDark);
      }
      s.gray(Team, 1, Raster::circle(c, r * 0.85f));
      s.gray(Base, kBase, Raster::circle(c, r * 0.7f));
      for (int i = 0; i < 3; ++i) s.gray(Accent, 1, Raster::circle(s.rot(r * 0.35f, static_cast<float>(i - 1) * r * 0.3f), std::max(1.0f, r * 0.1f)));
      break;
    case 12:   // gólem: szögletes test, vállak
      s.gray(Team, 1, Raster::polygon({s.rot(r * 0.9f, -r * 0.9f), s.rot(r * 0.9f, r * 0.9f), s.rot(-r * 0.9f, r * 0.9f), s.rot(-r * 0.9f, -r * 0.9f)}));
      s.gray(Base, kBase, Raster::polygon({s.rot(r * 0.7f, -r * 0.7f), s.rot(r * 0.7f, r * 0.7f), s.rot(-r * 0.7f, r * 0.7f), s.rot(-r * 0.7f, -r * 0.7f)}));
      for (const float sy : {-1.0f, 1.0f}) s.gray(Base, kDark, Raster::circle(s.rot(0, sy * r * 1.0f), r * 0.38f));
      s.gray(Accent, 1, Raster::circle(s.rot(r * 0.35f, 0), r * 0.2f));
      break;
    case 13:   // gubó: tojásdad, erezett
      s.gray(Team, 1, s.ellipse(r * 1.05f, r * 0.8f));
      s.gray(Base, kMid, s.ellipse(r * 0.85f, r * 0.62f));
      for (const float k : {-0.4f, 0.0f, 0.4f}) s.gray(Base, kDark, Raster::segment(s.rot(-r * 0.7f, k * r), s.rot(r * 0.7f, k * r * 0.6f), 1));
      break;
    default: {   // harcos (az alapértelmezett is): fegyverrel, kézzel
      s.gray(Team, 1, Raster::circle(c, r));
      s.gray(Base, kBase, Raster::circle(c, r * 0.8f));
      s.gray(Base, kLight, Raster::circle(s.rot(r * 0.2f, 0), r * 0.35f));
      const auto a = s.rot(r * 0.4f, r * 0.7f), b = s.rot(r * 1.7f, r * 0.9f);
      s.fill(Raster::segment(a, b, std::max(2.0f, r * 0.3f)), Fixed, 60, 50, 50);
      s.fill(Raster::segment(a, b, 1), Fixed, 200, 220, 230);
      s.gray(Team, 1, Raster::circle(s.rot(r * 0.2f, -r * 0.8f), r * 0.45f));
      break;
    }
  }
  // sötét körvonal az egész alakzat körül (a régi rajz sötét széle; minden talajon olvasható)
}

}  // namespace

int shape_index(const std::string& shape) {
  for (int i = 0; i < kShapeKinds; ++i) {
    if (shape == kNames[i]) return i;
  }
  if (shape == "landzsas" || shape == "spear") return 0;
  return 0;
}

render::Image unit_shape_layer(int shape, int dir, int layer) {
  Raster s;
  s.ang = static_cast<float>(dir % kShapeDirs) * 3.14159265f / 4.0f;
  s.ca = std::cos(s.ang);
  s.sa = std::sin(s.ang);
  draw(s, std::clamp(shape, 0, kShapeKinds - 1));
  render::Image img;
  img.width = img.height = kShapeSize;
  img.rgba.assign(static_cast<size_t>(kShapeSize * kShapeSize * 4), 0);
  const int K = Raster::K, W = kShapeSize * K;
  for (int y = 0; y < kShapeSize; ++y) {
    for (int x = 0; x < kShapeSize; ++x) {
      int n = 0, any = 0;
      float r = 0, g = 0, b = 0;
      for (int j = 0; j < K; ++j) {
        for (int i = 0; i < K; ++i) {
          const auto& q = s.sub[static_cast<size_t>((y * K + j) * W + x * K + i)];
          if (q.layer >= 0) ++any;
          if (q.layer != layer) continue;
          ++n;
          r += q.r; g += q.g; b += q.b;
        }
      }
      uint8_t* o = img.at(x, y);
      if (n > 0) {
        o[0] = static_cast<uint8_t>(r / static_cast<float>(n));
        o[1] = static_cast<uint8_t>(g / static_cast<float>(n));
        o[2] = static_cast<uint8_t>(b / static_cast<float>(n));
        o[3] = static_cast<uint8_t>(255 * n / (K * K));
      }
      (void)any;
    }
  }
  return img;
}

}  // namespace totu::client
