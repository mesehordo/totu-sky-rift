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
#include <vector>

#include "content/json.hpp"
#include "map/terrain.hpp"

namespace totu::map {

constexpr int kMaxMapSize = 512;
constexpr int kMinLevel = -1;
constexpr int kMaxLevel = 2;

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

  uint64_t hash() const;

 private:
  size_t index(int x, int y) const { return static_cast<size_t>(y) * static_cast<size_t>(w_) + static_cast<size_t>(x); }

  int w_ = 0, h_ = 0;
  std::vector<uint16_t> terrain_;
  std::vector<int8_t> level_;
  std::vector<uint8_t> cliff_;
};

}  // namespace totu::map
