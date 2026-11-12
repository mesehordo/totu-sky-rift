// SPDX-License-Identifier: GPL-3.0-or-later
// Szkriptréteg (M3): homokozott, determinisztikus Lua 5.4 a küldetésekhez és a modokhoz.
//
// Egy ScriptHost = egy mod saját Lua-állapota (a modok nem látják egymás változóit). A szimulációhoz
// sim::SimHooks-ként kapcsolódik, így az eseményei a tick rögzített pontjain futnak, és az állapota (S tábla,
// időzítők, véletlenszám-folyam) a szimuláció hashének és mentésének része.
//
// Homokozó: nincs io, os, package, debug, load, dofile, loadfile, collectgarbage, string.dump; bájtkód nem tölthető
// (csak szöveges forrás); a pairs kulcssorrendben jár (szám < szöveg < logikai); a tostring nem ír ki memóriacímet;
// a setmetatable nem enged __gc-t és __mode-ot (a szemétgyűjtés időzítése ne hasson a játékra); a math saját,
// determinisztikus (math.random a mod saját folyama). Korlátok: utasításkeret tickenként és memóriakorlát.
//
// A szkript által látott globálisok: a fenti könyvtárak, log(...), require(név), S (mentett adat), Game (API) és a
// set_shared() csak olvasható adattáblái. Események (ha a szkript megadja): on_start(), on_tick(tick),
// on_death(id, owner); az időzítők a megnevezett globális függvényt hívják.
// Mentés: csak az S tábla, az időzítők és a véletlenszám-folyam mentődik. Betöltéskor a szkriptek felső szintje
// újra lefut (függvénydefiníciók), az on_start nem; ezért a megmaradó adat helye az S.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "content/json.hpp"
#include "sim/sim.hpp"

namespace totu::script {

struct Limits {
  int64_t instructions_per_tick = 5'000'000;   // a tick összes szkripthívására együtt
  size_t memory_bytes = size_t{64} << 20;      // a Lua-állapot teljes memóriája
};

class ScriptHost final : public sim::SimHooks {
 public:
  // name: a mod neve (a naplóban és a véletlenszám-folyam kiválasztásában); seed: a játszma magja.
  ScriptHost(std::string name, uint64_t seed, Limits limits = {});
  ~ScriptHost() override;
  ScriptHost(const ScriptHost&) = delete;
  ScriptHost& operator=(const ScriptHost&) = delete;

  // require(név) forrása (pl. a mod VFS-éből); nincs megadva → a require hibát ad.
  using ModuleLoader = std::function<std::optional<std::string>(const std::string& module)>;
  void set_module_loader(ModuleLoader loader);

  // Csak olvasható globális tábla JSON-adatból (pl. sablonok). A számok egészként, ha egészek, különben
  // a fixpontos érték pontos lebegőpontos alakjában jelennek meg.
  void set_shared(const std::string& name, const content::Value& data);

  // Egy szkriptfájl felső szintjének futtatása (függvények megadása). false: hiba, az üzenet a naplóban.
  bool run(const std::string& chunk_name, const std::string& source);

  // Új játszma kezdete: az on_start() hívása. Betöltött játszmánál nem kell.
  void start(sim::Sim& sim);

  void on_tick(sim::Sim& sim) override;
  void on_death(sim::Sim& sim, sim::EntityId id, uint8_t owner) override;
  uint64_t hash() const override;
  void save(core::ByteWriter& w) const override;
  bool load(core::ByteReader& r) override;

  const std::string& name() const;
  const std::vector<std::string>& messages() const;   // log() és hibák; nem része a játékállapotnak
  size_t errors() const;                              // eddigi szkripthibák száma
  size_t memory_used() const;

  struct Impl;

 private:
  std::unique_ptr<Impl> impl_;
};

}  // namespace totu::script
