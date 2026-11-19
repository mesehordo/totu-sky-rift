// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/ground_overlay.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <queue>
#include <string>
#include <vector>

namespace totu::client {

namespace {

struct Rng {   // xorshift64*: rögzített, gépfüggetlen
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ULL) {}
  double next() {
    s ^= s >> 12;
    s ^= s << 25;
    s ^= s >> 27;
    return static_cast<double>((s * 2685821657736338717ULL) >> 11) / static_cast<double>(1ULL << 53);
  }
};

using Grid = std::vector<double>;

// érték-zaj (a régi `_value_noise`): `cell` csempés rácsponti véletlenek, simított bilineáris interpolációval
Grid value_noise(Rng& rng, int h, int w, int cell) {
  const int gh = h / cell + 2, gw = w / cell + 2;
  std::vector<double> g(static_cast<size_t>(gh * gw));
  for (double& v : g) v = rng.next();
  Grid out(static_cast<size_t>(h * w));
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const double fx = static_cast<double>(x) / cell, fy = static_cast<double>(y) / cell;
      const int ix = static_cast<int>(fx), iy = static_cast<int>(fy);
      double ax = fx - ix, ay = fy - iy;
      ax = ax * ax * (3 - 2 * ax);
      ay = ay * ay * (3 - 2 * ay);
      auto at = [&](int xx, int yy) { return g[static_cast<size_t>(std::min(yy, gh - 1) * gw + std::min(xx, gw - 1))]; };
      const double a = at(ix, iy) * (1 - ax) + at(ix + 1, iy) * ax, b = at(ix, iy + 1) * (1 - ax) + at(ix + 1, iy + 1) * ax;
      out[static_cast<size_t>(y * w + x)] = a * (1 - ay) + b * ay;
    }
  }
  return out;
}

Grid fbm(Rng& rng, int h, int w, std::initializer_list<int> cells = {14, 6, 3}, std::initializer_list<double> amps = {0.55, 0.3, 0.15}) {
  Grid out(static_cast<size_t>(h * w), 0.0);
  auto ai = amps.begin();
  for (const int c : cells) {
    const Grid n = value_noise(rng, h, w, std::max(1, c));
    for (size_t i = 0; i < out.size(); ++i) out[i] += n[i] * *ai;
    ++ai;
  }
  return out;
}

Grid blur(const Grid& a, int h, int w, int n) {   // n-szeres 3 × 3-as átlag
  Grid cur = a, nxt(a.size());
  for (int k = 0; k < n; ++k) {
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        double s = 0;
        int c = 0;
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            const int xx = x + dx, yy = y + dy;
            if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
            s += cur[static_cast<size_t>(yy * w + xx)];
            ++c;
          }
        }
        nxt[static_cast<size_t>(y * w + x)] = s / c;
      }
    }
    std::swap(cur, nxt);
  }
  return cur;
}

}  // namespace

