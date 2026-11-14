// SPDX-License-Identifier: GPL-3.0-or-later
// Ooni (M7, ADR-008; MECH-LANT-1–3): a népmodul viselkedési próbái (src/sim/factions/ooni.cpp).
#include <algorithm>
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

namespace {

void learn(sim::Sim& s, uint8_t owner, const char* id) {   // a kutatás kész (a szabályai a nyers sablonból)
  auto& r = s.player_mut(owner).researched;
  r.push_back(id);
  std::sort(r.begin(), r.end());
}

Fixed dist(sim::Sim& s, sim::EntityId a, sim::EntityId b) {
  const auto& pa = *s.world().get<sim::Position>(a);
  const auto& pb = *s.world().get<sim::Position>(b);
  return core::length(pa.x - pb.x, pa.y - pb.y);
}

sim::EntityId hq(sim::Sim& s, uint8_t owner) {
  sim::EntityId out = 0;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& b) {
    if (!out && b.def == def_of("oon_nian_csarnok") && s.world().get<sim::Health>(e)->owner == owner) out = e;
  });
  return out;
}

bool torii(sim::Sim& s, sim::EntityId u, sim::EntityId gate) {
  sim::Command c;
  c.kind = sim::Command::Kind::Module;
  c.def = "ooni.torii";
  c.owner = 1;
  c.units = {u};
  c.target = gate;
  return s.mod_command(c);
}

}  // namespace

TEST_CASE("Ooni Torii-kapupár: belépés, 2 s várakozás rejtve, átkelés a párhoz; utána 8 s töltés; a pár elestével kijönnek") {
  Match m = ooni();
  sim::Sim& s = *m.sim;
  const auto a = s.place_building(def_of("oon_torii"), 1, 12, 16, true);
  const auto b = s.place_building(def_of("oon_torii"), 1, 26, 15, true);
  const auto u1 = put(s, "oon_oni_harcos", 1, 130, 190);
  const auto u2 = put(s, "oon_oni_harcos", 1, 140, 190);
  CHECK(torii(s, u1, a));
  m.run(2);
  CHECK(s.mod_hidden(u1));   // a kapuban vár
  m.run(60);
  CHECK(!s.mod_hidden(u1));
  CHECK(dist(s, u1, b) < Fixed::from_int(3));
  // a kapu most tölt: a második egység 2 s után sem kel át, csak a 8 s lejártával
  CHECK(torii(s, u2, a));
  m.run(62);
  CHECK(dist(s, u2, a) < Fixed::from_int(3) && s.mod_hidden(u2));
  m.run(240 - 62);
  CHECK(dist(s, u2, b) < Fixed::from_int(3));
  // a pár elpusztul: a várakozó kijön
  const auto u3 = put(s, "oon_oni_harcos", 1, 130, 190);
  CHECK(torii(s, u3, a));
  m.run(2);
  CHECK(s.mod_hidden(u3));
  s.deal_damage(0, 2, sim::kNoDef, b, 100000000, false, true);
  m.run(3);
  CHECK(!s.mod_hidden(u3) && dist(s, u3, a) < Fixed::from_int(3));
}

TEST_CASE("Ooni Torii lerakás: a pár második kapuja csak látott helyre (fény nélkül is); legfeljebb `torii_pairs` pár (Kettős kapu: 2)") {
  Match m = ooni();
  sim::Sim& s = *m.sim;
  std::string err;
  s.place_building(def_of("oon_torii"), 1, 12, 16, true);
  m.run(7);   // a látótér 6 tickenként frissül
  CHECK(!s.can_place(1, def_of("oon_torii"), 23, 15, &err));   // sötét, nem látott
  std::printf("  nem látott: %s\n", err.c_str());
  put(s, "oon_kohajito", 1, 265, 160);
  m.run(7);
  CHECK(s.can_place(1, def_of("oon_torii"), 23, 15, &err));    // látott: fény nélkül is
  s.place_building(def_of("oon_torii"), 1, 23, 15, true);
  CHECK(!s.can_place(1, def_of("oon_torii"), 11, 11, &err));   // egy pár a határ
  std::printf("  harmadik kapu: %s\n", err.c_str());
  learn(s, 1, "oon_kettos_kapu");
  CHECK(s.can_place(1, def_of("oon_torii"), 11, 11, &err));
}

TEST_CASE("Ooni őrök: a Kőkutya (hűséggel +2) és a bekapcsolt Kapuőrzés (+3) páncélt ad a 2,5 csempén belüli épületnek, legfeljebb 6") {
  Match m = ooni();
  sim::Sim& s = *m.sim;
  const auto b = s.place_building(def_of("oon_lakohaz"), 1, 12, 16, true);
  const auto& bp = *s.world().get<sim::Position>(b);
  const int bx = (bp.x * Fixed::from_int(10)).floor_int(), by = (bp.y * Fixed::from_int(10)).floor_int();
  const auto dog = put(s, "oon_kokutya", 1, bx + 15, by);
  m.run(31);
  CHECK(s.mod_armor(b) == 0);   // hűség nélkül
  learn(s, 1, "oon_kokutya_huseg");
  m.run(30);
  CHECK(s.mod_armor(b) == 2);
  CHECK(s.use_ability({dog}, "oon_kapuorzes", {}, {}, 0));
  m.run(30);
  CHECK(s.mod_armor(b) == 5);
  const auto dog2 = put(s, "oon_kokutya", 1, bx - 15, by);
  CHECK(s.use_ability({dog2}, "oon_kapuorzes", {}, {}, 0));
  m.run(30);
  CHECK(s.mod_armor(b) == 6);   // 10 helyett a határ
  const auto far = s.place_building(def_of("oon_lakohaz"), 1, 4, 16, true);
  m.run(30);
  CHECK(s.mod_armor(far) == 0);
}

