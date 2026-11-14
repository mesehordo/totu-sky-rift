// SPDX-License-Identifier: GPL-3.0-or-later
// Termelés (M6, 4. lépés; ADR-007): sor, költség és létszám foglalása, a kész egység helye, sorhossz, visszavonás,
// létszámkorlát, gyülekezőpont (pont és lelőhely), a legrövidebb sor, kutatás, hős.
#include <cstdio>
#include <string>
#include <utility>

#include "check.hpp"
#include "match_util.hpp"

using namespace totu;
using namespace totu::testing;
using core::Fixed;

namespace {

uint16_t def_of(const char* id) { return content_once().rules->def(id); }

sim::EntityId hq_of(sim::Sim& s, uint8_t owner) {
  sim::EntityId out = sim::kNoEntity;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building&) {
    if (out == sim::kNoEntity && s.world().get<sim::Health>(e)->owner == owner) out = e;
  });
  return out;
}

int count_units(sim::Sim& s, uint8_t owner, uint16_t def) {
  int n = 0;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit& u) {
    if (u.def == def && s.world().get<sim::Health>(e)->owner == owner) ++n;
  });
  return n;
}

sim::EntityId newest_unit(sim::Sim& s, uint8_t owner, uint16_t def) {
  sim::EntityId out = sim::kNoEntity;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit& u) {
    if (u.def == def && s.world().get<sim::Health>(e)->owner == owner && (out == sim::kNoEntity || e > out)) out = e;
  });
  return out;
}

}  // namespace

TEST_CASE("termelés: telepes – fizetés, létszám azonnal, 12 s múlva a főépület alatt jelenik meg") {
  Match m("BUILD-ALAP");
  CHECK(m.ok);
  if (!m.ok) return;
  sim::Sim& s = *m.sim;
  const uint16_t settler = def_of("urs_telepes");
  const sim::EntityId hq = hq_of(s, 1);
  const int64_t wood = s.player(1).res[sim::kWood], stone = s.player(1).res[sim::kGold];
  std::string why;
  CHECK(s.train(1, settler, 0, &why));
  CHECK(s.player(1).res[sim::kGold] == stone - 20000 && s.player(1).res[sim::kWood] <= wood - 30000 + 8000);
  CHECK(s.player(1).supply_used == 6);
  m.run(359);
  CHECK(count_units(s, 1, settler) == 5);
  m.run(1);
  CHECK(count_units(s, 1, settler) == 6);
  CHECK(s.player(1).supply_used == 6);
  const auto& p = *s.world().get<sim::Position>(newest_unit(s, 1, settler));
  const auto& b = *s.world().get<sim::Building>(hq);
  CHECK(p.y > Fixed::from_int(b.ty + b.h));   // gyülekezőpont nélkül alul
  CHECK(s.world().get<sim::Producer>(hq)->queue.empty());
}

TEST_CASE("termelés: a sor legfeljebb 5, visszavonás teljes visszatérítéssel, létszámkorlát") {
  Match m("BUILD-ALAP");
  CHECK(m.ok);
  if (!m.ok) return;
  sim::Sim& s = *m.sim;
  const uint16_t settler = def_of("urs_telepes");
  const sim::EntityId hq = hq_of(s, 1);
  s.add_resources(1, sim::kWood, 2000);
  s.add_resources(1, sim::kGold, 2000);
  std::string why;
  for (int i = 0; i < 5; ++i) CHECK(s.train(1, settler, 0, &why));
  CHECK(!s.train(1, settler, 0, &why) && why == "a gyártósor megtelt");
  CHECK(s.player(1).supply_used == 10);
  const int64_t wood = s.player(1).res[sim::kWood];
  sim::Command c;
  c.kind = sim::Command::Kind::CancelQueue;
  c.target = hq;
  c.rx = 2;
  s.queue(std::move(c));
  m.run(1);
  CHECK(s.world().get<sim::Producer>(hq)->queue.size() == 4);
  CHECK(s.player(1).res[sim::kWood] - wood >= 30000 && s.player(1).supply_used == 9);
  // létszámkorlát: 12, ebből 9 foglalt; a harcosok háza pajzshordozója 2 létszám
  const sim::EntityId barracks = s.place_building(def_of("urs_harcosok_haza"), 1, 12, 14, true);
  CHECK(s.train(1, def_of("urs_pajzshordozo"), 0, &why));    // 11
  CHECK(!s.train(1, def_of("urs_pajzshordozo"), 0, &why) && why == "elérted a létszámkorlátot");
  CHECK(s.world().get<sim::Producer>(barracks)->queue.size() == 1);
  // előfeltétel: a berserkerhez rúnakovács és berserker-irány kell
  CHECK(!s.train(1, def_of("urs_berserker"), 0, &why));
  // másik nép egysége vagy épület nem képezhető itt
  CHECK(!s.train(1, def_of("urs_nagyterem"), 0, &why));
}

