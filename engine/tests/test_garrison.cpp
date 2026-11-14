// SPDX-License-Identifier: GPL-3.0-or-later
// Helyőrség és tornyok (D39), kutatási szabályok (KUT-RULE), felemelkedés és fejlődési irány (MECH-ASC), főépület-
// képesség (Eskü-riadó), Égkő-fény (MECH-RIFT) – M6, 8. lépés; a régi játék szerint.
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
Fixed fx(int num, int den) { return Fixed::from_ratio(num, den); }

// a BUILD-ALAP alsó, sík sávja (y 17–19; az 1. játékos munkásai fölötte, a 2. játékoséi a pálya tetején járnak)
sim::EntityId put(sim::Sim& s, const char* id, uint8_t owner, int x10, int y10, bool hold = true) {
  const sim::EntityId e = s.spawn_unit(def_of(id), owner, fx(x10, 10), fx(y10, 10));
  if (hold) {
    if (auto* c = s.world().get<sim::Combat>(e)) c->order = sim::Combat::Hold;
  }
  return e;
}

int64_t taken(sim::Sim& s, sim::EntityId e) {
  const auto* h = s.world().alive(e) ? s.world().get<sim::Health>(e) : nullptr;
  if (!h) return -1000000;   // elpusztult
  return int64_t{h->max - h->hp} * 1000 + s.world().get<sim::Vital>(e)->deficit;
}

int64_t next_hit(Match& m, sim::EntityId target, int limit = 300) {
  const int64_t before = taken(*m.sim, target);
  for (int i = 0; i < limit && taken(*m.sim, target) == before; ++i) m.sim->step();
  return taken(*m.sim, target) - before;
}

void research_now(Match& m, uint8_t owner, const char* id) {
  sim::Sim& s = *m.sim;
  s.add_resources(owner, sim::kWood, 5000);
  s.add_resources(owner, sim::kStone, 5000);
  s.add_resources(owner, sim::kMeteor, 5000);
  std::string err;
  CHECK(s.research(owner, id, &err));
  if (!err.empty()) std::printf("  kutatás %s: %s\n", id, err.c_str());
  m.run(R().research(id)->time_ticks + 2);
}

sim::EntityId hq_of(sim::Sim& s, uint8_t owner) {
  sim::EntityId out = sim::kNoEntity;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& b) {
    if (b.def == def_of("urs_nagyterem") && s.world().get<sim::Health>(e)->owner == owner) out = e;
  });
  return out;
}

}  // namespace

TEST_CASE("helyőrség: beszállás a szabad helyekig, bent rejtve és sebezhetetlenül; kiszállás az épület köré") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto house = s.place_building(def_of("urs_hosszuhaz"), 1, 2, 17, true);   // 4 hely
  std::vector<sim::EntityId> us;
  for (int i = 0; i < 5; ++i) us.push_back(put(s, "urs_pajzshordozo", 1, 60 + i * 9, 185, false));
  CHECK(s.order_garrison(us, house));
  m.run(150);
  CHECK(s.garrison_count(house) == 4);
  int inside = 0;
  for (const auto e : us) inside += s.sheltered(e) ? 1 : 0;
  CHECK(inside == 4);
  const auto out = us[4];   // az ötödiknek nem jutott hely
  CHECK(!s.sheltered(out));
  // bent: nem látszik az ellenségnek, nem célozható
  const auto foe = put(s, "urs_szigonyvezo", 2, 60, 195, false);
  m.run(6);
  CHECK(!s.is_visible(2, us[0]));
  CHECK(!s.order_attack(foe, us[0]));
  // kiszállás: mindenki az épület köré, újra mozgatható
  CHECK(s.unload(house));
  CHECK(s.garrison_count(house) == 0);
  for (int i = 0; i < 4; ++i) {
    CHECK(!s.sheltered(us[static_cast<size_t>(i)]));
    const auto& p = *s.world().get<sim::Position>(us[static_cast<size_t>(i)]);
    CHECK(!(p.x >= fx(20, 10) && p.x < fx(40, 10) && p.y >= fx(170, 10) && p.y < fx(190, 10)));   // nem az épületben
    CHECK(s.world().get<sim::Body>(us[static_cast<size_t>(i)]) != nullptr);
  }
  CHECK(s.order_move(us[0], fx(80, 10), fx(185, 10)));
  m.run(60);
  CHECK(s.world().get<sim::Position>(us[0])->x > fx(60, 10));
  // a beszállás képesség nélkül (kardfogú) nem megy, és a félkész épületbe sem
  const auto cat = put(s, "urs_kardfogu", 1, 50, 190, false);
  CHECK(!s.order_garrison({cat}, house));
  const auto site = s.place_building(def_of("urs_hosszuhaz"), 1, 12, 17, false);
  CHECK(!s.order_garrison({us[0]}, site));
}

