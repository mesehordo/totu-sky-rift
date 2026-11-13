// SPDX-License-Identifier: GPL-3.0-or-later
// Építés (M6, 3. lépés; ADR-007): lerakás, fizetés, az Ursanok „belépős” építése, haladás és életerő, kilépés és
// visszatérés a gyűjtéshez, létszám, leadóhely, megszakítás visszatérítéssel, parancs általi megszakítás.
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

sim::EntityId find_building(sim::Sim& s, uint16_t def) {
  sim::EntityId out = sim::kNoEntity;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& b) {
    if (b.def == def && out == sim::kNoEntity) out = e;
  });
  return out;
}

sim::Command build_cmd(sim::EntityId worker, const char* def, int x, int y) {
  sim::Command c;
  c.kind = sim::Command::Kind::Build;
  c.units = {worker};
  c.def = def;
  c.rx = x;
  c.ry = y;
  return c;
}

}  // namespace

TEST_CASE("építés: lerakhatóság – terep, víz, szirt és szint, foglalt hely, pálya széle, előfeltétel") {
  Match m("BUILD-ALAP");
  CHECK(m.ok);
  if (!m.ok) return;
  sim::Sim& s = *m.sim;
  const uint16_t house = def_of("urs_hosszuhaz"), tower = def_of("urs_ortorony");
  std::string why;
  CHECK(s.can_place(1, house, 12, 14, &why));
  CHECK(!s.can_place(1, house, 14, 3, &why) && why == "nem építhető terep");            // tó
  CHECK(!s.can_place(1, house, 19, 14, &why) && why == "szirtre és szintkülönbségre nem építhető");   // a magaslat széle
  CHECK(s.can_place(1, house, 23, 15, &why));                                             // a magaslat tetején
  CHECK(!s.can_place(1, house, 6, 12, &why) && why == "foglalt hely");                   // a főépület
  CHECK(!s.can_place(1, house, 3, 5, &why) && why == "foglalt hely");                    // fa
  CHECK(!s.can_place(1, house, 9, 3, &why) && why == "foglalt hely");                    // aranylelőhely
  CHECK(!s.can_place(1, house, 31, 5, &why) && why == "a térképen kívül");
  CHECK(!s.can_place(1, tower, 12, 14, &why) && why == "hiányzik: urs_harcosok_haza");   // előfeltétel
  CHECK(s.can_place(0, tower, 12, 14, &why));                                             // előfeltétel nélkül
}

TEST_CASE("építés: Ursan – fizetés, belépés, haladás és életerő, kilépés, vissza a gyűjtéshez, létszám") {
  Match m("BUILD-ALAP");
  CHECK(m.ok);
  if (!m.ok) return;
  sim::Sim& s = *m.sim;
  m.run(30 * 6);   // a telepesek gyűjtenek
  const auto ws = m.workers(1);
  const sim::EntityId wk = ws[0];
  const sim::EntityId was_res = s.world().get<sim::Worker>(wk)->res;
  CHECK(was_res != 0);
  const int64_t wood0 = s.player(1).res[sim::kWood], stone0 = s.player(1).res[sim::kStone];
  s.queue(build_cmd(wk, "urs_tanya", 11, 15));   // tanya: 95 fa, 20 arany, 30 s, +10 létszám
  m.run(1);
  CHECK(s.player(1).res[sim::kWood] - wood0 <= -95000 + 8000);   // levonva (közben hozhattak fát)
  CHECK(s.player(1).res[sim::kStone] == stone0 - 20000);
  CHECK(s.world().get<sim::Worker>(wk)->state == sim::Worker::Build);
  // odamegy, lerakja, bemászik
  sim::EntityId farm = sim::kNoEntity;
  int t = 0;
  for (; t < 30 * 20 && farm == sim::kNoEntity; ++t) {
    s.step();
    farm = find_building(s, def_of("urs_tanya"));
  }
  CHECK(farm != sim::kNoEntity);
  if (farm == sim::kNoEntity) return;
  const sim::EntityDef& fd = content_once().rules->defs[def_of("urs_tanya")];
  CHECK(s.world().get<sim::Health>(farm)->hp == fd.hp / 10);   // félkészen 10%
  CHECK(!s.world().get<sim::Building>(farm)->complete);
  CHECK(s.player(1).supply_cap == 12);                        // félkész: még nem ad létszámot
  int entered_at = -1;
  for (int i = 0; i < 30 * 10 && entered_at < 0; ++i) {
    s.step();
    if (s.world().get<sim::Worker>(wk)->inside == farm) entered_at = i;
  }
  CHECK(entered_at >= 0);
  CHECK(s.world().pool<sim::Body>().get(wk) == nullptr);      // bent nem akadály, nem célpont
  CHECK(s.world().get<sim::Building>(farm)->builder_in == wk);
  // a haladás pontosan az építési idő (30 s = 900 tick) alatt telik; az életerő közben nő
  int ticks = 0, hp_prev = s.world().get<sim::Health>(farm)->hp;
  bool monotone = true;
  while (!s.world().get<sim::Building>(farm)->complete && ticks < 2000) {
    s.step();
    ++ticks;
    const int hp = s.world().get<sim::Health>(farm)->hp;
    monotone = monotone && hp >= hp_prev;
    hp_prev = hp;
  }
  std::printf("  tanya: lerakás után %d tick a belépésig, %d tick az építés\n", entered_at + 1, ticks);
  CHECK(ticks >= 895 && ticks <= 902);
  CHECK(monotone && s.world().get<sim::Health>(farm)->hp == fd.hp);
  m.run(1);
  CHECK(s.player(1).supply_cap == 22);
  // kijött, nem a lábnyomban áll, és visszament gyűjteni
  const sim::Worker& w = *s.world().get<sim::Worker>(wk);
  CHECK(w.inside == 0 && s.world().pool<sim::Body>().get(wk) != nullptr);
  CHECK(w.state == sim::Worker::Go || w.state == sim::Worker::Work || w.state == sim::Worker::Return);
  const auto& p = *s.world().get<sim::Position>(wk);
  const bool in_fp = p.x > Fixed::from_int(11) && p.x < Fixed::from_int(14) && p.y > Fixed::from_int(15) && p.y < Fixed::from_int(18);
  CHECK(!in_fp);
}

