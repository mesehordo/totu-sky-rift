// SPDX-License-Identifier: GPL-3.0-or-later
// A gépi ellenfél API-ja (M9; GEPI_ELLENFEL.md 2. pont): az `AI` tábla egy gépi játékos homokozott Lua-állapotában.
//
// AI-FOG = 0: a gép csak a saját egységeit, épületeit és nyersanyagát, a felderített térképet és a most látott idegen
// egységeket/épületeket látja; a szabályok és az adatok (sablonok, költségek, a népek `ai` terve) nyilvánosak. A
// parancsai ugyanazok a szimulációs műveletek, mint a játékoséi, és csak a saját egységeire hatnak. Minden kiadott
// parancs a parancsnaplóba kerül (kivonat; a próbák két meccs gépi döntéseit hasonlítják össze vele).
#include <string>
#include <vector>

#include "lauxlib.h"
#include "lua.h"
#include "map/map.hpp"
#include "script/host_internal.hpp"
#include "sim/rules.hpp"

namespace totu::script::detail {

namespace {

using sim::EntityId;
using sim::Sim;

uint8_t me(lua_State* L) { return ai_state(L).owner; }

const sim::EntityDef* def_of(Sim& s, EntityId e, bool* complete = nullptr) {
  if (!s.rules()) return nullptr;
  uint16_t d = sim::kNoDef;
  if (complete) *complete = true;
  if (const auto* u = s.world().get<sim::Unit>(e)) {
    d = u->def;
  } else if (const auto* b = s.world().get<sim::Building>(e)) {
    d = b->def;
    if (complete) *complete = b->complete != 0;
  }
  return d == sim::kNoDef ? nullptr : &s.rules()->defs[d];
}

int owner_of(Sim& s, EntityId e) {
  const sim::Health* h = s.world().alive(e) ? s.world().get<sim::Health>(e) : nullptr;
  return h && h->hp > 0 ? h->owner : -1;
}

// a gép ismeri-e az entitást: a sajátja, vagy most látja
bool known(Sim& s, uint8_t p, EntityId e) {
  const int o = owner_of(s, e);
  if (o < 0) return false;
  return o == p || s.is_visible(p, e);
}

void set_bool(lua_State* L, const char* k, bool v) { lua_pushboolean(L, v); lua_setfield(L, -2, k); }
void set_int(lua_State* L, const char* k, lua_Integer v) { lua_pushinteger(L, v); lua_setfield(L, -2, k); }
void set_str(lua_State* L, const char* k, const std::string& v) { lua_pushlstring(L, v.data(), v.size()); lua_setfield(L, -2, k); }

void push_info(lua_State* L, Sim& s, uint8_t p, EntityId e) {
  const sim::Health* h = s.world().get<sim::Health>(e);
  const sim::Position* pos = s.world().get<sim::Position>(e);
  lua_createtable(L, 0, 20);
  set_int(L, "id", e);
  set_int(L, "owner", h->owner);
  if (pos) {
    push_fixed_value(L, pos->x); lua_setfield(L, -2, "x");
    push_fixed_value(L, pos->y); lua_setfield(L, -2, "y");
  }
  set_int(L, "hp", h->hp);
  set_int(L, "max", h->max);
  bool complete = true;
  const sim::EntityDef* d = def_of(s, e, &complete);
  set_bool(L, "building", s.rules() && s.world().get<sim::Building>(e) != nullptr);
  set_bool(L, "complete", complete);
  if (d) {
    set_str(L, "kind", d->id);
    set_bool(L, "worker", d->worker);
    set_bool(L, "hero", d->hero);
    set_bool(L, "attack", d->attack.has);
    set_bool(L, "ranged", d->attack.ranged());
    set_bool(L, "tower", d->building && d->attack.has);
    set_bool(L, "wall", d->shape == "wall");
    set_bool(L, "dropoff", d->dropoff != 0);
    set_int(L, "supply", d->supply);
    set_str(L, "class", d->utype);
  }
  const sim::Movement* m = s.world().get<sim::Movement>(e);
  set_bool(L, "moving", m && m->moving);
  if (h->owner == p && s.rules()) {   // a saját egység belső állapota
    const sim::Worker* wk = s.world().get<sim::Worker>(e);
    set_bool(L, "idle", wk ? wk->state == sim::Worker::Idle : !(m && m->moving));
    if (wk) {
      set_int(L, "task", wk->state);
      if ((wk->state == sim::Worker::Go || wk->state == sim::Worker::Work || wk->state == sim::Worker::Return) && wk->last_kind < 3) {
        static const char* names[] = {"wood", "gold", "meteor"};
        lua_pushstring(L, names[wk->last_kind]);
        lua_setfield(L, -2, "gathering");
      }
    }
    set_bool(L, "sheltered", s.sheltered(e));
  }
}

// a táblában (idx) felsorolt azonosítók közül a sajátok
std::vector<EntityId> own_ids(lua_State* L, Sim& s, int idx) {
  std::vector<EntityId> out;
  const uint8_t p = me(L);
  if (lua_type(L, idx) == LUA_TNUMBER) {
    const EntityId e = id_arg(L, idx);
    if (owner_of(s, e) == p) out.push_back(e);
    return out;
  }
  luaL_checktype(L, idx, LUA_TTABLE);
  const auto n = static_cast<lua_Integer>(lua_rawlen(L, idx));
  for (lua_Integer i = 1; i <= n; ++i) {
    lua_rawgeti(L, idx, i);
    const EntityId e = id_arg(L, -1);
    lua_pop(L, 1);
    if (owner_of(s, e) == p) out.push_back(e);
  }
  return out;
}

std::string ids_text(const std::vector<EntityId>& v) {
  std::string t;
  for (const EntityId e : v) t += std::to_string(e) + ",";
  return t;
}

void record(lua_State* L, const std::string& what) {
  ai_state(L).record(std::to_string(sim_of(L).tick()) + "|" + what);
}

uint16_t def_arg(lua_State* L, Sim& s, int i) {
  const char* id = luaL_checkstring(L, i);
  const uint16_t d = s.rules() ? s.rules()->def(id) : sim::kNoDef;
  if (d == sim::kNoDef) luaL_error(L, "ismeretlen sablon: %s", id);
  return d;
}

// --- lekérdezések --------------------------------------------------------------------------------------------------

int a_me(lua_State* L) { lua_pushinteger(L, me(L)); return 1; }
int a_difficulty(lua_State* L) { lua_pushstring(L, ai_state(L).difficulty.c_str()); return 1; }
int a_personality(lua_State* L) { lua_pushstring(L, ai_state(L).personality.c_str()); return 1; }
int a_tick(lua_State* L) { lua_pushinteger(L, static_cast<lua_Integer>(sim_of(L).tick())); return 1; }
int a_night(lua_State* L) { lua_pushinteger(L, sim_of(L).night_permille()); return 1; }
int a_weather(lua_State* L) { lua_pushstring(L, sim_of(L).weather_info().kind.c_str()); return 1; }

// AI.player() → a saját állapot: nyersanyag, létszám, felemelkedés, irány, szövetség, kutatások
int a_player(lua_State* L) {
  Sim& s = sim_of(L);
  const sim::PlayerState& p = s.player(me(L));
  lua_createtable(L, 0, 12);
  set_int(L, "wood", p.resource(sim::kWood));
  set_int(L, "gold", p.resource(sim::kGold));
  set_int(L, "meteor", p.resource(sim::kMeteor));
  set_int(L, "supply_used", p.supply_used);
  set_int(L, "supply_cap", p.supply_cap);
  set_bool(L, "ascended", p.ascended);
  set_str(L, "path", p.path);
  set_str(L, "ally", p.ally);
  set_str(L, "faction", p.faction);
  set_bool(L, "defeated", p.defeated);
  set_bool(L, "cheat", p.cheat_gather);
  lua_createtable(L, static_cast<int>(p.researched.size()), 0);
  lua_Integer i = 0;
  for (const auto& r : p.researched) { lua_pushstring(L, r.c_str()); lua_rawseti(L, -2, ++i); }
  lua_setfield(L, -2, "researched");
  return 1;
}

// AI.players() → a nyilvános adatok (a lobbiból): játékos, nép, játszik-e, kiesett-e – nyersanyag és sereg nélkül
int a_players(lua_State* L) {
  Sim& s = sim_of(L);
  lua_newtable(L);
  lua_Integer n = 0;
  for (int q = 1; q <= Sim::kMaxPlayers; ++q) {
    const sim::PlayerState& p = s.player(q);
    if (!p.active) continue;
    lua_createtable(L, 0, 4);
    set_int(L, "id", q);
    set_str(L, "faction", p.faction);
    set_bool(L, "defeated", p.defeated);
    set_bool(L, "me", q == me(L));
    lua_rawseti(L, -2, ++n);
  }
  return 1;
}

// AI.my() → a saját egységek és épületek adatai (azonosító szerint)
int a_my(lua_State* L) {
  Sim& s = sim_of(L);
  const uint8_t p = me(L);
  std::vector<EntityId> ids;
  s.world().pool<sim::Health>().each([&](EntityId e, const sim::Health& h) {
    if (h.hp > 0 && h.owner == p) ids.push_back(e);
  });
  lua_createtable(L, static_cast<int>(ids.size()), 0);
  lua_Integer n = 0;
  for (const EntityId e : ids) { push_info(L, s, p, e); lua_rawseti(L, -2, ++n); }
  return 1;
}

// AI.enemies() → a most látott idegen egységek és épületek (a Vadon is, owner = 0; a csapattársaké nem)
int a_enemies(lua_State* L) {
  Sim& s = sim_of(L);
  const uint8_t p = me(L);
  std::vector<EntityId> ids;
  s.world().pool<sim::Health>().each([&](EntityId e, const sim::Health& h) {
    if (h.hp > 0 && !s.allied(p, h.owner)) ids.push_back(e);
  });
  lua_newtable(L);
  lua_Integer n = 0;
  for (const EntityId e : ids) {
    if (!s.is_visible(p, e)) continue;
    push_info(L, s, p, e);
    lua_rawseti(L, -2, ++n);
  }
  return 1;
}

int a_info(lua_State* L) {
  Sim& s = sim_of(L);
  const EntityId e = id_arg(L, 1);
  if (!known(s, me(L), e)) return 0;
  push_info(L, s, me(L), e);
  return 1;
}

bool tile_flag(Sim& s, uint8_t p, int x, int y, bool explored) {
  if (!s.map_) return false;
  const int w = s.map_->width(), h = s.map_->height();
  if (x < 0 || y < 0 || x >= w || y >= h) return false;
  const auto& v = explored ? s.player(p).explored : s.player(p).visible;
  if (v.empty()) return true;   // köd nélkül
  return v[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] != 0;
}

int a_explored(lua_State* L) {
  Sim& s = sim_of(L);
  lua_pushboolean(L, tile_flag(s, me(L), static_cast<int>(luaL_checkinteger(L, 1)), static_cast<int>(luaL_checkinteger(L, 2)), true));
  return 1;
}
int a_visible(lua_State* L) {
  Sim& s = sim_of(L);
  lua_pushboolean(L, tile_flag(s, me(L), static_cast<int>(luaL_checkinteger(L, 1)), static_cast<int>(luaL_checkinteger(L, 2)), false));
  return 1;
}
int a_explored_count(lua_State* L) {
  Sim& s = sim_of(L);
  lua_Integer n = 0;
  for (const uint8_t v : s.player(me(L)).explored) n += v ? 1 : 0;
  lua_pushinteger(L, n);
  return 1;
}

// AI.map() → {w, h, starts = {{x, y}, …}} – a pálya nyilvános adatai (a kezdőhelyek, nem a foglaltságuk)
int a_map(lua_State* L) {
  Sim& s = sim_of(L);
  lua_createtable(L, 0, 3);
  if (!s.map_) return 1;
  set_int(L, "w", s.map_->width());
  set_int(L, "h", s.map_->height());
  lua_newtable(L);
  lua_Integer n = 0;
  for (const auto& [x, y] : s.map_->objects().starts) {
    lua_createtable(L, 2, 0);
    lua_pushinteger(L, x); lua_rawseti(L, -2, 1);
    lua_pushinteger(L, y); lua_rawseti(L, -2, 2);
    lua_rawseti(L, -2, ++n);
  }
  lua_setfield(L, -2, "starts");
  return 1;
}

// AI.resources("gold") → a felderített csempéken álló lelőhelyek {id, x, y, amount?}; a készlet csak ha most látszik
// N-03: a gép a lelőhelyekről csak azt tudja, amit utoljára látott – a köd mögött kimerült (eltűnt) lelőhely a listában
// marad, amíg a helyét újra nem látja. Az emlékezet a gép saját állapotában (`S._ismert`: azonosító → {k, tx, ty,
// xr, yr, a}), így a mentés és a hash része; a kimenet azonosító szerint rendezett (determinisztikus).
int a_resources(lua_State* L) {
  Sim& s = sim_of(L);
  const uint8_t p = me(L);
  const int kind = sim::resource_index(luaL_checkstring(L, 1));
  lua_rawgeti(L, LUA_REGISTRYINDEX, saved_table_ref(L));   // S
  lua_getfield(L, -1, "_ismert");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, "_ismert");
  }
  const int mem = lua_gettop(L);
  // frissítés: a felderített csempén álló lelőhelyek (a fák nélkül); a láthatóakról a pontos készlet
  std::vector<std::pair<EntityId, sim::ResourceNode>> nodes;
  s.world().pool<sim::ResourceNode>().each([&](EntityId e, const sim::ResourceNode& r) {
    if (r.size > 1) nodes.emplace_back(e, r);
  });
  for (const auto& [e, r] : nodes) {
    if (!tile_flag(s, p, r.tx, r.ty, true)) continue;
    const bool seen = tile_flag(s, p, r.tx, r.ty, false);
    lua_rawgeti(L, mem, static_cast<lua_Integer>(e));
    const bool had = lua_istable(L, -1);
    lua_pop(L, 1);
    if (seen && r.amount <= 0) {
      lua_pushnil(L);
      lua_rawseti(L, mem, static_cast<lua_Integer>(e));
      continue;
    }
    if (!seen && had) continue;   // ködben: a régi tudás marad
    const sim::Position* pos = s.world().get<sim::Position>(e);
    if (!pos) continue;
    lua_createtable(L, 0, 6);
    set_int(L, "k", r.kind);
    set_int(L, "tx", r.tx);
    set_int(L, "ty", r.ty);
    set_int(L, "xr", pos->x.raw());
    set_int(L, "yr", pos->y.raw());
    set_int(L, "a", seen ? r.amount : -1);
    lua_rawseti(L, mem, static_cast<lua_Integer>(e));
  }
  // az ismertek azonosító szerint; ami eltűnt, és a helye most látható, az elfelejtődik
  std::vector<lua_Integer> ids;
  lua_pushnil(L);
  while (lua_next(L, mem) != 0) {
    if (lua_isinteger(L, -2)) ids.push_back(lua_tointeger(L, -2));
    lua_pop(L, 1);
  }
  std::sort(ids.begin(), ids.end());
  lua_newtable(L);
  const int out = lua_gettop(L);
  lua_Integer n = 0;
  auto field = [&](const char* k) {
    lua_getfield(L, -1, k);
    const lua_Integer v = lua_tointeger(L, -1);
    lua_pop(L, 1);
    return v;
  };
  for (const lua_Integer id : ids) {
    lua_rawgeti(L, mem, id);
    const lua_Integer k = field("k"), tx = field("tx"), ty = field("ty"), xr = field("xr"), yr = field("yr"), a = field("a");
    lua_pop(L, 1);
    const auto e = static_cast<EntityId>(id);
    const bool gone = !s.world().alive(e) || !s.world().get<sim::ResourceNode>(e);
    if (gone && tile_flag(s, p, static_cast<int>(tx), static_cast<int>(ty), false)) {
      lua_pushnil(L);
      lua_rawseti(L, mem, id);
      continue;
    }
    if (k != kind) continue;
    lua_createtable(L, 0, 4);
    set_int(L, "id", id);
    push_fixed_value(L, core::Fixed::from_raw(static_cast<int32_t>(xr))); lua_setfield(L, -2, "x");
    push_fixed_value(L, core::Fixed::from_raw(static_cast<int32_t>(yr))); lua_setfield(L, -2, "y");
    if (a >= 0 && tile_flag(s, p, static_cast<int>(tx), static_cast<int>(ty), false)) set_int(L, "amount", a);
    lua_rawseti(L, out, ++n);
  }
  lua_replace(L, mem - 1);   // az eredmény a veremben a S helyére
  lua_settop(L, mem - 1);
  return 1;
}

