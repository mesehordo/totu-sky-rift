// SPDX-License-Identifier: GPL-3.0-or-later
// Visszajátszás (M2): kezdőmag + az összes parancs + rendszeres ellenőrzőpontok (teljes hash és komponensenkénti
// hash). Bináris, little-endian (core/bytes), így minden rendszeren ugyanaz a fájl. Az ellenőrzés újrajátssza a
// parancsokat, és eltéréskor megmondja az első eltérő ellenőrzőpontot és a komponenseket, amelyek eltértek.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "sim/sim.hpp"

namespace totu::map {
class TerrainTable;
}

namespace totu::sim {

struct Checkpoint {
  int64_t tick = 0;
  uint64_t hash = 0;
  std::vector<std::pair<std::string, uint64_t>> components;
};

// Szkriptfájl a visszajátszásban (M3): a forrás magában a fájlban van, így az ellenőrzés önálló. A mod-ok a
// felsorolás sorrendjében kapnak saját Lua-állapotot; egy modon belül a fájlok sorrendben futnak.
struct ScriptFile {
  std::string mod;
  std::string name;
  std::string source;
  friend bool operator==(const ScriptFile&, const ScriptFile&) = default;
};

struct Replay {
  uint64_t seed = 0;
  int64_t ticks = 0;
  int64_t checkpoint_every = 30;
  std::vector<Command> commands;
  std::vector<ScriptFile> scripts;
  std::string map;                 // M4: a pálya (map.json szövege); üres = pálya és navigáció nélkül
  std::vector<std::string> factions;   // M6 (4-es változat): meccs a pálya kezdőhelyein, játékosonként a nép
  uint8_t weather = 1;                 // M6: MatchSetup::WeatherMode (0 ki, 1 hangulat, 2 hatással, 3 + pálya)
  std::vector<Checkpoint> checkpoints;
};

// Felvétel: a megadott parancsokkal végigfuttatja a szimulációt, és rögzíti az ellenőrzőpontokat.
// Pályás visszajátszáshoz (map nem üres) a terepfajták és mozgásosztályok táblája kell (a tartalomból).
Replay record(uint64_t seed, int64_t ticks, std::vector<Command> commands, int64_t checkpoint_every = 30,
              std::vector<ScriptFile> scripts = {}, std::string map = {}, const map::TerrainTable* terrain = nullptr);
// Meccs-visszajátszás (M6): a `base` a mag, a hossz, a parancsok, a szkriptek, a pálya, a népek és az időjárás-mód;
// a szabályadatok (a tartalomból) a futáshoz kellenek, a fájlba nem kerülnek.
Replay record_match(Replay base, const map::TerrainTable& terrain, const Rules& rules);

struct VerifyResult {
  bool ok = false;
  size_t checked = 0;
  int64_t first_bad_tick = -1;
  std::vector<std::string> differing_components;   // az első eltérő ellenőrzőpontnál
  std::string error;
  std::vector<std::string> script_messages;   // a szkriptek naplója az ellenőrző futásból
};
VerifyResult verify(const Replay& r, const map::TerrainTable* terrain = nullptr, const Rules* rules = nullptr);

// Szkript nélkül az 1-es, szkripttel a 2-es, pályával vagy M4-es parancsokkal a 3-as változat.
std::vector<uint8_t> encode(const Replay& r);
std::optional<Replay> decode(const std::vector<uint8_t>& data);

}  // namespace totu::sim