TEST_CASE("építés: leadóhely a fánál, nincs pénz, megszakítás 75%-kal, mozgásparancs visszaadja a tervet") {
  Match m("BUILD-ALAP");
  CHECK(m.ok);
  if (!m.ok) return;
  sim::Sim& s = *m.sim;
  const auto ws = m.workers(1);
  // fatelep az erdő mellé: utána a fát oda hordják (közelebb van, mint a főépület)
  s.queue(build_cmd(ws[0], "urs_fatelep", 5, 6));
  m.run(30 * 40);
  const sim::EntityId camp = find_building(s, def_of("urs_fatelep"));
  CHECK(camp != sim::kNoEntity && s.world().get<sim::Building>(camp)->complete);
  const sim::EntityId hq = find_building(s, def_of("urs_nagyterem"));
  int64_t camp_trips = 0;
  for (int i = 0; i < 30 * 30; ++i) {
    s.step();
    s.world().pool<sim::Worker>().each([&](sim::EntityId e, const sim::Worker& wk) {
      if (wk.state == sim::Worker::Return && wk.move_to == camp) ++camp_trips;
      (void)e;
    });
  }
  CHECK(camp_trips > 0);
  (void)hq;
  // nincs elég nyersanyag: semmi sem vonódik le
  std::string why;
  const int64_t before = s.player(1).res[sim::kStone];
  CHECK(!s.order_build(ws[1], def_of("urs_runaszentely"), 12, 14, &why));
  CHECK(s.player(1).res[sim::kStone] == before);
  // megszakítás: 75% vissza, a munkás kijön, a hely szabad
  const int64_t wood0 = s.player(1).res[sim::kWood];
  CHECK(s.order_build(ws[1], def_of("urs_hosszuhaz"), 12, 14, &why));
  CHECK(s.player(1).res[sim::kWood] == wood0 - 100000);
  sim::EntityId house = sim::kNoEntity;
  for (int i = 0; i < 30 * 20 && (house == sim::kNoEntity || !s.world().get<sim::Worker>(ws[1])->inside); ++i) {
    s.step();
    house = find_building(s, def_of("urs_hosszuhaz"));
  }
  CHECK(house != sim::kNoEntity && s.world().get<sim::Worker>(ws[1])->inside == house);
  const int64_t wood1 = s.player(1).res[sim::kWood];
  sim::Command cancel;
  cancel.kind = sim::Command::Kind::CancelBuild;
  cancel.target = house;
  s.queue(std::move(cancel));
  m.run(1);
  CHECK(!s.world().alive(house));
  CHECK(s.player(1).res[sim::kWood] - wood1 >= 75000 && s.player(1).res[sim::kWood] - wood1 < 75000 + 9000);
  CHECK(s.world().get<sim::Worker>(ws[1])->inside == 0 && m.nav->passable(0, 2 * 12 + 1, 2 * 14 + 1));
  // a még le nem rakott terv: mozgásparancsra a teljes költség vissza
  const int64_t wood2 = s.player(1).res[sim::kWood];
  CHECK(s.order_build(ws[2], def_of("urs_hosszuhaz"), 16, 12, &why));
  sim::Command mv;
  mv.kind = sim::Command::Kind::GroupMove;
  mv.units = {ws[2]};
  mv.x = Fixed::from_int(10);
  mv.y = Fixed::from_int(18);
  s.queue(std::move(mv));
  m.run(1);
  CHECK(s.player(1).res[sim::kWood] - wood2 >= 0 && s.player(1).res[sim::kWood] - wood2 < 9000);
  CHECK(s.world().get<sim::Worker>(ws[2])->state == sim::Worker::Idle);
}

