// SPDX-License-Identifier: GPL-3.0-or-later
// A kép nélküli egységek rajza (a régi `render.draw_shape`): felülnézeti alakzat a sablon `Identity.shape` mezője
// szerint (dolgozó, harcos, pajzsos, íjász, lovas, varázsló, vadállat, repülő, bogár, köpködő, ostromgép, iszonyat,
// gólem, gubó), 8 irányban. Négy réteg, hogy a rajzoló színezhesse: a csapatszín, a nép alapszíne és kiemelőszíne
// (szürkeárnyalatos, a színezés szorozza), és a saját színű részletek (fegyver, íj). A rétegek képpontonként nem
// fedik egymást (a legfelső elem nyer), így bármilyen sorrendben rárakva a régi rajzot adják.
#pragma once

#include <string>

#include "render/image.hpp"

namespace totu::client {

constexpr int kShapeSize = 40;          // a kép oldala (képpont); az alakzat sugara 11 képpont
constexpr float kShapeRadius = 11.0f;
constexpr int kShapeDirs = 8;           // 0 = K, 2 = D … (a világ iránya)
constexpr int kShapeLayers = 4;         // 0 csapat, 1 alap, 2 kiemelő, 3 saját színű
constexpr int kShapeKinds = 14;

// az alakzat sorszáma a név szerint (ismeretlen: a harcos)
int shape_index(const std::string& shape);
render::Image unit_shape_layer(int shape, int dir, int layer);

}  // namespace totu::client
