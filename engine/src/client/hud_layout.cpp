// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/hud_layout.hpp"

#include <algorithm>
#include <cmath>

namespace totu::client {

int py_round(double v) {
  const double f = std::floor(v);
  const double d = v - f;
  if (d > 0.5) return static_cast<int>(f) + 1;
  if (d < 0.5) return static_cast<int>(f);
  const int fi = static_cast<int>(f);
  return fi % 2 == 0 ? fi : fi + 1;
}

int key_slot(int key) {
  if (key >= 'a' && key <= 'z') key -= 'a' - 'A';
  for (int i = 0; i < 12; ++i) {
    if (kSlotKeys[i] == key) return i;
  }
  return -1;
}

namespace {

constexpr double kHudMaxH = 0.33;   // az alsó sáv legfeljebb a képernyőmagasság ennyied része

HudLayout layout_at(int sw, int sh, double s) {
  HudLayout L;
  L.s = s;
  const int pad = std::max(4, py_round(8 * s)), gap = std::max(6, py_round(11 * s)), cg = std::max(2, py_round(4 * s));
  const int fp = std::max(3, py_round(5 * s));   // a szekciók saját kerete
  const int cell = (py_round(214 * s) - 2 * pad - 2 * fp - 2 * cg) / 3;
  const int inner = cell * 3 + cg * 2;
  const int H = inner + 2 * fp + 2 * pad;
  const int g2 = cell * 2 + cg, g4 = cell * 4 + cg * 3;
  const int ban = std::max(12, py_round(22 * s));
  const int bh = std::max(7, py_round(12 * s)), bgap = std::max(2, py_round(3 * s));
  const int P = inner - 2 * (bh + bgap);   // portré 1:1, alatta életerő és mana/energia
  const int gin = std::max(5, py_round(9 * s));
  const int stats = py_round(176 * s);
  // minimap, banner1, karakterlap, tárgyak, kijelölés, banner2, parancsrács
  const int widths[7] = {inner, ban, P + gin + stats, g2, g4, ban, g4};
  const bool framed[7] = {true, false, true, true, true, false, true};
  int W = 2 * pad + gap * 6;
  for (int i = 0; i < 7; ++i) W += widths[i] + (framed[i] ? 2 * fp : 0);
  const int x0 = (sw - W) / 2, y0 = sh - H;
  L.pad = pad; L.gap = gap; L.fp = fp; L.cell = cell; L.cg = cg;
  L.hud = {x0, y0, W, H};
  Rect* slots[7] = {&L.minimap, &L.banner1, &L.char_sheet, &L.inv, &L.sel, &L.banner2, &L.cmd};
  int x = x0 + pad;
  for (int i = 0; i < 7; ++i) {
    if (framed[i]) {
      *slots[i] = {x + fp, y0 + pad + fp, widths[i], inner};
      L.frames.push_back(slots[i]->inflate(2 * fp, 2 * fp));
      x += widths[i] + 2 * fp + gap;
    } else {
      *slots[i] = {x, y0 + pad, widths[i], inner + 2 * fp};
      x += widths[i] + gap;
    }
  }
  const Rect& c = L.char_sheet;
  L.portrait = {c.x, c.y, P, P};
  L.hpbar = {c.x, c.y + P + bgap, P, bh};
  L.mpbar = {c.x, c.y + P + 2 * bgap + bh, P, bh};
  L.stats = {c.x + P + gin, c.y, stats, inner};
  L.divider = c.x + P + gin / 2;
  auto grid = [&](const Rect& r, int cols, Rect* out) {
    for (int i = 0; i < cols * 3; ++i) out[i] = {r.x + (i % cols) * (cell + cg), r.y + (i / cols) * (cell + cg), cell, cell};
  };
  grid(L.inv, 2, L.inv_cells.data());
  grid(L.sel, 4, L.sel_cells.data());
  grid(L.cmd, 4, L.cmd_cells.data());
  const int th = std::max(24, py_round(34 * s)), rw = py_round(128 * s), ty = std::max(4, py_round(6 * s));
  const int TW = th + gap + 4 * rw + gap + th;
  const int tx = (sw - TW) / 2;
  L.allies = {tx, ty, th, th};
  L.resbar = {tx + th + gap, ty, 4 * rw, th};
  for (int i = 0; i < 4; ++i) L.res[static_cast<size_t>(i)] = {tx + th + gap + i * rw, ty, rw, th};
  L.gear = {tx + th + gap + 4 * rw + gap, ty, th, th};
  L.faith = {tx + th + gap + 4 * rw + gap + th + gap, ty, py_round(rw * 0.9), th};   // Áhítat / Tóének
  L.topbar = {tx, ty, TW, th};
  const int iw = py_round(th * 1.9);   // tétlen munkások jelzője: jobbra, közvetlenül a HUD fölött
  L.idle = {L.hud.right() - iw - std::max(6, py_round(10 * s)), L.hud.y - th - std::max(2, py_round(4 * s)), iw, th};
  L.quest_x = 2 * pad + 4;
  L.quest_y = ty + th + std::max(4, py_round(8 * s));
  L.title_x = 2 * pad + 4;
  L.title_y = ty;
  L.font = {std::max(12, py_round(19 * s)), std::max(10, py_round(14 * s)), std::max(10, py_round(12 * s)),
            std::max(10, py_round(13 * s)), std::max(11, py_round(16 * s))};
  return L;
}

}  // namespace

HudLayout hud_layout(int sw, int sh) {
  // a sáv kitölti a képernyő szélességét (referencia: TOTU HUD-kép), de legfeljebb a magasság harmada
  const HudLayout base = layout_at(sw, sh, 1.0);
  double s = std::min((sw - 8) / static_cast<double>(base.hud.w), sh * kHudMaxH / static_cast<double>(base.hud.h));
  HudLayout L = layout_at(sw, sh, s);
  while (L.hud.w > sw - 8 && s > 0.3) {
    s *= 0.98;
    L = layout_at(sw, sh, s);
  }
  return L;
}

}  // namespace totu::client