// AI.template(id) → a sablon nyilvános adatai
int a_template(lua_State* L) {
  Sim& s = sim_of(L);
  const sim::EntityDef& d = s.rules()->defs[def_arg(L, s, 1)];
  lua_createtable(L, 0, 20);
  set_str(L, "id", d.id);
  set_str(L, "faction", d.faction);
  set_bool(L, "building", d.building);
  set_bool(L, "worker", d.worker);
  set_bool(L, "hero", d.hero);
  set_bool(L, "attack", d.attack.has);
  set_bool(L, "ranged", d.attack.ranged());
  set_bool(L, "tower", d.building && d.attack.has);
  set_bool(L, "wall", d.shape == "wall");
  set_bool(L, "dropoff", d.dropoff != 0);
  set_int(L, "dropoff_mask", d.dropoff);
  set_int(L, "fw", d.fw);
  set_int(L, "fh", d.fh);
  set_bool(L, "altar", d.altar);
  set_bool(L, "diplomacy", d.diplomacy);
  set_bool(L, "flying", d.flying);
  set_int(L, "hp", d.hp);
  set_int(L, "supply", d.supply);
  set_int(L, "supply_provides", d.supply_provides);
  set_int(L, "garrison", d.garrison);
  set_int(L, "count", d.count);
  set_str(L, "class", d.utype);
  lua_createtable(L, 0, 3);
  set_int(L, "wood", d.cost[static_cast<size_t>(sim::kWood)]);
  set_int(L, "gold", d.cost[static_cast<size_t>(sim::kGold)]);
  set_int(L, "meteor", d.cost[static_cast<size_t>(sim::kMeteor)]);
  lua_setfield(L, -2, "cost");
  lua_Integer n = 0;
  lua_newtable(L);
  for (const auto& t : d.tags) { lua_pushstring(L, t.c_str()); lua_rawseti(L, -2, ++n); }
  lua_setfield(L, -2, "tags");
  n = 0;
  lua_newtable(L);
  for (const uint16_t u : d.produces) { lua_pushstring(L, s.rules()->defs[u].id.c_str()); lua_rawseti(L, -2, ++n); }
  lua_setfield(L, -2, "produces");
  n = 0;
  lua_newtable(L);
  for (const uint16_t a : d.abilities) {   // {id, type}: a gép csak az egyszeri használatúakat (nem kapcsolókat) hívja
    if (a >= s.rules()->abilities.size()) continue;
    const sim::AbilityDef& ad = s.rules()->abilities[a];
    using T = sim::AbilityDef::Type;
    const char* type = ad.type == T::SelfBuff ? "selfbuff" : ad.type == T::Nova ? "nova" : ad.type == T::Bolt ? "bolt"
                     : ad.type == T::Summon ? "summon" : ad.type == T::Area ? "area" : "other";
    lua_createtable(L, 0, 2);
    set_str(L, "id", ad.id);
    lua_pushstring(L, type);
    lua_setfield(L, -2, "type");
    lua_rawseti(L, -2, ++n);
  }
  lua_setfield(L, -2, "abilities");
  return 1;
}

