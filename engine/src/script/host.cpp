// SPDX-License-Identifier: GPL-3.0-or-later
// A szkriptréteg megvalósítása (M3). Lásd host.hpp. A Lua C++-ként fordul (lua_build.cpp), ezért a lua.h-t
// közvetlenül, extern "C" nélkül használjuk, és a Lua-hibák C++-kivételként futnak át ezen a kódon.
#include "script/host.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

namespace totu::script {

namespace {

constexpr int kHookStep = 1000;            // a számlálóhorog ennyi utasításonként fut
constexpr size_t kMaxMessages = 10000;
constexpr int kMaxDepth = 32;              // mentett táblák legnagyobb mélysége
constexpr const char* kLoadedKey = "totu.loaded";
constexpr const char* kReadOnlyMeta = "totu.readonly";

}  // namespace

struct ScriptHost::Impl {
  std::string name;
  Limits limits;
  lua_State* L = nullptr;
  size_t mem = 0;
  int64_t used = 0;          // a tickben eddig elhasznált utasítás
  bool exhausted = false;    // a tick utasításkerete elfogyott
  core::Pcg32 rng{0, 0};
  sim::Sim* sim = nullptr;   // csak eseménykezelés közben nem üres
  ModuleLoader loader;
  std::vector<std::string> messages;
  size_t errors = 0;
  int s_ref = LUA_NOREF;     // az S tábla a registryben

  struct Timer {
    int64_t tick = 0;
    uint64_t id = 0;
    std::string handler;
    std::string payload;     // sorosított érték
  };
  std::vector<Timer> timers;   // (tick, id) szerint növekvő
  uint64_t next_timer = 1;

  void message(std::string text) {
    if (messages.size() < kMaxMessages) {
      messages.push_back("[" + name + "] " + std::move(text));
    } else if (messages.size() == kMaxMessages) {
      messages.push_back("[" + name + "] (további üzenetek elhagyva)");
    }
  }
  void error_message(const std::string& what, lua_State* st) {
    ++errors;
    const char* m = lua_tostring(st, -1);
    message("hiba (" + what + "): " + (m ? m : "ismeretlen hiba"));
  }

  // Védett futtatás a motor saját Lua-műveleteihez (kívülről nem futhat Lua-hiba).
  template <class F>
  bool protect(const char* what, F&& f);

  // A felhasználói kód hívása: a verem tetején a függvény és nargs argumentum. Hibát a naplóba ír.
  bool call_user(int nargs, const std::string& what);

