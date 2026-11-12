// SPDX-License-Identifier: GPL-3.0-or-later
// Navigációs rács (ADR-006, M4): fél csempés navcellák mozgásosztályonként. Minden navcellához tartozik a szabad tér:
// a Csebisev-távolság (navcellában) a legközelebbi akadályig, a pálya széle is akadály; 0 = járhatatlan. Egy egység
// méretosztálya a sugarából jön, és akkor fér el egy navcellában, ha ott a szabad tér legalább a méretosztály igénye
// (kicsi 1, közepes 2, nagy 3 – adatban). Így a nagy egységnek csak a kellően széles átjáró járható (NAV-MAMMOTH).
// Statikus akadály (épület alapterülete, erőforrás, fa) csempés téglalapként kerül be; a változás után csak a
// környék számolódik újra (a teljes újraszámolással bitre azonos eredménnyel – teszt).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "map/map.hpp"
#include "map/terrain.hpp"

namespace totu::map {

struct SizeClass {
  std::string id;
  Fixed max_radius;      // ennél nem nagyobb sugarú egységek tartoznak ide
  uint8_t clearance = 1; // a szükséges szabad tér navcellában
};

struct NavConfig {
  int clearance_cap = 8;   // a szabad tér felső határa (navcella)
  std::vector<SizeClass> sizes{{"small", Fixed::from_ratio(1, 2), 1},
                               {"medium", Fixed::from_ratio(3, 4), 2},
                               {"large", Fixed::from_int(2), 3}};
  // a `game.json` `navigation` részéből (a hiányzó mezők alapértéke marad)
  static NavConfig from_json(const content::Value* nav, std::vector<std::string>& errors);
};

constexpr int kNavPerTile = 2;   // navcella csempénként (egy irányban)

struct TileRect {
  int x = 0, y = 0, w = 0, h = 0;
};

class NavGrid {
 public:
  NavGrid(const Map& map, const TerrainTable& table, NavConfig config = {});

  int width() const { return nw_; }     // navcellában
  int height() const { return nh_; }
  const NavConfig& config() const { return config_; }
  int classes() const { return static_cast<int>(class_ids_.size()); }   // nem repülő mozgásosztályok
  bool in_bounds(int nx, int ny) const { return nx >= 0 && ny >= 0 && nx < nw_ && ny < nh_; }

  // A mozgásosztály (TerrainTable sorszám) navrács-indexe; a repülőnek nincs (-1).
  int grid_of(uint8_t movement_class) const;
  // Méretosztály a sugárból (az első, amelynek max_radius ≥ sugár; különben a legnagyobb).
  int size_class(Fixed radius) const;

  uint8_t clearance(int grid, int nx, int ny) const { return clear_[static_cast<size_t>(grid)][index(nx, ny)]; }
  bool passable(int grid, int nx, int ny) const { return in_bounds(nx, ny) && clearance(grid, nx, ny) > 0; }
  bool fits(int grid, int size, int nx, int ny) const {
    return in_bounds(nx, ny) && clearance(grid, nx, ny) >= config_.sizes[static_cast<size_t>(size)].clearance;
  }
  Fixed speed(int grid, int nx, int ny) const;   // a terep és az osztály szorzója (0, ha járhatatlan)

  // Statikus akadály csempés téglalapban; egymásra rakható (számlálós), az eltávolítás visszaállítja.
  void add_obstacle(TileRect r);
  void remove_obstacle(TileRect r);

  // A terep vagy a szint (pl. szerkesztőben) megváltozott: előbb a Map::compute_cliffs, utána ez; a csempék (és a
  // szirtek miatt egy csempényi környékük) navcelláinak járhatósága újraszámolódik.
  void terrain_changed(TileRect r);

  void rebuild_all();                // teljes újraszámolás (teszthez és betöltéshez)
  uint64_t hash() const;

 private:
  size_t index(int nx, int ny) const { return static_cast<size_t>(ny) * static_cast<size_t>(nw_) + static_cast<size_t>(nx); }
  void update_base(int grid, TileRect r);                    // járhatóság a terepből és az akadályokból
  void recompute(int grid, int x0, int y0, int x1, int y1);  // szabad tér a [x0,x1)×[y0,y1) navcellákban
  void after_change(TileRect r);

  const Map& map_;
  const TerrainTable& table_;
  NavConfig config_;
  int nw_ = 0, nh_ = 0;
  std::vector<uint8_t> class_ids_;                // grid → mozgásosztály
  std::vector<std::vector<uint8_t>> base_;        // grid → 1, ha a terep szerint járható
  std::vector<std::vector<uint8_t>> clear_;       // grid → szabad tér
  std::vector<uint8_t> blocked_;                  // statikus akadályok száma navcellánként
};

}  // namespace totu::map
