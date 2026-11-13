// SPDX-License-Identifier: GPL-3.0-or-later
// Aurethi Birodalom (M7, ADR-008): a népmodul viselkedési próbái (src/sim/factions/imperium.cpp) – alapozás és kopás,
// Reaktorpajzs, Lélekhorgony (horgony, újratestesülés, mentés), gyűjtők, Égkő-fény gépjavítás, Túlhajtás.
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "check.hpp"
#include "match_util.hpp"

using namespace totu;
using namespace totu::testing;
using core::Fixed;

// ellenőrzés, és bukáskor a próba vége (a további lépések értelmetlenek)
#define IMP_REQUIRE(expr) \
  do {                    \
    CHECK(expr);          \
    if (!(expr)) return;  \
  } while (0)

// a komponens mindig frissen (a tárolók átméreteződhetnek)
#define HP(e) (*s.world().get<sim::Health>(e))
#define BD(e) (*s.world().get<sim::Building>(e))

namespace {

const sim::Rules& R() { return *content_once().rules; }
uint16_t def_of(const char* id) { return R().def(id); }
Fixed fx(int n, int d) { return Fixed::from_ratio(n, d); }

Match imperium() { return Match("BUILD-ALAP", true, true, {"imperium", "ursan"}); }

sim::EntityId put(sim::Sim& s, const char* id, uint8_t owner, int x10, int y10) {
  const auto e = s.spawn_unit(def_of(id), owner, fx(x10, 10), fx(y10, 10));
  if (auto* c = s.world().get<sim::Combat>(e)) c->order = sim::Combat::Hold;
  return e;
}

std::vector<sim::EntityId> all_of(sim::Sim& s, const char* id, uint8_t owner) {
  std::vector<sim::EntityId> v;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit& u) {
    if (u.def == def_of(id) && s.world().get<sim::Health>(e)->owner == owner && s.world().get<sim::Health>(e)->hp > 0) {
      v.push_back(e);
    }
  });
  return v;
}

// halál a közös halálsorban (a tick végén, halál-eseménnyel)
void slay(sim::Sim& s, sim::EntityId e) {
  s.world().get<sim::Health>(e)->hp = 0;
  s.dying_.push_back(e);
}

void rich(sim::Sim& s) {
  for (int r = 0; r < sim::kResources; ++r) s.add_resources(1, r, 20000);
}

sim::Building hq_of(sim::Sim& s, uint8_t owner) {
  sim::Building out;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& b) {
    if (b.def == def_of("imp_forum") && s.world().get<sim::Health>(e)->owner == owner) out = b;
  });
  return out;
}

sim::EntityId building_at(sim::Sim& s, const char* id, int tx, int ty) {
  sim::EntityId r = sim::kNoEntity;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& b) {
    if (b.def == def_of(id) && b.tx == tx && b.ty == ty) r = e;
  });
  return r;
}

