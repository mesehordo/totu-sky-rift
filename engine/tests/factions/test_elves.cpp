// SPDX-License-Identifier: GPL-3.0-or-later
// Szürke elfek (M7, ADR-008): a népmodul viselkedési próbái (src/sim/factions/elves.cpp) – energiamező és ellátás,
// behívás, Áhítat (fohász-parancs, Arany fohász), Napfúró és Napáldás, Rezonancia, Égkő-fény (energia, varázssebzés);
// determinizmus és folytatás mentésből.
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

Match elves() { return Match("BUILD-ALAP", false, true, {"elves", "ursan"}); }

void rich(sim::Sim& s) {
  for (int r = 0; r < sim::kResources; ++r) s.add_resources(1, r, 20000);
}

sim::EntityId building_of(sim::Sim& s, const char* id, uint8_t owner) {
  sim::EntityId out = sim::kNoEntity;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& b) {
    if (!out && b.def == def_of(id) && s.world().get<sim::Health>(e)->owner == owner) out = e;
  });
  return out;
}

sim::EntityId newest(sim::Sim& s, const char* id, uint8_t owner) {
  sim::EntityId out = sim::kNoEntity;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit& u) {
    if (u.def == def_of(id) && s.world().get<sim::Health>(e)->owner == owner) out = e;
  });
  return out;
}

sim::EntityId put(sim::Sim& s, const char* id, uint8_t owner, int x10, int y10) {
  const auto e = s.spawn_unit(def_of(id), owner, fx(x10, 10), fx(y10, 10));
  if (auto* c = s.world().get<sim::Combat>(e)) c->order = sim::Combat::Hold;
  return e;
}

int32_t progress(sim::Sim& s, sim::EntityId b) {
  const auto* pr = s.world().get<sim::Producer>(b);
  return pr && !pr->queue.empty() ? pr->queue.front().progress : -1;
}

sim::EntityId stone_node(sim::Sim& s) {
  sim::EntityId out = sim::kNoEntity;
  s.world().pool<sim::ResourceNode>().each([&](sim::EntityId e, const sim::ResourceNode& n) {
    if (!out && n.kind == sim::kStone) out = e;
  });
  return out;
}

void pray(sim::Sim& s, const std::vector<sim::EntityId>& units, sim::EntityId shrine) {
  sim::Command c;
  c.kind = sim::Command::Kind::Module;
  c.tick = s.tick();
  c.owner = 1;
  c.def = "elves.pray";
  c.units = units;
  c.target = shrine;
  s.queue(std::move(c));
}

void finish(sim::Sim& s, const char* research) {
  auto& r = s.player_mut(1).researched;
  r.insert(std::lower_bound(r.begin(), r.end(), std::string(research)), research);
}

}  // namespace

TEST_CASE("Szürke elfek: a népmodul betöltve") {
  Match m = elves();
  CHECK(m.ok);
  bool found = false;
  for (const auto& mod : m.sim->modules_) found = found || std::string(mod->name()) == "elves";
  CHECK(found);
}

TEST_CASE("Szürke elfek energiamező: a nem `no_power` épület csak kész Napobeliszk 6,5 csempés körébe kerülhet") {
  Match m = elves();
  sim::Sim& s = *m.sim;
  rich(s);
  std::string err;
  // a főépület (Napszentély) nem ad mezőt: obeliszk nélkül a csarnok nem rakható le
  CHECK(!s.can_place(1, def_of("elf_harcos_csarnok"), 14, 9, &err));
  CHECK(err.find("energiamező") != std::string::npos);
  // a `no_power` obeliszk bárhová
  CHECK(s.can_place(1, def_of("elf_obeliszk"), 12, 9));
  const auto ob = s.place_building(def_of("elf_obeliszk"), 1, 12, 9, true);
  // a középpont (15,5; 10,5) 2,5 csempére az obeliszk közepétől (13; 10)
  CHECK(s.can_place(1, def_of("elf_harcos_csarnok"), 14, 9));
  // (3,5; 18,5): ~12,8 csempe – kívül
  err.clear();
  CHECK(!s.can_place(1, def_of("elf_harcos_csarnok"), 2, 17, &err));
  CHECK(err.find("energiamező") != std::string::npos);
  // félkész obeliszk még nem ad mezőt
  s.kill(ob);
  m.run(2);
  s.place_building(def_of("elf_obeliszk"), 1, 12, 9, false);
  CHECK(!s.can_place(1, def_of("elf_harcos_csarnok"), 14, 9));
  // a másik nép (ursan) nem kötött a mezőhöz
  for (int r = 0; r < sim::kResources; ++r) s.add_resources(2, r, 20000);
  err.clear();
  s.can_place(2, def_of("urs_harcosok_haza"), 20, 14, &err);
  CHECK(err.find("energiamező") == std::string::npos);
}