// AI.plan([nép]) → a nép `ai` terve (alapból a sajátomé; a kisnépé a szövetség után)
int a_plan(lua_State* L) {
  Sim& s = sim_of(L);
  std::string f = luaL_optstring(L, 1, "");
  if (f.empty()) f = s.player(me(L)).faction;
  lua_settop(L, 0);
  lua_pushstring(L, f.c_str());
  if (faction_ai(L) == 0) return 0;
  // a gépnek a Game.faction_ai-n felül (amelyet a régi szkriptek az S-ben tárolnak, ezért nem bővül): a szövetségi
  // sorrend és fejlődési irányonként a terv
  const sim::FactionDef* fd = s.rules()->faction(f);
  const content::Value* fa = fd && fd->raw ? fd->raw->find("Faction") : nullptr;
  const content::Value* ai = fa ? fa->find("ai") : nullptr;
  lua_newtable(L);
  lua_Integer k0 = 0;
  if (const content::Value* ap = ai ? ai->find("ally_pref") : nullptr; ap && ap->is_array()) {
    for (const auto& x : ap->items()) {
      if (x.is_string()) { lua_pushstring(L, x.text().c_str()); lua_rawseti(L, -2, ++k0); }
    }
  }
  lua_setfield(L, -2, "ally_pref");
  // fejlődési irányonként a terv (M9: az új gép a választott irány épületeit és seregét is felveszi)
  lua_newtable(L);
  if (const content::Value* ps = ai ? ai->find("paths") : nullptr; ps && ps->is_object()) {
    for (const auto& [pid, pv] : ps->members()) {
      lua_createtable(L, 0, 2);
      lua_newtable(L);
      lua_Integer k = 0;
      if (const content::Value* b = pv.find("build"); b && b->is_array()) {
        for (const auto& x : b->items()) {
          if (x.is_string()) { lua_pushstring(L, x.text().c_str()); lua_rawseti(L, -2, ++k); }
        }
      }
      lua_setfield(L, -2, "build");
      lua_newtable(L);
      k = 0;
      if (const content::Value* a = pv.find("army"); a && a->is_object()) {
        for (const auto& [uid, n] : a->members()) {
          lua_createtable(L, 2, 0);
          lua_pushstring(L, uid.c_str());
          lua_rawseti(L, -2, 1);
          lua_pushinteger(L, n.as_int().value_or(1));
          lua_rawseti(L, -2, 2);
          lua_rawseti(L, -2, ++k);
        }
      }
      lua_setfield(L, -2, "army");
      lua_setfield(L, -2, pid.c_str());
    }
  }
  lua_setfield(L, -2, "paths");
  return 1;
}

