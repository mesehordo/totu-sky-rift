// SPDX-License-Identifier: GPL-3.0-or-later
// Szövetséges kisnépek (M7 9. lépés; ADR-008, D26, MECH-ALLY, KIS-1–2): a szövetség megkötése, és mind a 13 kisnép
// csomagja – minden épület felépíthető, minden egység képezhető, minden kutatás elkészül – a gazda (ursan) játékosnál.
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
uint16_t def_of(const std::string& id) { return R().def(id); }

void rich(sim::Sim& s) {
  for (int r = 0; r < sim::kResources; ++r) s.add_resources(1, r, 50000);
}

const std::vector<std::pair<int, int>> kSpots = {{12, 9}, {16, 9}, {12, 13}, {16, 13}, {0, 16}, {3, 16}, {12, 17}, {16, 17}};

}  // namespace

TEST_CASE("szövetség: kész Diplomácia csarnoka kell, egyszer, véglegesen, az ára levonódik; előtte a kisnép elemei zárva") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  rich(s);
  std::string err;
  CHECK(!s.choose_ally(1, "pinguu", &err));   // csarnok nélkül nem
  CHECK(s.missing_requirements(1, R().defs[def_of("pin_jegdardas")]));
  s.place_building(def_of("urs_diplomacia"), 1, 12, 9, true);
  const int32_t stone0 = s.player(1).resource(sim::kStone), met0 = s.player(1).resource(sim::kMeteor);
  CHECK(!s.choose_ally(1, "ursan", &err));    // főnép nem lehet
  CHECK(s.choose_ally(1, "pinguu", &err));
  CHECK(s.player(1).ally == "pinguu");
  const auto& cost = R().faction("ursan")->ally_cost;
  CHECK(stone0 - s.player(1).resource(sim::kStone) == cost[sim::kStone] && met0 - s.player(1).resource(sim::kMeteor) == cost[sim::kMeteor]);
  CHECK(!s.choose_ally(1, "tokla", &err));    // egyszer
  CHECK(!s.missing_requirements(1, R().defs[def_of("pin_jegdardas")]));
}

TEST_CASE("kisnépek: mind a 13 csomagja – épületek felépíthetők, egységek képezhetők, kutatások elkészülnek") {
  int peoples = 0, built = 0, trained = 0, researched = 0;
  for (const sim::FactionDef& f : R().factions) {
    if (!f.ally) continue;
    ++peoples;
    Match m("BUILD-ALAP");
    sim::Sim& s = *m.sim;
    rich(s);
    s.place_building(def_of("urs_diplomacia"), 1, 12, 9, true);
    std::string err;
    CHECK(s.choose_ally(1, f.id, &err));
    // a csomag épületei (a nép sablonjaiból): építés a gazda munkásával
    std::vector<uint16_t> blds, units;
    std::vector<std::string> res;
    for (size_t i = 0; i < R().defs.size(); ++i) {
      const auto& d = R().defs[i];
      if (d.faction != f.id) continue;
      (d.building ? blds : units).push_back(static_cast<uint16_t>(i));
    }
    for (const auto& r : R().researches) {
      if (r.faction == f.id) res.push_back(r.id);
    }
    size_t spot = 1;
    for (const uint16_t b : blds) {
      const auto& d = R().defs[b];
      if (d.fw <= 0) continue;
      for (const auto& q : d.reqs) {   // az épületfeltételek (a csomag másik épülete)
        if (q.kind == sim::EntityDef::Req::Building && spot < kSpots.size()) {
          s.place_building(def_of(q.id), 1, kSpots[spot].first, kSpots[spot].second, true);
          ++spot;
        }
      }
      if (spot >= kSpots.size()) break;
      const auto [x, y] = kSpots[spot++];
      const bool ok = s.order_build(m.workers(1).front(), b, x, y, &err);
      if (!ok) std::printf("  %s: %s\n", d.id.c_str(), err.c_str());
      CHECK(ok);
      m.run(d.time_ticks + 400);
      bool done = false;
      s.world().pool<sim::Building>().each([&](sim::EntityId, const sim::Building& bb) { done = done || (bb.def == b && bb.complete); });
      if (!done) std::printf("  %s nem készült el\n", d.id.c_str());
      CHECK(done);
      built += done ? 1 : 0;
    }
    s.place_building(def_of("urs_tanya"), 1, 0, 16, true);
    s.place_building(def_of("urs_tanya"), 1, 3, 16, true);
    m.run(1);
    rich(s);
    for (const uint16_t u : units) {
      bool producible = false;
      for (const uint16_t b : blds) producible = producible || std::find(R().defs[b].produces.begin(), R().defs[b].produces.end(), u) != R().defs[b].produces.end();
      if (!producible) continue;   // idézett vagy nem képezhető lény
      const bool ok = s.train(1, u, 0, &err);
      if (!ok) std::printf("  %s: %s\n", R().defs[u].id.c_str(), err.c_str());
      CHECK(ok);
      trained += ok ? 1 : 0;
    }
    for (const auto& r : res) {
      const bool ok = s.research(1, r, &err);
      if (!ok) std::printf("  %s: %s\n", r.c_str(), err.c_str());
      researched += ok ? 1 : 0;
    }
  }
  std::printf("  %d kisnép: %d épület, %d egység, %d kutatás\n", peoples, built, trained, researched);
  CHECK(peoples == 13);
}
