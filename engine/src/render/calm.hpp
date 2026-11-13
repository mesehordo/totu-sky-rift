// SPDX-License-Identifier: GPL-3.0-or-later
// „Nyugodt + belső átlag” – a játéktér figuráinak (egység, épület, nyersanyag, tereptárgy) képszűrője, a régi játék
// szerint (rts/calm.py). A képfájlok nem változnak: az atlaszkészítő a képkockákat a csomagolás előtt szűri (a játék
// futás közben nem számol). A csempékre, a talajmatricákra és a felületre nem hat.
//   1. felület: a belső részen (5 × 5-ös alfa-minimum > 220) 3 × 3-as medián, élvédelemmel keverve;
//   2. belső tinta: a simított világosság helyi maximumánál sötétebb pontok halk, lilásfekete tintát kapnak;
//   3. külső körvonal: 1 képpontos, a sziluett belsejének helyi színéből (0,40 × átlag + (8, 6, 8)), a kép alá.
// Grafikus kártya nélkül tesztelhető.
#pragma once

#include <string>

#include "render/image.hpp"

namespace totu::render {

// Melyik sprite-fajtára hat (a régi: units, buildings, resources, props).
bool calm_applies(const std::string& sheet_type);

// A képkocka szűrése helyben (egyenes, nem előszorzott alfa); a képkocka a kép (fx, fy, fw, fh) téglalapja.
void calm_frame(Image& img, int fx, int fy, int fw, int fh);

}  // namespace totu::render