// AI.roster() → a saját nép egységei, épületei, kutatásai (a sablon `Roster`-e)
int a_roster(lua_State* L) {
  Sim& s = sim_of(L);
  const sim::FactionDef* f = s.rules() ? s.rules()->faction(s.player(me(L)).faction) : nullptr;
  const content::Value* r = f && f->raw ? f->raw->find("Roster") : nullptr;
  lua_createtable(L, 0, 3);
  for (const char* key : {"units", "buildings", "researches"}) {
    lua_newtable(L);
    lua_Integer n = 0;
    if (const content::Value* a = r ? r->find(key) : nullptr; a && a->is_array()) {
      for (const auto& x : a->items()) {
        if (x.is_string()) { lua_pushstring(L, x.text().c_str()); lua_rawseti(L, -2, ++n); }
      }
    }
    lua_setfield(L, -2, key);
  }
  return 1;
}

// --- parancsok (csak a saját egységekre) ------------------------------------------------------------------------------

int a_train(lua_State* L) {
  Sim& s = sim_of(L);
  const uint16_t d = def_arg(L, s, 1);
  std::string err;
  const bool ok = s.train(me(L), d, 0, &err);
  record(L, "train|" + s.rules()->defs[d].id + (ok ? "|1" : "|0"));
  lua_pushboolean(L, ok);
  lua_pushstring(L, err.c_str());
  return 2;
}