  void push_global(const char* key);
};

namespace {

using Impl = ScriptHost::Impl;

Impl& impl(lua_State* L) { return **static_cast<Impl**>(lua_getextraspace(L)); }

// --- memória és utasításkeret --------------------------------------------------------------------------------------

void* limited_alloc(void* ud, void* ptr, size_t osize, size_t nsize) {
  Impl& I = *static_cast<Impl*>(ud);
  const size_t old = ptr ? osize : 0;   // ptr == NULL esetén az osize az objektum típusa, nem méret
  if (nsize == 0) {
    std::free(ptr);
    I.mem -= old;
    return nullptr;
  }
  if (nsize > old && I.mem - old + nsize > I.limits.memory_bytes) return nullptr;   // a Lua ekkor memóriahibát ad
  void* p = std::realloc(ptr, nsize);
  if (!p) return nullptr;
  I.mem = I.mem - old + nsize;
  return p;
}

void count_hook(lua_State* L, lua_Debug*) {
  Impl& I = impl(L);
  I.used += kHookStep;
  if (I.used > I.limits.instructions_per_tick) {
    I.exhausted = true;
    luaL_error(L, "a tick utasításkerete kimerült");
  }
}

int traceback(lua_State* L) {
  const char* msg = lua_tostring(L, 1);
  if (!msg) msg = "(nem szöveges hibaobjektum)";
  luaL_traceback(L, L, msg, 1);
  return 1;
}

// --- determinisztikus kulcssorrend ---------------------------------------------------------------------------------

struct Key {
  int kind = 0;            // 0 szám, 1 szöveg, 2 logikai
  bool is_int = false;
  lua_Integer i = 0;
  lua_Number f = 0;
  std::string_view s;
  bool b = false;
  int slot = 0;            // helye az ideiglenes kulcstömbben
};

bool int_less_float(lua_Integer i, lua_Number f) {   // i < f; a lebegőpontos kulcs sosem egész értékű
  if (f >= 9223372036854775808.0) return true;
  if (f < -9223372036854775808.0) return false;
  return i <= static_cast<lua_Integer>(std::floor(f));
}

bool key_less(const Key& a, const Key& b) {
  if (a.kind != b.kind) return a.kind < b.kind;
  switch (a.kind) {
    case 0:
      if (a.is_int && b.is_int) return a.i < b.i;
      if (!a.is_int && !b.is_int) return a.f < b.f;
      if (a.is_int) return int_less_float(a.i, b.f);
      return !int_less_float(b.i, a.f);
    case 1: {
      const size_t n = std::min(a.s.size(), b.s.size());
      const int c = n ? std::memcmp(a.s.data(), b.s.data(), n) : 0;
      return c != 0 ? c < 0 : a.s.size() < b.s.size();
    }
    default:
      return !a.b && b.b;
  }
}

// A t (abszolút index) kulcsait rendezve egy új tömbbe teszi a verem tetejére. skip_bad: a nem rendezhető kulcsokat
// (tábla, függvény …) kihagyja; különben hibát ad.
void push_sorted_keys(lua_State* L, int t, bool skip_bad) {
  lua_newtable(L);
  const int tmp = lua_gettop(L);
  std::vector<Key> keys;
  int n = 0;
  lua_pushnil(L);
  while (lua_next(L, t) != 0) {
    lua_pop(L, 1);
    Key k;
    switch (lua_type(L, -1)) {
      case LUA_TNUMBER:
        k.kind = 0;
        k.is_int = lua_isinteger(L, -1) != 0;
        if (k.is_int) k.i = lua_tointeger(L, -1); else k.f = lua_tonumber(L, -1);
        break;
      case LUA_TSTRING: {
        k.kind = 1;
        size_t len = 0;
        const char* p = lua_tolstring(L, -1, &len);   // szöveg kulcs: nem alakít át, a lua_next biztonságos marad
        k.s = std::string_view(p, len);
        break;
      }
      case LUA_TBOOLEAN:
        k.kind = 2;
        k.b = lua_toboolean(L, -1) != 0;
        break;
      default:
        if (skip_bad) continue;
        luaL_error(L, "a pairs csak szám, szöveg és logikai kulcsot jár be rendezve (itt: %s)", luaL_typename(L, -1));
    }
    k.slot = ++n;
    lua_pushvalue(L, -1);
    lua_rawseti(L, tmp, n);   // a kulcs (és a szöveg) életben marad a rendezés alatt
    keys.push_back(k);
  }
  std::sort(keys.begin(), keys.end(), key_less);
  lua_createtable(L, n, 0);
  for (int i = 0; i < n; ++i) {
    lua_rawgeti(L, tmp, keys[static_cast<size_t>(i)].slot);
    lua_rawseti(L, -2, i + 1);
  }
  lua_remove(L, tmp);
}

int ordered_next(lua_State* L) {
  // felértékek: 1 tábla, 2 rendezett kulcsok, 3 pozíció
  lua_Integer pos = lua_tointeger(L, lua_upvalueindex(3));
  const auto n = static_cast<lua_Integer>(lua_rawlen(L, lua_upvalueindex(2)));
  while (pos < n) {
    ++pos;
    lua_rawgeti(L, lua_upvalueindex(2), pos);
    lua_pushvalue(L, -1);
    if (lua_rawget(L, lua_upvalueindex(1)) != LUA_TNIL) {   // a bejárás közben törölt kulcsot kihagyjuk
      lua_pushinteger(L, pos);
      lua_replace(L, lua_upvalueindex(3));
      return 2;
    }
    lua_pop(L, 2);
  }
  lua_pushinteger(L, pos);
  lua_replace(L, lua_upvalueindex(3));
  return 0;
}

// iterátor a t (abszolút index) tábla rendezett bejárásához: iter, t, nil
int push_ordered_iter(lua_State* L, int t) {
  lua_pushvalue(L, t);
  push_sorted_keys(L, t, false);
  lua_pushinteger(L, 0);
  lua_pushcclosure(L, ordered_next, 3);
  lua_pushvalue(L, t);
  lua_pushnil(L);
  return 3;
}

int l_pairs(lua_State* L) {
  luaL_checkany(L, 1);
  if (luaL_getmetafield(L, 1, "__pairs") != LUA_TNIL) {
    lua_pushvalue(L, 1);
    lua_call(L, 1, 3);
    return 3;
  }
  luaL_checktype(L, 1, LUA_TTABLE);
  return push_ordered_iter(L, 1);
}

// --- biztonságos szövegalak: soha nem ír memóriacímet ----------------------------------------------------------------

int l_tostring(lua_State* L) {
  luaL_checkany(L, 1);
  lua_settop(L, 1);
  const int t = lua_type(L, 1);
  if (t == LUA_TTABLE || t == LUA_TFUNCTION || t == LUA_TUSERDATA || t == LUA_TTHREAD || t == LUA_TLIGHTUSERDATA) {
    if (luaL_getmetafield(L, 1, "__tostring") != LUA_TNIL) {
      lua_pushvalue(L, 1);
      lua_call(L, 1, 1);
      if (!lua_isstring(L, -1)) luaL_error(L, "a '__tostring' nem szöveget adott");
      return 1;
    }
    const int nt = luaL_getmetafield(L, 1, "__name");
    if (nt == LUA_TSTRING) return 1;
    if (nt != LUA_TNIL) lua_pop(L, 1);
    lua_pushstring(L, lua_typename(L, t));
    return 1;
  }
  luaL_tolstring(L, 1, nullptr);
  return 1;
}

void push_safe_string(lua_State* L, int idx) {
  idx = lua_absindex(L, idx);
  lua_pushcfunction(L, l_tostring);
  lua_pushvalue(L, idx);
  lua_call(L, 1, 1);
}

int l_log(lua_State* L) {
  const int n = lua_gettop(L);
  std::string out;
  for (int i = 1; i <= n; ++i) {
    push_safe_string(L, i);
    size_t len = 0;
    const char* s = lua_tolstring(L, -1, &len);
    if (i > 1) out += ' ';
    out.append(s, len);
    lua_pop(L, 1);
  }
  impl(L).message(out);
  return 0;
}

// string.format: a %p (memóriacím) és a %a (gépenként eltérő hexa alak) tiltva; táblát, függvényt a biztonságos
// szövegalakjával ír ki.
int l_format(lua_State* L) {
  size_t n = 0;
  const char* f = luaL_checklstring(L, 1, &n);
  for (size_t i = 0; i < n; ++i) {
    if (f[i] != '%') continue;
    ++i;
    while (i < n && std::strchr("-+ #0123456789.", f[i]) != nullptr) ++i;
    if (i < n && (f[i] == 'p' || f[i] == 'a' || f[i] == 'A')) {
      luaL_error(L, "a string.format '%%%c' formátuma nem használható (determinizmus)", f[i]);
    }
  }
  const int top = lua_gettop(L);
  lua_pushvalue(L, lua_upvalueindex(1));
  for (int k = 1; k <= top; ++k) {
    const int t = lua_type(L, k);
    if (k > 1 && (t == LUA_TTABLE || t == LUA_TFUNCTION || t == LUA_TUSERDATA || t == LUA_TTHREAD)) {
      push_safe_string(L, k);
    } else {
      lua_pushvalue(L, k);
    }
  }
  lua_call(L, top, 1);
  return 1;
}

// setmetatable: __gc és __mode nélkül (a szemétgyűjtés időzítése ne befolyásolhassa a játékot)
int l_setmetatable(lua_State* L) {
  if (lua_type(L, 2) == LUA_TTABLE) {
    for (const char* key : {"__gc", "__mode"}) {
      lua_pushstring(L, key);
      if (lua_rawget(L, 2) != LUA_TNIL) luaL_error(L, "a '%s' metamódszer nem használható (determinizmus)", key);
      lua_pop(L, 1);
    }
  }
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_pushvalue(L, 1);
  lua_pushvalue(L, 2);
  lua_call(L, 2, 1);
  return 1;
}

// --- determinisztikus math -----------------------------------------------------------------------------------------

void push_integral(lua_State* L, lua_Number f) {
  if (f >= -9223372036854775808.0 && f < 9223372036854775808.0) {
    lua_pushinteger(L, static_cast<lua_Integer>(f));
  } else {
    lua_pushnumber(L, f);
  }
}

int m_floor(lua_State* L) {
  if (lua_isinteger(L, 1)) { lua_settop(L, 1); return 1; }
  push_integral(L, std::floor(luaL_checknumber(L, 1)));
  return 1;
}
int m_ceil(lua_State* L) {
  if (lua_isinteger(L, 1)) { lua_settop(L, 1); return 1; }
  push_integral(L, std::ceil(luaL_checknumber(L, 1)));
  return 1;
}
int m_abs(lua_State* L) {
  if (lua_isinteger(L, 1)) {
    const lua_Integer n = lua_tointeger(L, 1);
    if (n < 0) lua_pushinteger(L, static_cast<lua_Integer>(0u - static_cast<lua_Unsigned>(n)));
    else lua_pushinteger(L, n);
  } else {
    lua_pushnumber(L, std::fabs(luaL_checknumber(L, 1)));
  }
  return 1;
}
int m_sqrt(lua_State* L) {   // az IEEE 754 szerint pontosan kerekített, minden gépen ugyanaz
  lua_pushnumber(L, std::sqrt(luaL_checknumber(L, 1)));
  return 1;
}
int m_fmod(lua_State* L) {   // pontos művelet
  const lua_Number a = luaL_checknumber(L, 1), b = luaL_checknumber(L, 2);
  lua_pushnumber(L, std::fmod(a, b));
  return 1;
}
int m_minmax(lua_State* L, int op_greater) {
  const int n = lua_gettop(L);
  int best = 1;
  luaL_checknumber(L, 1);
  for (int i = 2; i <= n; ++i) {
    luaL_checknumber(L, i);
    if (op_greater ? lua_compare(L, best, i, LUA_OPLT) : lua_compare(L, i, best, LUA_OPLT)) best = i;
  }
  lua_pushvalue(L, best);
  return 1;
}
int m_max(lua_State* L) { return m_minmax(L, 1); }
int m_min(lua_State* L) { return m_minmax(L, 0); }
int m_tointeger(lua_State* L) {
  int ok = 0;
  const lua_Integer n = lua_tointegerx(L, 1, &ok);
  if (ok && lua_type(L, 1) == LUA_TNUMBER) lua_pushinteger(L, n);
  else lua_pushnil(L);
  return 1;
}
int m_type(lua_State* L) {
  luaL_checkany(L, 1);
  if (lua_type(L, 1) == LUA_TNUMBER) lua_pushstring(L, lua_isinteger(L, 1) ? "integer" : "float");
  else lua_pushnil(L);
  return 1;
}
int m_ult(lua_State* L) {
  const auto a = static_cast<lua_Unsigned>(luaL_checkinteger(L, 1));
  const auto b = static_cast<lua_Unsigned>(luaL_checkinteger(L, 2));
  lua_pushboolean(L, a < b);
  return 1;
}

uint64_t next64(core::Pcg32& r) {
  const uint64_t hi = r.next();
  return (hi << 32) | r.next();
}

int m_random(lua_State* L) {
  core::Pcg32& rng = impl(L).rng;
  const int n = lua_gettop(L);
  if (n == 0) {   // [0, 1): 53 véletlen bit, pontosan ábrázolható
    lua_pushnumber(L, static_cast<lua_Number>(next64(rng) >> 11) * 0x1.0p-53);
    return 1;
  }
  lua_Integer lo = 1, hi = 0;
  if (n == 1) {
    hi = luaL_checkinteger(L, 1);
  } else {
    lo = luaL_checkinteger(L, 1);
    hi = luaL_checkinteger(L, 2);
  }
  luaL_argcheck(L, lo <= hi, n, "üres tartomány");
  const uint64_t span = static_cast<uint64_t>(hi) - static_cast<uint64_t>(lo);   // a darabszám - 1
  uint64_t r = next64(rng);
  if (span != UINT64_MAX) {
    const uint64_t count = span + 1;
    const uint64_t limit = UINT64_MAX - (UINT64_MAX % count);   // torzítás nélkül: elutasítás
    while (r >= limit) r = next64(rng);
    r %= count;
  }
  lua_pushinteger(L, static_cast<lua_Integer>(static_cast<uint64_t>(lo) + r));
  return 1;
}

void open_math(lua_State* L) {
  static const luaL_Reg funcs[] = {
      {"abs", m_abs},   {"ceil", m_ceil}, {"floor", m_floor},         {"fmod", m_fmod}, {"max", m_max},
      {"min", m_min},   {"sqrt", m_sqrt}, {"tointeger", m_tointeger}, {"type", m_type}, {"ult", m_ult},
      {"random", m_random}, {nullptr, nullptr}};
  luaL_newlib(L, funcs);
  lua_pushnumber(L, 3.141592653589793238462643383279502884);
  lua_setfield(L, -2, "pi");
  lua_pushnumber(L, HUGE_VAL);
  lua_setfield(L, -2, "huge");
  lua_pushinteger(L, LUA_MAXINTEGER);
  lua_setfield(L, -2, "maxinteger");
  lua_pushinteger(L, LUA_MININTEGER);
  lua_setfield(L, -2, "mininteger");
  lua_setglobal(L, "math");
}

// --- csak olvasható táblák -----------------------------------------------------------------------------------------

int ro_index(lua_State* L) {
  lua_getiuservalue(L, 1, 1);
  lua_pushvalue(L, 2);
  lua_rawget(L, -2);
  return 1;
}
int ro_newindex(lua_State* L) { return luaL_error(L, "csak olvasható tábla"); }
int ro_len(lua_State* L) {
  lua_getiuservalue(L, 1, 1);
  lua_pushinteger(L, static_cast<lua_Integer>(lua_rawlen(L, -1)));
  return 1;
}
int ro_pairs(lua_State* L) {
  lua_getiuservalue(L, 1, 1);
  return push_ordered_iter(L, lua_gettop(L));
}

// A verem tetején lévő táblát csak olvasható nézetre cseréli. A beágyazott táblákat a hívó előbb becsomagolja.
void wrap_readonly(lua_State* L) {
  lua_newuserdatauv(L, 0, 1);
  lua_insert(L, -2);
  lua_setiuservalue(L, -2, 1);
  luaL_setmetatable(L, kReadOnlyMeta);
}

void create_readonly_meta(lua_State* L) {
  luaL_newmetatable(L, kReadOnlyMeta);
  static const luaL_Reg funcs[] = {
      {"__index", ro_index}, {"__newindex", ro_newindex}, {"__len", ro_len}, {"__pairs", ro_pairs}, {nullptr, nullptr}};
  luaL_setfuncs(L, funcs, 0);
  lua_pushstring(L, "csak olvasható tábla");
  lua_setfield(L, -2, "__name");
  lua_pushboolean(L, 0);
  lua_setfield(L, -2, "__metatable");   // a getmetatable nem adja ki, a setmetatable nem cseréli
  lua_pop(L, 1);
}

void push_json(lua_State* L, const content::Value& v, int depth) {
  luaL_checkstack(L, 4, "túl mély adat");
  if (depth > 64) luaL_error(L, "túl mély adat");
  switch (v.type()) {
    case content::Value::Type::Null: lua_pushnil(L); break;
    case content::Value::Type::Bool: lua_pushboolean(L, v.as_bool()); break;
    case content::Value::Type::Number:
      if (auto i = v.as_int()) {
        lua_pushinteger(L, static_cast<lua_Integer>(*i));
      } else if (auto f = v.as_fixed()) {
        lua_pushnumber(L, static_cast<lua_Number>(f->raw()) / 65536.0);   // pontos érték
      } else {
        lua_pushstring(L, v.text().c_str());
      }
      break;
    case content::Value::Type::String: lua_pushlstring(L, v.text().data(), v.text().size()); break;
    case content::Value::Type::Array: {
      lua_createtable(L, static_cast<int>(v.items().size()), 0);
      lua_Integer i = 0;
      for (const auto& item : v.items()) {
        push_json(L, item, depth + 1);
        lua_rawseti(L, -2, ++i);
      }
      wrap_readonly(L);
      break;
    }
    case content::Value::Type::Object:
      lua_createtable(L, 0, static_cast<int>(v.members().size()));
      for (const auto& [key, val] : v.members()) {
        lua_pushlstring(L, key.data(), key.size());
        push_json(L, val, depth + 1);
        lua_rawset(L, -3);
      }
      wrap_readonly(L);
      break;
  }
}

// --- sorosítás (S tábla, időzítő-adat) -----------------------------------------------------------------------------

bool storable(lua_State* L, int idx) {
  const int t = lua_type(L, idx);
  return t == LUA_TNIL || t == LUA_TBOOLEAN || t == LUA_TNUMBER || t == LUA_TSTRING || t == LUA_TTABLE;
}

void serialize(lua_State* L, int idx, core::ByteWriter& w, int depth, bool strict) {
  idx = lua_absindex(L, idx);
  luaL_checkstack(L, 6, "túl mély adat");
  switch (lua_type(L, idx)) {
    case LUA_TNIL: w.u8(0); return;
    case LUA_TBOOLEAN: w.u8(lua_toboolean(L, idx) ? 2 : 1); return;
    case LUA_TNUMBER:
      if (lua_isinteger(L, idx)) {
        w.u8(3);
        w.i64(lua_tointeger(L, idx));
      } else {
        w.u8(4);
        const lua_Number f = lua_tonumber(L, idx);
        uint64_t bits = 0;
        std::memcpy(&bits, &f, sizeof bits);
        w.u64(bits);
      }
      return;
    case LUA_TSTRING: {
      size_t len = 0;
      const char* s = lua_tolstring(L, idx, &len);
      w.u8(5);
      w.str(std::string_view(s, len));
      return;
    }
    case LUA_TTABLE: {
      if (depth >= kMaxDepth) {
        if (strict) luaL_error(L, "túl mély (vagy körkörös) tábla");
        w.u8(0);
        return;
      }
      push_sorted_keys(L, idx, !strict);
      const int keys = lua_gettop(L);
      const auto n = static_cast<lua_Integer>(lua_rawlen(L, keys));
      std::vector<lua_Integer> use;   // a menthető értékű kulcsok
      for (lua_Integer i = 1; i <= n; ++i) {
        lua_rawgeti(L, keys, i);
        lua_rawget(L, idx);
        const bool ok = storable(L, -1) && (lua_type(L, -1) != LUA_TTABLE || depth + 1 < kMaxDepth);
        lua_pop(L, 1);
        if (ok) {
          use.push_back(i);
        } else if (strict) {
          luaL_error(L, "nem menthető érték (csak nil, logikai, szám, szöveg és tábla)");
        }
      }
      w.u8(6);
      w.u32(static_cast<uint32_t>(use.size()));
      for (const lua_Integer i : use) {
        lua_rawgeti(L, keys, i);
        serialize(L, -1, w, depth + 1, strict);
        lua_rawget(L, idx);
        serialize(L, -1, w, depth + 1, strict);
        lua_pop(L, 1);
      }
      lua_pop(L, 1);
      return;
    }
    default:
      if (strict) luaL_error(L, "nem menthető érték: %s", luaL_typename(L, idx));
      w.u8(0);
      return;
  }
}

// Visszaállítja és a verem tetejére teszi az értéket; hibás adatnál false (a veremre ekkor is kerül egy érték).
bool deserialize(lua_State* L, core::ByteReader& r, int depth) {
  luaL_checkstack(L, 4, "túl mély adat");
  switch (r.u8()) {
    case 0: lua_pushnil(L); return r.ok();
    case 1: lua_pushboolean(L, 0); return r.ok();
    case 2: lua_pushboolean(L, 1); return r.ok();
    case 3: lua_pushinteger(L, static_cast<lua_Integer>(r.i64())); return r.ok();
    case 4: {
      const uint64_t bits = r.u64();
      lua_Number f = 0;
      std::memcpy(&f, &bits, sizeof f);
      lua_pushnumber(L, f);
      return r.ok();
    }
    case 5: {
      const std::string s = r.str();
      lua_pushlstring(L, s.data(), s.size());
      return r.ok();
    }
    case 6: {
      if (depth > kMaxDepth) { lua_pushnil(L); return false; }
      const uint32_t n = r.u32();
      lua_newtable(L);
      for (uint32_t i = 0; i < n && r.ok(); ++i) {
        if (!deserialize(L, r, depth + 1) || lua_isnil(L, -1)) { lua_pop(L, 1); return false; }
        if (!deserialize(L, r, depth + 1)) { lua_pop(L, 2); return false; }
        lua_rawset(L, -3);
      }
      return r.ok();
    }
    default:
      lua_pushnil(L);
      return false;
  }
}

std::string to_string(const core::ByteWriter& w) {
  return std::string(reinterpret_cast<const char*>(w.data().data()), w.data().size());
}

// --- Game API ------------------------------------------------------------------------------------------------------

sim::Sim& need_sim(lua_State* L) {
  Impl& I = impl(L);
  if (!I.sim) luaL_error(L, "a Game csak eseménykezelőből (on_start, on_tick, on_death, időzítő) hívható");
  return *I.sim;
}

core::Fixed to_fixed(lua_State* L, int idx, const char* what) {
  if (lua_isinteger(L, idx)) {
    const lua_Integer v = lua_tointeger(L, idx);
    if (v < -32768 || v > 32767) luaL_error(L, "'%s' tartományon kívül", what);
    return core::Fixed::from_int(static_cast<int32_t>(v));
  }
  if (lua_type(L, idx) != LUA_TNUMBER) luaL_error(L, "'%s': számot vártam, ez %s", what, luaL_typename(L, idx));
  const lua_Number d = lua_tonumber(L, idx);
  if (!(d > -32768.0 && d < 32768.0)) luaL_error(L, "'%s' tartományon kívül", what);
  const long long raw = std::llround(d * 65536.0);   // 2 hatványával szorzás pontos, a kerekítés is
  if (raw < INT32_MIN || raw > INT32_MAX) luaL_error(L, "'%s' tartományon kívül", what);
  return core::Fixed::from_raw(static_cast<int32_t>(raw));
}

void push_fixed(lua_State* L, core::Fixed f) { lua_pushnumber(L, static_cast<lua_Number>(f.raw()) / 65536.0); }

core::Fixed field_fixed(lua_State* L, int t, const char* key, core::Fixed def) {
  lua_getfield(L, t, key);
  const core::Fixed v = lua_isnil(L, -1) ? def : to_fixed(L, -1, key);
  lua_pop(L, 1);
  return v;
}

int32_t field_int(lua_State* L, int t, const char* key, int32_t def, int32_t lo, int32_t hi) {
  lua_getfield(L, t, key);
  lua_Integer v = def;
  if (!lua_isnil(L, -1)) {
    if (!lua_isinteger(L, -1)) luaL_error(L, "'%s': egész számot vártam", key);
    v = lua_tointeger(L, -1);
    if (v < lo || v > hi) luaL_error(L, "'%s' tartományon kívül (%d..%d)", key, static_cast<int>(lo), static_cast<int>(hi));
  }
  lua_pop(L, 1);
  return static_cast<int32_t>(v);
}

sim::EntityId check_id(lua_State* L, int idx) {
  const lua_Integer v = luaL_checkinteger(L, idx);
  return v > 0 && v <= static_cast<lua_Integer>(UINT32_MAX) ? static_cast<sim::EntityId>(v) : sim::kNoEntity;
}

int g_tick(lua_State* L) {
  lua_pushinteger(L, static_cast<lua_Integer>(need_sim(L).tick()));
  return 1;
}

int g_spawn(lua_State* L) {
  sim::Sim& s = need_sim(L);
  luaL_checktype(L, 1, LUA_TTABLE);
  sim::Command c;
  c.kind = sim::Command::Kind::Spawn;
  c.tick = s.tick();
  c.owner = static_cast<uint8_t>(field_int(L, 1, "owner", 0, 1, 255));
  if (c.owner == 0) luaL_error(L, "'owner' megadása kötelező (1..255)");
  c.x = field_fixed(L, 1, "x", core::Fixed{});
  c.y = field_fixed(L, 1, "y", core::Fixed{});
  c.hp = field_int(L, 1, "hp", 100, 1, 1000000);
  c.speed = field_fixed(L, 1, "speed", core::Fixed::from_ratio(5, 100));
  c.damage = field_int(L, 1, "damage", 10, 0, 1000000);
  c.spread = field_int(L, 1, "spread", 4, 0, 1000000);
  c.cooldown = field_int(L, 1, "cooldown", 30, 1, 1000000);
  c.range = field_fixed(L, 1, "range", core::Fixed::from_ratio(3, 2));
  lua_pushinteger(L, static_cast<lua_Integer>(s.spawn(c)));
  return 1;
}

int g_move(lua_State* L) {
  sim::Sim& s = need_sim(L);
  const core::Fixed x = to_fixed(L, 2, "x"), y = to_fixed(L, 3, "y");
  lua_Integer moved = 0;
  if (lua_type(L, 1) == LUA_TTABLE) {
    const auto n = static_cast<lua_Integer>(lua_rawlen(L, 1));
    for (lua_Integer i = 1; i <= n; ++i) {
      lua_rawgeti(L, 1, i);
      if (s.order_move(check_id(L, -1), x, y)) ++moved;
      lua_pop(L, 1);
    }
  } else {
    if (s.order_move(check_id(L, 1), x, y)) ++moved;
  }
  lua_pushinteger(L, moved);
  return 1;
}

int g_kill(lua_State* L) {
  sim::Sim& s = need_sim(L);
  lua_pushboolean(L, s.kill(check_id(L, 1)));
  return 1;
}

int g_unit(lua_State* L) {
  sim::Sim& s = need_sim(L);
  const sim::EntityId e = check_id(L, 1);
  if (!s.world().alive(e)) return 0;
  const sim::Health* h = s.world().get<sim::Health>(e);
  const sim::Position* p = s.world().get<sim::Position>(e);
  const sim::Movement* m = s.world().get<sim::Movement>(e);
  if (!h || !p || h->hp <= 0) return 0;
  lua_createtable(L, 0, 9);
  lua_pushinteger(L, e); lua_setfield(L, -2, "id");
  lua_pushinteger(L, h->owner); lua_setfield(L, -2, "owner");
  push_fixed(L, p->x); lua_setfield(L, -2, "x");
  push_fixed(L, p->y); lua_setfield(L, -2, "y");
  lua_pushinteger(L, h->hp); lua_setfield(L, -2, "hp");
  lua_pushinteger(L, h->max); lua_setfield(L, -2, "max");
  lua_pushboolean(L, m && m->moving); lua_setfield(L, -2, "moving");
  const bool eco = s.rules() != nullptr;   // a gazdasági komponensek csak szabályokkal vannak regisztrálva
  lua_pushboolean(L, eco && s.world().get<sim::Building>(e) != nullptr); lua_setfield(L, -2, "building");
  const sim::Worker* wk = eco ? s.world().get<sim::Worker>(e) : nullptr;
  lua_pushboolean(L, wk && wk->state == sim::Worker::Idle); lua_setfield(L, -2, "idle");
  return 1;
}

// az élő egységek azonosítói növekvő sorrendben (owner megadva: csak az övéi)
int g_units(lua_State* L) {
  sim::Sim& s = need_sim(L);
  const lua_Integer owner = luaL_optinteger(L, 1, -1);
  lua_newtable(L);
  lua_Integer n = 0;
  s.world().pool<sim::Health>().each([&](sim::EntityId e, const sim::Health& h) {
    if (h.hp <= 0 || (owner >= 0 && h.owner != owner)) return;
    lua_pushinteger(L, e);
    lua_rawseti(L, -2, ++n);
  });
  return 1;
}

int g_count(lua_State* L) {
  sim::Sim& s = need_sim(L);
  const lua_Integer owner = luaL_optinteger(L, 1, -1);
  lua_Integer n = 0;
  s.world().pool<sim::Health>().each([&](sim::EntityId, const sim::Health& h) {
    if (h.hp > 0 && (owner < 0 || h.owner == owner)) ++n;
  });
  lua_pushinteger(L, n);
  return 1;
}

bool is_identifier(std::string_view s) {
  if (s.empty() || s.size() > 128) return false;
  for (size_t i = 0; i < s.size(); ++i) {
    const char c = s[i];
    const bool alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    if (!alpha && !(i > 0 && c >= '0' && c <= '9')) return false;
  }
  return true;
}

// Game.after(késleltetés_tick, "kezelő_neve", adat?) → időzítő-azonosító. A kezelő a megnevezett globális függvény,
// hívása: kezelő(adat, azonosító). Az adat mentődik, ezért csak nil/logikai/szám/szöveg/tábla lehet.
int g_after(lua_State* L) {
  sim::Sim& s = need_sim(L);
  Impl& I = impl(L);
  const lua_Integer delay = luaL_checkinteger(L, 1);
  luaL_argcheck(L, delay >= 1 && delay <= 1000000000, 1, "a késleltetés 1 és 10^9 tick között lehet");
  size_t len = 0;
  const char* h = luaL_checklstring(L, 2, &len);
  if (!is_identifier(std::string_view(h, len))) luaL_argerror(L, 2, "a kezelő egy globális függvény neve");
  core::ByteWriter w;
  lua_settop(L, 3);
  serialize(L, 3, w, 0, true);
  Impl::Timer t;
  t.tick = s.tick() + delay;
  t.id = I.next_timer++;
  t.handler.assign(h, len);
  t.payload = to_string(w);
  auto it = std::upper_bound(I.timers.begin(), I.timers.end(), t.tick,
                             [](int64_t tick, const Impl::Timer& x) { return tick < x.tick; });
  I.timers.insert(it, std::move(t));
  lua_pushinteger(L, static_cast<lua_Integer>(I.next_timer - 1));
  return 1;
}

int g_cancel(lua_State* L) {
  Impl& I = impl(L);
  const lua_Integer id = luaL_checkinteger(L, 1);
  const auto it = std::find_if(I.timers.begin(), I.timers.end(),
                               [&](const Impl::Timer& t) { return static_cast<lua_Integer>(t.id) == id; });
  if (it == I.timers.end()) {
    lua_pushboolean(L, 0);
  } else {
    I.timers.erase(it);
    lua_pushboolean(L, 1);
  }
  return 1;
}

// --- meccs (M6 9. lépés: a szkriptelt ellenfélhez) ------------------------------------------------------------------
uint8_t check_owner(lua_State* L, int i) {
  const lua_Integer o = luaL_checkinteger(L, i);
  if (o < 1 || o > sim::Sim::kMaxPlayers) luaL_error(L, "érvénytelen játékos: %d", static_cast<int>(o));
  return static_cast<uint8_t>(o);
}

uint16_t check_def(lua_State* L, sim::Sim& s, int i) {
  const char* id = luaL_checkstring(L, i);
  const uint16_t d = s.rules() ? s.rules()->def(id) : sim::kNoDef;
  if (d == sim::kNoDef) luaL_error(L, "ismeretlen sablon: %s", id);
  return d;
}

// Game.player(owner) → {wood, gold, meteor, supply_used, supply_cap, ascended, path, defeated}
int g_player(lua_State* L) {
  sim::Sim& s = need_sim(L);
  const sim::PlayerState& p = s.player(check_owner(L, 1));
  lua_createtable(L, 0, 10);
  lua_pushinteger(L, p.resource(sim::kWood)); lua_setfield(L, -2, "wood");
  lua_pushinteger(L, p.resource(sim::kGold)); lua_setfield(L, -2, "gold");
  lua_pushinteger(L, p.resource(sim::kMeteor)); lua_setfield(L, -2, "meteor");
  lua_pushinteger(L, p.supply_used); lua_setfield(L, -2, "supply_used");
  lua_pushinteger(L, p.supply_cap); lua_setfield(L, -2, "supply_cap");
  lua_pushboolean(L, p.ascended); lua_setfield(L, -2, "ascended");
  lua_pushstring(L, p.path.c_str()); lua_setfield(L, -2, "path");
  lua_pushboolean(L, p.defeated); lua_setfield(L, -2, "defeated");
  lua_pushstring(L, p.ally.c_str()); lua_setfield(L, -2, "ally");
  lua_pushstring(L, p.faction.c_str()); lua_setfield(L, -2, "faction");
  return 1;
}

// Game.kind(id) → a sablon azonosítója (egység vagy épület), vagy nil
int g_kind(lua_State* L) {
  sim::Sim& s = need_sim(L);
  const sim::EntityId e = check_id(L, 1);
  if (!s.world().alive(e) || !s.rules()) return 0;
  uint16_t d = sim::kNoDef;
  bool complete = true;
  if (const auto* u = s.world().get<sim::Unit>(e)) d = u->def;
  else if (const auto* b = s.world().get<sim::Building>(e)) {
    d = b->def;
    complete = b->complete != 0;
  }
  if (d == sim::kNoDef) return 0;
  lua_pushstring(L, s.rules()->defs[d].id.c_str());
  lua_pushboolean(L, complete);
  return 2;
}

int g_train(lua_State* L) {
  sim::Sim& s = need_sim(L);
  lua_pushboolean(L, s.train(check_owner(L, 1), check_def(L, s, 2)));
  return 1;
}

int g_research(lua_State* L) {
  sim::Sim& s = need_sim(L);
  std::string err;
  const bool ok = s.research(check_owner(L, 1), luaL_checkstring(L, 2), &err);
  lua_pushboolean(L, ok);
  lua_pushstring(L, err.c_str());
  return 2;
}

// Game.build(owner, épület, x, y): egy munkás építeni a (x, y) csempe közelébe
int g_build(lua_State* L) {
  sim::Sim& s = need_sim(L);
  const uint8_t o = check_owner(L, 1);
  const uint16_t d = check_def(L, s, 2);
  const auto x = static_cast<int>(luaL_checkinteger(L, 3)), y = static_cast<int>(luaL_checkinteger(L, 4));
  lua_pushboolean(L, s.build_near(o, d, x, y));
  return 1;
}

// Game.attack(azonosítók, x, y): támadó mozgás
int g_attack(lua_State* L) {
  sim::Sim& s = need_sim(L);
  luaL_checktype(L, 1, LUA_TTABLE);
  const core::Fixed x = to_fixed(L, 2, "x"), y = to_fixed(L, 3, "y");
  lua_Integer n = 0;
  const auto len = static_cast<lua_Integer>(lua_rawlen(L, 1));
  for (lua_Integer i = 1; i <= len; ++i) {
    lua_rawgeti(L, 1, i);
    if (s.order_attack_move(check_id(L, -1), x, y)) ++n;
    lua_pop(L, 1);
  }
  lua_pushinteger(L, n);
  return 1;
}

// Game.gather(munkás, "wood" | "gold" | "meteor"): a legközelebbi ilyen lelőhelyre
int g_gather(lua_State* L) {
  sim::Sim& s = need_sim(L);
  lua_pushboolean(L, s.gather_nearest(check_id(L, 1), sim::resource_index(luaL_checkstring(L, 2))));
  return 1;
}

int g_choose_path(lua_State* L) {
  sim::Sim& s = need_sim(L);
  lua_pushboolean(L, s.choose_path(check_owner(L, 1), luaL_checkstring(L, 2)));
  return 1;
}

int g_choose_ally(lua_State* L) {
  sim::Sim& s = need_sim(L);
  lua_pushboolean(L, s.choose_ally(check_owner(L, 1), luaL_checkstring(L, 2)));
  return 1;
}

// Game.ai_ally(p [, előírt]) → a gép szövetségválasztása (KIS-1); a megkötött kisnép, vagy nil
int g_ai_ally(lua_State* L) {
  sim::Sim& s = need_sim(L);
  const std::string pick = s.ai_choose_ally(check_owner(L, 1), luaL_optstring(L, 2, ""));
  if (pick.empty()) return 0;
  lua_pushstring(L, pick.c_str());
  return 1;
}

// Game.faction_ai(nép) → {build = {…}, army = {{id, n}, …}} – a nép `ai` tervéből (a kisnépé a szövetség után)
int g_faction_ai(lua_State* L) {
  sim::Sim& s = need_sim(L);
  const sim::FactionDef* f = s.rules() ? s.rules()->faction(luaL_checkstring(L, 1)) : nullptr;
  const content::Value* fa = f && f->raw ? f->raw->find("Faction") : nullptr;
  const content::Value* ai = fa ? fa->find("ai") : nullptr;
  if (!ai) return 0;
  lua_createtable(L, 0, 2);
  lua_newtable(L);
  int i = 0;
  if (const content::Value* b = ai->find("build"); b && b->is_array()) {
    for (const auto& x : b->items()) {
      if (!x.is_string()) continue;
      lua_pushstring(L, x.text().c_str());
      lua_rawseti(L, -2, ++i);
    }
  }
  lua_setfield(L, -2, "build");
  lua_newtable(L);
  i = 0;
  if (const content::Value* a = ai->find("army"); a && a->is_object()) {
    for (const auto& [id, n] : a->members()) {
      lua_createtable(L, 2, 0);
      lua_pushstring(L, id.c_str());
      lua_rawseti(L, -2, 1);
      lua_pushinteger(L, n.as_int().value_or(1));
      lua_rawseti(L, -2, 2);
      lua_rawseti(L, -2, ++i);
    }
  }
  lua_setfield(L, -2, "army");
  // a főnép teljes terve (az adatvezérelt gépnek, `ai_nep.lua`): sorrendek, létszámok, nyersanyagarány, sablonok
  auto strings = [&](const char* key) {
    lua_newtable(L);
    int k = 0;
    if (const content::Value* a = ai->find(key); a && a->is_array()) {
      for (const auto& x : a->items()) {
        if (!x.is_string()) continue;
        lua_pushstring(L, x.text().c_str());
        lua_rawseti(L, -2, ++k);
      }
    }
    lua_setfield(L, -2, key);
  };
  strings("build_order");
  strings("research_order");
  strings("path_pref");
  lua_pushinteger(L, ai->find("workers") ? ai->find("workers")->as_int().value_or(14) : 14);
  lua_setfield(L, -2, "workers");
  lua_pushinteger(L, ai->find("attack_supply") ? ai->find("attack_supply")->as_int().value_or(16) : 16);
  lua_setfield(L, -2, "attack_supply");
  lua_newtable(L);   // gather = {{"wood", 62}, …} – százalékban, a sablon sorrendjében
  i = 0;
  if (const content::Value* g = ai->find("gather_ratio"); g && g->is_object()) {
    for (const auto& [res, v] : g->members()) {
      lua_createtable(L, 2, 0);
      lua_pushstring(L, res.c_str());
      lua_rawseti(L, -2, 1);
      lua_pushinteger(L, (int64_t{v.as_fixed().value_or(core::Fixed{}).raw()} * 100) >> 16);
      lua_rawseti(L, -2, 2);
      lua_rawseti(L, -2, ++i);
    }
  }
  lua_setfield(L, -2, "gather");
  for (const char* key : {"hq", "worker", "supply_building"}) {
    if (const content::Value* v = fa->find(key); v && v->is_string()) {
      lua_pushstring(L, v->text().c_str());
      lua_setfield(L, -2, key);
    }
  }
  return 1;
}

int g_winner(lua_State* L) {
  lua_pushinteger(L, need_sim(L).winner());
  return 1;
}

void open_game(lua_State* L) {
  static const luaL_Reg funcs[] = {{"tick", g_tick},   {"spawn", g_spawn}, {"move", g_move},   {"kill", g_kill},
                                   {"unit", g_unit},   {"units", g_units}, {"count", g_count}, {"after", g_after},
                                   {"cancel", g_cancel}, {"player", g_player}, {"kind", g_kind}, {"train", g_train},
                                   {"research", g_research}, {"build", g_build}, {"attack", g_attack},
                                   {"choose_path", g_choose_path}, {"gather", g_gather}, {"choose_ally", g_choose_ally}, {"ai_ally", g_ai_ally}, {"faction_ai", g_faction_ai}, {"winner", g_winner}, {nullptr, nullptr}};
  luaL_newlib(L, funcs);
  wrap_readonly(L);
  lua_setglobal(L, "Game");
}

// --- require -------------------------------------------------------------------------------------------------------

bool is_module_name(std::string_view s) {
  if (s.empty() || s.size() > 128 || s.front() == '.' || s.back() == '.') return false;
  for (const char c : s) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
    if (!ok) return false;
  }
  return s.find("..") == std::string_view::npos;
}

int l_require(lua_State* L) {
  size_t len = 0;
  const char* p = luaL_checklstring(L, 1, &len);
  const std::string name(p, len);
  if (!is_module_name(name)) luaL_argerror(L, 1, "érvénytelen modulnév (betű, szám, _ és pont)");
  lua_getfield(L, LUA_REGISTRYINDEX, kLoadedKey);
  const int loaded = lua_gettop(L);
  if (lua_getfield(L, loaded, name.c_str()) != LUA_TNIL) return 1;
  lua_pop(L, 1);
  Impl& I = impl(L);
  if (!I.loader) luaL_error(L, "nincs modulbetöltő: '%s' nem tölthető be", name.c_str());
  const std::optional<std::string> src = I.loader(name);
  if (!src) luaL_error(L, "nincs ilyen modul: '%s'", name.c_str());
  const std::string chunk = "=" + name;
  if (luaL_loadbufferx(L, src->data(), src->size(), chunk.c_str(), "t") != LUA_OK) lua_error(L);   // csak szöveg
  lua_pushstring(L, name.c_str());
  lua_call(L, 1, 1);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    lua_pushboolean(L, 1);
  }
  lua_pushvalue(L, -1);
  lua_setfield(L, loaded, name.c_str());
  return 1;
}