// a Fabrikátor felépíti (a munkás a helyszínre megy); true: elkészült
bool build(Match& m, sim::EntityId worker, const char* id, int tx, int ty, sim::EntityId* out) {
  std::string err;
  if (!m.sim->order_build(worker, def_of(id), tx, ty, &err)) {
    std::printf("  építés elutasítva (%s): %s\n", id, err.c_str());
    return false;
  }
  for (int i = 0; i < 6000; ++i) {
    m.run(1);
    const sim::EntityId b = building_at(*m.sim, id, tx, ty);
    if (b && m.sim->world().get<sim::Building>(b)->complete) {
      if (out) *out = b;
      return true;
    }
    if (!b && i > 30 && std::string(id).find("alapozas") != std::string::npos) {   // a lapkő elkészülve eltűnik
      bool laid = false;
      m.sim->world().pool<sim::Building>().each([&](sim::EntityId, const sim::Building& bb) {
        laid = laid || (bb.def == def_of(id) && bb.tx == tx && bb.ty == ty);
      });
      if (!laid) return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("Aurethi Birodalom: a népmodul betöltve") {
  Match m("BUILD-ALAP");
  bool found = false;
  for (const auto& mod : m.sim->modules_) found = found || std::string(mod->name()) == "imperium";
  CHECK(found);
  // a lapkövet az adat jelöli (`Legacy.foundation_tile`), nem az azonosító
  for (const char* id : {"imp_alapozas", "imp_nagy_alapozas"}) {
    const content::Value* l = R().defs[def_of(id)].raw->find("Legacy");
    CHECK(l && l->find("foundation_tile") && l->find("foundation_tile")->as_bool());
  }
}

TEST_CASE("Aurethi alapozás: a kész lapkő alapozás marad; rajta teljes, nélküle fél életerő és 0,2%/s kopás 50%-ig") {
  Match m = imperium();
  IMP_REQUIRE(m.ok);
  sim::Sim& s = *m.sim;
  rich(s);
  const sim::Building hq = hq_of(s, 1);
  const auto fab = all_of(s, "imp_fabrikator", 1);
  IMP_REQUIRE(!fab.empty());
  const auto w = fab.front();
  const int ax = hq.tx + hq.w + 3, ay = hq.ty;   // az alapozás a főépület mellett
  CHECK(build(m, w, "imp_alapozas", ax, ay, nullptr));
  CHECK(building_at(s, "imp_alapozas", ax, ay) == sim::kNoEntity);   // a lapkő helyén alapozás (nem épület)
  sim::EntityId on = sim::kNoEntity;
  CHECK(build(m, w, "imp_reaktor", ax, ay, &on));
  CHECK(HP(on).hp == HP(on).max);   // alapozáson: teljes életerő
  sim::EntityId off = sim::kNoEntity;
  const int bx = ax, by = ay + 4;
  CHECK(build(m, w, "imp_reaktor", bx, by, &off));
  std::printf("  alapozás nélkül: %d / %d\n", HP(off).hp, HP(off).max);
  CHECK(HP(off).hp <= HP(off).max / 2 + 1);   // alapozás nélkül: fél életerő (+ az e tickbeli faji gyógyulás)
  // kopás: 0,002 × max / s, a faji gyógyulás (0,25/s) ellenében; 20 s alatt, 50% alá soha
  HP(off).hp = HP(off).max * 3 / 4;
  if (auto* v = s.world().get<sim::Vital>(off)) v->deficit = 0;
  const int before = HP(off).hp;
  m.run(20 * sim::kTickHz);
  const int lost = before - HP(off).hp;
  std::printf("  kopás 20 s alatt: %d életerő (max %d)\n", lost, HP(off).max);
  const int expect = (HP(off).max * 2 * 20) / 1000 - 5;   // 0,2%/s × 20 s − 0,25/s × 20 s
  CHECK(lost >= expect - 2 && lost <= expect + 2);
  m.run(400 * sim::kTickHz);
  CHECK(HP(off).hp >= HP(off).max / 2 && HP(off).hp <= HP(off).max / 2 + 1);   // a kopás 50%-on megáll
  // a főépület alapozáson kezd (1 csempés perem): a perem csempéi sem akadályok
  CHECK(HP(on).hp == HP(on).max);
}

TEST_CASE("Aurethi Reaktorpajzs: a pajzs csak Lélekreaktor 6 csempéjén belül vagy Égkő-fényben töltődik") {
  Match m = imperium();
  IMP_REQUIRE(m.ok);
  sim::Sim& s = *m.sim;
  const auto far = put(s, "imp_legios", 1, 200, 150);
  const auto bear = put(s, "urs_berserker", 2, 200, 170);
  m.run(1);
  CHECK(!s.mod_shield_ok(far));
  CHECK(s.mod_shield_ok(bear));   // más nép: semleges
  s.place_building(def_of("imp_reaktor"), 1, 22, 14, true);   // középpont (23, 15): 3 csempe
  m.run(1);
  CHECK(s.mod_shield_ok(far));
  const auto out = put(s, "imp_legios", 1, 300, 180);   // 7,6 csempe
  m.run(1);
  CHECK(!s.mod_shield_ok(out));
  s.place_building(def_of("imp_egko"), 1, 28, 16, true);   // Égkő-fény (5 csempe)
  m.run(2);
  CHECK(s.in_rift(out));
  CHECK(s.mod_shield_ok(out));
}

TEST_CASE("Aurethi Égkő-fény: a gépek +1 életerő/s (a faji 0,25/s mellett); a gyűjtők: követ és meteoritot csak az Arató") {
  Match m = imperium();
  IMP_REQUIRE(m.ok);
  sim::Sim& s = *m.sim;
  const auto in = put(s, "imp_legios", 1, 200, 160);
  const auto out = put(s, "imp_legios", 1, 200, 30);
  s.place_building(def_of("imp_egko"), 1, 21, 17, true);
  m.run(2);
  IMP_REQUIRE(s.in_rift(in));
  IMP_REQUIRE(!s.in_rift(out));
  HP(in).hp = 50;
  HP(out).hp = 50;
  m.run(20 * sim::kTickHz);
  std::printf("  20 s gyógyulás: Égkő-fényben %d, nélküle %d\n", HP(in).hp - 50, HP(out).hp - 50);
  CHECK(HP(out).hp - 50 == 5);    // 0,25/s × 20 s
  CHECK(HP(in).hp - 50 >= 25 && HP(in).hp - 50 <= 26);   // 1,25/s × 20 s (tickenként felfelé kerekítve)
  // gyűjtők
  const auto fab = put(s, "imp_fabrikator", 1, 100, 175);
  const auto arato = put(s, "imp_arato", 1, 110, 175);
  sim::EntityId stone = sim::kNoEntity, wood = sim::kNoEntity;
  s.world().pool<sim::ResourceNode>().each([&](sim::EntityId e, const sim::ResourceNode& n) {
    if (n.kind == sim::kStone && !stone) stone = e;
    if (n.kind == sim::kWood && !wood) wood = e;
  });
  IMP_REQUIRE(stone);
  IMP_REQUIRE(wood);
  CHECK(!s.order_gather(fab, stone));
  CHECK(s.order_gather(arato, stone));
  CHECK(s.order_gather(fab, wood));
}

TEST_CASE("Aurethi Lélekhorgony: a gép helyén horgony (15%), teljesen helyreállva újra az egység; a mentő elviszi") {
  Match m = imperium();
  IMP_REQUIRE(m.ok);
  sim::Sim& s = *m.sim;
  const auto leg = put(s, "imp_legios", 1, 190, 95);
  const int max = R().defs[def_of("imp_legios")].hp;
  slay(s, leg);
  m.run(1);
  auto anchors = all_of(s, "imp_lelekhorgony", 1);
  IMP_REQUIRE(anchors.size() == 1);
  const auto a = anchors.front();
  CHECK(HP(a).max == std::max(50, max));
  CHECK(HP(a).hp == HP(a).max * 15 / 100);
  // a hős és az idézett nem hagy horgonyt; más nép sem
  slay(s, put(s, "urs_berserker", 2, 110, 40));
  m.run(1);
  CHECK(all_of(s, "imp_lelekhorgony", 2).empty());
  // mentés: a Légiós elviszi a horgonyt a (11; 9,5) pontra
  const auto carrier = put(s, "imp_legios", 1, 230, 95);
  CHECK(!s.use_ability({carrier}, "lelekhorgony_mentes", fx(110, 10), fx(95, 10), a));   // kutatás nélkül nem
  auto& done = s.player_mut(1).researched;
  done.push_back("imp_lelekhorgony_mentes");
  std::sort(done.begin(), done.end());
  CHECK(s.use_ability({carrier}, "lelekhorgony_mentes", fx(110, 10), fx(95, 10), a));
  bool carried = false;
  for (int i = 0; i < 40 * sim::kTickHz && s.world().alive(a); ++i) {
    m.run(1);
    carried = carried || s.world().get<sim::Sheltered>(a) != nullptr;
    if (carried && !s.world().get<sim::Sheltered>(a)) break;
  }
  CHECK(carried);
  IMP_REQUIRE(s.world().alive(a));
  CHECK(!s.world().get<sim::Sheltered>(a));
  const auto& ap = *s.world().get<sim::Position>(a);
  std::printf("  a horgony letéve: (%.2f, %.2f)\n", ap.x.to_double(), ap.y.to_double());
  CHECK(core::length(ap.x - fx(110, 10), ap.y - fx(95, 10)) <= Fixed::from_int(2));
  // újratestesülés
  HP(a).hp = HP(a).max;
  if (auto* v = s.world().get<sim::Vital>(a)) v->deficit = 0;
  m.run(2);
  CHECK(!s.world().alive(a) || s.world().get<sim::Health>(a)->hp <= 0);
  CHECK(all_of(s, "imp_lelekhorgony", 1).empty());
  CHECK(all_of(s, "imp_legios", 1).size() == 2);
}

TEST_CASE("Aurethi Túlhajtás: az épülő épület hátralévő idejének fele azonnal; a Fabrikátor a max életereje 40%-át veszti") {
  Match m = imperium();
  IMP_REQUIRE(m.ok);
  sim::Sim& s = *m.sim;
  rich(s);
  const sim::Building hq = hq_of(s, 1);
  const auto w = all_of(s, "imp_fabrikator", 1).front();
  const int tx = hq.tx + hq.w + 3, ty = hq.ty;
  std::string err;
  IMP_REQUIRE(s.order_build(w, def_of("imp_reaktor"), tx, ty, &err));
  sim::EntityId b = sim::kNoEntity;
  for (int i = 0; i < 3000; ++i) {
    m.run(1);
    b = building_at(s, "imp_reaktor", tx, ty);
    if (b && s.world().get<sim::Building>(b)->progress > 3000) break;
  }
  IMP_REQUIRE(b);
  const int64_t total = int64_t{R().defs[def_of("imp_reaktor")].time_ticks} * 1000;
  const int64_t p0 = BD(b).progress;
  const int hp0 = HP(w).hp;
  CHECK(s.use_ability({w}, "tulhajtas", {}, {}, 0));
  const int64_t expect = p0 + (((total - p0) * fx(1, 2).raw()) >> 16);
  CHECK(BD(b).progress == expect);
  std::printf("  túlhajtás: %lld -> %lld (össz %lld), Fabrikátor %d -> %d\n", (long long)p0, (long long)BD(b).progress, (long long)total, hp0, HP(w).hp);
  CHECK(HP(w).hp == hp0 - HP(w).max * 2 / 5);
  CHECK(!s.use_ability({w}, "tulhajtas", {}, {}, 0));   // töltődési idő (20 s)
  // kész épületnél a hiányzó életerő fele
  m.run(25 * sim::kTickHz);
  for (int i = 0; i < 3000 && !BD(b).complete; ++i) m.run(1);
  IMP_REQUIRE(BD(b).complete);
  HP(b).hp = HP(b).max / 2;
  CHECK(s.order_repair({w}, b));
  bool used = false;
  for (int i = 0; i < 600 && !used; ++i) {
    m.run(1);
    const int h = HP(b).hp;
    used = s.use_ability({w}, "tulhajtas", {}, {}, 0);
    if (used) CHECK(HP(b).hp == h + (HP(b).max - h) / 2);
  }
  CHECK(used);
}

TEST_CASE("Aurethi Birodalom: determinisztikus, és mentésből folytatva ugyanaz") {
  auto setup = [](Match& m) {
    sim::Sim& s = *m.sim;
    rich(s);
    const sim::Building hq = hq_of(s, 1);
    const auto w = all_of(s, "imp_fabrikator", 1).front();
    s.order_build(w, def_of("imp_alapozas"), hq.tx + hq.w + 3, hq.ty, nullptr);
    s.place_building(def_of("imp_egko"), 1, 21, 17, true);
    for (int i = 0; i < 3; ++i) put(s, "imp_legios", 1, 200 + i * 6, 150);
    put(s, "urs_berserker", 2, 220, 160);
    if (auto* c = s.world().get<sim::Combat>(all_of(s, "urs_berserker", 2).front())) c->order = sim::Combat::None;
  };
  Match a = imperium(), b = imperium();
  setup(a);
  setup(b);
  a.run(400);
  b.run(400);
  CHECK(a.sim->hash() == b.sim->hash());
  const auto saved = a.sim->save();
  a.run(900);
  Match c("BUILD-ALAP", true, false, {"imperium", "ursan"});
  CHECK(c.sim->load(saved));
  c.sim->restore_obstacles(*c.map);
  c.run(900);
  CHECK(c.sim->hash() == a.sim->hash());
}
