// SPDX-License-Identifier: GPL-3.0-or-later
// Gépi ellenfél (M9; GEPI_ELLENFEL.md): az AI-csomagok betöltése és a gépi játékosok Lua-állapota.
//
// Egy AI-csomag az `ai/<azonosító>/` mappa (a VFS rétegein át: `base/ai/…`, később `mods/<név>/ai/…`): `ai.json`
// (név, leírás, népek, belépési pont, alapértelmezések), `personalities.json`, `difficulties.json` és Lua-modulok; a
// `require("a.b")` a csomagon belüli `a/b.lua`-t tölti. Gépi játékosonként saját, homokozott ScriptHost fut (`AI`
// tábla, `Game` nélkül), a meccs `sim::AiSlot` beállítása szerint; a szimuláció része (hash, mentés, visszajátszás).
#pragma once

#include <cstdint>
#include <filesystem>
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

struct AiPackage {
  std::string id;
  std::map<std::string, std::string> files;   // a csomagon belüli útvonal (pl. "main.lua") → szöveg
  const std::string* file(const std::string& path) const;
};

// a hat nehézségi szint (GEPI_ELLENFEL.md 1. és 6. pont), sorrendben
const std::vector<std::string>& ai_difficulties();

std::optional<AiPackage> load_ai_package(const content::Vfs& vfs, const std::string& id, std::string* error = nullptr);
// a VFS rétegeinek telepített csomagjai (`ai/*/ai.json`), ábécérendben
std::vector<std::string> list_ai_packages(const content::Vfs& vfs);
// a csomag személyiségei (a `personalities.json` sorrendjében)
std::vector<std::string> ai_personalities(const AiPackage& pkg);

// Egy gépi játékos Lua-állapota (a `slot` szerint; hibánál nullptr és az ok). Csatolás: `Sim::add_hooks`, majd új
// meccsnél `start`.
std::unique_ptr<ScriptHost> make_ai(const AiPackage& pkg, const sim::AiSlot& slot, uint64_t seed, std::string* error = nullptr);
// A meccs összes gépi játékosa (a `slots` sorrendjében csatolva; `start`: új meccs – az on_start hívása; betöltött
// mentésnél false). Hibánál üres lista és az ok.
std::vector<std::unique_ptr<ScriptHost>> attach_ais(sim::Sim& s, const std::vector<sim::AiSlot>& slots,
                                                    const std::vector<AiPackage>& packages, uint64_t seed, bool start,
                                                    std::string* error = nullptr);

// a csomag fájljai a visszajátszásba (Replay::ai_files)
void add_to_replay(sim::Replay& r, const AiPackage& pkg);

}  // namespace totu::script
