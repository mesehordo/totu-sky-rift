// SPDX-License-Identifier: GPL-3.0-or-later
// Huk’ucha (M7, ADR-008; MECH-POND-1–5): a népmodul viselkedési próbái (src/sim/factions/hukucha.cpp).
#include <cstdio>
#include <string>

#include "check.hpp"
#include "match_util.hpp"

using namespace totu;
using namespace totu::testing;
using core::Fixed;

namespace {

const sim::Rules& R() { return *content_once().rules; }
uint16_t def_of(const char* id) { return R().def(id); }
Fixed fx(int n, int d) { return Fixed::from_ratio(n, d); }

Match huk() { return Match("BUILD-ALAP", true, true, {"hukucha", "ursan"}); }

int count(sim::Sim& s, const char* id, uint8_t owner) {
  int n = 0;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit& u) {
    const auto* h = s.world().get<sim::Health>(e);
    n += u.def == def_of(id) && h->owner == owner && h->hp > 0 ? 1 : 0;
  });
  return n;
}

}  // namespace

TEST_CASE("Huk: a népmodul betöltve") {
  Match m = huk();
  bool found = false;
  for (const auto& mod : m.sim->modules_) found = found || std::string(mod->name()) == "hukucha";
  CHECK(found);
}

TEST_CASE("Huk: a főépület tavon áll (2 csempés perem); épület csak saját tóra; a tavacska tóvá válik") {
  Match m = huk();
  sim::Sim& s = *m.sim;
  const uint16_t house = def_of("huk_gyongybuborek");   // 2 × 2
  std::string err;
  CHECK(s.can_place(1, house, 9, 12, &err));             // a főépület (5–8 × 11–14) peremén
  CHECK(!s.can_place(1, house, 15, 13, &err));           // tó nélkül nem
  std::printf("  tó nélkül: %s\n", err.c_str());
  CHECK(s.can_place(2, def_of("urs_hosszuhaz"), 15, 13, &err));   // a más népet nem köti
  // tavacska: a Tóásó kiássa (4 s), utána tó, és az épület eltűnik
  for (int r = 0; r < sim::kResources; ++r) s.add_resources(1, r, 1000);
  const int32_t wood = s.player(1).resource(sim::kWood);
  CHECK(s.order_build(m.workers(1).front(), def_of("huk_tavacska"), 15, 13, &err));
  CHECK(s.player(1).resource(sim::kWood) == wood - 15);
  m.run(400);
  int left = 0;
  s.world().pool<sim::Building>().each([&](sim::EntityId, const sim::Building& b) { left += b.def == def_of("huk_tavacska"); });
  CHECK(left == 0);
  CHECK(s.can_place(1, house, 15, 13, &err));
  CHECK(!s.can_place(1, def_of("huk_tavacska"), 15, 13, &err));   // már tó
}

TEST_CASE("Huk: tavon a saját nép ×1,3, más ×0,7; a tóval körülvett épület 1,6-szor gyorsabban épül; Gyöngyház-pajzs") {
  Match m = huk();
  sim::Sim& s = *m.sim;
  const auto frog = s.spawn_unit(def_of("huk_csuzlis"), 1, fx(95, 10), fx(155, 10));
  const auto bear = s.spawn_unit(def_of("urs_berserker"), 2, fx(105, 10), fx(165, 10));
  const auto dry = s.spawn_unit(def_of("huk_csuzlis"), 1, fx(200, 10), fx(185, 10));
  CHECK(s.mod_speed(frog) == fx(13, 10));
  CHECK(s.mod_speed(bear) == fx(7, 10));
  CHECK(s.mod_speed(dry) == Fixed::from_int(1));
  // a főépület peremén álló 2 × 2-es épület kerete részben tó: 1 + 0,6 × arány
  const auto b = s.place_building(def_of("huk_gyongybuborek"), 1, 9, 15, false);
  const Fixed mb = s.mod_build(b);
  std::printf("  építésgyorsítás a peremen: ×%.3f\n", mb.to_double());
  CHECK(mb > Fixed::from_int(1) && mb <= fx(16, 10));
  // Gyöngyház-pajzs: csak saját tavon töltődik
  const auto shell = s.spawn_unit(def_of("huk_gyongyveto"), 1, fx(95, 10), fx(165, 10));
  const auto shell_dry = s.spawn_unit(def_of("huk_gyongyveto"), 1, fx(200, 10), fx(175, 10));
  CHECK(s.mod_shield_ok(shell) && !s.mod_shield_ok(shell_dry));
}

TEST_CASE("Huk: ikra-akna – a pontra rakva, ellenség érkezésekor ebihalak kelnek ki (tavon 5, máshol 3), az akna elfogy") {
  Match m = huk();
  sim::Sim& s = *m.sim;
  const auto layer = s.spawn_unit(def_of("huk_ikrarako"), 1, fx(95, 10), fx(165, 10));
  s.world().get<sim::Caster>(layer)->energy = 100000;
  CHECK(s.use_ability({layer}, "ikrarakas", fx(95, 10), fx(155, 10), 0));   // saját tóra
  CHECK(count(s, "huk_ikra", 1) == 1);
  CHECK(!s.use_ability({layer}, "ikrarakas", fx(95, 10), fx(155, 10), 0));  // újratöltés
  m.run(10);
  CHECK(count(s, "huk_ikra", 1) == 1);   // nincs ellenség: vár
  sim::EntityId mine = sim::kNoEntity;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit& u) { if (u.def == def_of("huk_ikra")) mine = e; });
  const auto& mp = *s.world().get<sim::Position>(mine);
  const auto foe = s.spawn_unit(def_of("urs_berserker"), 2, mp.x + fx(5, 10), mp.y);
  s.world().get<sim::Combat>(foe)->order = sim::Combat::Hold;
  m.run(10);
  CHECK(count(s, "huk_ikra", 1) == 0);
  std::printf("  kikelt ebihal: %d\n", count(s, "huk_ebihal", 1));
  CHECK(count(s, "huk_ebihal", 1) == 5);
  m.run(11 * 30);
  CHECK(count(s, "huk_ebihal", 1) == 0);   // élettartam
}

TEST_CASE("Huk: determinisztikus, és mentésből folytatva ugyanaz (a tótérkép is)") {
  auto setup = [](Match& m) {
    sim::Sim& s = *m.sim;
    for (int r = 0; r < sim::kResources; ++r) s.add_resources(1, r, 1000);
    std::string err;
    s.order_build(m.workers(1).front(), def_of("huk_tavacska"), 15, 13, &err);
    const auto layer = s.spawn_unit(def_of("huk_ikrarako"), 1, fx(95, 10), fx(165, 10));
    s.world().get<sim::Caster>(layer)->energy = 100000;
    s.use_ability({layer}, "ikrarakas", fx(95, 10), fx(155, 10), 0);
    s.spawn_unit(def_of("urs_berserker"), 2, fx(130, 10), fx(156, 10));
  };
  Match a = huk(), b = huk();
  setup(a);
  setup(b);
  a.run(200);
  b.run(200);
  CHECK(a.sim->hash() == b.sim->hash());
  const auto saved = a.sim->save();
  a.run(400);
  Match c("BUILD-ALAP", true, false, {"hukucha", "ursan"});
  CHECK(c.sim->load(saved));
  c.sim->restore_obstacles(*c.map);
  c.run(400);
  CHECK(c.sim->hash() == a.sim->hash());
  std::string err;
  CHECK(c.sim->can_place(1, def_of("huk_gyongybuborek"), 15, 13, &err));   // a tótérkép a mentésben
}