TEST_CASE("Szürke elfek ellátás: a mező nélküli csarnok sora áll, a mező nélküli Jádeoszlop nem lő") {
  Match m = elves();
  sim::Sim& s = *m.sim;
  rich(s);
  const auto ob = s.place_building(def_of("elf_obeliszk"), 1, 12, 9, true);
  const auto hall = s.place_building(def_of("elf_harcos_csarnok"), 1, 14, 9, true);
  CHECK(s.train(1, def_of("elf_tollas_harcos"), hall));
  m.run(60);
  const int32_t p0 = progress(s, hall);
  CHECK(p0 >= 55);
  s.kill(ob);
  m.run(2);
  const int32_t p1 = progress(s, hall);
  m.run(90);
  CHECK(progress(s, hall) == p1);   // ellátatlan: nem halad
  s.place_building(def_of("elf_obeliszk"), 1, 12, 9, true);
  m.run(30);
  CHECK(progress(s, hall) >= p1 + 29);   // újra ellátva: halad
  // torony: mező nélkül nem lő, mezővel igen
  const auto tower = s.place_building(def_of("elf_jade_oszlop"), 1, 22, 15, true);
  const auto foe = put(s, "urs_pajzshordozo", 2, 255, 165);
  const int32_t hp0 = s.world().get<sim::Health>(foe)->hp;
  m.run(120);
  CHECK(s.world().get<sim::Health>(foe)->hp == hp0);
  s.place_building(def_of("elf_obeliszk"), 1, 19, 15, true);
  m.run(120);
  CHECK(s.world().get<sim::Health>(foe)->hp < hp0);
  (void)tower;
}

TEST_CASE("Szürke elfek behívás: a gyülekezőpont mezőadójánál jelenik meg az új egység") {
  Match m = elves();
  sim::Sim& s = *m.sim;
  rich(s);
  s.place_building(def_of("elf_obeliszk"), 1, 12, 9, true);
  const auto hall = s.place_building(def_of("elf_harcos_csarnok"), 1, 14, 9, true);
  s.place_building(def_of("elf_obeliszk"), 1, 24, 16, true);   // közepe (25; 17)
  s.set_rally(hall, Fixed::from_int(27), Fixed::from_int(16), sim::kNoEntity);
  CHECK(s.train(1, def_of("elf_tollas_harcos"), hall));
  sim::EntityId u = sim::kNoEntity;
  for (int i = 0; i < 700 && !u; ++i) {
    m.run(1);
    u = newest(s, "elf_tollas_harcos", 1);
  }
  CHECK(u != sim::kNoEntity);
  if (!u) return;
  const auto& p = *s.world().get<sim::Position>(u);
  CHECK(core::length(p.x - Fixed::from_int(25), p.y - Fixed::from_int(17)) < Fixed::from_int(3));
  // gyülekezőpont mezőn kívül: a csarnok mellett marad
  s.set_rally(hall, Fixed::from_int(3), Fixed::from_int(18), sim::kNoEntity);
  CHECK(s.train(1, def_of("elf_tollas_harcos"), hall));
  sim::EntityId v = u;
  for (int i = 0; i < 700 && v == u; ++i) {
    m.run(1);
    v = newest(s, "elf_tollas_harcos", 1);
  }
  CHECK(v != u);
  const auto& q = *s.world().get<sim::Position>(v);
  CHECK(core::length(q.x - Fixed::from_int(15), q.y - fx(21, 2)) < Fixed::from_int(4));
}