// --- az állapot felépítése -----------------------------------------------------------------------------------------

void build_environment(lua_State* L) {
  luaL_requiref(L, "_G", luaopen_base, 1);
  luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1);
  luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
  luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
  luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);
  lua_settop(L, 0);

  for (const char* name : {"dofile", "loadfile", "load", "collectgarbage", "print", "next", "warn"}) {
    lua_pushnil(L);
    lua_setglobal(L, name);
  }
  lua_getglobal(L, "string");
  lua_pushnil(L);
  lua_setfield(L, -2, "dump");
  lua_getfield(L, -1, "format");
  lua_pushcclosure(L, l_format, 1);
  lua_setfield(L, -2, "format");
  lua_pop(L, 1);

  lua_pushcfunction(L, l_pairs);
  lua_setglobal(L, "pairs");
  lua_pushcfunction(L, l_tostring);
  lua_setglobal(L, "tostring");
  lua_getglobal(L, "setmetatable");
  lua_pushcclosure(L, l_setmetatable, 1);
  lua_setglobal(L, "setmetatable");
  lua_pushcfunction(L, l_log);
  lua_setglobal(L, "log");
  lua_pushcfunction(L, l_require);
  lua_setglobal(L, "require");
  lua_newtable(L);
  lua_setfield(L, LUA_REGISTRYINDEX, kLoadedKey);

  open_math(L);
  create_readonly_meta(L);
  open_game(L);

  lua_newtable(L);
  lua_pushvalue(L, -1);
  impl(L).s_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_setglobal(L, "S");
}

