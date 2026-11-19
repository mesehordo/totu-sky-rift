// SPDX-License-Identifier: GPL-3.0-or-later
// Helyi diagnosztika (docs/quality/DIAGNOSTICS_DESIGN.md, 1. sémaváltozat): eseményszámlálók engedélyezőlistával,
// képkocka- és tickidő-hisztogram, kézi export JSON-fájlba a felhasználói mappába. Semmi nem megy hálózatra, és a
// gyűjtés nem ír a szimulációba (az RNG, a parancssor és az állapothash változatlan).
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace totu::client {

// a meccs adatai az exporthoz (a hívó tölti ki; a nem ismert érték üres / −1 → `null`)
struct DiagSession {
  std::string mode;                    // skirmish | mission | chain | net
  std::string map_id;                  // a pálya azonosítója (fájlnév kiterjesztés és útvonal nélkül)
  int64_t seed = -1;
  int players = -1;
  std::vector<std::string> factions;
  std::string victory, tempo;
  int64_t duration_ticks = -1;
  std::string result;                  // won | lost | quit | "" (tart)
  int peak_units = -1;
};

class Diagnostics {
 public:
  static Diagnostics& get();   // a kliens egyetlen gyűjtője

  // az eseményszótár kódjai (az engedélyezőlista); más kód nem kerül be
  static const std::vector<std::string>& codes();
  void count(const std::string& code);
  // egyszer számol kulcsonként (pl. hiányzó kép, hiányzó fordítás); a kulcs nem kerül az exportba
  void count_once(const std::string& code, const std::string& key);
  int64_t counter(const std::string& code) const;

  void frame_ms(double ms);   // a képkocka ideje
  void tick_ms(double ms);    // a szimulációs tick ideje
  void note_units(int n);     // a csúcs-egységszámhoz

  // a hisztogramból: az adott hányad (0..1) felső becslése, vagy −1, ha nincs mérés
  double frame_percentile(double q) const;
  double tick_percentile(double q) const;

  // az export szövege (1. séma); `session_id` munkamenetenként véletlen
  std::string to_json(const DiagSession& s, const std::string& renderer) const;
  void reset();   // próbákhoz

  static constexpr int kBins = 400;            // 0,25 ms-os rekeszek 0–100 ms-ig, a végén a túlcsorduló
  static constexpr double kBinMs = 0.25;
  static constexpr double kTickBudgetMs = 1000.0 / 30.0;

 private:
  Diagnostics();
  mutable std::mutex mu_;
  std::map<std::string, int64_t> counts_;
  std::set<std::string> seen_keys_;            // a `count_once` kulcsai (csak a memóriában)
  std::array<uint32_t, kBins + 1> frame_hist_{}, tick_hist_{};
  uint64_t frames_ = 0, ticks_ = 0, ticks_over_ = 0;
  int peak_units_ = -1;
  std::string session_id_;
  static double percentile(const std::array<uint32_t, kBins + 1>& h, uint64_t n, double q);
};

// atomikus írás (ideiglenes fájl + átnevezés) a `dir` mappába `diagnosztika_<ÉÉÉÉHHNN_ÓÓPPMM>.json` néven; a fájl
// neve (útvonal nélkül) a `file_name`-be; hibánál false és rövid ok
bool save_diagnostics(const std::string& dir, const std::string& json, std::string* file_name, std::string* error);

}  // namespace totu::client