TEST_CASE("Szürke elfek Áhítat: a szentély 4 helye – 5 tétlen munkásból 4 fohászkodik, 0,3 / s fejenként") {
  Match m = elves();
  sim::Sim& s = *m.sim;
  const auto hq = building_of(s, "elf_napszentely", 1);
  CHECK(hq != sim::kNoEntity);
  if (!hq) return;
  m.run(5);
  const sim::Building b = *s.world().get<sim::Building>(hq);
  for (int i = 0; i < 5; ++i) put(s, "elf_kofarago", 1, b.tx * 10 - 5, b.ty * 10 + 5 + i * 8);
  const int64_t f0 = s.player(1).res[sim::kFaith];
  m.run(300);   // 20 ciklus × 4 × 0,3 × 0,5 = 12 Áhítat
  const int64_t gain = s.player(1).res[sim::kFaith] - f0;
  std::printf("  Áhítat 10 s alatt: %lld ezred\n", static_cast<long long>(gain));
  CHECK(gain >= 11900 && gain <= 12000);
  CHECK(s.player(1).gathered[sim::kFaith] >= gain);
}

TEST_CASE("Szürke elfek Napfúró: 0,4 passzív + mellette álló munkások, carry / (gather_time + 7,4); Napáldás ×1,5") {
  Match m = elves();
  sim::Sim& s = *m.sim;
  const auto node = stone_node(s);
  CHECK(node != sim::kNoEntity);
  if (!node) return;
  const sim::ResourceNode n0 = *s.world().get<sim::ResourceNode>(node);
  const auto mine = s.place_building(def_of("elf_napfuro"), 1, n0.tx, n0.ty, true);
  const sim::ResourceDef& rd = R().resources[sim::kStone];
  const double per_s = rd.carry / (rd.gather_time.raw() / 65536.0 + 7.4);   // 6 / 10 = 0,6
  const auto amount = [&] { return s.world().get<sim::ResourceNode>(node)->amount; };
  int32_t a0 = amount();
  m.run(300);   // 10 s × 0,4 × 0,6 = 2,4 → 2 egység
  const double want1 = 0.4 * per_s * 10;
  std::printf("  Napfúró passzív (10 s): %d egység, várt %.2f\n", a0 - amount(), want1);
  CHECK(a0 - amount() >= static_cast<int>(want1) - 1 && a0 - amount() <= static_cast<int>(want1) + 1);
  // két munkás a bánya mellett: (0,4 + 2) × 0,6 × 10 = 14,4
  const sim::Building bb = *s.world().get<sim::Building>(mine);
  put(s, "elf_kofarago", 1, bb.tx * 10 - 4, bb.ty * 10 + 5);
  put(s, "elf_kofarago", 1, bb.tx * 10 - 4, bb.ty * 10 + 15);
  a0 = amount();
  m.run(300);
  const double want2 = 2.4 * per_s * 10;
  std::printf("  Napfúró 2 munkással (10 s): %d egység, várt %.2f\n", a0 - amount(), want2);
  CHECK(a0 - amount() >= static_cast<int>(want2) - 1 && a0 - amount() <= static_cast<int>(want2) + 1);
  // Napáldás: 25 Áhítat, 30 s ×1,5; újratöltés alatt nem ismételhető, Áhítat nélkül nem megy
  CHECK(!s.use_power(1, mine, "napaldas"));
  s.add_resources(1, sim::kFaith, 60);
  CHECK(s.use_power(1, mine, "napaldas"));
  CHECK(s.player(1).resource(sim::kFaith) == 35);
  CHECK(!s.use_power(1, mine, "napaldas"));
  a0 = amount();
  m.run(300);
  const double want3 = want2 * 1.5;
  std::printf("  Napfúró Napáldással (10 s): %d egység, várt %.2f\n", a0 - amount(), want3);
  CHECK(a0 - amount() >= static_cast<int>(want3) - 1 && a0 - amount() <= static_cast<int>(want3) + 1);
  // a nem bánya épület nem kapja
  CHECK(!s.use_power(1, building_of(s, "elf_napszentely", 1), "napaldas"));
}

TEST_CASE("Szürke elfek Égkő-fény: az energiatöltés ×2,0 (a közös ×1,5 helyett) az Égkő 5 csempés körében") {
  Match m = elves();
  sim::Sim& s = *m.sim;
  s.place_building(def_of("elf_egko"), 1, 18, 12, true);
  const auto near = put(s, "elf_tollas_harcos", 1, 190, 140);
  const auto far = put(s, "elf_tollas_harcos", 1, 25, 100);
  m.run(1);
  s.world().get<sim::Caster>(near)->energy = 0;
  s.world().get<sim::Caster>(far)->energy = 0;
  m.run(300);
  const int32_t en = s.world().get<sim::Caster>(near)->energy, ef = s.world().get<sim::Caster>(far)->energy;
  std::printf("  energia 10 s alatt: Égkő mellett %d, távol %d ezred\n", en, ef);
  CHECK(ef >= 7700 && ef <= 8100);              // 0,8 / s (tickenként ezredre kerekítve)
  CHECK(en >= 15600 && en <= 16200);            // 0,8 × 2,0 / s
}