TEST_CASE("helyőrség: a lerombolt épületből a bent lévők kiszállnak; bentről nem kap parancsot") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto house = s.place_building(def_of("urs_hosszuhaz"), 1, 2, 17, true);
  const auto a = put(s, "urs_berserker", 1, 15, 175, false);
  const auto b = put(s, "urs_berserker", 1, 15, 190, false);
  CHECK(s.order_garrison({a, b}, house));
  m.run(30);
  CHECK(s.garrison_count(house) == 2);
  CHECK(!s.order_move(a, fx(80, 10), fx(185, 10)) || s.sheltered(a));   // bentről nem mozdul
  sim::Command mv;
  mv.kind = sim::Command::Kind::Move;
  mv.tick = s.tick();
  mv.units = {a};
  mv.x = fx(80, 10);
  mv.y = fx(185, 10);
  s.queue(mv);
  m.run(5);
  CHECK(s.sheltered(a));
  s.deal_damage(0, 2, sim::kNoDef, house, 10000000, false);   // lerombolva
  m.run(1);
  CHECK(!s.world().alive(house));
  CHECK(!s.sheltered(a) && !s.sheltered(b));
  CHECK(s.world().get<sim::Health>(a)->hp == 150);   // épen (a régi `unload` szerint)
}

TEST_CASE("helyőrség-íj: az üres Hosszúház nem lő, bent 2 egységgel 5 + 2 × 4 = 13 sebzést; az Őrtorony 12 + 4 / fő") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto house = s.place_building(def_of("urs_hosszuhaz"), 1, 2, 17, true);
  const auto foe = put(s, "urs_berserker", 2, 65, 178);   // 2,5 csempére a háztól
  m.run(60);
  CHECK(taken(s, foe) == 0);   // üresen nincs fegyvere
  const auto g1 = put(s, "urs_berserker", 1, 12, 180, false);
  const auto g2 = put(s, "urs_berserker", 1, 12, 186, false);
  CHECK(s.order_garrison({g1, g2}, house));
  const int64_t hit = next_hit(m, foe);
  std::printf("  Hosszúház 2 bent lévővel: %lld (13 − 1 páncél)\n", static_cast<long long>(hit));
  CHECK(hit == 12000);
  // Őrtorony: saját íj 12; 4 bent lévővel 12 + 16; a Gyors helyőrség után fejenként 7
  Match t("BUILD-ALAP");
  sim::Sim& ts = *t.sim;
  const auto tower = ts.place_building(def_of("urs_ortorony"), 1, 2, 17, true);
  const auto f2 = put(ts, "urs_berserker", 2, 70, 178);
  t.run(6);
  CHECK(next_hit(t, f2) == 11000);
  std::vector<sim::EntityId> gs;
  for (int i = 0; i < 4; ++i) gs.push_back(put(ts, "urs_pajzshordozo", 1, 12, 172 + i * 6, false));
  CHECK(ts.order_garrison(gs, tower));
  t.run(40);
  CHECK(ts.garrison_count(tower) == 4);
  next_hit(t, f2);
  const int64_t h4 = next_hit(t, f2);
  CHECK(h4 == 27000);
  research_now(t, 1, "urs_gyors_helyorseg");
  const auto f3 = put(ts, "urs_berserker", 2, 70, 186);   // az előzőt közben lelőtte
  t.run(6);
  next_hit(t, f3);
  const int64_t h7 = next_hit(t, f3);
  std::printf("  Őrtorony: üresen 11, 4 bent lévővel %lld, Gyors helyőrséggel %lld\n", static_cast<long long>(h4 / 1000),
              static_cast<long long>(h7 / 1000));
  CHECK(h7 == 39000);
}

