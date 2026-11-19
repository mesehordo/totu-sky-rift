// SPDX-License-Identifier: GPL-3.0-or-later
// Győzelmi feltétel modból (M13): a `victory/<azonosító>/` mappa (a VFS rétegein át: az alap és a modok) egy
// `victory.json` leírással és egy `victory.lua` szkripttel. A szkript saját, homokozott ScriptHost-ban fut ("victory"
// nevű mod-állapot): látja a `Game` táblát, és a `Victory` táblán át dönt (win, defeat, conquest, status). A
// szimuláció része (hash, mentés), és a visszajátszásba a forrásával együtt kerül (Replay::scripts, mod = "victory").
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "content/vfs.hpp"
#include "script/host.hpp"
#include "sim/replay.hpp"
#include "sim/sim.hpp"

namespace totu::script {

struct VictoryInfo {
  std::string id;
  std::map<std::string, std::string> name, description;   // nyelvenként ("hu", "en"); a szöveges alak mindkettő
  std::string label(const std::string& lang) const;          // a név a nyelven (különben angol, magyar, azonosító)
};

struct VictoryPackage {
  VictoryInfo info;
  std::string source;   // victory.lua
};

// a VFS rétegeinek győzelmi szkriptjei (`victory/*/victory.json`), ábécérendben; a hibás leírás kimarad
std::vector<VictoryInfo> list_victories(const content::Vfs& vfs);
std::optional<VictoryPackage> load_victory(const content::Vfs& vfs, const std::string& id, std::string* error = nullptr);

// A győzelmi szkript Lua-állapota, a szimulációhoz csatolva (`start`: új meccs – az on_start hívása). A szkript
// fordítási hibájánál nullptr (nincs csatolva); az on_start hibájánál csatolva marad, és `errors() > 0`.
std::unique_ptr<ScriptHost> attach_victory(sim::Sim& s, const VictoryPackage& pkg, uint64_t seed, bool start,
                                           std::string* error = nullptr);
// a szkript a visszajátszásba (Replay::scripts; a győzelmi mód is)
void add_to_replay(sim::Replay& r, const VictoryPackage& pkg);

}  // namespace totu::script
