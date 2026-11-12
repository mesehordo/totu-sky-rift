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
  std::vector<Checkpoint> checkpoints;
};

// Felvétel: a megadott parancsokkal végigfuttatja a szimulációt, és rögzíti az ellenőrzőpontokat.
Replay record(uint64_t seed, int64_t ticks, std::vector<Command> commands, int64_t checkpoint_every = 30,
              std::vector<ScriptFile> scripts = {});

struct VerifyResult {
  bool ok = false;
  size_t checked = 0;
  int64_t first_bad_tick = -1;
  std::vector<std::string> differing_components;   // az első eltérő ellenőrzőpontnál
  std::string error;
  std::vector<std::string> script_messages;   // a szkriptek naplója az ellenőrző futásból
};
VerifyResult verify(const Replay& r);

// Szkript nélkül az 1-es, szkripttel a 2-es változat (a szkriptek forrásával).
std::vector<uint8_t> encode(const Replay& r);
std::optional<Replay> decode(const std::vector<uint8_t>& data);

}  // namespace totu::sim