TEST_CASE("kutatási szabályok: Kiadós tanya (+4 létszám tanyánként), Medve-ösztön (40% alatt ×1,2), Tüskés pajzsfal") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  s.place_building(def_of("urs_tanya"), 1, 2, 16, true);
  m.run(1);
  const int32_t cap0 = s.player(1).supply_cap;
  research_now(m, 1, "urs_kiados_tanya");
  CHECK(s.player(1).supply_cap == cap0 + 4);
  // Tüskés pajzsfal: a berserker (közelharcos) a pajzshordozó ellen nem kap ×1,25-öt
  s.place_building(def_of("urs_harcosok_haza"), 1, 12, 9, true);
  s.place_building(def_of("urs_runakovacs"), 1, 16, 9, true);
  const auto sh = put(s, "urs_pajzshordozo", 1, 80, 185);
  const auto bz = put(s, "urs_berserker", 2, 88, 185);
  m.run(6);
  CHECK(s.order_attack(bz, sh));
  CHECK(next_hit(m, sh) == 18000);   // 16 × 1,25 − 2
  research_now(m, 1, "urs_tuskes_pajzsfal");
  const auto sh2 = put(s, "urs_pajzshordozo", 1, 80, 195);   // az előzőt közben levágta
  const auto bz2 = put(s, "urs_berserker", 2, 88, 195);
  m.run(6);
  CHECK(s.order_attack(bz2, sh2));
  CHECK(next_hit(m, sh2) == 14000);   // 16 − 2
  // Medve-ösztön: 40% alatt × 1,2
  s.place_building(def_of("urs_eskufa"), 1, 12, 13, true);
  research_now(m, 1, "urs_medve_osztön");
  const auto me = put(s, "urs_berserker", 1, 30, 190);
  const auto target = put(s, "urs_berserker", 2, 38, 190);
  s.world().get<sim::Health>(me)->hp = 50;   // 33%
  m.run(6);
  CHECK(s.order_attack(me, target));
  const int64_t rage = next_hit(m, target);
  std::printf("  Medve-ösztön 33%%-on: %lld (16 × 1,2 − 1)\n", static_cast<long long>(rage));
  CHECK(rage >= 18198 && rage <= 18200);   // 1,2 fixpontosan
}

TEST_CASE("felemelkedés és fejlődési irány: 3 különböző fajta kész épület; az irány egyszer, hatással és zárral") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  CHECK(!s.choose_path(1, "tundra"));   // felemelkedés nélkül nem
  s.place_building(def_of("urs_fatelep"), 1, 2, 17, true);   // leadóhely: nem számít
  s.place_building(def_of("urs_hosszuhaz"), 1, 5, 17, true);
  s.place_building(def_of("urs_hosszuhaz"), 1, 12, 17, true);   // azonos fajta: egyszer számít
  s.place_building(def_of("urs_harcosok_haza"), 1, 12, 9, true);
  m.run(16);
  CHECK(!s.player(1).ascended);
  CHECK(s.missing_requirements(1, R().defs[def_of("urs_vadak_karama")]));
  s.place_building(def_of("urs_runakovacs"), 1, 16, 9, true);
  m.run(16);
  CHECK(s.player(1).ascended);
  const auto cat = put(s, "urs_kardfogu", 1, 30, 190);
  const auto zerk = put(s, "urs_berserker", 1, 40, 190);
  CHECK(s.choose_path(1, "tundra"));
  CHECK(!s.choose_path(1, "berserker"));   // egyszer
  CHECK(s.player(1).path == "tundra");
  CHECK(s.world().get<sim::Health>(cat)->max == 270 && s.world().get<sim::Health>(cat)->hp == 270);   // +60 (vad, lovas)
  CHECK(s.world().get<sim::Health>(zerk)->max == 150);
  CHECK(!s.missing_requirements(1, R().defs[def_of("urs_vadak_karama")]));
  CHECK(s.missing_requirements(1, R().defs[def_of("urs_verfogadalom")]));   // a másik irány zárva
  // a tundra-irány +1 életerő/s a vadaknak és a lovasoknak
  s.world().get<sim::Health>(cat)->hp = 200;
  m.run(300);
  std::printf("  tundra: a kardfogú 10 s alatt 200 → %d\n", s.world().get<sim::Health>(cat)->hp);
  CHECK(s.world().get<sim::Health>(cat)->hp >= 209 && s.world().get<sim::Health>(cat)->hp <= 211);
}

