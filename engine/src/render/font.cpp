// SPDX-License-Identifier: GPL-3.0-or-later
#include "render/font.hpp"

#include <algorithm>

#include "stb_truetype.h"

namespace totu::render {

std::vector<uint32_t> utf8_codepoints(std::string_view s) {
  std::vector<uint32_t> out;
  for (size_t i = 0; i < s.size();) {
    const auto c = static_cast<uint8_t>(s[i]);
    const int n = c < 0x80 ? 0 : (c >> 5) == 0x6 ? 1 : (c >> 4) == 0xE ? 2 : (c >> 3) == 0x1E ? 3 : -1;
    if (n < 0) {
      ++i;
      continue;
    }
    uint32_t cp = n == 0 ? c : n == 1 ? (c & 0x1Fu) : n == 2 ? (c & 0x0Fu) : (c & 0x07u);
    bool ok = true;
    for (int k = 1; k <= n; ++k) {
      if (i + static_cast<size_t>(k) >= s.size()) {
        ok = false;
        break;
      }
      const auto cc = static_cast<uint8_t>(s[i + static_cast<size_t>(k)]);
      if ((cc & 0xC0) != 0x80) {
        ok = false;
        break;
      }
      cp = (cp << 6) | (cc & 0x3Fu);
    }
    if (ok) out.push_back(cp);
    i += ok ? static_cast<size_t>(n) + 1 : 1;
  }
  return out;
}

const Glyph* FontBake::find(uint32_t cp) const {
  const auto it = std::lower_bound(glyphs.begin(), glyphs.end(), cp, [](const auto& g, uint32_t v) { return g.first < v; });
  return it != glyphs.end() && it->first == cp ? &it->second : nullptr;
}

float FontBake::width(std::string_view utf8) const {
  float w = 0;
  for (const uint32_t cp : utf8_codepoints(utf8)) {
    const Glyph* g = find(cp);
    if (!g) g = find('?');
    if (g) w += g->advance;
  }
  return w;
}

bool bake_font(const std::vector<uint8_t>& ttf, float px, FontBake& out) {
  stbtt_fontinfo info;
  if (ttf.empty() || !stbtt_InitFont(&info, ttf.data(), stbtt_GetFontOffsetForIndex(ttf.data(), 0))) return false;
  std::vector<int> cps;
  for (int c = 32; c < 127; ++c) cps.push_back(c);
  for (const int c : {0xC1, 0xC9, 0xCD, 0xD3, 0xD6, 0x150, 0xDA, 0xDC, 0x170, 0xE1, 0xE9, 0xED, 0xF3, 0xF6, 0x151, 0xFA,
                      0xFC, 0x171, 0x2192, 0xD7, 0x2022, 0x2013, 0xB7}) {
    cps.push_back(c);
  }
  const int w = 512, h = 256;
  std::vector<uint8_t> alpha(static_cast<size_t>(w) * h);
  std::vector<stbtt_packedchar> packed(cps.size());
  stbtt_pack_context pc;
  if (!stbtt_PackBegin(&pc, alpha.data(), w, h, 0, 1, nullptr)) return false;
  stbtt_PackSetOversampling(&pc, 1, 1);
  stbtt_pack_range range{};
  range.font_size = px;
  range.array_of_unicode_codepoints = cps.data();
  range.num_chars = static_cast<int>(cps.size());
  range.chardata_for_range = packed.data();
  const int ok = stbtt_PackFontRanges(&pc, ttf.data(), 0, &range, 1);
  stbtt_PackEnd(&pc);
  if (!ok) return false;
  out.atlas.width = w;
  out.atlas.height = h;
  out.atlas.rgba.assign(static_cast<size_t>(w) * h * 4, 0);
  for (size_t i = 0; i < alpha.size(); ++i) {   // fehér, előszorzott alfával
    const uint8_t a = alpha[i];
    out.atlas.rgba[i * 4 + 0] = out.atlas.rgba[i * 4 + 1] = out.atlas.rgba[i * 4 + 2] = out.atlas.rgba[i * 4 + 3] = a;
  }
  out.glyphs.clear();
  for (size_t i = 0; i < cps.size(); ++i) {
    const stbtt_packedchar& p = packed[i];
    Glyph g;
    g.u0 = static_cast<float>(p.x0) / w;
    g.v0 = static_cast<float>(p.y0) / h;
    g.u1 = static_cast<float>(p.x1) / w;
    g.v1 = static_cast<float>(p.y1) / h;
    g.x0 = p.xoff;
    g.y0 = p.yoff;
    g.x1 = p.xoff2;
    g.y1 = p.yoff2;
    g.advance = p.xadvance;
    out.glyphs.emplace_back(static_cast<uint32_t>(cps[i]), g);
  }
  std::sort(out.glyphs.begin(), out.glyphs.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  int asc = 0, desc = 0, gap = 0;
  stbtt_GetFontVMetrics(&info, &asc, &desc, &gap);
  const float scale = stbtt_ScaleForPixelHeight(&info, px);
  out.size = px;
  out.ascent = static_cast<float>(asc) * scale;
  out.line = static_cast<float>(asc - desc + gap) * scale;
  return true;
}

}  // namespace totu::render
