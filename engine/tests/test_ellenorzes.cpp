// SPDX-License-Identifier: GPL-3.0-or-later
// A hibajegyzék nyitott ellenőrzései (C-101…C-115): célzott próbák a mentés, a determinizmus, a gazdaság és az
// útkeresés határeseteire.
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

sim::EntityId hq_of(sim::Sim& s, uint8_t owner) {
  sim::EntityId out = sim::kNoEntity;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building&) {
    if (out == sim::kNoEntity && s.world().get<sim::Health>(e)->owner == owner) out = e;
  });
  return out;
}

// mentés a `at`. tickben, betöltés friss meccsbe; mindkettő még `more` tickig fut: a hash egyezik
bool roundtrip(const std::string& map, const std::vector<std::string>& factions, int at, int more,
               void (*setup)(sim::Sim&) = nullptr) {
  Match a(map, true, true, factions);
  if (!a.ok) return false;
  if (setup) setup(*a.sim);
  a.run(at);
  const auto saved = a.sim->save();
  a.run(more);
  Match b(map, true, false, factions);
  if (!b.sim->load(saved)) return false;
  b.sim->restore_obstacles(*b.map);
  b.run(more);
  return a.sim->hash() == b.sim->hash();
}

}  // namespace

TEST_CASE("ellenorzes C-104: mentes-betoltes-folytatas minden jatszhato neppel") {
  int n = 0;
  for (const sim::FactionDef& f : R().factions) {
    if (f.neutral || f.ally) continue;
    const bool ok = roundtrip("ECO-ALAP", {f.id, "ursan"}, 300, 300);
    if (!ok) std::printf("  eltérés: %s\n", f.id.c_str());
    CHECK(ok);
    ++n;
  }
  std::printf("  %d nép\n", n);
  CHECK(n >= 6);
}

TEST_CASE("ellenorzes C-102 C-103: fuggo parancs es tick kozott megolt egyseg a mentesben") {
  // C-102: a következő tickekre szóló parancs a mentésben (sorosítva) és utána ugyanúgy hajtódik végre
  CHECK(roundtrip("BUILD-ALAP", {"ursan", "wullof"}, 10, 200, [](sim::Sim& s) {
    const auto u = put(s, "urs_berserker", 1, 100, 100);
    sim::Command c;
    c.kind = sim::Command::Kind::Move;
    c.owner = 1;
    c.tick = 40;
    c.units = {u, u, 999999};   // ismétlődő és nem létező azonosító is
    c.x = fx(150, 10);
    c.y = fx(140, 10);
    s.queue(c);
  }));
  // C-103: a tickek között megölt (törlésre jelölt) egység: a mentés és a folytatás ugyanaz
  Match a("BUILD-ALAP", true, true, {"ursan", "wullof"});
  const auto v = put(*a.sim, "wul_landzsas", 2, 150, 120);
  a.run(5);
  a.sim->kill(v);
  const auto saved = a.sim->save();
  a.run(60);
  Match b("BUILD-ALAP", true, false, {"ursan", "wullof"});
  CHECK(b.sim->load(saved));
  b.sim->restore_obstacles(*b.map);
  b.run(60);
  CHECK(a.sim->hash() == b.sim->hash());
  CHECK(!b.sim->world().alive(v));
}

TEST_CASE("ellenorzes C-109 C-112: az epulettel elvesz a sor letszama, a letszamot ado egyseg halala csokkenti a korlatot") {
  Match m("BUILD-ALAP", true, true, {"ursan", "wullof"});
  sim::Sim& s = *m.sim;
  m.run(1);
  const int32_t base = s.player(1).supply_used;
  std::string why;
  for (int i = 0; i < 3; ++i) CHECK(s.train(1, def_of("urs_telepes"), 0, &why));
  CHECK(s.player(1).supply_used > base);
  const auto hq = hq_of(s, 1);
  s.deal_damage(0, 2, sim::kNoDef, hq, 100000000, false);
  m.run(2);
  CHECK(!s.world().alive(hq));
  CHECK(s.player(1).supply_used == base);   // a sorral együtt a foglalás is megszűnt
  // C-112: a Wullof emu létszámot ad; halála után a korlát visszaesik
  m.run(1);
  const int32_t cap0 = s.player(2).supply_cap;
  const auto emu = put(s, "wul_emu", 2, 200, 200);
  m.run(2);
  const int32_t cap1 = s.player(2).supply_cap;
  CHECK(cap1 >= cap0 && cap1 <= cap0 + 6);
  s.kill(emu);
  m.run(2);
  CHECK(s.player(2).supply_cap == cap0);
}

TEST_CASE("ellenorzes C-110 C-111: telt sor es letszamkorlat mellett nincs fizetes") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  s.add_resources(1, sim::kWood, 5000);
  s.add_resources(1, sim::kGold, 5000);
  std::string why;
  int ok = 0;
  for (int i = 0; i < 12; ++i) ok += s.train(1, def_of("urs_telepes"), 0, &why) ? 1 : 0;
  CHECK(ok <= sim::Rules::kQueueMax);
  const auto res = s.player(1).res;
  const int32_t used = s.player(1).supply_used;
  CHECK(!s.train(1, def_of("urs_telepes"), 0, &why));
  CHECK(s.player(1).res == res && s.player(1).supply_used == used);   // az elutasított képzés nem fizet
  CHECK(s.player(1).supply_used <= s.player(1).supply_cap);
  m.run(2);
  CHECK(s.player(1).supply_used <= s.player(1).supply_cap);
}

TEST_CASE("ellenorzes C-115: a palya szelere kuldott egyseg a palyan belul marad") {
  Match m("BUILD-ALAP", true, true, {"ursan", "wullof"});
  sim::Sim& s = *m.sim;
  const auto u = put(s, "urs_berserker", 1, 20, 20);
  const Fixed W = Fixed::from_int(m.map->width()), H = Fixed::from_int(m.map->height());
  s.order_move(u, Fixed{} - Fixed::from_int(5), H + Fixed::from_int(5));   // a pályán kívüli célpont
  bool inside = true;
  for (int i = 0; i < 600; ++i) {
    m.run(1);
    const auto& p = *s.world().get<sim::Position>(u);
    inside = inside && p.x >= Fixed{} && p.y >= Fixed{} && p.x <= W && p.y <= H;
  }
  CHECK(inside);
}
