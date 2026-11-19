// SPDX-License-Identifier: GPL-3.0-or-later
// A festett, helyzetfüggő egérkurzorok (a régi `cursor_kind`, `faction_ui.set_cursor`; assets/ui/cursors): a
// képernyő szélén görgetőnyíl, parancsmódban támadás / varázslat / mozgás / építés (érvénytelen helyen tiltójel), a
// pálya fölött a mutatott dolog szerint saját / ellenséges / semleges kijelölés, gyűjtés (nyersanyagonként), javítás.
// Csak a játék kliense (SDL); a hiányzó kép helyén a rendszer kurzora marad.
#pragma once

#include <map>
#include <string>

struct SDL_Cursor;

namespace totu::client {

class Cursors {
 public:
  ~Cursors();
  // a kurzorok betöltése (`size`: a képernyőn látszó méret képpontban, a felület-méretarány szerint)
  void load(const std::string& assets_dir, int size);
  // a kurzor kiválasztása (csak változáskor állítja); ismeretlen: a nyíl
  void set(const std::string& kind);

 private:
  struct Entry {
    SDL_Cursor* cursor = nullptr;
  };
  std::map<std::string, Entry> cursors_;
  std::string current_;
};

}  // namespace totu::client
