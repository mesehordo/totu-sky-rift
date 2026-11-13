// SPDX-License-Identifier: GPL-3.0-or-later
// Elemek és a közös képességkeret (M7 1. lépés; ADR-008, D22, D43): a 13 elem, a mérgező és lassító fegyver, az ugrás,
// a főépület-képességek (terület, idézés, szél), az ének csak auraként.
#include <algorithm>
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
Fixed fx(int n, int d) { return Fixed::from_ratio(n, d); }

sim::EntityId put(sim::Sim& s, const char* id, uint8_t owner, int x10, int y10) {
  const auto e = s.spawn_unit(def_of(id), owner, fx(x10, 10), fx(y10, 10));
  if (auto* c = s.world().get<sim::Combat>(e)) c->order = sim::Combat::Hold;
  return e;
}

int64_t taken(sim::Sim& s, sim::EntityId e) {
  if (!s.world().alive(e)) return 1000000000;   // elpusztult
  const auto* h = s.world().get<sim::Health>(e);
  return int64_t{h->max - h->hp} * 1000 + s.world().get<sim::Vital>(e)->deficit;
}

sim::Afflict& af(sim::Sim& s, sim::EntityId e) { return *s.world().get<sim::Afflict>(e); }

}  // namespace

TEST_CASE("elemek: tűz (égés, víz eloltja), méreg (gépen nem), jég (kioltja az égést)") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto t = put(s, "urs_mammut", 2, 40, 180);   // 650 életerő, 3 páncél
  s.deal_damage(0, 1, sim::kNoDef, t, 40000, false, true, sim::Element::Fire);   // 40 varázs → égés 10/s
  CHECK(af(s, t).burn_dps == 10000);
  const int64_t before = taken(s, t);
  m.run(30);
  CHECK(taken(s, t) - before >= 9900 && taken(s, t) - before <= 10100);   // 1 s égés, páncélon át
  s.deal_damage(0, 1, sim::kNoDef, t, 1000, false, true, sim::Element::Ice);
  CHECK(af(s, t).burn_until == 0);   // a jég kioltja
  s.deal_damage(0, 1, sim::kNoDef, t, 1000, false, true, sim::Element::Water);
  s.deal_damage(0, 1, sim::kNoDef, t, 40000, false, true, sim::Element::Fire);
  CHECK(af(s, t).burn_until <= s.tick() && af(s, t).wet_until <= s.tick());   // vizesen nem gyullad meg, felszárad
  s.deal_damage(0, 1, sim::kNoDef, t, 40000, false, true, sim::Element::Poison);   // 40 × 0,15 = 6/s
  CHECK(af(s, t).poison_dps >= 5998 && af(s, t).poison_dps <= 6000);
  const auto golem = put(s, "urs_runaorias", 2, 80, 180);   // gép
  s.deal_damage(0, 1, sim::kNoDef, golem, 40000, false, true, sim::Element::Poison);
  CHECK(af(s, golem).poison_until == 0);
}

TEST_CASE("elemek: villám (rövid kábulás, 3 s szünet; vízben kétszer hosszabb és szünet nélkül), gyökér, alvás") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto t = put(s, "urs_berserker", 2, 40, 180);
  s.deal_damage(0, 1, sim::kNoDef, t, 1000, false, true, sim::Element::Lightning);
  CHECK(af(s, t).stun_until == s.tick() + R().element_fx.shock_ticks);
  m.run(15);
  s.deal_damage(0, 1, sim::kNoDef, t, 1000, false, true, sim::Element::Lightning);
  CHECK(af(s, t).stun_until < s.tick());   // szünet
  s.deal_damage(0, 1, sim::kNoDef, t, 1000, false, true, sim::Element::Water);
  s.deal_damage(0, 1, sim::kNoDef, t, 1000, false, true, sim::Element::Lightning);
  CHECK(af(s, t).stun_until == s.tick() + R().element_fx.shock_ticks * 2);   // vízben
  // gyökér: nem mozdul, de üthet; alvás: az első sebzésre felébred; a hős csak egy pillanatra kábul
  const auto r = put(s, "urs_berserker", 2, 80, 180);
  s.deal_damage(0, 1, sim::kNoDef, r, 1000, false, true, sim::Element::Root);
  CHECK(s.unit_speed_mult(r).raw() == 0 && !s.world().get<sim::Afflict>(r)->stunned(s.tick()));
  const auto z = put(s, "urs_berserker", 2, 120, 180);
  s.deal_damage(0, 1, sim::kNoDef, z, 0, false, true, sim::Element::Sleep);
  CHECK(af(s, z).stunned(s.tick()));
  s.deal_damage(0, 1, sim::kNoDef, z, 1000, false, true);
  CHECK(!af(s, z).stunned(s.tick()));
  const auto hero = put(s, "urs_hos_ragna", 2, 160, 180);
  s.deal_damage(0, 1, sim::kNoDef, hero, 0, false, true, sim::Element::Sleep);
  CHECK(af(s, hero).sleep_until == 0 && af(s, hero).stun_until == s.tick() + 30);
}