TEST_CASE("Szürke elfek: determinizmus és folytatás mentésből") {
  const auto setup = [](Match& m) {
    sim::Sim& s = *m.sim;
    rich(s);
    s.place_building(def_of("elf_obeliszk"), 1, 12, 9, true);
    const auto hall = s.place_building(def_of("elf_harcos_csarnok"), 1, 14, 9, true);
    const auto far = s.place_building(def_of("elf_obeliszk"), 1, 24, 16, true);
    s.set_rally(hall, Fixed::from_int(27), Fixed::from_int(16), sim::kNoEntity);
    s.train(1, def_of("elf_tollas_harcos"), hall);
    s.train(1, def_of("elf_tollas_harcos"), hall);
    const sim::ResourceNode n = *s.world().get<sim::ResourceNode>(stone_node(s));
    const auto mine = s.place_building(def_of("elf_napfuro"), 1, n.tx, n.ty, true);
    s.use_power(1, mine, "napaldas");
    const auto hq = building_of(s, "elf_napszentely", 1);
    const sim::Building b = *s.world().get<sim::Building>(hq);
    for (int i = 0; i < 3; ++i) put(s, "elf_kofarago", 1, b.tx * 10 - 5, b.ty * 10 + 5 + i * 8);
    pray(s, {put(s, "elf_kofarago", 1, (b.tx + b.w + 5) * 10, b.ty * 10 + 5)}, hq);
    finish(s, "elf_rezonancia");
    s.place_building(def_of("elf_egko"), 1, 18, 12, true);
    put(s, "elf_tollas_harcos", 1, 190, 140);
    s.kill(far);   // a mezőadó halála: a következő egység már nem hívódik be
  };
  Match a = elves(), b = elves();
  setup(a);
  setup(b);
  a.run(150);
  b.run(150);
  CHECK(a.sim->hash() == b.sim->hash());
  const auto saved = a.sim->save();
  a.run(750);
  Match c("BUILD-ALAP", false, false, {"elves", "ursan"});
  CHECK(c.sim->load(saved));
  c.sim->restore_obstacles(*c.map);
  c.run(750);
  CHECK(c.sim->hash() == a.sim->hash());
}

TEST_CASE("Szürke elfek fohász-parancs (`elves.pray`): a munkások a szentélyhez mennek és fohászkodnak; Arany fohász 0,4 / s") {
  Match m = elves();
  sim::Sim& s = *m.sim;
  const auto hq = building_of(s, "elf_napszentely", 1);
  CHECK(hq != sim::kNoEntity);
  if (!hq) return;
  const sim::Building b = *s.world().get<sim::Building>(hq);
  std::vector<sim::EntityId> ws;
  for (int i = 0; i < 3; ++i) ws.push_back(put(s, "elf_kofarago", 1, (b.tx + b.w + 5) * 10, b.ty * 10 + 5 + i * 10));
  m.run(5);
  const int64_t f0 = s.player(1).res[sim::kFaith];
  m.run(150);
  CHECK(s.player(1).res[sim::kFaith] == f0);   // távol: nem fohászkodnak
  pray(s, ws, sim::kNoEntity);   // 0: a legközelebbi szentély
  m.run(300);
  const int64_t f1 = s.player(1).res[sim::kFaith];
  m.run(300);   // 20 ciklus × 3 × 0,3 × 0,5 = 9 Áhítat
  const int64_t gain = s.player(1).res[sim::kFaith] - f1;
  std::printf("  fohász-parancs, 3 munkás, 10 s: %lld ezred\n", static_cast<long long>(gain));
  CHECK(gain >= 8900 && gain <= 9000);
  finish(s, "elf_arany_fohasz");
  const int64_t f2 = s.player(1).res[sim::kFaith];
  m.run(300);   // 20 × 3 × 0,4 × 0,5 = 12
  const int64_t gain2 = s.player(1).res[sim::kFaith] - f2;
  std::printf("  Arany fohásszal, 10 s: %lld ezred\n", static_cast<long long>(gain2));
  CHECK(gain2 >= 11900 && gain2 <= 12000);
  // más nép parancsa nem ide tartozik; az idegen munkás nem mozdul
  const auto foe = put(s, "urs_pajzshordozo", 2, 50, 150);
  const sim::Position fp = *s.world().get<sim::Position>(foe);
  pray(s, {foe}, hq);
  m.run(30);
  CHECK(s.world().get<sim::Position>(foe)->x == fp.x);
}

