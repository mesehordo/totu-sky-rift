// SPDX-License-Identifier: GPL-3.0-or-later
// A terep fedőrétege a régi játék szerint (rts/scenery.py `Scenery`): kitaposott földutak a kezdőhelyek, a pálya
// közepe és a közeli aranybányák között (fél felbontású, kanyargós Dijkstra), valamint a talajfoltok – moha, száraz
// fű, avar az erdő közelében – zajból, szemcsés széllel. Csak megjelenítés: egyszer készül, rögzített véletlenből.
#pragma once

#include <cstdint>

#include "map/map.hpp"
#include "map/terrain.hpp"
#include "render/image.hpp"

namespace totu::client {

constexpr int kOverlayPx = 8;   // képpont csempénként (a régi PX)

// a teljes pálya fedőrétege (szélesség × 8, magasság × 8), előszorzás nélküli RGBA
render::Image ground_overlay(const map::Map& m, const map::TerrainTable& terrain, uint64_t seed);

}  // namespace totu::client