uint64_t stream_for(const std::string& name) {
  core::StateHash h;
  h.add_bytes("totu-script");
  h.add_bytes(name);
  return h.value();
}

}  // namespace

// --- Impl tagfüggvények --------------------------------------------------------------------------------------------

template <class F>
bool ScriptHost::Impl::protect(const char* what, F&& f) {
  struct Ctx {
    F* fn;
  } ctx{&f};
  lua_pushcfunction(L, [](lua_State* st) -> int {
    auto* c = static_cast<Ctx*>(lua_touserdata(st, 1));
    lua_remove(st, 1);
    (*c->fn)(st);
    return 0;
  });
  lua_pushlightuserdata(L, &ctx);
  if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
    error_message(what, L);
    lua_pop(L, 1);
    return false;
  }
  return true;
}

bool ScriptHost::Impl::call_user(int nargs, const std::string& what) {
  const int fn = lua_gettop(L) - nargs;
  lua_pushcfunction(L, traceback);
  lua_insert(L, fn);
  const int st = lua_pcall(L, nargs, 0, fn);
  if (st != LUA_OK) {
    error_message(what, L);
    lua_pop(L, 1);
  }
  lua_remove(L, fn);
  return st == LUA_OK;
}

void ScriptHost::Impl::push_global(const char* key) {
  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
  lua_pushstring(L, key);
  lua_rawget(L, -2);   // nyers: a _G esetleges metatáblája nem futhat a motor kódjában
  lua_remove(L, -2);
}

