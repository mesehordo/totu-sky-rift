// SPDX-License-Identifier: GPL-3.0-or-later
// Ooni (M7, ADR-008; MECH-LANT-1–3): a népmodul viselkedési próbái (src/sim/factions/ooni.cpp).
#include <cstdio>
#include <string>
#include <vector>

#include "check.hpp"
#include "match_util.hpp"

using namespace totu;
using namespace totu::testing;
using core::Fixed;

namespace {

const sim::Rules& R() { return *content_once().rules; }
uint16_t def_of(const char* id) { return R().def(id); }
Fixed fx(int n, int d) { return Fixed::from_ratio(n, d); }

Match ooni() { return Match("BUILD-ALAP", true, true, {"ooni", "ursan"}); }

sim::EntityId put(sim::Sim& s, const char* id, uint8_t owner, int x10, int y10) {
  const auto e = s.spawn_unit(def_of(id), owner, fx(x10, 10), fx(y10, 10));
  if (auto* c = s.world().get<sim::Combat>(e)) c->order = sim::Combat::Hold;
  return e;
}

bool broken(sim::Sim& s, sim::EntityId e) { return s.mod_speed(e) != Fixed::from_int(1); }

}  // namespace

TEST_CASE("Ooni: a népmodul betöltve") {
  Match m("BUILD-ALAP");
  bool found = false;
  for (const auto& mod : m.sim->modules_) found = found || std::string(mod->name()) == "ooni";
  CHECK(found);
}

TEST_CASE("Ooni harci kedv: sérüléstől fogy, hét társ eleste megtöri (sebzés ×0,7, sebesség ×1,2), töltődve 50-nél helyreáll") {
  Match m = ooni();
  sim::Sim& s = *m.sim;
  // a lámpásfényen (a Nian-csarnok 8 csempéje) kívül
  const auto u = put(s, "oon_oni_harcos", 1, 185, 185);
  const auto foe = put(s, "urs_berserker", 2, 300, 50);
  m.run(1);
  CHECK(!broken(s, u));
  // fél életerőnyi sebzés: 100 → 70 (−0,5 × 60) – még nem tört meg
  const int32_t half = s.world().get<sim::Health>(u)->max / 2;
  s.deal_damage(0, 2, sim::kNoDef, u, int64_t{half} * 1000, false, true);
  m.run(1);
  CHECK(!broken(s, u));
  // hét társ elesik 5 csempén belül: 70 − 7 × 10 = 0 → megtört
  for (int i = 0; i < 7; ++i) {
    const auto a = put(s, "oon_oni_harcos", 1, 175 + i * 3, 175);
    m.run(1);
    s.deal_damage(0, 2, sim::kNoDef, a, 100000000, false, true);
    m.run(1);
  }
  CHECK(broken(s, u));
  CHECK(s.mod_damage(u, foe) == fx(7, 10) && s.mod_speed(u) == fx(6, 5));
  // töltődés 2/s (3 s nyugalom után): 50-ig ~25 s
  m.run(20 * 30);
  CHECK(broken(s, u));
  m.run(10 * 30);
  CHECK(!broken(s, u));
}

TEST_CASE("Ooni: a hős eleste a 8 csempés körben azonnal megtöri a harci kedvet (D21)") {
  Match m = ooni();
  sim::Sim& s = *m.sim;
  const auto u = put(s, "oon_oni_harcos", 1, 185, 185);
  const auto hero = put(s, "oon_hos_akahana", 1, 140, 185);   // 4,5 csempére
  m.run(1);
  s.deal_damage(0, 2, sim::kNoDef, hero, 100000000, false, true);
  m.run(1);
  CHECK(broken(s, u));
}

TEST_CASE("Ooni Taiko-dobos: mellette 10/s-mal töltődik (0 → 50 5 s alatt), és × 1,5-tel épül az épület") {
  Match m = ooni();
  sim::Sim& s = *m.sim;
  const auto u = put(s, "oon_oni_harcos", 1, 185, 185);
  m.run(1);
  s.deal_damage(0, 2, sim::kNoDef, u, int64_t{s.world().get<sim::Health>(u)->max / 2} * 1000, false, true);   // −30
  for (int i = 0; i < 7; ++i) {   // megtörés a társak elestével (−70)
    const auto a = put(s, "oon_oni_harcos", 1, 175 + i * 3, 178);
    m.run(1);
    s.deal_damage(0, 2, sim::kNoDef, a, 100000000, false, true);
    m.run(1);
  }
  m.run(1);
  CHECK(broken(s, u));
  put(s, "oon_dobos", 1, 195, 185);
  m.run(5 * 30 + 15);   // 3 s nyugalomig csak a dob (8/s), utána 10/s: 5,5 s alatt ≥ 50
  CHECK(!broken(s, u));
  // építésgyorsítás
  const auto site = s.place_building(def_of("oon_lakohaz"), 1, 12, 17, false);
  CHECK(s.mod_build(site) == Fixed::from_int(1));
  put(s, "oon_dobos", 1, 130, 175);
  CHECK(s.mod_build(site) == fx(3, 2));
}

