// SPDX-License-Identifier: GPL-3.0-or-later
// Terepfajták és mozgásosztályok (ADR-006, M4): a tartalom `terrain` és `movement` névteréből.
// A járhatósági osztályok (land, shallow, deep, cliff, ramp, bridge, blocked …) a terepadatból jönnek, nevük szerint
// kapnak sorszámot (ábécérendben, így minden gépen ugyanazt). A mozgásosztály osztályonként adja meg, hol mehet az
// egység és milyen szorzóval; a hiányzó osztály járhatatlan.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "content/loader.hpp"
#include "core/fixed.hpp"

namespace totu::map {

using core::Fixed;

constexpr int kMaxPassClasses = 32;

// A megjelenítő adatai (a szimuláció nem használja): táj, csempesor, rang (a magasabb rangú szomszéd ráfolyik az
// alacsonyabbra), árnyalat, part (a víz előtti sáv csempesora; üres = nincs).
struct TerrainArt {
  std::string set = "tileset", row = "grass", shore;
  int prio = 0;
  uint8_t tint[3] = {255, 255, 255};
};

struct TerrainType {
  std::string id;
  std::string name;
  char ch = '?';               // alapértelmezett jel a pálya szöveges soraiban
  uint8_t pass = 0;            // járhatósági osztály sorszáma
  Fixed speed = Fixed::from_int(1);
  bool buildable = true;
  bool hides = false;
  TerrainArt art;
};

struct MovementClass {
  std::string id;
  std::string name;
  bool flying = false;
  uint32_t pass_mask = 0;                  // bitenként: mely járhatósági osztályokon mehet
  std::vector<Fixed> pass_speed;           // osztályonkénti szorzó (kMaxPassClasses elem)
};

class TerrainTable {
 public:
  // A tartalomból tölt; a hibák az `errors` végére kerülnek. A sorrend azonosító szerinti (determinisztikus).
  bool load(content::ContentSet& content, std::vector<std::string>& errors);

  const std::vector<TerrainType>& terrains() const { return terrains_; }
  const std::vector<MovementClass>& movement_classes() const { return classes_; }
  const std::vector<std::string>& pass_classes() const { return pass_names_; }

  std::optional<uint16_t> terrain(const std::string& id) const;
  std::optional<uint16_t> terrain_by_char(char c) const;
  std::optional<uint8_t> movement_class(const std::string& id) const;
  std::optional<uint8_t> pass_class(const std::string& name) const;

  bool passable(uint8_t movement_class, uint16_t terrain) const;
  Fixed speed(uint8_t movement_class, uint16_t terrain) const;   // terep × osztály; 0, ha járhatatlan

 private:
  std::vector<TerrainType> terrains_;
  std::vector<MovementClass> classes_;
  std::vector<std::string> pass_names_;
};

}  // namespace totu::map