// --- ScriptHost ----------------------------------------------------------------------------------------------------

ScriptHost::ScriptHost(std::string name, uint64_t seed, Limits limits) : impl_(std::make_unique<Impl>()) {
  Impl& I = *impl_;
  I.name = std::move(name);
  I.limits = limits;
  I.rng = core::Pcg32(seed, stream_for(I.name));
  I.L = lua_newstate(limited_alloc, &I);
  if (!I.L) {
    I.message("a Lua-állapot nem hozható létre (memória)");
    ++I.errors;
    return;
  }
  *static_cast<Impl**>(lua_getextraspace(I.L)) = &I;
  lua_atpanic(I.L, [](lua_State* st) -> int {
    std::fprintf(stderr, "Lua: védetlen hiba: %s\n", lua_tostring(st, -1));
    std::abort();
  });
  I.protect("környezet", [](lua_State* st) { build_environment(st); });
  lua_sethook(I.L, count_hook, LUA_MASKCOUNT, kHookStep);
}

ScriptHost::~ScriptHost() {
  if (impl_->L) lua_close(impl_->L);
}

void ScriptHost::set_module_loader(ModuleLoader loader) { impl_->loader = std::move(loader); }

void ScriptHost::set_shared(const std::string& name, const content::Value& data) {
  Impl& I = *impl_;
  if (!I.L) return;
  I.protect("megosztott adat", [&](lua_State* st) {
    push_json(st, data, 0);
    lua_setglobal(st, name.c_str());
  });
}