render::Image ground_overlay(const map::Map& m, const map::TerrainTable& terrain, uint64_t seed) {
  const int w = m.width(), h = m.height();
  Rng rng(seed * 7919 + 1234567);
  const auto& tt = terrain.terrains();
  auto kind = [&](int x, int y) {   // 0 egyéb, 1 fű, 2 föld, 3 víz, 4 szikla
    const uint16_t t = m.terrain(x, y);
    if (t >= tt.size()) return 0;
    const std::string& id = tt[t].id;
    if (id == "grass" || id == "grass_lush" || id == "grass_dry" || id == "meadow" || id == "forest_floor") return 1;
    if (id == "dirt" || id == "mud" || id == "gravel" || id == "road") return 2;
    if (id == "shallow_water" || id == "deep_water") return 3;
    if (id == "rock" || id == "cliff") return 4;
    return 0;
  };
  Grid land(static_cast<size_t>(w * h)), grassm(static_cast<size_t>(w * h)), wood(static_cast<size_t>(w * h), 0.0);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const int k = kind(x, y);
      land[static_cast<size_t>(y * w + x)] = (k == 1 || k == 2) ? 1.0 : 0.0;
      grassm[static_cast<size_t>(y * w + x)] = k == 1 ? 1.0 : 0.0;
    }
  }
  const auto& ob = m.objects();
  for (const auto& [tx, ty] : ob.trees) {
    if (tx >= 0 && ty >= 0 && tx < w && ty < h) wood[static_cast<size_t>(ty * w + tx)] = 1.0;
  }
  Grid forest = blur(wood, h, w, 3);
  for (double& v : forest) v = std::clamp(v * 3.0, 0.0, 1.0);

  // ---- kitaposott ösvények (fél felbontás, Dijkstra) ----
  const int H2 = (h + 1) / 2, W2 = (w + 1) / 2;
  Grid cost(static_cast<size_t>(H2 * W2));
  const Grid noise = fbm(rng, H2, W2, {5, 2}, {0.65, 0.35});
  std::vector<uint8_t> blocked_gold(static_cast<size_t>(w * h), 0);
  for (const auto& r : ob.resources) {
    for (int yy = r.y; yy < r.y + r.size; ++yy) {
      for (int xx = r.x; xx < r.x + r.size; ++xx) {
        if (xx >= 0 && yy >= 0 && xx < w && yy < h) blocked_gold[static_cast<size_t>(yy * w + xx)] = 1;
      }
    }
  }
  for (int y = 0; y < H2; ++y) {
    for (int x = 0; x < W2; ++x) {
      double c = 0;
      for (int yy = 2 * y; yy <= 2 * y + 1; ++yy) {
        for (int xx = 2 * x; xx <= 2 * x + 1; ++xx) {
          if (xx >= w || yy >= h) continue;
          const int k = kind(xx, yy);
          c += (k == 3 || k == 4) ? 40 : k == 2 ? 0.15 : 0.25;
          if (wood[static_cast<size_t>(yy * w + xx)] > 0) c += 6;
          if (blocked_gold[static_cast<size_t>(yy * w + xx)]) c += 40;
        }
      }
      const double nz = noise[static_cast<size_t>(y * W2 + x)];
      cost[static_cast<size_t>(y * W2 + x)] = c + nz * nz * 6.0;
    }
  }
  Grid path2(static_cast<size_t>(H2 * W2), 0.0);
  std::vector<std::pair<int, int>> starts;
  for (const auto& [sx, sy] : ob.starts) starts.emplace_back(std::min(W2 - 1, sx / 2), std::min(H2 - 1, sy / 2));
  if (!starts.empty()) {
    const std::pair<int, int> center{W2 / 2, H2 / 2};
    std::vector<int> ang(starts.size());
    for (size_t i = 0; i < ang.size(); ++i) ang[i] = static_cast<int>(i);
    std::sort(ang.begin(), ang.end(), [&](int a, int b) {
      return std::atan2(starts[a].second - center.second, starts[a].first - center.first) <
             std::atan2(starts[b].second - center.second, starts[b].first - center.first);
    });
    std::vector<std::pair<int, int>> golds;
    for (const auto& r : ob.resources) {
      if (r.kind == "gold") golds.emplace_back((r.x + r.size / 2) / 2, (r.y + r.size / 2) / 2);
    }
    const int n = static_cast<int>(starts.size());
    for (int k = 0; k < n; ++k) {
      const auto s = starts[static_cast<size_t>(ang[static_cast<size_t>(k)])];
      std::vector<std::pair<int, int>> targets = {center};
      if (n > 1) targets.push_back(starts[static_cast<size_t>(ang[static_cast<size_t>((k + 1) % n)])]);
      if (!golds.empty()) {
        auto best = *std::min_element(golds.begin(), golds.end(), [&](const auto& a, const auto& b) {
          return (a.first - s.first) * (a.first - s.first) + (a.second - s.second) * (a.second - s.second) <
                 (b.first - s.first) * (b.first - s.first) + (b.second - s.second) * (b.second - s.second);
        });
        if ((best.first - s.first) * (best.first - s.first) + (best.second - s.second) * (best.second - s.second) < 15 * 15) targets.push_back(best);
      }
      // Dijkstra a kezdőhelyből
      std::vector<double> dist(static_cast<size_t>(H2 * W2), 1e18);
      std::vector<int> prev(static_cast<size_t>(H2 * W2), -1);
      using Q = std::pair<double, int>;
      std::priority_queue<Q, std::vector<Q>, std::greater<Q>> pq;
      const int src = s.second * W2 + s.first;
      dist[static_cast<size_t>(src)] = 0;
      pq.push({0, src});
      static const int nb[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
      while (!pq.empty()) {
        const auto [d, i] = pq.top();
        pq.pop();
        if (d > dist[static_cast<size_t>(i)]) continue;
        const int x = i % W2, y = i / W2;
        for (int q = 0; q < 8; ++q) {
          const int nx = x + nb[q][0], ny = y + nb[q][1];
          if (nx < 0 || ny < 0 || nx >= W2 || ny >= H2) continue;
          const int j = ny * W2 + nx;
          const double nd = d + cost[static_cast<size_t>(j)] * (q < 4 ? 1.0 : 1.41);
          if (nd < dist[static_cast<size_t>(j)]) {
            dist[static_cast<size_t>(j)] = nd;
            prev[static_cast<size_t>(j)] = i;
            pq.push({nd, j});
          }
        }
      }
      for (const auto& t : targets) {
        int node = std::clamp(t.second, 0, H2 - 1) * W2 + std::clamp(t.first, 0, W2 - 1);
        for (int guard = 0; node >= 0 && guard < W2 * H2; ++guard) {
          path2[static_cast<size_t>(node)] = 1.0;
          node = prev[static_cast<size_t>(node)];
        }
      }
    }
  }
  Grid path(static_cast<size_t>(w * h));
  const Grid wid = fbm(rng, h, w, {8, 3}, {0.5, 0.5});
  const double base_w = 0.75 + rng.next() * 0.1;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const double p = path2[static_cast<size_t>(std::min(y / 2, H2 - 1) * W2 + std::min(x / 2, W2 - 1))];
      path[static_cast<size_t>(y * w + x)] = std::clamp(p * 1.15 * (base_w + wid[static_cast<size_t>(y * w + x)] * 0.5), 0.0, 1.0);
    }
  }
  path = blur(path, h, w, 1);   // a fél felbontás lépcsői helyett lágy szél

  // ---- foltok (a régi erdei terv: moha, száraz fű, avar) és az ösvény ----
  const int Wp = w * kOverlayPx, Hp = h * kOverlayPx;
  std::vector<float> rgb(static_cast<size_t>(Wp) * static_cast<size_t>(Hp) * 3, 0.0f), alpha(static_cast<size_t>(Wp) * static_cast<size_t>(Hp), 0.0f);
  const Grid keep = blur(land, h, w, 1);
  auto sample = [&](const Grid& g, int px, int py) {   // bilineáris a csempe-rácsból
    const double fx = (px + 0.5) / kOverlayPx - 0.5, fy = (py + 0.5) / kOverlayPx - 0.5;
    const int ix = std::clamp(static_cast<int>(std::floor(fx)), 0, w - 1), iy = std::clamp(static_cast<int>(std::floor(fy)), 0, h - 1);
    const int jx = std::min(ix + 1, w - 1), jy = std::min(iy + 1, h - 1);
    const double ax = std::clamp(fx - ix, 0.0, 1.0), ay = std::clamp(fy - iy, 0.0, 1.0);
    auto at = [&](int xx, int yy) { return g[static_cast<size_t>(yy * w + xx)]; };
    return (at(ix, iy) * (1 - ax) + at(jx, iy) * ax) * (1 - ay) + (at(ix, jy) * (1 - ax) + at(jx, jy) * ax) * ay;
  };
  struct Layer {
    float r, g, b;
    double thr, amax;
    int kind;   // 0 moha, 1 száraz, 2 avar, 3 ösvény
  };
  const Layer plan[] = {{46, 92, 34, 0.5, 0.65, 0}, {150, 144, 74, 0.6, 0.55, 1}, {112, 70, 34, 0.35, 0.65, 2}, {128, 104, 70, 0.45, 0.9, 3}};
  Grid grass_soft(static_cast<size_t>(w * h));
  for (size_t i = 0; i < grass_soft.size(); ++i) grass_soft[i] = grassm[i] * std::clamp(1 - path[i] * 1.5, 0.0, 1.0);
  for (const Layer& L : plan) {
    Grid field;
    if (L.kind == 2) {
      const Grid f2 = fbm(rng, h, w, {5, 2}, {0.6, 0.4});
      field.resize(forest.size());
      for (size_t i = 0; i < field.size(); ++i) field[i] = forest[i] * (0.6 + 0.4 * f2[i]);
    } else if (L.kind == 3) {
      field = path;
    } else {
      field = fbm(rng, h, w);
    }
    Grid a_tile(static_cast<size_t>(w * h));
    for (size_t i = 0; i < a_tile.size(); ++i) {
      const double mask = L.kind == 3 ? land[i] : grass_soft[i];
      a_tile[i] = std::clamp((field[i] - L.thr) / 0.18, 0.0, 1.0) * mask * keep[i];
    }
    for (int py = 0; py < Hp; ++py) {
      for (int px = 0; px < Wp; ++px) {
        double a = sample(a_tile, px, py);
        if (a <= 0) continue;
        const double grain = rng.next();
        a = std::clamp((a + (grain - 0.5) * 0.35 - 0.3) / 0.35, 0.0, 1.0) * L.amax;   // szemcsés, organikus szél
        if (L.kind == 2 && rng.next() < 0.45) a = 0;
        if (a <= 0) continue;
        const size_t i = static_cast<size_t>(py) * static_cast<size_t>(Wp) + static_cast<size_t>(px);
        const float fa = static_cast<float>(a);
        rgb[i * 3 + 0] = rgb[i * 3 + 0] * (1 - fa) + L.r * fa;
        rgb[i * 3 + 1] = rgb[i * 3 + 1] * (1 - fa) + L.g * fa;
        rgb[i * 3 + 2] = rgb[i * 3 + 2] * (1 - fa) + L.b * fa;
        alpha[i] = alpha[i] + fa * (1 - alpha[i]);
      }
    }
  }
  render::Image img;
  img.width = Wp;
  img.height = Hp;
  img.rgba.assign(static_cast<size_t>(Wp) * static_cast<size_t>(Hp) * 4, 0);
  for (size_t i = 0; i < alpha.size(); ++i) {
    const float a = alpha[i];
    if (a < 1e-3f) continue;
    for (int c = 0; c < 3; ++c) img.rgba[i * 4 + static_cast<size_t>(c)] = static_cast<uint8_t>(std::clamp(rgb[i * 3 + static_cast<size_t>(c)] / a, 0.0f, 255.0f));
    img.rgba[i * 4 + 3] = static_cast<uint8_t>(std::clamp(a * 255.0f, 0.0f, 255.0f));
  }
  return img;
}

