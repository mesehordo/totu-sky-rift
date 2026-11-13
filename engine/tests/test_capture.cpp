// SPDX-License-Identifier: GPL-3.0-or-later
// Elfoglalás, megszállás, előléptetés (M7 10. lépés; D30, spec/ELFOGLALAS_ES_ELOLEPTETES.md).
#include <cstdio>
#include <cstdlib>
#include <string>

#include "check.hpp"
#include "match_util.hpp"

using namespace totu;
using namespace totu::testing;
using core::Fixed;

namespace {

const sim::Rules& R() { return *content_once().rules; }
uint16_t def_of(const char* id) { return R().def(id); }
Fixed fx(int a, int b) { return Fixed::from_ratio(a, b); }

sim::EntityId put(sim::Sim& s, const char* id, uint8_t owner, int x10, int y10) {
  const auto e = s.spawn_unit(def_of(id), owner, fx(x10, 10), fx(y10, 10));
  s.place_at_free(e, fx(x10, 10), fx(y10, 10));
  return e;
}

sim::Health& hp(sim::Sim& s, sim::EntityId e) { return *s.world().get<sim::Health>(e); }

Match duel() { return Match("BUILD-ALAP", true, true, {"ursan", "wullof"}); }

// a 2. játékos (Wullof) képző épülete a (12,9)-en
sim::EntityId enemy_hall(sim::Sim& s) {
  for (size_t i = 0; i < R().defs.size(); ++i) {
    const auto& d = R().defs[i];
    bool free_unit = false;   // van feltétel nélküli egysége (az elfoglaló képezheti)
    for (const uint16_t u : d.produces) free_unit = free_unit || R().defs[u].reqs.empty();
    if (d.building && d.faction == "wullof" && free_unit && d.fw == 3) {
      return s.place_building(static_cast<uint16_t>(i), 2, 12, 9, true);
    }
  }
  return sim::kNoEntity;
}

}  // namespace

TEST_CASE("elfoglalás: 50% alatti idegen épület, 8 s varázslás, a használó beköltözik; a lista képezhető, a képesség nem") {
  Match m = duel();
  sim::Sim& s = *m.sim;
  const auto b = enemy_hall(s);
  CHECK(b != sim::kNoEntity);
  const auto u = put(s, "urs_telepes", 1, 110, 100);
  CHECK(!s.use_ability({u}, "elfoglalas", {}, {}, b));   // ép épületre nem
  hp(s, b).hp = hp(s, b).max * 4 / 10;
  const auto hq = s.place_building(R().faction("wullof")->hq, 2, 20, 20, true);
  hp(s, hq).hp = 10;
  CHECK(!s.use_ability({u}, "elfoglalas", {}, {}, hq));  // a főépület nem
  CHECK(s.use_ability({u}, "elfoglalas", {}, {}, b));
  m.run(100);
  CHECK(hp(s, b).owner == 2);                            // még varázsol
  m.run(240);
  CHECK(hp(s, b).owner == 1);
  CHECK(!s.world().alive(u));                            // beköltözött
  // a saját listája: az első feltétel nélküli egysége képezhető
  for (int r = 0; r < sim::kResources; ++r) s.add_resources(1, r, 5000);
  bool trained = false;
  std::string err;
  for (const uint16_t ud : R().defs[s.world().get<sim::Building>(b)->def].produces) {
    if (!s.missing_requirements(1, R().defs[ud]) && s.train(1, ud, b, &err)) {
      trained = true;
      break;
    }
    std::printf("  %s: %s\n", R().defs[ud].id.c_str(), err.c_str());
  }
  CHECK(trained);
}

TEST_CASE("elfoglalás: a sebzés megszakítja a varázslást") {
  Match m = duel();
  sim::Sim& s = *m.sim;
  const auto b = enemy_hall(s);
  hp(s, b).hp = hp(s, b).max / 4;
  const auto u = put(s, "urs_telepes", 1, 110, 100);
  CHECK(s.use_ability({u}, "elfoglalas", {}, {}, b));
  m.run(60);
  hp(s, u).hp -= 5;
  m.run(300);
  CHECK(hp(s, b).owner == 2);
  CHECK(s.world().alive(u));
  CHECK(!s.world().get<sim::Seize>(u));
}

