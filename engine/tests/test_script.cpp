// SPDX-License-Identifier: GPL-3.0-or-later
// A szkriptréteg tesztjei (M3): homokozó, determinisztikus bejárás és math, korlátok, csak olvasható adat,
// Game API és események, időzítők, mentés–betöltés, szkriptes visszajátszás.
#include <string>

#include "check.hpp"
#include "content/json.hpp"
#include "script/host.hpp"
#include "sim/replay.hpp"

using namespace totu;

namespace {

bool has_message(const script::ScriptHost& h, const std::string& part) {
  for (const auto& m : h.messages()) {
    if (m.find(part) != std::string::npos) return true;
  }
  return false;
}

// a szkript `R` globálisába írt eredményt a log-on át olvassuk ki: log(R)
std::string last_log(const script::ScriptHost& h) {
  if (h.messages().empty()) return {};
  const std::string& m = h.messages().back();
  const auto p = m.find("] ");
  return p == std::string::npos ? m : m.substr(p + 2);
}

std::string eval(const std::string& code, script::Limits lim = {}) {
  script::ScriptHost h("teszt", 1, lim);
  h.run("teszt", "log(" + code + ")");
  return last_log(h);
}

}  // namespace

TEST_CASE("szkript: a homokozóban nincs fájl-, rendszer- és bájtkód-hozzáférés") {
  CHECK(eval("type(io)") == "nil");
  CHECK(eval("type(os)") == "nil");
  CHECK(eval("type(debug)") == "nil");
  CHECK(eval("type(package)") == "nil");
  CHECK(eval("type(load)") == "nil");
  CHECK(eval("type(dofile)") == "nil");
  CHECK(eval("type(loadfile)") == "nil");
  CHECK(eval("type(collectgarbage)") == "nil");
  CHECK(eval("type(print)") == "nil");
  CHECK(eval("type(next)") == "nil");
  CHECK(eval("type(string.dump)") == "nil");
  CHECK(eval("type(string.format)") == "function");
  CHECK(eval("type(coroutine.wrap)") == "function");
  // bájtkód nem tölthető: sem közvetlenül, sem require-rel
  script::ScriptHost h("teszt", 1);
  CHECK(!h.run("bajtkod", std::string("\x1bLua\x54\x00", 6)));
  h.set_module_loader([](const std::string&) { return std::optional<std::string>(std::string("\x1bLuaT", 5)); });
  CHECK(!h.run("req", "require('akarmi')"));
  CHECK(!h.run("ut", "require('../titok')"));    // útvonal-kitörés tiltva
  CHECK(h.errors() == 3);
}

TEST_CASE("szkript: require a modul forrásából, egyszer betöltve") {
  script::ScriptHost h("teszt", 1);
  int calls = 0;
  h.set_module_loader([&](const std::string& m) -> std::optional<std::string> {
    ++calls;
    if (m == "seged.szamol") return std::string("local M = {} function M.dupla(x) return 2 * x end return M");
    return std::nullopt;
  });
  CHECK(h.run("fo", "local a = require('seged.szamol') local b = require('seged.szamol') log(a.dupla(21), a == b)"));
  CHECK(last_log(h) == "42 true");
  CHECK(calls == 1);
  CHECK(!h.run("hiany", "require('nincs.ilyen')"));
  CHECK(has_message(h, "nincs ilyen modul"));
}

TEST_CASE("szkript: a pairs kulcssorrendben jár, címet sehol nem ír ki") {
  CHECK(eval("(function() local t = {b=1, a=2, [3]='x', [1]='y', [2.5]='z', [-1]=0, [true]=1, aa=3} "
             "local o = {} for k in pairs(t) do o[#o+1] = tostring(k) end return table.concat(o, ',') end)()") ==
        "-1,1,2.5,3,a,aa,b,true");
  CHECK(eval("(pcall(pairs, {[{}] = 1}))") == "false");                // tábla kulcs: nincs determinisztikus sorrend
  CHECK(eval("tostring({})") == "table");
  CHECK(eval("tostring(print or log)") == "function");
  CHECK(eval("string.format('%s|%d', {}, 7)") == "table|7");
  CHECK(eval("(pcall(string.format, '%p', {}))") == "false");
  CHECK(eval("(pcall(string.format, '%a', 1.0))") == "false");
  CHECK(eval("string.format('%.3f %g', 1/3, 0.1)") == "0.333 0.1");
  CHECK(eval("(pcall(setmetatable, {}, {__gc = function() end}))") == "false");
  CHECK(eval("(pcall(setmetatable, {}, {__mode = 'k'}))") == "false");
  CHECK(eval("getmetatable(setmetatable({}, {__index = {x = 5}})).__index.x") == "5");
  // nagy tömb rendezése: a pivot nem véletlen (rögzített), az eredmény minden gépen ugyanaz
  CHECK(eval("(function() local t = {} for i = 1, 500 do t[i] = (i * 7919) % 500 end "
             "table.sort(t) return t[1] .. ',' .. t[250] .. ',' .. t[500] end)()") == "0,249,499");
}