TEST_CASE("Ooni kutatások: Lámpásmágia (fényben varázssebzés ×1,2), Messzefény (+2 fénysugár)") {
  Match m = ooni();
  sim::Sim& s = *m.sim;
  const auto pap = put(s, "oon_lampaspap", 1, 110, 150);   // a csarnok fényében
  const auto dark = put(s, "oon_lampaspap", 1, 290, 185);
  const auto foe = put(s, "urs_berserker", 2, 300, 50);
  m.run(1);
  CHECK(s.mod_damage_taken(pap, foe, false, true) == Fixed::from_int(1));
  learn(s, 1, "oon_lampas_magia");
  const Fixed lm = s.mod_damage_taken(pap, foe, false, true);
  CHECK(lm.raw() >= fx(6, 5).raw() - 1 && lm.raw() <= fx(6, 5).raw() + 1);
  CHECK(s.mod_damage_taken(pap, foe, false, false) == Fixed::from_int(1));   // nem varázs
  CHECK(s.mod_damage_taken(dark, foe, false, true) == Fixed::from_int(1));   // sötétben
  // Messzefény: a 8-as kör szélén túl (9 csempe) csak a kutatással gyógyul
  const auto& hp = *s.world().get<sim::Position>(hq(s, 1));
  const auto edge = s.spawn_unit(def_of("oon_oni_harcos"), 1, hp.x + Fixed::from_int(9), hp.y);
  if (auto* c = s.world().get<sim::Combat>(edge)) c->order = sim::Combat::Hold;
  auto& eh = *s.world().get<sim::Health>(edge);
  eh.hp = eh.max - 20;
  m.run(31);
  CHECK(eh.hp == eh.max - 20);
  learn(s, 1, "oon_messzefeny");
  m.run(30);
  CHECK(eh.hp == eh.max - 18);
}

TEST_CASE("Ooni Harci dobok: a sérülés harci kedv-vesztesége × 0,6 – fél életerő és hét társ eleste után sem törik meg") {
  Match m = ooni();
  sim::Sim& s = *m.sim;
  learn(s, 1, "oon_harci_dobok");
  const auto u = put(s, "oon_oni_harcos", 1, 185, 185);
  m.run(1);
  s.deal_damage(0, 2, sim::kNoDef, u, int64_t{s.world().get<sim::Health>(u)->max / 2} * 1000, false, true);   // −18
  for (int i = 0; i < 7; ++i) {   // −70: 100 − 18 − 70 = 12
    const auto a = put(s, "oon_oni_harcos", 1, 175 + i * 3, 175);
    m.run(1);
    s.deal_damage(0, 2, sim::kNoDef, a, 100000000, false, true);
    m.run(1);
  }
  CHECK(!broken(s, u));
}

TEST_CASE("Ooni Tanuki tárgylelet: a bányászott rakományok némelyikénél tárgy kerül a földre (2%)") {
  Match m = ooni();
  sim::Sim& s = *m.sim;
  sim::EntityId node = 0;
  s.world().pool<sim::ResourceNode>().each([&](sim::EntityId e, const sim::ResourceNode&) { if (!node) node = e; });
  for (int i = 0; i < 16; ++i) {
    const auto t = s.spawn_unit(def_of("oon_tanuki"), 1, fx(80 + (i % 4) * 6, 10), fx(60 + (i / 4) * 6, 10));
    s.order_gather(t, node);
  }
  m.run(30 * 240);
  int items = 0;
  s.world().pool<sim::GroundItem>().each([&](sim::EntityId, const sim::GroundItem&) { ++items; });
  std::printf("  lelet: %d tárgy, %lld kő\n", items, static_cast<long long>(s.player(1).gathered[sim::kStone]));
  CHECK(items >= 1);
}

TEST_CASE("Ooni Vörös dojó irány: a Taiko-dobos építésgyorsítása × 1,75 (`build_boost_mult`); Vérszomj: ölésenként +20 életerő") {
  Match m = ooni();
  sim::Sim& s = *m.sim;
  const auto site = s.place_building(def_of("oon_lakohaz"), 1, 12, 17, false);
  put(s, "oon_dobos", 1, 130, 175);
  CHECK(s.mod_build(site) == fx(3, 2));
  s.player_mut(1).path = "voros";
  CHECK(s.mod_build(site) == fx(7, 4));
  // Vérszomj: az oni harcos ölése visszaad 20 életerőt (kutatás nélkül nem); a természetes gyógyulás mellett a különbség
  const auto oni = put(s, "oon_oni_harcos", 1, 185, 185);
  auto& h = *s.world().get<sim::Health>(oni);
  auto gain = [&]() {
    h.hp = h.max - 50;
    const int32_t before = h.hp;
    const auto v = put(s, "urs_telepes", 2, 190, 185);
    s.deal_damage(oni, 1, sim::kNoDef, v, 100000000, false, true);
    m.run(1);
    return h.hp - before;
  };
  const int32_t plain = gain();
  auto& r = s.player_mut(1).researched;
  r.insert(std::lower_bound(r.begin(), r.end(), std::string("oon_verszomj")), "oon_verszomj");
  const int32_t thirst = gain();
  std::printf("  ölés után: kutatás nélkül %+d, Vérszomjjal %+d\n", plain, thirst);
  CHECK(thirst - plain == 20);
}
