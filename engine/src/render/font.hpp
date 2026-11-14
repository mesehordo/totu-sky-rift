// SPDX-License-Identifier: GPL-3.0-or-later
// Betűk (M6 9. lépés, ideiglenes HUD): a játék saját TTF-betűje (a régi játék Rováskönyv Text betűje,
// `assets/fonts`) egy méretben atlaszba sütve (stb_truetype), a magyar ékezetes betűkkel. A sütés grafikus kártya
// nélkül fut (tesztelhető); a rajzolás a sprite-köteggel, fehér betűk árnyalattal.
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "render/image.hpp"

namespace totu::render {

struct Glyph {
  float u0 = 0, v0 = 0, u1 = 0, v1 = 0;   // az atlaszban (0..1)
  float x0 = 0, y0 = 0, x1 = 0, y1 = 0;   // a tollhoz képest (képpont; y lefelé, az alapvonaltól)
  float advance = 0;
};

struct FontBake {
  Image atlas;                                     // fehér, az alfa a betű (előszorzott)
  std::vector<std::pair<uint32_t, Glyph>> glyphs;  // kódpont szerint rendezve
  float size = 0, ascent = 0, line = 0;
  const Glyph* find(uint32_t cp) const;
  float width(std::string_view utf8) const;        // a szöveg szélessége képpontban
};

// A TTF-adatból `px` magasságban: az ASCII és a magyar ékezetes betűk (és néhány jel: →, ×, •, …, ✓, −). false: hibás betű.
bool bake_font(const std::vector<uint8_t>& ttf, float px, FontBake& out);

// UTF-8 → kódpontok (a hibás bájt kihagyva)
std::vector<uint32_t> utf8_codepoints(std::string_view s);

}  // namespace totu::render
