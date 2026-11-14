// SPDX-License-Identifier: GPL-3.0-or-later
// Pálya (ADR-006, M4): csempénként terepfajta és szint (−1 mélység … 2 nagyon magas, D33), a szinthatáron
// automatikus szirtekkel. Formátum (`map.json`):
//   { "width": 96, "height": 96,
//     "terrain": { "legend": { ".": "grass" }, "rows": [ "....", … ] }   – vagy –   { "rle": "grass*90 deep_water*6 …" },
//     "levels":  { "rows": [ "0011", … ] }  (jelek: - 0 1 2)            – vagy –   { "rle": "0*500 1*20 …" },
//     "auto_cliffs": true }
// A jelmagyarázat elhagyható: ekkor a terepfajták saját `char` jele érvényes. A szint elhagyható (mind 0).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "content/json.hpp"
#include "map/terrain.hpp"

namespace totu::map {

constexpr int kMaxMapSize = 512;
constexpr int kMinLevel = -1;
constexpr int kMaxLevel = 2;

// Pályaobjektumok (M6, ADR-007 3. pont): kezdőhelyek, fák, nyersanyag-lelőhelyek. A pálya csak leírja őket; a
// szimuláció a meccs indításakor hozza létre belőlük az entitásokat (a fák és a lelőhelyek akadályok a navrácsban).
struct MapResource {
  std::string kind;           // a nyersanyag azonosítója (`game.json` → `resources`), pl. `stone`, `meteor`
  int x = 0, y = 0;           // a bal felső csempe
  int size = 2;               // négyzetes, csempében
  int amount = -1;            // -1: a nyersanyag `node_amount` értéke
};
struct MapObjects {
  std::vector<std::pair<int, int>> starts;   // játékosonként a főépület közepe (csempe)
  std::vector<std::pair<int, int>> trees;    // fák csempénként (a sorrend a pálya sorrendje: fentről le, balról jobbra)
  std::vector<MapResource> resources;
  std::vector<std::pair<int, std::string>> camp_peoples;   // szint → a kisnép, akinek a tábora ott áll (TABOR-1)
};

class Map {
 public:
  Map() = default;
  Map(int width, int height, uint16_t fill_terrain);

  // A hibák az `errors` végére kerülnek; hibás pályánál nullopt.
  static std::optional<Map> from_json(const content::Value& doc, const TerrainTable& table,
                                      std::vector<std::string>& errors);

  int width() const { return w_; }
  int height() const { return h_; }
  bool in_bounds(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }

  uint16_t terrain(int x, int y) const { return terrain_[index(x, y)]; }
  int level(int x, int y) const { return level_[index(x, y)]; }
  bool cliff(int x, int y) const { return cliff_[index(x, y)] != 0; }   // automatikus szirtperem

  void set_terrain(int x, int y, uint16_t t) { terrain_[index(x, y)] = t; }
  void set_level(int x, int y, int lv) { level_[index(x, y)] = static_cast<int8_t>(lv); }
  // A szirtperem újraszámolása a szintekből: szirt az a nem rámpa csempe, amelynek valamelyik (8) szomszédja
  // alacsonyabb szinten van.
  void compute_cliffs(const TerrainTable& table);

  // Járható-e a csempe a mozgásosztálynak (terep + szirt; az épületek és erőforrások a navrácsban).
  bool passable(const TerrainTable& table, uint8_t movement_class, int x, int y) const;

  uint64_t hash() const;   // a terep és a szintek (az objektumok nélkül)

  const MapObjects& objects() const { return objects_; }
  MapObjects& objects() { return objects_; }
  // éghajlat (D35: az időjárás ezen a pályán ebből válogat); üres = alapértelmezett ("temperate")
  const std::string& climate() const { return climate_; }

 private:
  size_t index(int x, int y) const { return static_cast<size_t>(y) * static_cast<size_t>(w_) + static_cast<size_t>(x); }

  int w_ = 0, h_ = 0;
  std::vector<uint16_t> terrain_;
  std::vector<int8_t> level_;
  std::vector<uint8_t> cliff_;
  MapObjects objects_;
  std::string climate_;
};

}  // namespace totu::map