TEST_CASE("Szürke elfek Rezonancia: a mezőadók köre +2 csempe (`Rules.power_add`)") {
  Match m = elves();
  sim::Sim& s = *m.sim;
  rich(s);
  s.place_building(def_of("elf_obeliszk"), 1, 12, 9, true);   // közepe (13; 10), 6,5 csempe
  std::string err;
  // a csarnok közepe (20,5; 10,5): ~7,5 csempe – a körön kívül
  CHECK(!s.can_place(1, def_of("elf_harcos_csarnok"), 19, 9, &err));
  CHECK(err.find("energiamező") != std::string::npos);
  finish(s, "elf_rezonancia");
  err.clear();
  s.can_place(1, def_of("elf_harcos_csarnok"), 19, 9, &err);
  CHECK(err.find("energiamező") == std::string::npos);
  // 12,8 csempe: így is kívül
  err.clear();
  CHECK(!s.can_place(1, def_of("elf_harcos_csarnok"), 2, 17, &err));
  CHECK(err.find("energiamező") != std::string::npos);
}

TEST_CASE("Szürke elfek Égkő-fény: a varázssebzés × 1,25 (a közös × 1 helyett) a támadó Égkő melletti helyén") {
  Match m = elves();
  sim::Sim& s = *m.sim;
  s.place_building(def_of("elf_egko"), 1, 18, 12, true);
  const auto near = put(s, "elf_tollas_harcos", 1, 190, 140);
  const auto far = put(s, "elf_tollas_harcos", 1, 25, 100);
  const auto t1 = put(s, "urs_pajzshordozo", 2, 60, 160);
  const auto t2 = put(s, "urs_pajzshordozo", 2, 80, 160);
  m.run(1);
  CHECK(s.in_rift(near) && !s.in_rift(far));
  const int32_t h1 = s.world().get<sim::Health>(t1)->hp, h2 = s.world().get<sim::Health>(t2)->hp;
  const uint16_t d = def_of("elf_tollas_harcos");
  s.deal_damage(near, 1, d, t1, 20000, false, true);
  s.deal_damage(far, 1, d, t2, 20000, false, true);
  const int32_t l1 = h1 - s.world().get<sim::Health>(t1)->hp, l2 = h2 - s.world().get<sim::Health>(t2)->hp;
  std::printf("  varázssebzés: Égkő mellől %d, távolról %d\n", l1, l2);
  CHECK(l1 == 25);
  CHECK(l2 == 20);
  // a nem varázs sebzés nem változik
  const int32_t h3 = s.world().get<sim::Health>(t1)->hp;
  s.deal_damage(near, 1, d, t1, 20000, false, false);
  const int32_t h4 = s.world().get<sim::Health>(t2)->hp;
  s.deal_damage(far, 1, d, t2, 20000, false, false);
  CHECK(h3 - s.world().get<sim::Health>(t1)->hp == h4 - s.world().get<sim::Health>(t2)->hp);
}

TEST_CASE("Szürke elfek Napszemű papok: a Napszóró energiája × 1,5-tel töltődik (`energy_regen_ids`), másé nem") {
  Match m = elves();
  sim::Sim& s = *m.sim;
  const auto p = put(s, "elf_napszoro", 1, 25, 100);
  const auto o = put(s, "elf_tollas_harcos", 1, 45, 100);
  finish(s, "elf_napszemu_papok");
  m.run(1);
  s.world().get<sim::Caster>(p)->energy = 0;
  s.world().get<sim::Caster>(o)->energy = 0;
  m.run(300);
  const int32_t ep = s.world().get<sim::Caster>(p)->energy, eo = s.world().get<sim::Caster>(o)->energy;
  std::printf("  energia 10 s alatt: Napszóró %d, harcos %d ezred\n", ep, eo);
  CHECK(eo >= 7700 && eo <= 8100);   // 0,8 / s
  CHECK(ep >= 11700 && ep <= 12100);  // 0,8 × 1,5 / s
}
