// SPDX-License-Identifier: GPL-3.0-or-later
// Közös képesség-szabályok a régi játék szerint (M7, leltár-kapu): képességárak (életerő, áhítat), a célzott lövés saját
// mérge, területi sebzése és pattanása, a nova épületsebzése és önrobbanása, a gyógyítás célcímkéi, lassulás az
// önerősítés után, „nem gyenge” önerősítés alatt, a passzív képesség felfedése, a főépület-képesség pontfelfedése.
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
Fixed fx(int a, int b) { return Fixed::from_ratio(a, b); }

sim::EntityId put(sim::Sim& s, const char* id, uint8_t owner, int x10, int y10) {
  const auto e = s.spawn_unit(def_of(id), owner, fx(x10, 10), fx(y10, 10));
  s.place_at_free(e, fx(x10, 10), fx(y10, 10));
  if (auto* c = s.world().get<sim::Combat>(e)) c->order = sim::Combat::Hold;
  return e;
}

sim::Health& hp(sim::Sim& s, sim::EntityId e) { return *s.world().get<sim::Health>(e); }

void energize(sim::Sim& s, sim::EntityId e) {
  if (auto* c = s.world().get<sim::Caster>(e)) c->energy = 1000000;
}

Match duel() { return Match("BUILD-ALAP", true, true, {"ursan", "wullof"}); }

}  // namespace

TEST_CASE("nova: épületsebzés (`building_damage`) és önrobbanás (`self_destruct`) – a Mészbombás manó") {
  Match m = duel();
  sim::Sim& s = *m.sim;
  const auto b = s.place_building(def_of("wul_tipi"), 2, 16, 13, true);
  const auto u = put(s, "gho_meszbomba", 1, 155, 135);
  energize(s, u);
  const int32_t h0 = hp(s, b).hp;
  CHECK(s.use_ability({u}, "gho_meszrobbanas", {}, {}, 0));
  m.run(2);
  std::printf("  épület: %d -> %d\n", h0, s.world().alive(b) ? hp(s, b).hp : 0);
  CHECK(!s.world().alive(b) || hp(s, b).hp < h0);
  CHECK(!s.world().alive(u) || hp(s, u).hp <= 0);
}

TEST_CASE("célzott lövés: a képesség saját mérge (`poison`), területi sebzése (`splash`) és pattanása (`bounces`)") {
  // a Groomi köpései kutatáshoz kötöttek, és egymást kizárják: mindegyik a saját meccsében
  for (const char* which : {"savkopes", "tuzkopes"}) {
    Match m = duel();
    sim::Sim& s = *m.sim;
    const auto g = put(s, "wul_groomi", 2, 100, 100);
    energize(s, g);
    auto& rs = s.player_mut(2).researched;
    const std::string rid = std::string("wul_") + which;
    rs.insert(std::lower_bound(rs.begin(), rs.end(), rid), rid);
    const auto t = put(s, "urs_berserker", 1, 140, 100);
    const auto t2 = put(s, "urs_berserker", 1, 150, 105);
    m.run(60);
    energize(s, g);
    const int32_t h2 = hp(s, t2).hp;
    CHECK(s.use_ability({g}, which, {}, {}, t));
    m.run(60);
    if (std::string(which) == "savkopes") {
      const auto* af = s.world().get<sim::Afflict>(t);
      CHECK(af && af->poison_until > s.tick());   // a méreg a találat után is tart
    } else {
      std::printf("  a szomszéd (területi): %d -> %d\n", h2, hp(s, t2).hp);
      CHECK(hp(s, t2).hp < h2);
    }
  }
  Match m = duel();
  sim::Sim& s = *m.sim;
  const auto a = put(s, "wul_ijas_vadasz", 2, 100, 100);
  energize(s, a);
  std::vector<sim::EntityId> ts = {put(s, "urs_berserker", 1, 150, 100), put(s, "urs_berserker", 1, 170, 110),
                                   put(s, "urs_berserker", 1, 175, 90)};
  std::vector<int32_t> h0;
  for (const auto t : ts) h0.push_back(hp(s, t).hp);
  CHECK(s.use_ability({a}, "bumerang", {}, {}, ts[0]));
  m.run(120);
  int hit = 0;
  for (size_t i = 0; i < ts.size(); ++i) hit += hp(s, ts[i]).hp < h0[i] ? 1 : 0;
  std::printf("  a bumeráng %d célt talált\n", hit);
  CHECK(hit >= 2);
}

