// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/cursors.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "content/json.hpp"
#include "render/image.hpp"

namespace totu::client {

Cursors::~Cursors() {
  for (auto& [k, e] : cursors_) {
    if (e.cursor) SDL_DestroyCursor(e.cursor);
  }
}

void Cursors::load(const std::string& assets_dir, int size) {
  const std::filesystem::path dir = std::filesystem::path(assets_dir) / "ui" / "cursors";
  std::error_code ec;
  for (const auto& f : std::filesystem::directory_iterator(dir, ec)) {
    if (f.path().extension() != ".json") continue;
    std::ifstream in(f.path(), std::ios::binary);
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    const auto doc = content::parse(text);
    if (!doc) continue;
    const content::Value* id = doc->find("id");
    const content::Value* image = doc->find("image");
    if (!id || !id->is_string() || !image || !image->is_string()) continue;
    auto img = render::load_png((dir / image->text()).string());
    if (!img || img->width <= 0) continue;
    int base = 32, hx = 0, hy = 0;   // a leírás mérete és forrópontja (a képpontos kép ennek a többszöröse)
    if (const content::Value* sz = doc->find("size"); sz && sz->is_array() && !sz->items().empty()) {
      base = static_cast<int>(sz->items()[0].as_int().value_or(32));
    }
    if (const content::Value* hs = doc->find("hotspot"); hs && hs->is_array() && hs->items().size() == 2) {
      hx = static_cast<int>(hs->items()[0].as_int().value_or(0));
      hy = static_cast<int>(hs->items()[1].as_int().value_or(0));
    }
    // területátlagos kicsinyítés a kívánt méretre (élsimítás, mint a régi `smoothscale`)
    const int out = std::max(8, size);
    std::vector<uint8_t> px(static_cast<size_t>(out * out * 4), 0);
    const float k = static_cast<float>(img->width) / static_cast<float>(out);
    for (int y = 0; y < out; ++y) {
      for (int x = 0; x < out; ++x) {
        float acc[4] = {0, 0, 0, 0};
        int n = 0;
        for (int sy = static_cast<int>(static_cast<float>(y) * k); sy < static_cast<int>(static_cast<float>(y + 1) * k) && sy < img->height; ++sy) {
          for (int sx = static_cast<int>(static_cast<float>(x) * k); sx < static_cast<int>(static_cast<float>(x + 1) * k) && sx < img->width; ++sx) {
            const uint8_t* p = img->at(sx, sy);
            const float a = static_cast<float>(p[3]) / 255.0f;
            acc[0] += static_cast<float>(p[0]) * a;
            acc[1] += static_cast<float>(p[1]) * a;
            acc[2] += static_cast<float>(p[2]) * a;
            acc[3] += a;
            ++n;
          }
        }
        uint8_t* o = &px[static_cast<size_t>((y * out + x) * 4)];
        if (n > 0 && acc[3] > 0) {
          o[0] = static_cast<uint8_t>(std::clamp(acc[0] / acc[3], 0.0f, 255.0f));
          o[1] = static_cast<uint8_t>(std::clamp(acc[1] / acc[3], 0.0f, 255.0f));
          o[2] = static_cast<uint8_t>(std::clamp(acc[2] / acc[3], 0.0f, 255.0f));
          o[3] = static_cast<uint8_t>(std::clamp(acc[3] / static_cast<float>(n) * 255.0f, 0.0f, 255.0f));
        }
      }
    }
    SDL_Surface* s = SDL_CreateSurfaceFrom(out, out, SDL_PIXELFORMAT_RGBA32, px.data(), out * 4);
    if (!s) continue;
    const float hk = static_cast<float>(out) / static_cast<float>(std::max(1, base));
    SDL_Cursor* c = SDL_CreateColorCursor(s, std::clamp(static_cast<int>(static_cast<float>(hx) * hk), 0, out - 1),
                                          std::clamp(static_cast<int>(static_cast<float>(hy) * hk), 0, out - 1));
    SDL_DestroySurface(s);
    if (c) cursors_[id->text()].cursor = c;
  }
}

void Cursors::set(const std::string& kind) {
  if (kind == current_) return;
  auto it = cursors_.find(kind);
  if (it == cursors_.end()) it = cursors_.find("arrow");
  current_ = kind;
  if (it != cursors_.end() && it->second.cursor) SDL_SetCursor(it->second.cursor);
  else SDL_SetCursor(SDL_GetDefaultCursor());
}

}  // namespace totu::client