TEST_CASE("építés: determinisztikus (rögzített hash), és mentésből folytatva ugyanoda jut") {
  auto script = [](Match& m) {
    const auto ws = m.workers(1);
    m.sim->queue(build_cmd(ws[0], "urs_tanya", 11, 15));
    m.sim->queue(build_cmd(ws[1], "urs_fatelep", 5, 6));
    const auto ws2 = m.workers(2);
    m.sim->queue(build_cmd(ws2[0], "urs_hosszuhaz", 18, 8));
  };
  Match a("BUILD-ALAP"), b("BUILD-ALAP");
  CHECK(a.ok && b.ok);
  if (!a.ok || !b.ok) return;
  script(a);
  script(b);
  a.run(900);
  b.run(900);
  CHECK(a.sim->hash() == b.sim->hash());
  const auto saved = a.sim->save();
  a.run(900);
  Match c("BUILD-ALAP", true, false);
  CHECK(c.sim->load(saved));
  c.sim->restore_obstacles(*c.map);
  c.run(900);
  CHECK(c.sim->hash() == a.sim->hash());
  std::printf("  építési állapothash (1800 tick): 0x%016llx\n", static_cast<unsigned long long>(a.sim->hash()));
  CHECK(a.sim->hash() == 0xa8351b5df8af78e7ULL);   // minden rendszeren ugyanez
}

TEST_CASE("lebontás (D38): a kész épület 70%-kal bontható az Ursanoknál, az Aurethi Birodalomnál nem") {
  Match m("BUILD-ALAP", true, true, {"ursan", "imperium"});
  CHECK(m.ok);
  if (!m.ok) return;
  sim::Sim& s = *m.sim;
  const auto& r = *content_once().rules;
  CHECK(r.faction("ursan")->demolish && r.faction("ooni")->demolish && r.faction("wullof")->demolish);
  CHECK(!r.faction("imperium")->demolish && !r.faction("elves")->demolish);
  CHECK(r.demolish_refund_permille == 700 && r.cancel_refund_permille == 750);
  const sim::EntityId house = s.place_building(def_of("urs_hosszuhaz"), 1, 12, 14, true);
  const int64_t wood = s.player(1).res[sim::kWood];
  sim::Command d;
  d.kind = sim::Command::Kind::Demolish;
  d.target = house;
  s.queue(std::move(d));
  m.run(1);
  CHECK(!s.world().alive(house));
  CHECK(s.player(1).res[sim::kWood] - wood >= 70000 && s.player(1).res[sim::kWood] - wood < 70000 + 9000);
  CHECK(m.nav->passable(0, 2 * 12 + 1, 2 * 14 + 1));
  // a félkész nem bontható (azt megszakítani kell), és a másik nép kész épülete sem
  const sim::EntityId half = s.place_building(def_of("urs_hosszuhaz"), 1, 12, 14, false);
  CHECK(!s.demolish(half));
  sim::EntityId imp_hq = sim::kNoEntity;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building&) {
    if (s.world().get<sim::Health>(e)->owner == 2) imp_hq = e;
  });
  CHECK(imp_hq != sim::kNoEntity && !s.demolish(imp_hq) && s.world().alive(imp_hq));
}