TEST_CASE("fejlődési irány: a berserker-ág vérszomja (sebesülten erősebb), a rúna-ág épületpáncélja és tornya") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  s.place_building(def_of("urs_hosszuhaz"), 1, 5, 17, true);
  s.place_building(def_of("urs_harcosok_haza"), 1, 12, 9, true);
  s.place_building(def_of("urs_runakovacs"), 1, 16, 9, true);
  m.run(16);
  CHECK(s.choose_path(1, "berserker"));
  const auto me = put(s, "urs_berserker", 1, 30, 190);
  const auto pap = put(s, "urs_pajzshordozo", 2, 38, 190);
  s.world().get<sim::Afflict>(pap)->stun_until = 1000000;   // kábultan nem üt vissza (a berserker életereje marad)
  s.world().get<sim::Health>(me)->hp = 75;   // fél életerő: × (1 + 0,6 × 0,5)
  m.run(6);
  CHECK(s.order_attack(me, pap));
  const int64_t hit = next_hit(m, pap);
  std::printf("  vérszomj-ág fél életerővel: %lld (16 × 1,3 × 1,25 − 2)\n", static_cast<long long>(hit));
  CHECK(hit >= 23998 && hit <= 24000);
  // rúna-ág: épület +3 páncél, torony +6 sebzés
  Match r("BUILD-ALAP");
  sim::Sim& rs = *r.sim;
  rs.place_building(def_of("urs_hosszuhaz"), 1, 5, 17, true);
  rs.place_building(def_of("urs_harcosok_haza"), 1, 12, 9, true);
  rs.place_building(def_of("urs_runakovacs"), 1, 16, 9, true);
  const auto tower = rs.place_building(def_of("urs_ortorony"), 1, 2, 17, true);
  const auto f = put(rs, "urs_berserker", 2, 70, 178);
  r.run(16);
  CHECK(next_hit(r, f) == 11000);   // 12 − 1
  const auto atk = put(rs, "urs_berserker", 2, 45, 185, false);
  r.run(6);
  CHECK(rs.order_attack(atk, tower));
  const int64_t b0 = next_hit(r, tower);
  CHECK(rs.choose_path(1, "runa"));
  next_hit(r, f);   // a választás előtt kilőtt nyíl még a régi sebzéssel érkezik
  const int64_t h1 = next_hit(r, f);
  std::printf("  rúna-ág: a torony lövése %lld\n", static_cast<long long>(h1));
  CHECK(h1 == 17000);   // 12 + 6 (torony) − 1
  const int64_t b1 = next_hit(r, tower);
  std::printf("  rúna-ág: a torony %lld helyett %lld sebzést kap\n", static_cast<long long>(b0), static_cast<long long>(b1));
  CHECK(b1 == b0 - 3000);   // +3 épületpáncél
}