TEST_CASE("képességár: életerő (`hp_cost`; hp <= ár + 5 alatt nem) és az egységképesség áhítat-ára (`faith`)") {
  {
    Match m = duel();
    sim::Sim& s = *m.sim;
    const auto u = put(s, "cro_tuskekopo", 1, 100, 100);
    energize(s, u);
    hp(s, u).hp = 25;   // 20 + 5: még nem
    CHECK(!s.use_ability({u}, "tuskezapor", fx(140, 10), fx(100, 10), 0));
    hp(s, u).hp = hp(s, u).max;
    const int32_t h0 = hp(s, u).hp;
    CHECK(s.use_ability({u}, "tuskezapor", fx(140, 10), fx(100, 10), 0));
    m.run(60);
    std::printf("  életerő: %d -> %d\n", h0, hp(s, u).hp);
    CHECK(hp(s, u).hp <= h0 - 20);
  }
  Match m("BUILD-ALAP", true, true, {"elves", "ursan"});
  sim::Sim& s = *m.sim;
  const auto u = put(s, "elf_napszoro", 1, 100, 100);
  energize(s, u);
  CHECK(s.player(1).resource(sim::kFaith) < 40);
  CHECK(!s.use_ability({u}, "napkitores", fx(130, 10), fx(100, 10), 0));   // áhítat nélkül nem
  s.add_resources(1, sim::kFaith, 100);
  const int32_t f0 = s.player(1).resource(sim::kFaith);
  CHECK(s.use_ability({u}, "napkitores", fx(130, 10), fx(100, 10), 0));
  m.run(60);
  CHECK(f0 - s.player(1).resource(sim::kFaith) == 40);
}

TEST_CASE("önműködő gyógyítás célcímkéi (`heal_tags`): Livia horgonya a gépeket gyógyítja") {
  Match m("BUILD-ALAP", true, true, {"imperium", "ursan"});
  sim::Sim& s = *m.sim;
  const auto l = put(s, "imp_hos_livia", 1, 100, 100);
  energize(s, l);
  const auto mech = put(s, "imp_legios", 1, 120, 100);
  hp(s, mech).hp = hp(s, mech).max / 2;
  const int32_t h0 = hp(s, mech).hp;
  m.run(90);
  std::printf("  légiós (gép): %d -> %d\n", h0, hp(s, mech).hp);
  CHECK(hp(s, mech).hp > h0);
}