render::Image module_overlay(int w, int h, const std::vector<uint8_t>& creep, const std::vector<uint8_t>& pond) {
  render::Image img;
  const size_t n = static_cast<size_t>(std::max(0, w)) * static_cast<size_t>(std::max(0, h));
  const bool has_c = creep.size() == n && n > 0, has_p = pond.size() == n && n > 0;
  if (!has_c && !has_p) return img;
  constexpr int P = kOverlayPx;
  img.width = w * P;
  img.height = h * P;
  img.rgba.assign(static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 4, 0);
  // a csempemaszk 3 × 3-as simítása, majd kétszer lineáris mintavétel: kerekded part, mint a régi `smoothscale`
  auto smooth = [&](const std::vector<uint8_t>& m, int rad) {
    std::vector<float> f(n, 0.0f);
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        float sum = 0, cnt = 0;
        for (int dy = -rad; dy <= rad; ++dy) {
          for (int dx = -rad; dx <= rad; ++dx) {
            if (dx * dx + dy * dy > rad * rad + 1) continue;   // kerek ablak
            const int xx = std::clamp(x + dx, 0, w - 1), yy = std::clamp(y + dy, 0, h - 1);
            const float k = dx == 0 && dy == 0 ? 2.0f : 1.0f;
            sum += (m[static_cast<size_t>(yy * w + xx)] ? 1.0f : 0.0f) * k;
            cnt += k;
          }
        }
        f[static_cast<size_t>(y * w + x)] = sum / cnt;
      }
    }
    return f;
  };
  const std::vector<float> fc = has_c ? smooth(creep, 1) : std::vector<float>{}, fp = has_p ? smooth(pond, 2) : std::vector<float>{};
  auto sample = [&](const std::vector<float>& f, float tx, float ty) {
    const float gx = std::clamp(tx - 0.5f, 0.0f, static_cast<float>(w - 1)), gy = std::clamp(ty - 0.5f, 0.0f, static_cast<float>(h - 1));
    const int x0 = static_cast<int>(gx), y0 = static_cast<int>(gy);
    const int x1 = std::min(x0 + 1, w - 1), y1 = std::min(y0 + 1, h - 1);
    const float ax = gx - static_cast<float>(x0), ay = gy - static_cast<float>(y0);
    auto at = [&](int x, int y) { return f[static_cast<size_t>(y * w + x)]; };
    return (at(x0, y0) * (1 - ax) + at(x1, y0) * ax) * (1 - ay) + (at(x0, y1) * (1 - ax) + at(x1, y1) * ax) * ay;
  };
  auto hash = [](int x, int y) {
    uint32_t v = static_cast<uint32_t>(x) * 73856093u ^ static_cast<uint32_t>(y) * 19349663u;
    v ^= v >> 13;
    v *= 0x5bd1e995u;
    return static_cast<float>((v >> 8) & 0xffff) / 65535.0f;
  };
  auto vnoise = [&](float x, float y) {   // simított értékzaj (a gyökérerekhez és a víz árnyalatához)
    const int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(x0), fy = y - static_cast<float>(y0);
    const float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    const float a = hash(x0, y0), b = hash(x0 + 1, y0), c = hash(x0, y0 + 1), d = hash(x0 + 1, y0 + 1);
    return (a * (1 - sx) + b * sx) * (1 - sy) + (c * (1 - sx) + d * sx) * sy;
  };
  auto smoothstep = [](float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
  };
  for (int py = 0; py < img.height; ++py) {
    for (int px = 0; px < img.width; ++px) {
      const float tx = (static_cast<float>(px) + 0.5f) / P, ty = (static_cast<float>(py) + 0.5f) / P;
      uint8_t* o = &img.rgba[(static_cast<size_t>(py) * static_cast<size_t>(img.width) + static_cast<size_t>(px)) * 4];
      const float noise = hash(px / 2, py / 2);
      if (has_p) {   // tavacska: víz (a közepe sötétebb), habvonal, homoksáv
        const float v = sample(fp, tx, ty);
        if (v > 0.36f) {
          float r, g, b, a;
          if (v > 0.58f) {
            const float d = smoothstep(0.58f, 1.0f, v) * (0.75f + 0.25f * vnoise(tx * 0.7f, ty * 0.7f));
            r = 62 - 22 * d; g = 108 - 30 * d; b = 136 - 26 * d; a = 235;
          } else if (v > 0.5f) {
            r = 214; g = 226; b = 220; a = 200;
          } else {
            r = 184 + 10 * noise; g = 166 + 8 * noise; b = 118; a = 220 * smoothstep(0.36f, 0.44f, v);
          }
          o[0] = static_cast<uint8_t>(r); o[1] = static_cast<uint8_t>(g); o[2] = static_cast<uint8_t>(b); o[3] = static_cast<uint8_t>(a);
          continue;
        }
      }
      if (has_c) {   // fertőzött talaj (D53): szürkészöld, foltos, a széle lágy; vékony, sötét gyökérerek
        const float v = sample(fc, tx, ty);
        const float a = smoothstep(0.32f, 0.6f, v);
        if (a <= 0.0f) continue;
        // gyökérerek: egy simított zajmező szintvonalai (szabálytalan, elágazó vonalak), csak a folt belsejében
        const float nv = vnoise(tx * 0.9f, ty * 0.9f) * 0.65f + vnoise(tx * 2.3f + 17.0f, ty * 2.3f) * 0.35f;
        const float rw = v > 0.6f ? std::clamp(1.0f - std::abs(nv - 0.5f) / 0.03f, 0.0f, 1.0f) : 0.0f;   // lágy szélű ér
        const float m = 0.85f + 0.3f * noise;
        auto mix = [&](float base, float root) { return std::clamp((base + (root - base) * rw) * m, 0.0f, 255.0f); };
        o[0] = static_cast<uint8_t>(mix(112.0f, 62.0f));
        o[1] = static_cast<uint8_t>(mix(124.0f, 58.0f));
        o[2] = static_cast<uint8_t>(mix(96.0f, 44.0f));
        o[3] = static_cast<uint8_t>(std::clamp(a * (160.0f + 50.0f * rw), 0.0f, 255.0f));
      }
    }
  }
  return img;
}

}  // namespace totu::client
