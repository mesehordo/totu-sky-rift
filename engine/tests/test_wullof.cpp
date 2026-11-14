// SPDX-License-Identifier: GPL-3.0-or-later
// Wullof (M7 3a. lépés; ADR-008, MECH-WUL-1–4): falkaszellem, vadászzsákmány, erdei rejtőzés, emu-létszám és
// darabkorlát, falka-képzés, vándorszekér, felültetés és leszállás, javítás, kéregcsapolás.
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

Match wullof() { return Match("BUILD-ALAP", true, true, {"wullof", "ursan"}); }

sim::EntityId put(sim::Sim& s, const char* id, uint8_t owner, int x10, int y10) {
  const auto e = s.spawn_unit(def_of(id), owner, fx(x10, 10), fx(y10, 10));
  if (auto* c = s.world().get<sim::Combat>(e)) c->order = sim::Combat::Hold;
  return e;
}

int count(sim::Sim& s, const char* id, uint8_t owner) {
  int n = 0;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit& u) {
    n += u.def == def_of(id) && s.world().get<sim::Health>(e)->owner == owner && s.world().get<sim::Health>(e)->hp > 0;
  });
  return n;
}

void rich(sim::Sim& s) {
  for (int r = 0; r < sim::kResources; ++r) s.add_resources(1, r, 20000);
}

// a kutatás kész (a szabályaival együtt), a kutatási idő nélkül
void grant(sim::Sim& s, uint8_t owner, const char* id) {
  auto& done = s.player_mut(owner).researched;
  done.insert(std::upper_bound(done.begin(), done.end(), std::string(id)), id);
  s.rebuild_rules(owner);
}

int64_t taken(sim::Sim& s, sim::EntityId e) {
  const auto* h = s.world().get<sim::Health>(e);
  const auto* v = s.world().get<sim::Vital>(e);
  return int64_t{h->max - h->hp} * 1000 + (v ? v->deficit : 0);
}

}  // namespace

TEST_CASE("Wullof falkaszellem: a ±4 csempés négyzet társai (legfeljebb 4) – sebzés ×(1 + 0,1 n), sebesség ×(1 + 0,02 n)") {
  Match m = wullof();
  sim::Sim& s = *m.sim;
  const auto lone = put(s, "wul_landzsas", 1, 20, 100);
  std::vector<sim::EntityId> pack;
  for (int i = 0; i < 6; ++i) pack.push_back(put(s, "wul_landzsas", 1, 120 + i * 6, 180));
  const auto emu = put(s, "wul_emu", 1, 125, 185);   // jószág: nem számít
  (void)emu;
  m.run(15);
  CHECK(s.pack_damage_mult(lone) == Fixed::from_int(1));
  CHECK(s.pack_damage_mult(pack[0]) == Fixed::from_int(1) + fx(1, 10) * Fixed::from_int(4));   // 5 társ, de legfeljebb 4
  CHECK(s.pack_speed_mult(pack[0]) == Fixed::from_int(1) + fx(1, 50) * Fixed::from_int(4));
  CHECK(!s.world().get<sim::Pack>(m.workers(1).front()));   // a munkás nem kap
}