int a_research(lua_State* L) {
  Sim& s = sim_of(L);
  const std::string id = luaL_checkstring(L, 1);
  std::string err;
  const bool ok = s.research(me(L), id, &err);
  record(L, "research|" + id + (ok ? "|1" : "|0"));
  lua_pushboolean(L, ok);
  lua_pushstring(L, err.c_str());
  return 2;
}

// AI.build(épület, x, y): egy szabad munkás építeni a (x, y) csempe közelébe
int a_build(lua_State* L) {
  Sim& s = sim_of(L);
  const uint16_t d = def_arg(L, s, 1);
  const auto x = static_cast<int>(luaL_checkinteger(L, 2)), y = static_cast<int>(luaL_checkinteger(L, 3));
  const bool ok = s.build_near(me(L), d, x, y);
  record(L, "build|" + s.rules()->defs[d].id + "|" + std::to_string(x) + "," + std::to_string(y) + (ok ? "|1" : "|0"));
  lua_pushboolean(L, ok);
  return 1;
}

int move_like(lua_State* L, bool attack) {
  Sim& s = sim_of(L);
  const auto ids = own_ids(L, s, 1);
  const core::Fixed x = fixed_arg(L, 2, "x"), y = fixed_arg(L, 3, "y");
  lua_Integer n = 0;
  for (const EntityId e : ids) n += (attack ? s.order_attack_move(e, x, y) : s.order_move(e, x, y)) ? 1 : 0;
  record(L, std::string(attack ? "attack|" : "move|") + ids_text(ids) + "|" + std::to_string(x.raw()) + "," +
                std::to_string(y.raw()));
  lua_pushinteger(L, n);
  return 1;
}
int a_attack(lua_State* L) { return move_like(L, true); }
int a_move(lua_State* L) { return move_like(L, false); }

