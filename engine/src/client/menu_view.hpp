// SPDX-License-Identifier: GPL-3.0-or-later
// A menük rajzolása és a menü-ciklus (M8): a `Menu` (GL nélküli állapotgép és elrendezés) elemeit a motor saját
// rajzolójával festi meg, a régi `rts/menu.py` kinézete szerint – festett, lassan úszó háttér, oldalsáv logóval és
// díszvonalakkal, áttetsző panelek kettős kerettel, lebegő-kiemelős gombok, lenyílók, csúszkák; a betű a játék saját
// betűje (Rováskönyv Text, a címekhez Rováskönyv).
#pragma once

#include <string>

#include <SDL3/SDL.h>

#include "client/menu.hpp"
#include "render/rhi.hpp"
#include "render/sprite_batch.hpp"

namespace totu::client {

struct MenuRunOptions {
  std::string assets, content, maps_dir;
  std::string settings_file;   // ide ment a menü (üres: nem ment)
  long frames = -1;            // ennyi képkocka után kilép (füstteszt)
  std::string screenshot;      // az utolsó képkockáról
  MenuScreen start = MenuScreen::Main;
};

// A felbontás és a teljes képernyő alkalmazása az ablakra.
void apply_display(SDL_Window* win, const Settings& st);

// A menü futtatása. true: meccs indul (`out` kitöltve); false: kilépés (a `rc` hibánál 1).
bool run_menu(SDL_Window* win, render::Device& dev, render::SpriteBatch& batch, Settings& st, Locale& loc,
              const MenuRunOptions& o, MatchConfig* out, int* rc);

}  // namespace totu::client