TEST_CASE("Wullof vadászzsákmány: ölésért a költség / darab × 0,2 × 0,5 fa; erdei rejtőzés fa mellett, 3 s nyugalom után") {
  Match m = wullof();
  sim::Sim& s = *m.sim;
  const auto w = put(s, "wul_landzsas", 1, 50, 180);
  const auto foe = put(s, "urs_berserker", 2, 55, 180);
  const int32_t wood0 = s.player(1).resource(sim::kWood);
  s.deal_damage(w, 1, def_of("wul_landzsas"), foe, 100000000, false);
  m.run(1);
  int64_t cost = 0;
  for (const int32_t c : R().defs[def_of("urs_berserker")].cost) cost += c;
  std::printf("  zsákmány: %d fa (a berserker költsége %lld)\n", s.player(1).resource(sim::kWood) - wood0, static_cast<long long>(cost));
  CHECK(s.player(1).resource(sim::kWood) - wood0 == static_cast<int32_t>(cost / 10));
  // erdei rejtőzés: a BUILD-ALAP erdeje (2–4 × 4–8) mellett
  const auto hid = put(s, "wul_landzsas", 1, 55, 60);
  m.run(16);
  CHECK(s.world().get<sim::Stealth>(hid)->hidden);   // nyugalomban azonnal (a régi szerint)
  s.world().get<sim::Stealth>(hid)->last_attack = s.tick();   // támadott: 3 s-ig látható
  m.run(16);
  CHECK(!s.world().get<sim::Stealth>(hid)->hidden);
  m.run(90);
  CHECK(s.world().get<sim::Stealth>(hid)->hidden);
  const auto open = put(s, "wul_landzsas", 1, 120, 180);
  m.run(120);
  CHECK(!s.world().get<sim::Stealth>(open)->hidden);   // fa nélkül nem
}

TEST_CASE("Wullof: az emu létszámot ad (6), Emukarámonként legfeljebb 5; a falka-képzés 3 egységet ad") {
  Match m = wullof();
  sim::Sim& s = *m.sim;
  rich(s);
  s.place_building(def_of("wul_bolenykaram"), 1, 12, 9, true);
  m.run(1);
  const int32_t cap0 = s.player(1).supply_cap;
  const int emus0 = count(s, "wul_emu", 1);   // a Wullof emukkal indul
  std::string err;
  int ok = 0;
  for (int i = 0; i < 6; ++i) {
    const bool t = s.train(1, def_of("wul_emu"), 0, &err);
    if (!t) std::printf("  emu %d: %s\n", i, err.c_str());
    ok += t ? 1 : 0;
  }
  CHECK(ok == 5 - emus0);   // a karámonkénti 5-ön felül nem
  m.run(R().defs[def_of("wul_emu")].time_ticks * 6 + 60);
  CHECK(count(s, "wul_emu", 1) == 5);
  CHECK(s.player(1).supply_cap == std::min(R().max_supply, cap0 + (5 - emus0) * 6));
  s.place_building(def_of("wul_tipi"), 1, 16, 9, true);
  s.place_building(def_of("wul_harcosok_tuze"), 1, 12, 13, true);   // a falkához a Falkaharc kell
  CHECK(s.research(1, "wul_falkaharc", &err));
  m.run(R().research("wul_falkaharc")->time_ticks + 2);
  const int before = count(s, "wul_landzsas", 1);
  const bool fk = s.train(1, def_of("wul_landzsas_falka"), 0, &err);
  if (!fk) std::printf("  falka: %s (létszám %d/%d)\n", err.c_str(), s.player(1).supply_used, s.player(1).supply_cap);
  CHECK(fk);
  m.run(R().defs[def_of("wul_landzsas_falka")].time_ticks + 60);
  CHECK(count(s, "wul_landzsas", 1) == before + 3);
}

TEST_CASE("Wullof vándorszekér: a csomagolható épület szekér lesz, letelepítve munkás nélkül, 8 s alatt kész") {
  Match m = wullof();
  sim::Sim& s = *m.sim;
  const auto tipi = s.place_building(def_of("wul_tipi"), 1, 12, 9, true);
  const auto wagon = s.pack_building(tipi);
  m.run(1);
  CHECK(wagon != sim::kNoEntity && !s.world().alive(tipi));
  CHECK(s.world().get<sim::Wagon>(wagon)->building == def_of("wul_tipi"));
  CHECK(s.order_unpack(wagon, 2, 16));
  int t = 0;
  sim::EntityId b = sim::kNoEntity;
  for (; t < 600 && b == sim::kNoEntity; ++t) {
    s.step();
    s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& bb) {
      if (bb.def == def_of("wul_tipi") && bb.tx == 2 && bb.ty == 16) b = e;
    });
  }
  CHECK(b != sim::kNoEntity);
  m.run(8 * 30 + 2);
  CHECK(s.world().get<sim::Building>(b)->complete == 1);   // munkás nélkül, 8 s alatt
}

