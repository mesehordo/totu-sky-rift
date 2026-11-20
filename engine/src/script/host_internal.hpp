// SPDX-License-Identifier: GPL-3.0-or-later
// A szkriptgazda belső felülete (csak a script/ fordítási egységeinek): a gépi ellenfél API-ja (ai_api.cpp) ezen át
// éri el a gazda állapotát és a közös Lua-segédeket.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "lua.h"
#include "sim/sim.hpp"

namespace totu::script::detail {

// A gépi játékos beállítása és parancsnaplója (M9). A napló kivonata nem játékállapot (nincs a hashben és a
// mentésben): a próbák ezzel hasonlítják össze két meccs gépi parancsait.
struct AiState {
  uint8_t owner = 0;            // 0: nem gépi játékos (nincs AI tábla)
  std::string difficulty, personality;
  std::string forced_path, forced_ally;   // a régi `forced_path` / `forced_ally` (üres: a gép dönt)
  uint64_t digest = 1469598103934665603ULL;
  uint64_t count = 0;
  void record(std::string_view what) {
    for (const char c : what) {
      digest ^= static_cast<uint8_t>(c);
      digest *= 1099511628211ULL;
    }
    digest ^= 0xFF;
    digest *= 1099511628211ULL;
    ++count;
  }
};

AiState& ai_state(lua_State* L);
sim::Sim& sim_of(lua_State* L);                               // hiba, ha nem eseménykezelőből hívják
core::Fixed fixed_arg(lua_State* L, int idx, const char* what);
void push_fixed_value(lua_State* L, core::Fixed f);
sim::EntityId id_arg(lua_State* L, int idx);
void make_readonly(lua_State* L);                             // a verem tetején lévő táblát csak olvashatóvá teszi
int faction_ai(lua_State* L);                                 // a Game.faction_ai megvalósítása
void open_ai_api(lua_State* L);                               // az AI tábla (ai_api.cpp)
int saved_table_ref(lua_State* L);                            // az S tábla (a szkript mentett állapota) regiszterhivatkozása

}  // namespace totu::script::detail