TEST_CASE("Ooni lámpásfény: csak fénybe építhető; a fényben a sebesült ooni gyógyul (30 tickenként +2), a harci kedv legalább 30") {
  Match m = ooni();
  sim::Sim& s = *m.sim;
  std::string err;
  CHECK(s.can_place(1, def_of("oon_lakohaz"), 11, 13, &err));   // a Nian-csarnok mellett
  CHECK(!s.can_place(1, def_of("oon_lakohaz"), 20, 2, &err));   // messze, sötétben
  std::printf("  sötétben: %s\n", err.c_str());
  CHECK(s.can_place(1, def_of("urs_hosszuhaz"), 20, 2, &err) || err != "csak lámpásfénybe építhető");   // más népnek nem szab
  const auto u = put(s, "oon_oni_harcos", 1, 110, 150);   // a csarnok fényében
  auto& h = *s.world().get<sim::Health>(u);
  h.hp = h.max - 20;
  m.run(61);
  CHECK(h.hp >= h.max - 16 && h.hp <= h.max - 14);   // két gyógyulás (+2, +2) – a 30. és a 60. tickben
  for (int i = 0; i < 7; ++i) {   // fényben a harci kedv nem esik 30 alá: nem törik meg
    const auto a = put(s, "oon_oni_harcos", 1, 105 + i * 2, 155);
    m.run(1);
    s.deal_damage(0, 2, sim::kNoDef, a, 100000000, false, true);
    m.run(1);
  }
  m.run(16);
  CHECK(!broken(s, u));
}

TEST_CASE("Ooni Védőfény: az 5 csempés körben 50 ideiglenes pajzs 10 s-ig; energia és újratöltés") {
  Match m = ooni();
  sim::Sim& s = *m.sim;
  const auto pap = put(s, "oon_lampaspap", 1, 150, 180);
  const auto ally = put(s, "oon_oni_harcos", 1, 170, 180);
  const auto far = put(s, "oon_oni_harcos", 1, 150, 110);
  s.world().get<sim::Caster>(pap)->energy = 100000;
  const int32_t sh0 = s.world().get<sim::Vital>(ally)->shield;
  CHECK(s.use_ability({pap}, "oon_vedofeny", {}, {}, 0));
  CHECK(s.world().get<sim::Vital>(ally)->shield == sh0 + 50000);
  CHECK(s.world().get<sim::Vital>(far)->shield == s.world().get<sim::Vital>(far)->shield);
  CHECK(s.world().get<sim::Vital>(far)->shield == 0 || s.world().get<sim::Vital>(far)->shield < 50000);
  CHECK(!s.use_ability({pap}, "oon_vedofeny", {}, {}, 0));   // újratöltés
  // a pajzs előbb nyel: 30 sebzés után 20 marad, 10 s múlva elvész
  s.deal_damage(0, 2, sim::kNoDef, ally, 30000, false, true);
  CHECK(s.world().get<sim::Vital>(ally)->shield == sh0 + 20000);
  m.run(10 * 30 + 1);
  CHECK(s.world().get<sim::Vital>(ally)->shield <= sh0);
}

TEST_CASE("Ooni: determinisztikus, és mentésből folytatva ugyanaz") {
  auto setup = [](Match& m) {
    sim::Sim& s = *m.sim;
    for (int i = 0; i < 4; ++i) put(s, "oon_oni_harcos", 1, 170 + i * 4, 180);
    put(s, "oon_dobos", 1, 165, 185);
    const auto pap = put(s, "oon_lampaspap", 1, 160, 180);
    s.world().get<sim::Caster>(pap)->energy = 100000;
    s.use_ability({pap}, "oon_vedofeny", {}, {}, 0);
    for (int i = 0; i < 3; ++i) {
      if (auto* c = s.world().get<sim::Combat>(put(s, "urs_berserker", 2, 200 + i * 5, 175))) c->order = sim::Combat::None;
    }
  };
  Match a = ooni(), b = ooni();
  setup(a);
  setup(b);
  a.run(300);
  b.run(300);
  CHECK(a.sim->hash() == b.sim->hash());
  const auto saved = a.sim->save();
  a.run(400);
  Match c("BUILD-ALAP", true, false, {"ooni", "ursan"});
  CHECK(c.sim->load(saved));
  c.sim->restore_obstacles(*c.map);
  c.run(400);
  CHECK(c.sim->hash() == a.sim->hash());
}