TEST_CASE("szkript: determinisztikus math és hatványozás") {
  CHECK(eval("2^10") == "1024.0");
  CHECK(eval("2^-2") == "0.25");
  CHECK(eval("2^0.5 == math.sqrt(2)") == "true");
  CHECK(eval("(pcall(function(x) return 2^x end, 0.3))") == "false");
  CHECK(eval("math.floor(3.7) .. ' ' .. math.ceil(3.2) .. ' ' .. math.abs(-4) .. ' ' .. math.max(1, 9, 3)") == "3 4 4 9");
  CHECK(eval("math.type(1) .. math.type(1.0) .. tostring(math.type('1'))") == "integerfloatnil");
  CHECK(eval("type(math.sin)") == "nil");                            // a libm-függő függvények nincsenek
  // math.random: a játszma magja és a mod neve határozza meg
  script::ScriptHost a("mod_a", 5), b("mod_a", 5), c("mod_b", 5);
  const std::string code = "local o = {} for i = 1, 8 do o[i] = math.random(1, 1000) end o[9] = math.random() "
                           "log(table.concat(o, ','))";
  a.run("r", code);
  b.run("r", code);
  c.run("r", code);
  CHECK(last_log(a) == last_log(b));
  CHECK(last_log(a) != last_log(c));
  CHECK(eval("(pcall(math.random, 5, 1))") == "false");
}

TEST_CASE("szkript: utasításkeret és memóriakorlát") {
  script::Limits lim;
  lim.instructions_per_tick = 200000;
  script::ScriptHost h("teszt", 1, lim);
  CHECK(h.run("vegtelen", "function on_tick(t) S.t = t if t == 3 then while true do end end end"));
  sim::Sim s(1);
  s.add_hooks(&h);
  for (int i = 0; i < 6; ++i) s.step();
  CHECK(has_message(h, "utasításkerete kimerült"));
  CHECK(h.errors() == 1);
  CHECK(h.run("utana", "log(S.t)"));
  CHECK(last_log(h) == "5");                                         // a következő tickekben tovább fut
  // a szkript a saját pcall-jával sem kerülheti meg a keretet
  CHECK(!h.run("pcall", "local n = 0 for i = 1, 100 do pcall(function() while true do end end) n = n + 1 end "
                        "while true do end"));

  script::Limits small;
  small.memory_bytes = size_t{2} << 20;
  script::ScriptHost m("teszt", 1, small);
  CHECK(!m.run("sok", "local t = {} for i = 1, 10000000 do t[i] = i end"));
  CHECK(has_message(m, "not enough memory"));
  CHECK(m.memory_used() < small.memory_bytes);
  CHECK(m.run("utana", "local t = {} for i = 1, 1000 do t[i] = i end log(#t)"));   // utána is használható
  CHECK(last_log(m) == "1000");
}

TEST_CASE("szkript: a megosztott adat csak olvasható") {
  auto data = content::parse(R"({"nev": "Wullof", "elet": 120, "sebesseg": 2.5, "cimkek": ["gyalog", "elo"],
                                  "koltseg": {"fa": 50, "ko": 20}})");
  CHECK(data.has_value());
  script::ScriptHost h("teszt", 1);
  h.set_shared("Data", *data);
  CHECK(h.run("olvas", "log(Data.nev, Data.elet, Data.sebesseg, #Data.cimkek, Data.cimkek[2], Data.koltseg.fa)"));
  CHECK(last_log(h) == "Wullof 120 2.5 2 elo 50");
  CHECK(h.run("bejar", "local o = {} for k, v in pairs(Data.koltseg) do o[#o+1] = k .. '=' .. v end "
                       "for i, v in ipairs(Data.cimkek) do o[#o+1] = i .. v end log(table.concat(o, ' '))"));
  CHECK(last_log(h) == "fa=50 ko=20 1gyalog 2elo");
  CHECK(!h.run("ir1", "Data.elet = 1"));
  CHECK(!h.run("ir2", "Data.koltseg.fa = 1"));
  CHECK(!h.run("ir3", "rawset(Data, 'x', 1)"));
  CHECK(!h.run("ir4", "setmetatable(Data, nil)"));
  CHECK(!h.run("ir5", "Game.spawn = nil"));
  CHECK(h.run("meta", "log(getmetatable(Data))"));
  CHECK(last_log(h) == "false");
}