TEST_CASE("termelés: gyülekezőpont – pontra megy, fára küldve gyűjt; két épületnél a rövidebb sor kapja") {
  Match m("BUILD-ALAP");
  CHECK(m.ok);
  if (!m.ok) return;
  sim::Sim& s = *m.sim;
  const uint16_t settler = def_of("urs_telepes");
  const sim::EntityId hq = hq_of(s, 1);
  s.add_resources(1, sim::kWood, 2000);
  s.add_resources(1, sim::kGold, 2000);
  sim::Command r;
  r.kind = sim::Command::Kind::Rally;
  r.units = {hq};
  r.x = Fixed::from_int(14);
  r.y = Fixed::from_int(17);
  s.queue(std::move(r));
  m.run(1);
  CHECK(s.train(1, settler, 0));
  m.run(360 + 30 * 6);
  const sim::EntityId u1 = newest_unit(s, 1, settler);
  const auto& p1 = *s.world().get<sim::Position>(u1);
  CHECK(core::length(p1.x - Fixed::from_int(14), p1.y - Fixed::from_int(17)) < Fixed::from_int(2));
  // gyülekezés egy fára: az új telepes gyűjteni megy
  sim::EntityId tree = sim::kNoEntity;
  s.world().pool<sim::ResourceNode>().each([&](sim::EntityId e, const sim::ResourceNode& n) {
    if (tree == sim::kNoEntity && n.kind == sim::kWood) tree = e;
  });
  s.set_rally(hq, s.world().get<sim::Position>(tree)->x, s.world().get<sim::Position>(tree)->y, tree);
  CHECK(s.train(1, settler, 0));
  m.run(361);
  const sim::Worker& w = *s.world().get<sim::Worker>(newest_unit(s, 1, settler));
  CHECK(w.state == sim::Worker::Go || w.state == sim::Worker::Work);
  // két harcosok háza: a második egység a rövidebb (üres) sorba kerül
  const sim::EntityId b1 = s.place_building(def_of("urs_harcosok_haza"), 1, 12, 12, true);
  const sim::EntityId b2 = s.place_building(def_of("urs_harcosok_haza"), 1, 17, 12, true);
  m.run(1);
  CHECK(s.train(1, def_of("urs_pajzshordozo"), 0));
  CHECK(s.train(1, def_of("urs_pajzshordozo"), 0));
  CHECK(s.world().get<sim::Producer>(b1)->queue.size() == 1 && s.world().get<sim::Producer>(b2)->queue.size() == 1);
}

TEST_CASE("termelés: kutatás a sorban; hős egyszer, legfeljebb három; determinisztikus") {
  Match m("BUILD-ALAP");
  CHECK(m.ok);
  if (!m.ok) return;
  sim::Sim& s = *m.sim;
  s.add_resources(1, sim::kWood, 3000);
  s.add_resources(1, sim::kGold, 3000);
  s.add_resources(1, sim::kMeteor, 1000);
  const sim::EntityId forge = s.place_building(def_of("urs_runakovacs"), 1, 12, 13, true);
  std::string why;
  CHECK(!s.research(1, "urs_runapengek_nincs", &why));
  CHECK(s.research(1, "urs_runapengek", &why));
  CHECK(!s.research(1, "urs_runapengek", &why) && why == "már folyamatban");
  const auto* rd = content_once().rules->research("urs_runapengek");
  CHECK(rd && rd->time_ticks == 1800);
  m.run(1799);
  CHECK(s.player(1).researched.empty());
  m.run(1);
  CHECK(s.player(1).researched.size() == 1 && s.player(1).researched[0] == "urs_runapengek");
  CHECK(!s.research(1, "urs_runapengek", &why) && why == "már kész");
  CHECK(s.world().get<sim::Producer>(forge)->queue.empty());
  // hős: az Esküfánál; ugyanaz a hős csak egyszer, legfeljebb három
  s.place_building(def_of("urs_tanya"), 1, 17, 13, true);   // létszám
  s.place_building(def_of("urs_tanya"), 1, 17, 16, true);
  s.place_building(def_of("urs_eskufa"), 1, 11, 16, true);
  m.run(1);
  CHECK(s.train(1, def_of("urs_hos_ragna"), 0, &why));
  CHECK(!s.train(1, def_of("urs_hos_ragna"), 0, &why) && why == "ez a hős már a seregedben van");
  CHECK(s.train(1, def_of("urs_hos_hjalmar"), 0, &why) && s.train(1, def_of("urs_hos_bjarka"), 0, &why));
  m.run(30 * 50 * 3);
  CHECK(count_units(s, 1, def_of("urs_hos_ragna")) == 1 && count_units(s, 1, def_of("urs_hos_bjarka")) == 1);
  CHECK(!s.train(1, def_of("urs_hos_ragna"), 0, &why) && why == "ez a hős már a seregedben van");
}