int a_gather(lua_State* L) {
  Sim& s = sim_of(L);
  const auto ids = own_ids(L, s, 1);
  const std::string kind = luaL_checkstring(L, 2);
  bool ok = false;
  const int k = sim::resource_index(kind);
  // N-02: csak felderített lelőhelyet választ (a munkáshoz legközelebbit; egyenlőnél a kisebb azonosítót)
  const uint8_t p = me(L);
  std::vector<std::pair<EntityId, std::pair<core::Fixed, core::Fixed>>> cand;
  if (k >= 0) {
    s.world().pool<sim::ResourceNode>().each([&](EntityId e, const sim::ResourceNode& r) {
      if (r.kind != k || r.amount <= 0 || !tile_flag(s, p, r.tx, r.ty, true)) return;
      if (const sim::Position* pos = s.world().get<sim::Position>(e)) cand.push_back({e, {pos->x, pos->y}});
    });
  }
  for (const EntityId e : ids) {
    const sim::Position* wp = s.world().get<sim::Position>(e);
    if (!wp) continue;
    EntityId best = sim::kNoEntity;
    int64_t bd = 0;
    for (const auto& [n, xy] : cand) {
      const int64_t d = core::length_sq_raw(xy.first - wp->x, xy.second - wp->y);
      if (best == sim::kNoEntity || d < bd || (d == bd && n < best)) best = n, bd = d;
    }
    if (best != sim::kNoEntity && s.interrupt(e)) ok = s.order_gather(e, best) || ok;
  }
  record(L, "gather|" + ids_text(ids) + "|" + kind);
  lua_pushboolean(L, ok);
  return 1;
}