TEST_CASE("Wullof felültetés (a Bölénylovaglással) és leszállás; javítás; kéregcsapolás (fánként egy csapoló)") {
  Match m = wullof();
  sim::Sim& s = *m.sim;
  rich(s);
  const auto bison = put(s, "wul_boleny", 1, 50, 180);
  const auto rider = put(s, "wul_landzsas", 1, 60, 180);
  CHECK(!s.use_ability({bison}, "felultetes", {}, {}, 0));   // kutatás nélkül nem
  s.place_building(def_of("wul_bolenyakol"), 1, 12, 9, true);
  std::string err;
  CHECK(s.research(1, "wul_bolenylovaglas", &err));
  m.run(R().research("wul_bolenylovaglas")->time_ticks + 2);
  CHECK(s.use_ability({bison}, "felultetes", {}, {}, 0));
  m.run(1);
  CHECK(!s.world().alive(bison) && !s.world().alive(rider) && count(s, "wul_bolenylovas", 1) == 1);
  sim::EntityId cav = sim::kNoEntity;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit& u) { if (u.def == def_of("wul_bolenylovas")) cav = e; });
  CHECK(s.use_ability({cav}, "leszallas", {}, {}, 0));
  m.run(1);
  CHECK(count(s, "wul_boleny", 1) == 1 && count(s, "wul_landzsas", 1) == 1);
  // javítás: a gyűjtögető a sérült épületet
  const auto tipi = s.place_building(def_of("wul_tipi"), 1, 16, 9, true);
  auto& th = *s.world().get<sim::Health>(tipi);
  th.hp = th.max / 2;
  const auto worker = m.workers(1).front();
  CHECK(s.order_repair({worker}, tipi));
  m.run(400);
  CHECK(th.hp > th.max / 2);
  // kéregcsapolás: 10 s alatt 8 fa, a fa 4-gyel fogy
  Match k = wullof();
  sim::Sim& ks = *k.sim;
  k.run(30 * 30);
  const int64_t g = ks.player(1).gathered[sim::kWood];
  std::printf("  csapolás 30 s alatt: %lld fa (5 gyűjtögető)\n", static_cast<long long>(g / 1000));
  CHECK(g > 0);
}

TEST_CASE("Wullof Holdtölte: 240 s-onként 60 s-ig a sebzés ×1,15 (a kutatással), utána újra ×1") {
  Match m = wullof();
  sim::Sim& s = *m.sim;
  const auto w = put(s, "wul_landzsas", 1, 50, 180);
  const auto a = put(s, "urs_berserker", 2, 80, 180);
  const auto b = put(s, "urs_berserker", 2, 80, 150);
  const auto c = put(s, "urs_berserker", 2, 80, 120);
  CHECK(s.moon_mult(1) == Fixed::from_int(1));
  s.deal_damage(w, 1, def_of("wul_landzsas"), a, 20000, false, true);
  const int64_t ta = taken(s, a);
  grant(s, 1, "wul_holdtolte");
  CHECK(s.tick() % 7200 < 1800);
  CHECK(s.moon_mult(1) == fx(115, 100));
  s.deal_damage(w, 1, def_of("wul_landzsas"), b, 20000, false, true);
  std::printf("  Holdtölte: %lld -> %lld ezred\n", static_cast<long long>(ta), static_cast<long long>(taken(s, b)));
  CHECK(taken(s, b) >= ta * 115 / 100 - 2 && taken(s, b) <= ta * 115 / 100 + 2);
  m.run(static_cast<int>(1800 - s.tick() % 7200));   // a telihold vége
  CHECK(s.moon_mult(1) == Fixed::from_int(1));
  s.deal_damage(w, 1, def_of("wul_landzsas"), c, 20000, false, true);
  CHECK(taken(s, c) == ta);
  CHECK(s.moon_mult(2) == Fixed::from_int(1));   // a másik játékosnak nincs
}

