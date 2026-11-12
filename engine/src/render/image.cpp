// SPDX-License-Identifier: GPL-3.0-or-later
// PNG olvasása és írása a beépített stb_image / stb_image_write segítségével (a megvalósítás: stb_impl.cpp).
#include "render/image.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>

#include "stb_image.h"
#include "stb_image_write.h"

namespace totu::render {

std::optional<Image> decode_png(const std::vector<uint8_t>& bytes, std::string* error) {
  int w = 0, h = 0, n = 0;
  stbi_uc* px = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &n, 4);
  if (!px) {
    if (error) *error = stbi_failure_reason() ? stbi_failure_reason() : "ismeretlen hiba";
    return std::nullopt;
  }
  Image img;
  img.width = w;
  img.height = h;
  img.rgba.assign(px, px + static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
  stbi_image_free(px);
  return img;
}

std::optional<Image> load_png(const std::string& path, std::string* error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (error) *error = "nem olvasható: " + path;
    return std::nullopt;
  }
  const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  return decode_png(bytes, error);
}

std::vector<uint8_t> encode_png(const Image& img) {
  std::vector<uint8_t> out;
  stbi_write_png_to_func(
      [](void* ctx, void* data, int size) {
        auto* v = static_cast<std::vector<uint8_t>*>(ctx);
        const auto* p = static_cast<const uint8_t*>(data);
        v->insert(v->end(), p, p + size);
      },
      &out, img.width, img.height, 4, img.rgba.data(), img.width * 4);
  return out;
}

bool save_png(const std::string& path, const Image& img) {
  const auto bytes = encode_png(img);
  if (bytes.empty()) return false;
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return static_cast<bool>(out);
}

Image Image::half() const {
  Image r;
  r.width = std::max(1, width / 2);
  r.height = std::max(1, height / 2);
  r.rgba.assign(static_cast<size_t>(r.width) * static_cast<size_t>(r.height) * 4, 0);
  for (int y = 0; y < r.height; ++y) {
    for (int x = 0; x < r.width; ++x) {
      uint32_t acc[4] = {0, 0, 0, 0};
      for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
          const uint8_t* p = at(std::min(width - 1, 2 * x + dx), std::min(height - 1, 2 * y + dy));
          const uint32_t a = p[3];
          acc[0] += p[0] * a;   // előszorzott alfa
          acc[1] += p[1] * a;
          acc[2] += p[2] * a;
          acc[3] += a;
        }
      }
      uint8_t* q = r.at(x, y);
      if (acc[3]) {
        q[0] = static_cast<uint8_t>(acc[0] / acc[3]);
        q[1] = static_cast<uint8_t>(acc[1] / acc[3]);
        q[2] = static_cast<uint8_t>(acc[2] / acc[3]);
      }
      q[3] = static_cast<uint8_t>((acc[3] + 2) / 4);
    }
  }
  return r;
}

Image Image::crop(int x0, int y0, int w, int h) const {
  Image r;
  r.width = w;
  r.height = h;
  r.rgba.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 0);
  for (int y = 0; y < h; ++y) {
    const int sy = y0 + y;
    if (sy < 0 || sy >= height) continue;
    for (int x = 0; x < w; ++x) {
      const int sx = x0 + x;
      if (sx < 0 || sx >= width) continue;
      std::memcpy(r.at(x, y), at(sx, sy), 4);
    }
  }
  return r;
}

int diff_permille(const Image& a, const Image& b, int threshold) {
  if (a.width != b.width || a.height != b.height) return 1000;
  size_t bad = 0;
  const size_t n = static_cast<size_t>(a.width) * static_cast<size_t>(a.height);
  for (size_t i = 0; i < n; ++i) {
    int m = 0;
    for (int c = 0; c < 4; ++c) m = std::max(m, std::abs(int{a.rgba[i * 4 + static_cast<size_t>(c)]} - int{b.rgba[i * 4 + static_cast<size_t>(c)]}));
    if (m > threshold) ++bad;
  }
  return n ? static_cast<int>(bad * 1000 / n) : 0;
}

}  // namespace totu::render