bool ScriptHost::run(const std::string& chunk_name, const std::string& source) {
  Impl& I = *impl_;
  if (!I.L) return false;
  I.used = 0;
  I.exhausted = false;
  bool ok = false;
  I.protect(chunk_name.c_str(), [&](lua_State* st) {
    const std::string chunk = "=" + chunk_name;
    if (luaL_loadbufferx(st, source.data(), source.size(), chunk.c_str(), "t") != LUA_OK) {   // csak szöveg
      I.error_message(chunk_name, st);
      lua_pop(st, 1);
      return;
    }
    ok = I.call_user(0, chunk_name);
  });
  return ok;
}

void ScriptHost::start(sim::Sim& s) {
  Impl& I = *impl_;
  if (!I.L) return;
  I.used = 0;
  I.exhausted = false;
  I.sim = &s;
  I.protect("on_start", [&](lua_State* st) {
    I.push_global("on_start");
    if (lua_isfunction(st, -1)) I.call_user(0, "on_start");
    else lua_pop(st, 1);
  });
  I.sim = nullptr;
}

void ScriptHost::on_tick(sim::Sim& s) {
  Impl& I = *impl_;
  if (!I.L) return;
  I.used = 0;
  I.exhausted = false;
  I.sim = &s;
  I.protect("on_tick", [&](lua_State* st) {
    // 1. a lejárt időzítők (tick, azonosító) sorrendben
    while (!I.timers.empty() && I.timers.front().tick <= s.tick() && !I.exhausted) {
      Impl::Timer t = std::move(I.timers.front());
      I.timers.erase(I.timers.begin());
      I.push_global(t.handler.c_str());
      if (!lua_isfunction(st, -1)) {
        lua_pop(st, 1);
        ++I.errors;
        I.message("hiba: az időzítő kezelője nem függvény: " + t.handler);
        continue;
      }
      std::vector<uint8_t> bytes(t.payload.begin(), t.payload.end());
      core::ByteReader r(bytes);
      deserialize(st, r, 0);
      lua_pushinteger(st, static_cast<lua_Integer>(t.id));
      I.call_user(2, t.handler);
    }
    // 2. on_tick(tick)
    if (I.exhausted) return;
    I.push_global("on_tick");
    if (lua_isfunction(st, -1)) {
      lua_pushinteger(st, static_cast<lua_Integer>(s.tick()));
      I.call_user(1, "on_tick");
    } else {
      lua_pop(st, 1);
    }
  });
  I.sim = nullptr;
}