TEST_CASE("önerősítés: lejárta után lassulás (`after_slow`, Gőztúlnyomás); alatta nem gyenge (`no_weak`, Napfolt)") {
  {
    Match m("BUILD-ALAP", true, true, {"imperium", "ursan"});
    sim::Sim& s = *m.sim;
    const auto u = put(s, "imp_legios", 1, 100, 100);
    energize(s, u);
    auto& rs = s.player_mut(1).researched;
    rs.insert(std::lower_bound(rs.begin(), rs.end(), std::string("imp_goztulnyomas")), "imp_goztulnyomas");
    CHECK(s.use_ability({u}, "goztulnyomas", {}, {}, 0));
    const int32_t dur = R().abilities[static_cast<size_t>(R().ability_index("goztulnyomas"))].duration;
    m.run(dur + 2);
    const auto* af = s.world().get<sim::Afflict>(u);
    std::printf("  lassulás eddig: %lld (most %lld), szorzó %d\n", static_cast<long long>(af ? af->slow_until : -1),
                static_cast<long long>(s.tick()), af ? af->slow_mult.raw() : -1);
    CHECK(af && af->slow_until > s.tick() && af->slow_mult == fx(3, 5));
  }
  Match m("BUILD-ALAP", true, true, {"elves", "ursan"});
  sim::Sim& s = *m.sim;
  const auto a = put(s, "elf_tollas_harcos", 1, 100, 100);
  const auto b = put(s, "elf_tollas_harcos", 1, 100, 140);
  energize(s, b);
  auto& rs = s.player_mut(1).researched;
  rs.insert(std::lower_bound(rs.begin(), rs.end(), std::string("elf_napfolt")), "elf_napfolt");
  CHECK(s.use_ability({b}, "napfolt", {}, {}, 0));
  // egy ellene erős osztályú (típusszorzó > 1) támadó ugyanakkora ütése: a napfoltos nem kap többletet
  const std::string& tu = R().defs[def_of("elf_tollas_harcos")].utype;
  uint16_t sd = sim::kNoDef;
  for (size_t i = 0; i < R().defs.size() && sd == sim::kNoDef; ++i) {
    const auto& d = R().defs[i];
    if (!d.building && !d.utype.empty() && d.faction == "ursan" && R().type_mult(d.utype, tu) > Fixed::from_int(1)) sd = static_cast<uint16_t>(i);
  }
  CHECK(sd != sim::kNoDef);
  const auto sp = s.spawn_unit(sd, 2, fx(300, 10), fx(300, 10));
  for (const auto e : {a, b}) {
    hp(s, e).hp = hp(s, e).max;
    if (auto* v = s.world().get<sim::Vital>(e)) v->shield = 0;
  }
  const int32_t ha = hp(s, a).hp, hb = hp(s, b).hp;
  s.deal_damage(sp, 2, sd, a, 40000, false);
  s.deal_damage(sp, 2, sd, b, 40000, false);
  std::printf("  sebzés: normál %d, napfoltos %d\n", ha - hp(s, a).hp, hb - hp(s, b).hp);
  CHECK(ha - hp(s, a).hp > hb - hp(s, b).hp);
}

TEST_CASE("felfedés: a passzív képesség köre (`detect`, A Kórus szeme) és a főépület-képesség pontja (`reveal`, 10 s)") {
  {
    Match m("BUILD-ALAP", true, true, {"croftian", "hukucha"});
    sim::Sim& s = *m.sim;
    const auto hid = put(s, "huk_ikra", 2, 180, 100);   // mindig rejtett
    m.run(31);
    CHECK(!s.is_visible(1, hid));
    put(s, "cro_szemhordozo", 1, 100, 100);   // 8 csempére, a köre 9
    m.run(31);   // a rejtőzés 0,5 s-onként frissül
    CHECK(s.is_visible(1, hid));
  }
  Match m("BUILD-ALAP", true, true, {"elves", "ursan"});
  sim::Sim& s = *m.sim;
  sim::EntityId hq = sim::kNoEntity;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& b) {
    if (b.def == R().faction("elves")->hq && s.world().get<sim::Health>(e)->owner == 1) hq = e;
  });
  for (int r = 0; r < sim::kResources; ++r) s.add_resources(1, r, 5000);
  m.run(7);
  const auto& p = s.player(1);
  const int w = m.map->width();
  const auto at = static_cast<size_t>(3 * w + 28);   // a pálya sarka, a bázistól messze
  CHECK(p.visible.empty() || p.visible[at] == 0);
  const auto scout = put(s, "elf_tollas_harcos", 1, 285, 35);   // a célzott képesség csak látható pontra
  m.run(7);   // a látótér 0,2 s-onként frissül
  if (auto* c = hq != sim::kNoEntity ? s.world().get<sim::Caster>(hq) : nullptr) c->ready.clear();   // a kezdeti újratöltés
  CHECK(hq != sim::kNoEntity && s.use_power(1, hq, "gp_napcsova", fx(285, 10), fx(35, 10)));
  s.kill(scout);
  m.run(13);
  CHECK(s.player(1).visible[at] == 1);
  m.run(11 * 30);
  CHECK(s.player(1).visible[at] == 0);
}