// AI.gather_at(munkások, lelőhely): egy felderített lelőhelyre
int a_gather_at(lua_State* L) {
  Sim& s = sim_of(L);
  const auto ids = own_ids(L, s, 1);
  const EntityId node = id_arg(L, 2);
  const sim::ResourceNode* rn = s.world().alive(node) ? s.world().get<sim::ResourceNode>(node) : nullptr;
  bool ok = false;
  if (rn && tile_flag(s, me(L), rn->tx, rn->ty, true)) {
    for (const EntityId e : ids) ok = s.order_gather(e, node) || ok;
  }
  record(L, "gather_at|" + ids_text(ids) + "|" + std::to_string(node) + (ok ? "|1" : "|0"));
  lua_pushboolean(L, ok);
  return 1;
}

// AI.target(azonosítók, cél): közvetlen támadás egy látott célpontra (fókuszlövés)
int a_target(lua_State* L) {
  Sim& s = sim_of(L);
  const auto ids = own_ids(L, s, 1);
  const EntityId t = id_arg(L, 2);
  lua_Integer n = 0;
  if (known(s, me(L), t) && owner_of(s, t) != me(L)) {
    for (const EntityId e : ids) n += s.order_attack(e, t) ? 1 : 0;
  }
  record(L, "target|" + ids_text(ids) + "|" + std::to_string(t));
  lua_pushinteger(L, n);
  return 1;
}

int a_stop(lua_State* L) {
  Sim& s = sim_of(L);
  const auto ids = own_ids(L, s, 1);
  for (const EntityId e : ids) {   // N-01: mint a játékos Stop parancsa (javítás, elfoglalás, beszállás, varázslás is leáll)
    if (s.interrupt(e)) s.stop(e);
  }
  record(L, "stop|" + ids_text(ids));
  return 0;
}

