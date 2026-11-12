// SPDX-License-Identifier: GPL-3.0-or-later
// Képek (M5): PNG olvasása és írása RGBA8 formában (stb_image, csak PNG). Grafikus kártya nélkül is használható
// (tesztek, atlaszkészítő, képernyőkép-összevetés).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace totu::render {

struct Image {
  int width = 0, height = 0;
  std::vector<uint8_t> rgba;   // soronként fentről lefelé, képpontonként 4 bájt

  bool empty() const { return width <= 0 || height <= 0; }
  uint8_t* at(int x, int y) { return &rgba[(static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4]; }
  const uint8_t* at(int x, int y) const {
    return &rgba[(static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4];
  }
  // Fele akkora kép (2 × 2-es átlag, előszorzott alfával – nincs sötét szegély az átlátszó szélen).
  Image half() const;
  // Téglalap kivágása (a határon kívül átlátszó).
  Image crop(int x, int y, int w, int h) const;
};

std::optional<Image> load_png(const std::string& path, std::string* error = nullptr);
std::optional<Image> decode_png(const std::vector<uint8_t>& bytes, std::string* error = nullptr);
bool save_png(const std::string& path, const Image& img);
std::vector<uint8_t> encode_png(const Image& img);

// Két kép különbsége (képpontonként a legnagyobb csatornaeltérés), a képernyőkép-próbákhoz: a küszöböt meghaladó
// képpontok aránya ezrelékben.
int diff_permille(const Image& a, const Image& b, int threshold);

}  // namespace totu::render