void ScriptHost::on_death(sim::Sim& s, sim::EntityId id, uint8_t owner) {
  Impl& I = *impl_;
  if (!I.L || I.exhausted) return;   // ugyanabban a tickben az on_tick kerete számít
  I.sim = &s;
  I.protect("on_death", [&](lua_State* st) {
    I.push_global("on_death");
    if (lua_isfunction(st, -1)) {
      lua_pushinteger(st, static_cast<lua_Integer>(id));
      lua_pushinteger(st, owner);
      I.call_user(2, "on_death");
    } else {
      lua_pop(st, 1);
    }
  });
  I.sim = nullptr;
}

void ScriptHost::save(core::ByteWriter& w) const {
  Impl& I = *impl_;
  w.str("totu-script");
  w.u32(1);
  w.str(I.name);
  w.u64(I.rng.state());
  w.u64(I.rng.increment());
  w.u64(I.next_timer);
  w.u32(static_cast<uint32_t>(I.timers.size()));
  for (const auto& t : I.timers) {
    w.i64(t.tick);
    w.u64(t.id);
    w.str(t.handler);
    w.str(t.payload);
  }
  core::ByteWriter sw;
  if (I.L) {
    I.protect("S mentése", [&](lua_State* st) {
      lua_rawgeti(st, LUA_REGISTRYINDEX, I.s_ref);
      serialize(st, -1, sw, 0, false);   // a nem menthető értékek kimaradnak
      lua_pop(st, 1);
    });
  }
  w.str(to_string(sw));
}

