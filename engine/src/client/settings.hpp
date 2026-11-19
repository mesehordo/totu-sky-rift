// SPDX-License-Identifier: GPL-3.0-or-later
// Felhasználói beállítások (M8; a régi `rts/profiles.py` DEFAULT_SETTINGS és a beállítás-menü szerint): nyelv,
// hangerő (a régi `audio.VOLUME_ROWS` kategóriái), felbontás és teljes képernyő, felület-méretarány (akadálymentesség),
// színvak-barát csapatszínek, a parancsrács billentyűkiosztása, görgetés és kijelzés. A fájl JSON, a felhasználói
// beállítás-mappában (a kliens az SDL_GetPrefPath helyét adja; platformfüggetlen). Grafikus kártya nélkül tesztelhető.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace totu::client {

// A parancsrács (4 oszlop × 3 sor) billentyűi; az alap a régi játéké: W E R T / S D F G / X C V B.
class Keymap {
 public:
  static constexpr int kSlots = 12;
  static constexpr std::string_view kDefault = "WERTSDFGXCVB";
  Keymap();

  char key(int slot) const { return keys_[static_cast<size_t>(slot)]; }
  int slot_of(char key) const;   // -1: egyik hely sem
  // A–Z, kivéve a fenntartottakat (P: szünet, mint a régi játékban); a számjegyek a csapatoké
  static bool valid_key(char key);
  static constexpr int kInvalid = -2;
  // A hely új billentyűje. Ütközésnél NEM állít át, hanem a másik helyet adja (0..11); érvénytelen billentyűnél
  // kInvalid; siker: -1 (ugyanaz a billentyű ugyanarra a helyre is siker).
  int assign(int slot, char key);
  // mint az assign, de ütközésnél a két hely billentyűt cserél
  void assign_swap(int slot, char key);
  void reset() { *this = Keymap(); }
  // azonos billentyűjű helypárok (pl. kézzel szerkesztett fájlból)
  std::vector<std::pair<int, int>> conflicts() const;
  std::string str() const { return std::string(keys_.begin(), keys_.end()); }
  // 12 betűs szövegből; érvénytelen betű vagy ütközés esetén false és változatlan
  bool parse(std::string_view s);
  bool operator==(const Keymap&) const = default;

 private:
  std::array<char, kSlots> keys_{};
};

// A hangkategóriák (a régi `audio.VOLUME_ROWS` sorrendjében).
enum class Sound : uint8_t { Master = 0, Music, Units, Combat, Buildings, Interface, Environment, Count };
constexpr size_t kSoundCount = static_cast<size_t>(Sound::Count);
std::string_view sound_key(Sound s);    // a fájlbeli kulcs ("master", "music", …)
std::string sound_label(Sound s);       // a felirat kulcsa a .po fájlokban ("ui.sound.master", …)

struct Settings {
  std::string language = "hu";      // a nyelv kódja (a `locale/<kód>.po`)
  struct Volume {
    int value = 80;   // 0..100
    bool enabled = true;
    bool operator==(const Volume&) const = default;
  };
  std::array<Volume, kSoundCount> volume = {Volume{100, true}, Volume{60, true}, Volume{}, Volume{}, Volume{}, Volume{},
                                            Volume{}};
  int width = 1280, height = 720;   // ablakos felbontás
  bool fullscreen = false;
  int ui_scale = 100;               // a felület mérete százalékban (100 … 200; nagyobb betű és gomb)
  bool colorblind = false;          // színvak-barát csapatszínek
  Keymap keys;
  int scroll_speed = 900;           // kamera-görgetés képpont/mp (a régi alap)
  bool edge_scroll = true;
  bool show_fps = true;
  int fps_limit = 60;               // 30 / 60 / 120 / 0 = nincs korlát
  std::vector<std::string> mods;    // M13: az engedélyezett modok azonosítói (a kért sorrendben; az alap nélkül)

  static const std::vector<std::pair<int, int>>& resolutions();   // a régi menü listája
  static const std::vector<int>& ui_scales();                     // 100, 125, 150, 175, 200
  bool operator==(const Settings&) const = default;
};

// Kamera-görgetés egy képkockára (AS-01): a `scroll_speed` képernyő-képpont/mp, `dt` mp; a nagyítás osztja.
// Billentyű: kx, ky (−1/0/1); a képernyő szélső 8 képpontján a kurzor is görget, ha az `edge_scroll` be van kapcsolva
// és az egér az ablakban van. Eredmény: világ-képpontban.
struct ScrollInput {
  int kx = 0, ky = 0;
  float mx = -1, my = -1;
  int w = 0, h = 0;
  bool mouse_in = false;
};
void scroll_delta(const Settings& s, const ScrollInput& in, float dt, float zoom, float* dx, float* dy);

// JSON (tömör, determinisztikus kulcssorrend)
std::string settings_to_json(const Settings& s);
// Hibatűrő: az ismeretlen kulcs kimarad, a hibás érték alapértékre áll (figyelmeztetéssel). false: nem JSON-objektum.
bool settings_from_json(std::string_view text, Settings* out, std::vector<std::string>* warnings = nullptr);
// A fájl beolvasása; ha nincs, alapértékek és true. Hibás fájlnál alapértékek, false és hibaüzenet.
bool load_settings(const std::string& path, Settings* out, std::string* error = nullptr);
// Kiírás ideiglenes fájlon át (félbeszakadt írás nem rontja el a régit)
bool save_settings(const std::string& path, const Settings& s, std::string* error = nullptr);
// a beállításfájl a felhasználói mappában (pref_dir: az SDL_GetPrefPath eredménye, perjellel a végén vagy anélkül)
std::string settings_path(const std::string& pref_dir);

// Csapatszínek (8 játékos): a megszokott (piros, kék, zöld, sárga, lila, narancs, türkiz, fehér) vagy a színvak-barát
// (Okabe–Ito-paletta: vörösvakságnál és zöldvakságnál is megkülönböztethető).
using TeamColor = std::array<uint8_t, 4>;
const std::array<TeamColor, 8>& team_colors(bool colorblind);

}  // namespace totu::client