TEST_CASE("elemek: jel (+20% sebzés), páncéltörés (−2 páncél, épületen is), vakság (a lövés fele mellé), némítás") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto t = put(s, "urs_berserker", 2, 40, 180);
  s.deal_damage(0, 1, sim::kNoDef, t, 1000, false, true, sim::Element::Mark);
  int64_t b = taken(s, t);
  s.deal_damage(0, 1, sim::kNoDef, t, 10000, false, true);
  CHECK(taken(s, t) - b >= 11990 && taken(s, t) - b <= 12010);
  s.deal_damage(0, 1, sim::kNoDef, t, 0, false, true, sim::Element::Sunder);
  b = taken(s, t);
  s.deal_damage(0, 1, def_of("urs_berserker"), t, 10000, false, false);   // fizikai: 10 × 1,2 − (1 − 2)
  CHECK(taken(s, t) - b >= 12990 && taken(s, t) - b <= 13010);
  const auto house = s.place_building(def_of("urs_hosszuhaz"), 2, 12, 17, true);   // 3 páncél
  s.deal_damage(0, 1, sim::kNoDef, house, 0, false, true, sim::Element::Sunder);
  b = taken(s, house);
  s.deal_damage(0, 1, def_of("urs_berserker"), house, 10000, false, false);
  CHECK(taken(s, house) - b == 9000);   // 10 − (3 − 2)
  // vakság: a szigonyvető lövéseinek kb. fele mellé
  const auto sp = put(s, "urs_szigonyvezo", 1, 30, 190);
  s.deal_damage(0, 2, sim::kNoDef, sp, 0, false, true, sim::Element::Blind);
  int hits = 0;
  for (int i = 0; i < 200; ++i) {
    af(s, sp).blind_until = s.tick() + 100;
    const int64_t bb = taken(s, t);
    s.deal_damage(sp, 1, def_of("urs_szigonyvezo"), t, 1000, true, false);
    if (taken(s, t) != bb) ++hits;
    s.world().get<sim::Health>(t)->hp = s.world().get<sim::Health>(t)->max;
  }
  std::printf("  vakon: 200 lövésből %d talált\n", hits);
  CHECK(hits > 70 && hits < 130);
  // némítás: a képesség nem használható, a bekapcsolt kapcsoló kikapcsol
  const auto sh = put(s, "urs_pajzshordozo", 1, 60, 190);
  CHECK(s.use_ability({sh}, "pajzsfal", {}, {}, 0));
  s.deal_damage(0, 2, sim::kNoDef, sh, 0, false, true, sim::Element::Silence);
  CHECK(!s.toggled(sh, static_cast<uint16_t>(R().ability_index("pajzsfal"))));
  const auto zk = put(s, "urs_berserker", 1, 70, 190);
  s.deal_damage(0, 2, sim::kNoDef, zk, 0, false, true, sim::Element::Silence);
  CHECK(!s.use_ability({zk}, "verszomj", {}, {}, 0));
}

TEST_CASE("fegyver-elemek: mérgező és lassító fegyver; az elem a hozzárendelés szerint (a kézi felülírással)") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto t = put(s, "urs_berserker", 2, 40, 180);
  s.deal_damage(0, 1, def_of("dar_fullanclandzsas"), t, 9000, false, false);
  CHECK(af(s, t).poison_dps == 3000 && af(s, t).poison_until == s.tick() + 120);
  s.deal_damage(0, 1, def_of("tok_fagyorias"), t, 1000, false, false);
  std::printf("  lassítás: %.3f (sebesség-szorzó)\n", s.unit_speed_mult(t).to_double());
  // a Fagyóriás fegyvere jég elemű is: a jég 0,6-os lassítása erősebb a fegyver 0,7-esénél (a kisebb marad)
  const bool ice = R().defs[def_of("tok_fagyorias")].attack.element == sim::Element::Ice;
  CHECK(std::abs(int64_t{s.unit_speed_mult(t).raw()} - (ice ? R().element_fx.ice_slow : fx(7, 10)).raw()) <= 2);
  CHECK(R().defs[def_of("elf_napszoro")].attack.element == sim::Element::Fire);
  CHECK(R().abilities[static_cast<size_t>(R().ability_index("h_bja_szigony"))].element == sim::Element::Mark);   // kézi
}

