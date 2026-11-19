// SPDX-License-Identifier: GPL-3.0-or-later
// A kijelölés „pecsétje” a régi játék szerint (`UI.sel_sigil`): lágy belső fény, sötét kontúrú kettős gyűrű és négy
// nyílhegy a tengelyeken (`inward`: befelé mutatnak – a mozgáscél jele). Processzoron rajzolva, háromszoros
// felbontásban, majd átlagolva (élsimítás); előszorzás nélkül.
#pragma once

#include <array>
#include <cstdint>
#include <string>

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

// Az épületek csapatjelző lobogója (D51): minden főnépnek saját alakú, címer nélkül. 20 × 56-os képek (a talppont
// 5, 54): a rúd (a nép anyagából), a csúcsdísz (fehér – a nép kiemelőszíne árnyalja), a vászon (fehér – a csapatszín)
// és a szegélye (fehér – a kiemelőszín), 8 lengésfázisban. A stílus: 0 az általános fecskefarkú, 1–8 a főnépeké.
constexpr int kBannerStyles = 9;
// a munkás cipelt nyersanyaga (a régi `draw_unit_marks`): kis zsák sötét kontúrral, 12 × 12; a test 80 %-os szürke, a
// fény fehér (a rajzoló a nyersanyag színével árnyalja)
render::Image carry_sack_image();
// Talajjelek és a hős jelei (a régi `_mark_img`, `hero_sigil`, `hero_badge`): a hős jelvénye 21 × 21: kis korona (keret: sötét
// körvonal és arany szegély; a korona fehér – a csapatszín árnyalja), a talajjel színezhető és rögzített része
// (w + 8) × (0,56 w + 8), az ének forgó rúnagyűrűje 8 fázisban és a rejtőzés pöttyözött gyűrűje (w + 14) × (0,6 w + 14)
render::Image hero_badge_frame();
render::Image hero_badge_symbol();
render::Image hero_ground_tinted(float w);
render::Image hero_ground_fixed(float w);
render::Image chant_mark_image(float w, int phase);
render::Image hidden_mark_image(float w);
// a küldetés célgyűrűje (a régi `draw_objective_ring`): arany, lapított, forgó szaggatott peremmel és rúnákkal;
// `r` sugárra, (2r + 20) × (1,2 r + 20), 12 forgásfázis; a jelzőfény (lebegő kristály és fénysugár), 16 × 64
render::Image objective_ring_image(float r, int phase);
render::Image objective_beacon_image();
// légköri részecskék (a régi `scenery.Ambient`): additív ragyogás (előszorzott szín, nulla alfa – a rajzoló nem
// szorozhatja elő), 12 × 12; falevél 8 elforgatásban (fehér – a levél színe árnyalja), 8 × 8
render::Image ambient_glow_image();
render::Image ambient_leaf_image(int rot);
int banner_style(const std::string& faction);
render::Image banner_pole_image(int style);
render::Image banner_knob_image(int style);
render::Image banner_cloth_image(int style, int phase);
render::Image banner_trim_image(int style, int phase);

}  // namespace totu::client
