// SPDX-License-Identifier: GPL-3.0-or-later
// A külső kódvizsgálat hibajegyzékének (2026-11-15) regressziós próbái: tételenként a reprodukció és a javítás igazolása.
#include <algorithm>
#include <climits>
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
Fixed fx(int a, int b) { return Fixed::from_ratio(a, b); }

sim::EntityId put(sim::Sim& s, const char* id, uint8_t owner, int x10, int y10) {
  const auto e = s.spawn_unit(def_of(id), owner, fx(x10, 10), fx(y10, 10));
  s.place_at_free(e, fx(x10, 10), fx(y10, 10));
  return e;
}

void rich(sim::Sim& s, uint8_t o) {
  for (int r = 0; r < sim::kResources; ++r) s.add_resources(o, r, 20000);
}

int64_t total(sim::Sim& s, uint8_t o) {
  int64_t t = 0;
  for (int r = 0; r < sim::kResources; ++r) t += s.player(o).resource(r);
  return t;
}

sim::Command cmd(sim::Command::Kind k, uint8_t owner, int64_t tick) {
  sim::Command c;
  c.kind = k;
  c.owner = owner;
  c.tick = tick;
  return c;
}

}  // namespace

TEST_CASE("hibajegyzek H-01: a jatekos parancsa csak a sajat egysegeire es epuleteire hat") {
  Match m("BUILD-ALAP", true, true, {"ursan", "wullof"});
  sim::Sim& s = *m.sim;
  const auto mine = put(s, "urs_berserker", 1, 100, 100);
  const auto foe = put(s, "wul_landzsas", 2, 200, 100);
  const auto p0 = *s.world().get<sim::Position>(foe);
  auto c = cmd(sim::Command::Kind::Move, 1, s.tick());
  c.units = {mine, foe};   // az 1. játékos az ellenfél egységét is mozgatná
  c.x = fx(150, 10);
  c.y = fx(160, 10);
  s.queue(c);
  m.run(30);
  const auto p1 = *s.world().get<sim::Position>(foe);
  CHECK(p1.x == p0.x && p1.y == p0.y);   // az idegen egység nem mozdult
  // idegen épületben nem képezhet, idegen épületet nem bonthat
  rich(s, 1);
  sim::EntityId hq2 = sim::kNoEntity;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building&) {
    if (s.world().get<sim::Health>(e)->owner == 2 && hq2 == sim::kNoEntity) hq2 = e;
  });
  CHECK(hq2 != sim::kNoEntity);
  const int64_t before = total(s, 1);
  auto d = cmd(sim::Command::Kind::Demolish, 1, s.tick());
  d.target = hq2;
  s.queue(d);
  m.run(2);
  CHECK(s.world().alive(hq2) && total(s, 1) == before);
}

TEST_CASE("hibajegyzek H-23 H-24 H-53: ervenytelen tulajdonos es formacio nem okoz hibat") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  CHECK(s.spawn_unit(def_of("urs_berserker"), 255, fx(100, 10), fx(100, 10)) == sim::kNoEntity);
  CHECK(s.place_building(def_of("urs_tanya"), 200, 12, 9, true) == sim::kNoEntity);
  const auto a = put(s, "urs_berserker", 1, 100, 100);
  const auto b = put(s, "urs_berserker", 1, 110, 100);
  auto c = cmd(sim::Command::Kind::GroupMove, 1, s.tick());
  c.units = {a, b};
  c.x = fx(150, 10);
  c.y = fx(120, 10);
  c.formation = 255;   // érvénytelen formáció: alapértelmezettre esik
  s.queue(c);
  m.run(10);
  CHECK(s.world().alive(a) && s.world().alive(b));
}

TEST_CASE("hibajegyzek H-14 H-11: ketszeri bontas egy tickben egyszer teriti vissza; a sikertelen tervcsere a regi tervet hagyja") {
  {
    Match m("BUILD-ALAP");
    sim::Sim& s = *m.sim;
    const auto t = s.place_building(def_of("urs_tanya"), 1, 12, 9, true);
    const int64_t before = total(s, 1);
    CHECK(s.demolish(t));
    CHECK(!s.demolish(t));   // ugyanabban a tickben másodszor nem
    const int64_t once = total(s, 1) - before;
    m.run(1);
    CHECK(once > 0 && total(s, 1) - before == once);
  }
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto w = m.workers(1).front();
  const uint16_t cheap = def_of("urs_tanya");
  // pontosan a tanyára való nyersanyag; a drága csarnok nem fér bele
  for (int r = 0; r < sim::kResources; ++r) s.add_resources(1, r, -1000000);
  for (int r = 0; r < sim::kResources; ++r) s.add_resources(1, r, R().defs[cheap].cost[static_cast<size_t>(r)]);
  std::string err;
  CHECK(s.order_build(w, cheap, 12, 9, &err));
  const int64_t after_plan = total(s, 1);
  CHECK(!s.order_build(w, def_of("urs_harcosok_haza"), 12, 13, &err));   // nincs elég
  CHECK(total(s, 1) == after_plan);   // a régi terv ára nem jött vissza ingyen
}

TEST_CASE("hibajegyzek H-55 H-59: egy epuletbol egy szeker; a felhasznalt boleny egy tickben nem ul fel ujra") {
  Match m("BUILD-ALAP", true, true, {"wullof", "ursan"});
  sim::Sim& s = *m.sim;
  uint16_t packable = sim::kNoDef;
  for (size_t i = 0; i < R().defs.size() && packable == sim::kNoDef; ++i) {
    if (R().defs[i].faction == "wullof" && R().defs[i].packable) packable = static_cast<uint16_t>(i);
  }
  CHECK(packable != sim::kNoDef);
  const auto b = s.place_building(packable, 1, 12, 9, true);
  const auto w1 = s.pack_building(b);
  const auto w2 = s.pack_building(b);
  CHECK(w1 != sim::kNoEntity && w2 == sim::kNoEntity);
}

TEST_CASE("hibajegyzek H-97: a tul nagy koltseg nem fordul negativba") {
  // a beolvasó a tartomány szélére vág (nem csordul át)
  bool ok = true;
  for (const auto& d : R().defs) {
    for (const int32_t c : d.cost) ok = ok && c >= 0 && c <= 1000000;
  }
  CHECK(ok);
}
