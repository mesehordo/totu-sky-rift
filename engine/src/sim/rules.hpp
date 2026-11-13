// SPDX-License-Identifier: GPL-3.0-or-later
// Játékszabály-adatok a szimulációnak (M6, ADR-007): a `game.json` és a sablonok (egység, épület, nép) tömör,
// fixpontos C++-alakban. A szimuláció csak ezt olvassa (a JSON-t nem); a betöltés egyszer, a meccs előtt történik.
// A számok a régi játék adatai (D37); az idők másodpercben vannak az adatban, itt tickben (30 Hz).
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "content/json.hpp"
#include "content/loader.hpp"
#include "core/fixed.hpp"

namespace totu::sim {

using core::Fixed;

constexpr int kTickHz = 30;
constexpr int kResources = 4;   // a nyersanyagok rögzített sorrendje
enum Res : uint8_t { kWood = 0, kStone = 1, kMeteor = 2, kFaith = 3 };
const char* resource_id(int r);           // "wood", "stone", "meteor", "faith"
int resource_index(const std::string& id);   // -1, ha ismeretlen
constexpr uint16_t kNoDef = 0xFFFF;

int seconds_to_ticks(Fixed seconds);       // kerekítve

struct ResourceDef {
  Fixed gather_time;      // másodperc egy teljes rakományra (az alap `carry`-hez)
  int32_t carry = 0;      // alap rakomány
  int32_t node_amount = 0;
  bool special = false;   // nem gyűjthető lelőhelyről (áhítat)
};

struct EntityDef {
  std::string id;
  bool building = false;
  std::string faction;
  std::vector<std::string> tags;
  int32_t hp = 1, armor = 0;
  int32_t sight = 0;                          // csempe
  std::array<int32_t, kResources> cost{};    // nyersanyag-költség
  int32_t supply = 0;                         // a felhasznált létszám (egység)
  int32_t time_ticks = 0;                     // termelési / építési idő
  // egység
  Fixed speed;                                // csempe / tick
  Fixed radius = Fixed::from_ratio(4, 10);
  bool flying = false;
  bool worker = false;
  uint8_t gathers = 0;                        // bitmaszk (Res); a munkás alapból mindent
  std::array<int32_t, kResources> carry{};   // 0: a nyersanyag alap `carry`-je
  // épület
  int32_t fw = 0, fh = 0;                     // lábnyom (csempe)
  int32_t supply_provides = 0;
  uint8_t dropoff = 0;                        // bitmaszk (Res)
  // előfeltételek (`Requirements.all`): kész épület, kutatás, fejlődési irány, felemelkedés
  struct Req {
    enum Kind : uint8_t { Building, Research, Path, Ascended } kind = Building;
    std::string id;
  };
  std::vector<Req> reqs;
  // termelés (M6 4. lépés)
  bool hero = false;
  std::vector<uint16_t> produces;             // épület: a képezhető egységek (`Producer.units`)
  std::vector<std::string> researches;        // épület: a kutatások (`Researcher.list`)
  bool has_tag(const std::string& t) const;
};

struct ResearchDef {
  std::string id, faction;
  std::array<int32_t, kResources> cost{};
  int32_t time_ticks = 0;
  std::vector<EntityDef::Req> reqs;
};

struct FactionDef {
  std::string id;                             // pl. `ursan` (a sablon `faction_ursan`)
  uint16_t hq = kNoDef, worker = kNoDef;
  std::vector<std::pair<uint16_t, int>> start_units;
  std::string builder;                        // MECH-BUILD: stay, stay_single, enter, consume, leave
  bool demolish = false;                      // kész épület lebontható-e (D38: Ursanok, Ooni, Wullof)
  uint8_t start_gather = kWood;
  std::array<int32_t, kResources> start_resources{};
};

struct Rules {
  std::array<ResourceDef, kResources> resources{};
  std::array<int32_t, kResources> start_resources{};
  int32_t max_supply = 200;
  std::vector<std::pair<int32_t, int32_t>> upkeep_tiers;   // (felhasznált létszám felső határa, ezrelék)
  std::vector<EntityDef> defs;                             // azonosító szerint rendezve
  std::vector<FactionDef> factions;                        // azonosító szerint rendezve
  std::vector<ResearchDef> researches;                     // azonosító szerint rendezve
  static constexpr int kQueueMax = 5;                      // a termelési sor hossza (a régi játék szerint)
  static constexpr int kMaxHeroes = 3;

  // A `game` a `game.json` tartalma; a sablonok a `set`-ből. Hiányzó kötelező adatnál nullopt (az ok az errors-ban).
  static std::optional<Rules> load(const content::Value& game, content::ContentSet& set, std::vector<std::string>& errors);
  uint16_t def(const std::string& id) const;               // kNoDef, ha nincs
  const FactionDef* faction(const std::string& id) const;
  const ResearchDef* research(const std::string& id) const;
  int research_index(const std::string& id) const;         // -1, ha nincs
  int32_t upkeep_permille(int32_t supply_used) const;      // MECH-UPKEEP
  // A félkész épület megszakításakor visszajáró költség ezreléke (a régi játékban nem volt megszakítás – ADR-007;
  // alapérték a WC3-é: 75%; `game.json` → `build_cancel_refund`).
  int32_t cancel_refund_permille = 750;
  // Kész épület lebontásakor visszajáró költség ezreléke (D38: 70%; `game.json` → `demolish_refund`); csak annál a
  // népnél, amelynek adatában `demolish: true` áll.
  int32_t demolish_refund_permille = 700;
};

}  // namespace totu::sim