TEST_CASE("Eskü-riadó (főépület-képesség): minden munkás Fegyverbe! 30 s-ig, kutatás nélkül; 240 s újratöltés") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  m.run(90);
  const auto hq = hq_of(s, 1);
  CHECK(hq != sim::kNoEntity);
  CHECK(s.use_power(1, hq, "gp_eskufa_riado"));
  const auto fegy = static_cast<uint16_t>(R().ability_index("fegyverbe"));
  int buffed = 0;
  for (const auto w : m.workers(1)) buffed += s.buffed(w, fegy) ? 1 : 0;
  CHECK(buffed == 5);
  for (const auto w : m.workers(1)) CHECK(s.world().get<sim::Worker>(w)->state == sim::Worker::Idle);
  CHECK(!s.use_power(1, hq, "gp_eskufa_riado"));   // újratöltés
  CHECK(!s.use_power(2, hq, "gp_eskufa_riado"));   // nem a sajátja
  m.run(30 * 30 + 2);
  int working = 0;
  for (const auto w : m.workers(1)) working += s.world().get<sim::Worker>(w)->state != sim::Worker::Idle ? 1 : 0;
  CHECK(working == 5);   // a riadó után visszamentek dolgozni
  m.run(210 * 30);
  CHECK(s.use_power(1, hq, "gp_eskufa_riado"));
}

TEST_CASE("Égkő-fény: az Égkő 5 csempés körében a támadó varázsló +1 lőtávot, a hős 1,5-szörös energiát kap") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  s.place_building(def_of("urs_egko"), 1, 3, 18, true);
  const auto hja = put(s, "urs_hos_hjalmar", 1, 65, 185);   // 3 csempére
  const auto far = put(s, "urs_hos_hjalmar", 1, 150, 185);  // 11 csempére
  const auto zerk = put(s, "urs_berserker", 1, 50, 185);
  m.run(1);
  CHECK(s.in_rift(hja) && !s.in_rift(far) && s.in_rift(zerk));
  CHECK(s.unit_mods(hja).range_add == Fixed::from_int(1));
  CHECK(s.unit_mods(zerk).range_add.raw() == 0);   // nem varázsló
  const int32_t e0 = s.world().get<sim::Caster>(hja)->energy, f0 = s.world().get<sim::Caster>(far)->energy;
  m.run(30);
  const int32_t de = s.world().get<sim::Caster>(hja)->energy - e0, df = s.world().get<sim::Caster>(far)->energy - f0;
  std::printf("  energia 1 s alatt: Égkő mellett %d, távol %d (ezred)\n", de, df);
  CHECK(de >= df * 3 / 2 - 30 && de <= df * 3 / 2 + 30);
}

TEST_CASE("helyőrség és irány: determinisztikus, és mentésből folytatva ugyanaz") {
  auto setup = [](Match& m) {
    sim::Sim& s = *m.sim;
    const auto tower = s.place_building(def_of("urs_ortorony"), 1, 2, 17, true);
    s.place_building(def_of("urs_hosszuhaz"), 1, 5, 17, true);
    s.place_building(def_of("urs_harcosok_haza"), 1, 12, 9, true);
    s.place_building(def_of("urs_runakovacs"), 1, 16, 9, true);
    std::vector<sim::EntityId> gs;
    for (int i = 0; i < 3; ++i) gs.push_back(put(s, "urs_pajzshordozo", 1, 12, 172 + i * 6, false));
    s.order_garrison(gs, tower);
    put(s, "urs_berserker", 2, 110, 180, false);
    put(s, "urs_szigonyvezo", 2, 120, 185, false);
    m.run(16);
    s.choose_path(1, "runa");
    s.use_power(1, hq_of(s, 1), "gp_eskufa_riado");
  };
  Match a("BUILD-ALAP"), b("BUILD-ALAP");
  setup(a);
  setup(b);
  a.run(200);
  b.run(200);
  CHECK(a.sim->hash() == b.sim->hash());
  const auto saved = a.sim->save();
  a.run(400);
  Match c("BUILD-ALAP", true, false);
  CHECK(c.sim->load(saved));
  c.sim->restore_obstacles(*c.map);
  c.run(400);
  CHECK(c.sim->hash() == a.sim->hash());
  std::printf("  helyőrség-állapothash (616 tick): 0x%016llx\n", static_cast<unsigned long long>(a.sim->hash()));
  CHECK(a.sim->hash() == 0xcde66eec501ce873ULL);   // minden rendszeren ugyanez
}