int a_choose_path(lua_State* L) {
  Sim& s = sim_of(L);
  const std::string path = luaL_checkstring(L, 1);
  const bool ok = s.choose_path(me(L), path);
  record(L, "path|" + path + (ok ? "|1" : "|0"));
  lua_pushboolean(L, ok);
  return 1;
}

int a_choose_ally(lua_State* L) {
  Sim& s = sim_of(L);
  const std::string who = luaL_checkstring(L, 1);
  std::string err;
  const bool ok = s.choose_ally(me(L), who, &err);
  record(L, "ally|" + who + (ok ? "|1" : "|0"));
  lua_pushboolean(L, ok);
  lua_pushstring(L, err.c_str());
  return 2;
}

// AI.ability(azonosítók, képesség, x, y, cél?): képességhasználat a saját egységekkel
int a_ability(lua_State* L) {
  Sim& s = sim_of(L);
  const auto ids = own_ids(L, s, 1);
  const std::string ab = luaL_checkstring(L, 2);
  const core::Fixed x = lua_isnoneornil(L, 3) ? core::Fixed{} : fixed_arg(L, 3, "x");
  const core::Fixed y = lua_isnoneornil(L, 4) ? core::Fixed{} : fixed_arg(L, 4, "y");
  const EntityId t = lua_isnoneornil(L, 5) ? sim::kNoEntity : id_arg(L, 5);
  const bool ok = !ids.empty() && (t == sim::kNoEntity || known(s, me(L), t)) && s.use_ability(ids, ab, x, y, t);
  record(L, "ability|" + ids_text(ids) + "|" + ab + (ok ? "|1" : "|0"));
  lua_pushboolean(L, ok);
  return 1;
}

int a_learn(lua_State* L) {
  Sim& s = sim_of(L);
  const auto ids = own_ids(L, s, 1);
  const std::string ab = luaL_checkstring(L, 2);
  const bool ok = ids.size() == 1 && s.learn(ids[0], ab);
  record(L, "learn|" + ids_text(ids) + "|" + ab + (ok ? "|1" : "|0"));
  lua_pushboolean(L, ok);
  return 1;
}

// AI.garrison(azonosítók, épület): beszállás a saját épületbe
int a_garrison(lua_State* L) {
  Sim& s = sim_of(L);
  const auto ids = own_ids(L, s, 1);
  const EntityId b = id_arg(L, 2);
  const bool ok = owner_of(s, b) == me(L) && !ids.empty() && s.order_garrison(ids, b);
  record(L, "garrison|" + ids_text(ids) + "|" + std::to_string(b) + (ok ? "|1" : "|0"));
  lua_pushboolean(L, ok);
  return 1;
}

int a_unload(lua_State* L) {
  Sim& s = sim_of(L);
  const EntityId b = id_arg(L, 1);
  const bool ok = owner_of(s, b) == me(L) && s.unload(b);
  record(L, "unload|" + std::to_string(b) + (ok ? "|1" : "|0"));
  lua_pushboolean(L, ok);
  return 1;
}

}  // namespace

void open_ai_api(lua_State* L) {
  static const luaL_Reg funcs[] = {
      {"me", a_me}, {"difficulty", a_difficulty}, {"personality", a_personality}, {"tick", a_tick},
      {"night", a_night}, {"weather", a_weather}, {"player", a_player}, {"players", a_players}, {"my", a_my},
      {"enemies", a_enemies}, {"info", a_info}, {"explored", a_explored}, {"visible", a_visible},
      {"explored_count", a_explored_count}, {"map", a_map}, {"resources", a_resources}, {"template", a_template},
      {"plan", a_plan}, {"train", a_train}, {"research", a_research}, {"build", a_build}, {"attack", a_attack},
      {"move", a_move}, {"gather", a_gather}, {"stop", a_stop}, {"choose_path", a_choose_path},
      {"choose_ally", a_choose_ally}, {"ability", a_ability}, {"learn", a_learn}, {"garrison", a_garrison},
      {"unload", a_unload}, {"roster", a_roster}, {"gather_at", a_gather_at}, {"target", a_target},
      {nullptr, nullptr}};
  luaL_newlib(L, funcs);
  make_readonly(L);
  lua_setglobal(L, "AI");
}

}  // namespace totu::script::detail
