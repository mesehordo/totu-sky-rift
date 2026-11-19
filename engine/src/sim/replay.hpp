// SPDX-License-Identifier: GPL-3.0-or-later
// Visszajátszás (M2): kezdőmag + az összes parancs + rendszeres ellenőrzőpontok (teljes hash és komponensenkénti
// hash). Bináris, little-endian (core/bytes), így minden rendszeren ugyanaz a fájl. Az ellenőrzés újrajátssza a
// parancsokat, és eltéréskor megmondja az első eltérő ellenőrzőpontot és a komponenseket, amelyek eltértek.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "sim/sim.hpp"

namespace totu::map {
class TerrainTable;
class Map;
class NavGrid;
class Pathfinder;
}
namespace totu::script {
class ScriptHost;
}
namespace totu::sim {
class MissionHost;
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
  // M9 (5-ös változat): a gépi játékosok beállítása és az AI-csomagok fájljai (mod = a csomag azonosítója, name = a
  // csomagon belüli útvonal), így a visszajátszás a gép kódjával együtt önálló
  std::vector<AiSlot> ai;
  std::vector<ScriptFile> ai_files;
  // H-15 (6-os változat, ha bármelyik eltér az alapértéktől): a meccsbeállítás többi része
  bool upkeep = true, camps = false;
  uint8_t fog = 0, daytime = 2;   // MatchSetup::Fog, MatchSetup::Daytime
  // M11 (7-es változat): a küldetés leírása (mission.json szövege); vele a meccs kezdőbázis nélkül indul, a népek a
  // küldetés játékosaiból
  std::string mission;
  // M12 (8-as változat, ha nem az alapérték): győzelmi feltétel, tempóprofil, perc
  uint8_t victory = 0, tempo = 0;
  int32_t minutes = 0;
  // 9-es változat (ha van csapat): játékosonként a csapat (0: mindenki ellen)
  std::vector<uint8_t> teams;
  std::vector<Checkpoint> checkpoints;
};

// Egy visszajátszás (vagy hálózati meccs, M10) leírásából felépített, futtatható meccs: a pálya, a navigáció, a
// szkriptek és a gépek a szimulációval együtt élnek. A parancsok nélkül épül (a hívó adja őket a `sim`-nek).
struct MatchInstance {
  MatchInstance();
  ~MatchInstance();
  MatchInstance(const MatchInstance&) = delete;
  MatchInstance& operator=(const MatchInstance&) = delete;
  std::unique_ptr<Sim> sim;
  std::unique_ptr<map::Map> map;
  std::unique_ptr<map::NavGrid> nav;
  std::unique_ptr<map::Pathfinder> pf;
  std::unique_ptr<MissionHost> mission;   // M11: a küldetés (ha van) – a szkriptek előtt csatolva
  std::unique_ptr<Rules> rules;           // M11: a küldetés saját szabályai (objektumszerkesztő), ha vannak
  std::vector<std::unique_ptr<script::ScriptHost>> hosts, ais;
  std::vector<std::string> messages() const;   // a szkriptek és a gépek naplója
};
// `load_state`: mentésből folytatott meccs (újracsatlakozás) – a meccsindítás és az on_start nélkül
std::unique_ptr<MatchInstance> start_match(const Replay& r, const map::TerrainTable* terrain, const Rules* rules,
                                           std::string& error, bool load_state = false);

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

// Szkript nélkül az 1-es, szkripttel a 2-es, pályával vagy M4-es parancsokkal a 3-as, meccsel a 4-es, gépi
// játékosokkal az 5-ös változat.
std::vector<uint8_t> encode(const Replay& r);
std::optional<Replay> decode(const std::vector<uint8_t>& data);

}  // namespace totu::sim