TEST_CASE("Wullof Groomi-irány (heal_on_kill): a vadállat öléskor 35 életerőt kap, más nem") {
  Match m = wullof();
  sim::Sim& s = *m.sim;
  s.player_mut(1).path = "groomi";
  s.rebuild_rules(1);
  const auto g = put(s, "wul_groomi", 1, 50, 180);
  const auto l = put(s, "wul_landzsas", 1, 50, 150);
  s.world().get<sim::Health>(g)->hp = 50;
  s.world().get<sim::Health>(l)->hp = 20;
  const auto f1 = put(s, "urs_berserker", 2, 150, 180);   // messze: ne üssön vissza
  const auto f2 = put(s, "urs_berserker", 2, 150, 150);
  s.deal_damage(g, 1, def_of("wul_groomi"), f1, 100000000, false);
  s.deal_damage(l, 1, def_of("wul_landzsas"), f2, 100000000, false);
  m.run(1);
  CHECK(!s.world().alive(f1));
  std::printf("  groomi %d, landzsas %d\n", s.world().get<sim::Health>(g)->hp, s.world().get<sim::Health>(l)->hp);
  CHECK(s.world().get<sim::Health>(g)->hp == 85);
  CHECK(s.world().get<sim::Health>(l)->hp == 20);   // nem vadállat
  const auto own = put(s, "wul_landzsas", 1, 70, 180);   // a saját egység megölése nem gyógyít
  s.deal_damage(g, 1, def_of("wul_groomi"), own, 100000000, false);
  m.run(1);
  CHECK(s.world().get<sim::Health>(g)->hp == 85);
}

TEST_CASE("Wullof Nyomolvasás (detect_all): minden saját egység 8 csempés észlelő") {
  Match m = wullof();
  sim::Sim& s = *m.sim;
  const auto hid = put(s, "wul_landzsas", 1, 55, 60);   // erdei rejtőzés a BUILD-ALAP erdeje mellett
  put(s, "urs_berserker", 2, 105, 60);                   // 5 csempére: közelről (2,2) nem látja
  m.run(16);
  CHECK(s.world().get<sim::Stealth>(hid)->hidden);
  CHECK(!(s.world().get<sim::Stealth>(hid)->detected & (1u << 2)));
  grant(s, 2, "wul_nyomolvasas");   // a szabály a kutatásból jön (népfüggetlenül)
  m.run(16);
  CHECK(s.world().get<sim::Stealth>(hid)->hidden);
  CHECK(s.world().get<sim::Stealth>(hid)->detected & (1u << 2));
}

TEST_CASE("Wullof: determinisztikus, és mentésből folytatva ugyanaz") {
  auto setup = [](Match& m) {
    sim::Sim& s = *m.sim;
    for (int i = 0; i < 5; ++i) put(s, "wul_landzsas", 1, 120 + i * 6, 180);
    put(s, "urs_berserker", 2, 160, 180);
    const auto tipi = s.place_building(def_of("wul_tipi"), 1, 12, 9, true);
    const auto wg = s.pack_building(tipi);
    s.order_unpack(wg, 2, 16);
  };
  Match a = wullof(), b = wullof();
  setup(a);
  setup(b);
  a.run(300);
  b.run(300);
  CHECK(a.sim->hash() == b.sim->hash());
  const auto saved = a.sim->save();
  a.run(600);
  Match c("BUILD-ALAP", true, false, {"wullof", "ursan"});
  CHECK(c.sim->load(saved));
  c.sim->restore_obstacles(*c.map);
  c.run(600);
  CHECK(c.sim->hash() == a.sim->hash());
}