uint64_t ScriptHost::hash() const {
  core::ByteWriter w;
  save(w);
  core::StateHash h;
  h.add_bytes(to_string(w));
  return h.value();
}

bool ScriptHost::load(core::ByteReader& r) {
  Impl& I = *impl_;
  if (r.str() != "totu-script" || r.u32() != 1) return false;
  if (r.str() != I.name) return false;
  const uint64_t st = r.u64(), inc = r.u64();
  const uint64_t next_timer = r.u64();
  const uint32_t n = r.u32();
  std::vector<Impl::Timer> timers;
  for (uint32_t i = 0; i < n && r.ok(); ++i) {
    Impl::Timer t;
    t.tick = r.i64();
    t.id = r.u64();
    t.handler = r.str();
    t.payload = r.str();
    timers.push_back(std::move(t));
  }
  const std::string s_bytes = r.str();
  if (!r.ok() || !I.L) return false;
  bool ok = false;
  I.protect("S betöltése", [&](lua_State* L) {
    std::vector<uint8_t> bytes(s_bytes.begin(), s_bytes.end());
    core::ByteReader sr(bytes);
    if (!deserialize(L, sr, 0) || !sr.at_end() || !lua_istable(L, -1)) { lua_pop(L, 1); return; }
    const int fresh = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, I.s_ref);
    const int s = lua_gettop(L);
    // az S tábla ugyanaz marad (a szkript felső szintjén eltett hivatkozások érvényesek), csak a tartalma cserélődik
    lua_pushnil(L);
    while (lua_next(L, s) != 0) {
      lua_pop(L, 1);
      lua_pushvalue(L, -1);
      lua_pushnil(L);
      lua_rawset(L, s);
    }
    lua_pushnil(L);
    while (lua_next(L, fresh) != 0) {
      lua_pushvalue(L, -2);
      lua_insert(L, -2);
      lua_rawset(L, s);
    }
    lua_pop(L, 2);
    ok = true;
  });
  if (!ok) return false;
  I.rng.restore(st, inc);
  I.next_timer = next_timer;
  I.timers = std::move(timers);
  return true;
}

const std::string& ScriptHost::name() const { return impl_->name; }
const std::vector<std::string>& ScriptHost::messages() const { return impl_->messages; }
size_t ScriptHost::errors() const { return impl_->errors; }
size_t ScriptHost::memory_used() const { return impl_->mem; }

}  // namespace totu::script
