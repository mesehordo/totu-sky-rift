// SPDX-License-Identifier: GPL-3.0-or-later
// A menük rajzolása és a menü-ciklus (M8): a `Menu` (GL nélküli állapotgép és elrendezés) elemeit a motor saját
// rajzolójával festi meg, a régi `rts/menu.py` kinézete szerint – festett, lassan úszó háttér, oldalsáv logóval és
// díszvonalakkal, áttetsző panelek kettős kerettel, lebegő-kiemelős gombok, lenyílók, csúszkák; a betű a játék saját
// betűje (Rováskönyv Text, a címekhez Rováskönyv).
#pragma once

#include <functional>
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
  bool single = false;         // csak a kezdőképernyő (a szünetmenüből): a főmenübe lépve visszatér (false, kilépés nélkül)
  std::vector<content::ModInfo> mods;   // M13: a betöltött modok (az alappal)
  std::vector<content::ModInfo> available_mods;   // a modkezelő listája (az alap nélkül)
  uint64_t content_hash = 0;                      // a modlock (a LAN-keresés a más tartalmú játékot jelöli)
  std::function<std::string()> status;   // az állapotsor szövege képkockánként (üres: nincs)
  std::function<void()> tick;            // képkockánként (a zene folytatása)
  // a régi `MusicMenu`: a számok és a szóló; a választott szám lejátszása
  std::vector<std::pair<std::string, std::string>> tracks;
  std::string current_track;
  std::function<void(const std::string&)> play_track;
};

constexpr int kMenuRestart = 42;   // a `run_menu` visszatérési kódja: a játék induljon újra (a modkészlet változott)

// A felbontás és a teljes képernyő alkalmazása az ablakra.
void apply_display(SDL_Window* win, const Settings& st);

// A menü futtatása. true: meccs indul (`out` kitöltve); false: kilépés (a `rc` hibánál 1).
bool run_menu(SDL_Window* win, render::Device& dev, render::SpriteBatch& batch, Settings& st, Locale& loc,
              const MenuRunOptions& o, MatchConfig* out, int* rc);

// a lobbi egy képkockája (a régi `SetupScreen` lobbija): a betöltőkép fölött a helyek (név, nép, kész), a csevegés
// utolsó sorai és az állapot; a hívó cserél képkockát. Az első hívás tölti be a betűket.
struct LobbyLine {
  std::string name, faction;
  bool taken = false, ready = false, you = false;
};
void lobby_frame(SDL_Window* win, render::Device& dev, render::SpriteBatch& batch, const Locale& loc, const std::string& assets,
                 float ui_scale, const std::vector<LobbyLine>& seats, const std::vector<std::string>& chat, const std::string& status,
                 double t, const std::string* typing = nullptr);   // typing: a most írt üzenet (nullptr: nem ír)

}  // namespace totu::client