TEST_CASE("szkript: Game API, halál-esemény, időzítők") {
  script::ScriptHost h("kuldetes", 3);
  CHECK(h.run("k", R"(
    function on_start()
      S.hullam = 0
      S.halottak = {}
      Game.after(10, "hullam", {db = 3, x = 20})
    end
    function hullam(adat, id)
      S.hullam = S.hullam + 1
      S.ids = {}
      for i = 1, adat.db do
        S.ids[i] = Game.spawn{owner = 2, x = adat.x, y = 5 + i, hp = 50, speed = 0.1, range = 2}
      end
      Game.move(S.ids, 3, 6)
      S.tick = Game.tick()
    end
    function on_death(id, owner)
      S.halottak[#S.halottak + 1] = owner
    end
    function on_tick(t)
      if t == 0 then S.ved = Game.spawn{owner = 1, x = 3, y = 6, hp = 1000, damage = 30, spread = 0, cooldown = 5, range = 3} end
      if t == 15 then local u = Game.unit(S.ids[1]) S.mozog = u and u.moving end
      if t == 20 then S.megolt = Game.kill(S.ids[3]) S.ujra = Game.kill(S.ids[3]) end
    end
  )"));
  sim::Sim s(3);
  s.add_hooks(&h);
  h.start(s);
  for (int i = 0; i < 400; ++i) s.step();
  CHECK(h.errors() == 0);
  CHECK(h.run("ell", "log(S.hullam, S.tick, S.mozog, S.megolt, S.ujra, #S.halottak, Game == nil)"));
  CHECK(last_log(h) == "1 10 true true false 3 false");
  CHECK(h.run("api", "log(pcall(Game.tick))"));                      // eseményen kívül nem hívható
  CHECK(last_log(h).rfind("false", 0) == 0);
  CHECK(s.world().entity_count() == 1);                              // csak a védő maradt
}

TEST_CASE("szkript: mentés, betöltés, folytatás = megszakítás nélküli futás") {
  const std::string code = R"(
    function on_start() S.n = 0 S.lista = {} Game.after(5, "ismetel", {lepes = 7}) end
    function ismetel(a, id)
      S.n = S.n + 1
      S.lista[#S.lista + 1] = math.random(1, 100)
      Game.spawn{owner = 1 + S.n % 2, x = math.random(0, 30), y = math.random(0, 30), hp = 60, range = 4}
      Game.after(a.lepes, "ismetel", a)
    end
    function on_tick(t) if t % 20 == 0 then Game.move(Game.units(1), 15, 15) Game.move(Game.units(2), 15, 15) end end
  )";
  script::ScriptHost h_full("k", 9), h_first("k", 9);
  h_full.run("k", code);
  h_first.run("k", code);
  sim::Sim full(9), first(9);
  full.add_hooks(&h_full);
  first.add_hooks(&h_first);
  h_full.start(full);
  h_first.start(first);
  for (int t = 0; t < 200; ++t) { full.step(); first.step(); }
  CHECK(full.hash() == first.hash());
  const auto data = first.save();
  script::ScriptHost h_res("k", 1234);                               // a mag a mentésből jön
  h_res.run("k", code);
  sim::Sim resumed(1);
  resumed.add_hooks(&h_res);
  CHECK(resumed.load(data));
  CHECK(resumed.hash() == full.hash());
  for (int t = 0; t < 300; ++t) { full.step(); resumed.step(); }
  CHECK(resumed.hash() == full.hash());
  CHECK(resumed.save() == full.save());
  CHECK(h_full.errors() == 0 && h_res.errors() == 0);
  // a szkriptállapot a hash része: más S → más hash
  const uint64_t before = full.hash();
  h_full.run("piszkal", "S.n = S.n + 1");
  CHECK(full.hash() != before);
  // más csatlakozókkal nem tölthető be
  sim::Sim bare(1);
  CHECK(!bare.load(data));
}

TEST_CASE("szkript: szkriptes visszajátszás, eltéréskereső") {
  std::vector<sim::ScriptFile> scripts = {
      {"kuldetes", "fo", "function on_start() Game.after(3, 'jon', nil) end "
                         "function jon() for i = 1, 6 do Game.spawn{owner = 1 + i % 2, x = i * 2, y = 4, range = 6} end "
                         "S.k = (S.k or 0) + 1 if S.k < 4 then Game.after(40, 'jon', nil) end end"}};
  const sim::Replay r = sim::record(21, 300, {}, 30, scripts);
  CHECK(r.checkpoints.size() == 10);
  const auto bytes = sim::encode(r);
  const auto back = sim::decode(bytes);
  CHECK(back.has_value() && back->scripts == r.scripts);
  const auto v = sim::verify(*back);
  CHECK(v.ok);
  sim::Replay bad = *back;
  bad.scripts[0].source.replace(bad.scripts[0].source.find("S.k < 4"), 7, "S.k < 3");
  const auto w = sim::verify(bad);
  CHECK(!w.ok);
  bool script_diff = false;
  for (const auto& c : w.differing_components) script_diff |= c == "Szkript1";
  CHECK(script_diff);
  // szkript nélküli visszajátszás változatlanul az 1-es formátum
  const sim::Replay plain = sim::record(1, 10, {});
  CHECK(sim::encode(plain)[15] == 1);
}
