// SPDX-License-Identifier: GPL-3.0-or-later
// A kijelölés „pecsétje” a régi játék szerint (`UI.sel_sigil`): lágy belső fény, sötét kontúrú kettős gyűrű és négy
// nyílhegy a tengelyeken (`inward`: befelé mutatnak – a mozgáscél jele). Processzoron rajzolva, háromszoros
// felbontásban, majd átlagolva (élsimítás); előszorzás nélkül.
#pragma once

#include <array>
#include <cstdint>

#include "render/image.hpp"

namespace totu::client {

// `w`: a gyűrű szélessége képpontban; a kép (w + 12) × (0,6 w + 12)
render::Image sigil_image(std::array<uint8_t, 3> color, float w, bool inward);

// A gyülekezőpont képei (a régi `draw_rally_flag`, `draw_rally_flag_shadow`, `_draw_marching_arrows`). A színezhető
// részek fehérek (a rajzoló a csapatszínnel árnyalja), a többi a saját színében.
render::Image rally_arrow_image(float angle);    // 16 × 16, középre: menetelő nyíl az `angle` irányba (fehér)
render::Image rally_pole_image();                // 22 × 32: a rúd és a gomb (a talppont: 3, 30)
render::Image rally_cloth_image(int phase);      // 22 × 32: a fecskefarkú zászló (fehér, sötét kontúr), 8 fázis
render::Image rally_shadow_image(int phase);     // 40 × 20: a vetett árnyék (a rúd töve: 3, 3), 8 fázis

// Az épületek csapatjelző lobogója (D51), 16 × 56-os képek (a talppont 5, 54): a rúd (sötét fa), a gomb (fehér – a nép
// kiemelőszíne árnyalja), a lobogó (fehér – a csapatszín) és a szegélye (fehér – a nép kiemelőszíne), 8 lengésfázisban
render::Image banner_pole_image();
render::Image banner_knob_image();
render::Image banner_cloth_image(int phase);
render::Image banner_trim_image(int phase);

}  // namespace totu::client