TEST_CASE("ének csak auraként (D43): a kürtös maga is ×1,15, nem ×1,3225") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto horn = put(s, "urs_kurtos", 1, 50, 180);
  CHECK(s.use_ability({horn}, "harci_kurt", {}, {}, 0));
  m.run(1);
  const Fixed dm = s.unit_mods(horn).damage_mult;
  CHECK(std::abs(int64_t{dm.raw()} - fx(115, 100).raw()) <= 2);
}

TEST_CASE("ugrás (blink): a pont felé legfeljebb a hatótávig, szabad helyre; a landolás megsebzi a közelieket") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  sim::EntityId jumper = sim::kNoEntity;   // az első egység, amelyiknek a Békaugrás a képessége
  const auto bek = static_cast<uint16_t>(R().ability_index("bekaugras"));
  for (size_t i = 0; i < R().defs.size() && jumper == sim::kNoEntity; ++i) {
    const auto& d = R().defs[i];
    if (std::find(d.abilities.begin(), d.abilities.end(), bek) != d.abilities.end()) {
      jumper = s.spawn_unit(static_cast<uint16_t>(i), 1, fx(20, 10), fx(180, 10));
    }
  }
  CHECK(jumper != sim::kNoEntity);
  const auto foe = put(s, "urs_berserker", 2, 50, 180);
  CHECK(s.use_ability({jumper}, "bekaugras", fx(60, 10), fx(180, 10), 0));
  const auto& p = *s.world().get<sim::Position>(jumper);
  std::printf("  ugrás: 2,0 → %.2f (hatótáv 2,5)\n", p.x.to_double());
  CHECK(p.x <= fx(48, 10) && p.x >= fx(40, 10));   // a hatótáv + a navcella közepe
  CHECK(taken(s, foe) > 0);   // landolási sebzés
  CHECK(!s.use_ability({jumper}, "bekaugras", fx(60, 10), fx(180, 10), 0));   // újratöltés
}

TEST_CASE("főépület-képességek: terület (Ordan villáma, épületet is), idézés (Ősök hívása), szél; költség és célpont") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto forum = s.place_building(def_of("imp_forum"), 1, 2, 16, true);
  const auto foe = put(s, "urs_berserker", 2, 60, 190);
  const auto house = s.place_building(def_of("urs_hosszuhaz"), 2, 8, 17, true);
  m.run(6);
  CHECK(!s.use_power(1, forum, "gp_ordan_villama", fx(60, 10), fx(190, 10)));   // 100 meteorit kell
  s.add_resources(1, sim::kMeteor, 100);
  CHECK(s.use_power(1, forum, "gp_ordan_villama", fx(60, 10), fx(190, 10)));
  CHECK(s.player(1).resource(sim::kMeteor) == 0);
  m.run(31);
  CHECK(taken(s, foe) > 0 && taken(s, house) > 0);
  // idézés: 3 szellemfarkas 40 s-ra
  const auto tanacs = s.place_building(def_of("wul_tanacssator"), 1, 12, 9, true);
  m.run(6);
  int before = 0;
  s.world().pool<sim::Unit>().each([&](sim::EntityId, const sim::Unit& u) { before += u.def == def_of("wul_szellemfarkas") ? 1 : 0; });
  CHECK(s.use_power(1, tanacs, "gp_osok_hivasa", fx(130, 10), fx(130, 10)));
  int after = 0;
  s.world().pool<sim::Unit>().each([&](sim::EntityId, const sim::Unit& u) { after += u.def == def_of("wul_szellemfarkas") ? 1 : 0; });
  CHECK(after == before + 3);
  // szél: az ellenség lassul, a saját egység önerősítést kap
  const auto foe2 = put(s, "urs_berserker", 2, 61, 189);   // az előzőt a villám megölhette
  const auto nian = s.place_building(def_of("oon_nian_csarnok"), 1, 16, 9, true);
  const auto mine = put(s, "urs_berserker", 1, 55, 185);
  m.run(6);
  CHECK(s.use_power(1, nian, "gp_isteni_szel", fx(58, 10), fx(188, 10)));
  CHECK(s.unit_speed_mult(foe2) == fx(1, 2));
  CHECK(s.buffed(mine, static_cast<uint16_t>(R().ability_index("gp_isteni_szel_buff"))));
}