TEST_CASE("megszállás: a tulajdonos ott nem képez, a távolharcos megszálló kilő, 50% alatt kiűzik; férőhely a méret szerint") {
  Match m = duel();
  sim::Sim& s = *m.sim;
  const auto b = enemy_hall(s);
  const auto& bd = *s.world().get<sim::Building>(b);
  CHECK(s.occupy_capacity(bd) == 4);   // 3 × 3: közepes
  const auto a1 = put(s, "urs_szigonyvezo", 1, 110, 100);
  const auto a2 = put(s, "urs_szigonyvezo", 1, 110, 120);
  CHECK(s.use_ability({a1, a2}, "megszallas", {}, {}, b));
  m.run(120);
  const auto* oc = s.world().get<sim::Occupants>(b);
  CHECK(oc && oc->units.size() == 2 && oc->owner == 1);
  CHECK(hp(s, b).owner == 2);
  CHECK(s.occupied_against(b, 2));
  for (int r = 0; r < sim::kResources; ++r) s.add_resources(2, r, 5000);
  std::string err;
  const uint16_t ud = R().defs[bd.def].produces.front();
  CHECK(!s.train(2, ud, b, &err));
  // a 2. játékos egysége a közelben: a megszállók lövik
  const auto foe = put(s, "wul_landzsas", 2, 135, 150);
  const int32_t f0 = hp(s, foe).hp;
  m.run(90);
  CHECK(!s.world().alive(foe) || hp(s, foe).hp < f0);
  // 50% alá sérül: kiűzik őket
  hp(s, b).hp = hp(s, b).max * 4 / 10;
  m.run(2);
  CHECK(!s.world().get<sim::Occupants>(b));
  CHECK(!s.world().get<sim::Sheltered>(a1) && !s.world().get<sim::Sheltered>(a2));
}

TEST_CASE("megszállás: ha az épület ledől, a megszállók fél életerővel jönnek ki") {
  Match m = duel();
  sim::Sim& s = *m.sim;
  const auto b = enemy_hall(s);
  const auto a1 = put(s, "urs_szigonyvezo", 1, 110, 100);
  CHECK(s.use_ability({a1}, "megszallas", {}, {}, b));
  m.run(120);
  CHECK(s.world().get<sim::Sheltered>(a1));
  const int32_t h0 = hp(s, a1).hp;
  s.kill(b);
  m.run(2);
  CHECK(s.world().alive(a1) && !s.world().get<sim::Sheltered>(a1));
  std::printf("  %d -> %d\n", h0, hp(s, a1).hp);
  CHECK(hp(s, a1).hp == h0 / 2);
}

TEST_CASE("előléptetés: veterán (2× költség) és elit (5×) – életerő, sebzés, páncél; hős és munkás nem lép elő") {
  Match m = duel();
  sim::Sim& s = *m.sim;
  const auto u = put(s, "urs_berserker", 1, 110, 100);
  const auto& d = R().defs[def_of("urs_berserker")];
  int64_t cost = 0;
  for (const int32_t c : d.cost) cost += c;
  const int32_t max0 = hp(s, u).max;
  s.promote(u, cost * 2000 - 1);
  CHECK(!s.world().get<sim::Veteran>(u) || s.world().get<sim::Veteran>(u)->rank == 0);
  s.promote(u, 1);
  CHECK(s.world().get<sim::Veteran>(u)->rank == 1);
  CHECK(std::abs(hp(s, u).max - max0 * 11 / 10) <= 1);
  const Fixed dmg0 = s.research_stat(1, "Attack.damage", d.attack.damage, d);
  CHECK(s.stat(u, "Attack.damage", d.attack.damage) > dmg0);
  s.promote(u, cost * 3000);
  CHECK(s.world().get<sim::Veteran>(u)->rank == 2);
  CHECK(std::abs(hp(s, u).max - max0 * 12 / 10) <= 1);
  CHECK(s.stat(u, "Health.armor", Fixed::from_int(d.armor)) == Fixed::from_int(d.armor + 1));
  const auto w = m.workers(1).front();
  s.promote(w, 1000000);
  CHECK(!s.world().get<sim::Veteran>(w));
}

TEST_CASE("elfoglalás és megszállás: determinisztikus, és mentésből folytatva ugyanaz") {
  auto setup = [](Match& m) {
    sim::Sim& s = *m.sim;
    const auto b = enemy_hall(s);
    hp(s, b).hp = hp(s, b).max / 3;
    const auto u = put(s, "urs_telepes", 1, 110, 100);
    const auto a1 = put(s, "urs_szigonyvezo", 1, 110, 120);
    s.use_ability({u}, "elfoglalas", {}, {}, b);
    const auto b2 = s.place_building(def_of("wul_tipi"), 2, 16, 13, true);
    s.use_ability({a1}, "megszallas", {}, {}, b2);
    put(s, "wul_landzsas", 2, 190, 150);
  };
  Match a = duel(), b = duel();
  setup(a);
  setup(b);
  a.run(150);
  b.run(150);
  CHECK(a.sim->hash() == b.sim->hash());
  const auto saved = a.sim->save();
  a.run(400);
  Match c("BUILD-ALAP", true, false, {"ursan", "wullof"});
  CHECK(c.sim->load(saved));
  c.sim->restore_obstacles(*c.map);
  c.run(400);
  CHECK(c.sim->hash() == a.sim->hash());
}
