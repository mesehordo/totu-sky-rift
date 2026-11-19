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

}  // namespace totu::client
