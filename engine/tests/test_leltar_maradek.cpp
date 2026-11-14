// SPDX-License-Identifier: GPL-3.0-or-later
// A leltár-kapu utolsó sorai (M7, ADR-008): több egység egy képzésből, halálkori robbanás, bányai rejtőzés,
// típusszorzó-tiltás, nem építő munkás, párhuzamos gyártás, lelőhelyre építés, épület-aura, erdei rejtőzés
// irány-szabályként, a gép létszámegysége és oltára, a képesség spóra-ára.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "check.hpp"
#include "match_util.hpp"
#include "script/host.hpp"

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

int count_of(sim::Sim& s, uint8_t owner, uint16_t def) {
  int n = 0;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit& u) {
    n += u.def == def && hp(s, e).owner == owner && hp(s, e).hp > 0 ? 1 : 0;
  });
  return n;
}

uint16_t producer_of(uint16_t unit) {
  for (size_t i = 0; i < R().defs.size(); ++i) {
    const auto& p = R().defs[i].produces;
    if (R().defs[i].building && std::find(p.begin(), p.end(), unit) != p.end()) return static_cast<uint16_t>(i);
  }
  return sim::kNoDef;
}

void rich(sim::Sim& s, uint8_t o) {
  for (int r = 0; r < sim::kResources; ++r) s.add_resources(o, r, 20000);
}

}  // namespace

TEST_CASE("több egység egy képzésből (`Cost.count`: a Karmos párban jön); párhuzamos gyártás (`parallel`: a Kaptár)") {
  Match m("BUILD-ALAP", true, true, {"croftian", "ursan"});
  sim::Sim& s = *m.sim;
  rich(s, 1);
  const uint16_t k = def_of("cro_karmos");
  const uint16_t pd = producer_of(k);
  CHECK(pd != sim::kNoDef);
  const auto b = s.place_building(pd, 1, 12, 9, true);
  for (const auto& q : R().defs[k].reqs) {
    if (q.kind == sim::EntityDef::Req::Building) s.place_building(def_of(q.id.c_str()), 1, 16, 13, true);
  }
  m.run(2);
  std::string err;
  const int before = count_of(s, 1, k);
  CHECK(s.train(1, k, b, &err));
  const bool second = s.train(1, k, b, &err);
  m.run(1);
  const auto* pr = s.world().get<sim::Producer>(b);
  if (R().defs[pd].parallel && second) {   // a sor minden eleme halad
    CHECK(pr->queue.size() == 2 && pr->queue[0].progress > 0 && pr->queue[1].progress > 0);
  }
  m.run(R().defs[k].time_ticks + 30);
  std::printf("  karmos: %d -> %d (%s, párhuzamos: %d)\n", before, count_of(s, 1, k), R().defs[pd].id.c_str(), R().defs[pd].parallel ? 1 : 0);
  CHECK(count_of(s, 1, k) == before + (second ? 4 : 2));
}

TEST_CASE("halálkori robbanás (`death_burst`: a robbanó atka 35 sebzés a körben)") {
  Match m("BUILD-ALAP", true, true, {"croftian", "ursan"});
  sim::Sim& s = *m.sim;
  const auto a = put(s, "cro_robbanoatka", 1, 100, 100);
  const auto t = put(s, "urs_berserker", 2, 110, 100);
  const auto far = put(s, "urs_berserker", 2, 200, 100);
  const int32_t h0 = hp(s, t).hp, hf = hp(s, far).hp;
  s.deal_damage(0, 2, sim::kNoDef, a, 100000000, false, true);
  m.run(2);
  std::printf("  közeli: %d -> %d, távoli: %d -> %d\n", h0, hp(s, t).hp, hf, hp(s, far).hp);
  CHECK(hp(s, t).hp < h0);
  CHECK(hp(s, far).hp == hf);
}

TEST_CASE("bányai rejtőzés (`mine_stealth`: a Tanuki 3 s nyugalom után az arany mellett rejtett, máshol nem)") {
  Match m("BUILD-ALAP", true, true, {"ooni", "ursan"});
  sim::Sim& s = *m.sim;
  int gx = -1, gy = -1;
  s.world().pool<sim::ResourceNode>().each([&](sim::EntityId, const sim::ResourceNode& n) {
    if (n.kind == sim::kGold && gx < 0) gx = n.tx, gy = n.ty;
  });
  CHECK(gx >= 0);
  const auto near = put(s, "oon_tanuki", 1, gx * 10 + 25, gy * 10 + 5);
  const auto away = put(s, "oon_tanuki", 1, 280, 170);
  m.run(150);
  const auto* sn = s.world().get<sim::Stealth>(near);
  const auto* sa = s.world().get<sim::Stealth>(away);
  CHECK(sn && sn->hidden == 1);
  CHECK(sa && sa->hidden == 0);
}

TEST_CASE("típusszorzó-tiltás (`type_nostrength`): a Csirkepásztor ütése az osztályszorzó nélkül hat") {
  const uint16_t cp = def_of("zha_csirkepasztor");
  const std::string& cu = R().defs[cp].utype;
  // egy célpont-osztály, amely ellen ez az osztály nem semleges, és egy ugyanilyen osztályú, de szorzót kapó támadó
  uint16_t tdef = sim::kNoDef, other = sim::kNoDef;
  for (size_t i = 0; i < R().defs.size(); ++i) {
    const auto& d = R().defs[i];
    if (d.building || d.utype.empty() || d.faction != "ursan") continue;
    if (tdef == sim::kNoDef && R().type_mult(cu, d.utype) != Fixed::from_int(1)) tdef = static_cast<uint16_t>(i);
  }
  for (size_t i = 0; i < R().defs.size(); ++i) {
    const auto& d = R().defs[i];
    if (!d.building && d.utype == cu && !d.type_nostrength && static_cast<uint16_t>(i) != cp) other = static_cast<uint16_t>(i);
  }
  CHECK(tdef != sim::kNoDef && other != sim::kNoDef);
  if (tdef == sim::kNoDef || other == sim::kNoDef) return;
  Match m("BUILD-ALAP", true, true, {"ursan", "ursan"});
  sim::Sim& s = *m.sim;
  const auto t1 = s.spawn_unit(tdef, 1, fx(100, 10), fx(100, 10));
  const auto t2 = s.spawn_unit(tdef, 1, fx(100, 10), fx(140, 10));
  const auto a1 = s.spawn_unit(cp, 2, fx(300, 10), fx(100, 10));
  const auto a2 = s.spawn_unit(other, 2, fx(300, 10), fx(140, 10));
  const int32_t h1 = hp(s, t1).hp, h2 = hp(s, t2).hp;
  s.deal_damage(a1, 2, cp, t1, 30000, false);
  s.deal_damage(a2, 2, other, t2, 30000, false);
  std::printf("  szorzó nélkül %d, szorzóval %d\n", h1 - hp(s, t1).hp, h2 - hp(s, t2).hp);
  CHECK(h1 - hp(s, t1).hp != h2 - hp(s, t2).hp);
}

TEST_CASE("nem építő munkás (`Worker.builds: false`: az Arató nem épít); lelőhelyre építés (`on_resource`: a Napfúró)") {
  {
    Match m("BUILD-ALAP", true, true, {"imperium", "ursan"});
    sim::Sim& s = *m.sim;
    rich(s, 1);
    const auto a = put(s, "imp_arato", 1, 120, 100);
    std::string err;
    CHECK(!s.order_build(a, R().faction("imperium")->hq, 12, 9, &err));
    std::printf("  arató: %s\n", err.c_str());
    CHECK(err == "ez a munkás nem épít");
  }
  Match m("BUILD-ALAP", true, true, {"elves", "ursan"});
  sim::Sim& s = *m.sim;
  const uint16_t nf = def_of("elf_napfuro");
  int gx = -1, gy = -1;
  s.world().pool<sim::ResourceNode>().each([&](sim::EntityId, const sim::ResourceNode& n) {
    if (n.kind == sim::kGold && gx < 0) gx = n.tx, gy = n.ty;
  });
  std::string err;
  CHECK(!s.can_place(0, nf, 16, 13, &err));   // szabad földre nem
  std::printf("  szabad földre: %s\n", err.c_str());
  CHECK(err == "aranybányára vagy meteoritra kell építeni");
  CHECK(s.can_place(0, nf, gx, gy, &err));   // a lelőhely sarkára igen
}

TEST_CASE("épület-aura (`Aura.regen`: a Tápgomba körében a saját egység gyorsabban gyógyul)") {
  Match m("BUILD-ALAP", true, true, {"ursan", "ursan"});
  sim::Sim& s = *m.sim;
  s.place_building(def_of("cro_tapgomba"), 1, 16, 13, true);
  const auto near = put(s, "urs_berserker", 1, 175, 120);
  const auto far = put(s, "urs_berserker", 1, 30, 180);
  hp(s, near).hp = hp(s, far).hp = 20;
  m.run(300);
  std::printf("  10 s alatt: közel %d, távol %d\n", hp(s, near).hp, hp(s, far).hp);
  CHECK(hp(s, near).hp > hp(s, far).hp);
}

TEST_CASE("erdei rejtőzés irány-szabályként (`forest_stealth`, Jaguár-harcosok: a gyalogos a fák mellett rejtett)") {
  Match m("BUILD-ALAP", true, true, {"elves", "ursan"});
  sim::Sim& s = *m.sim;
  int wx = -1, wy = -1;
  s.world().pool<sim::ResourceNode>().each([&](sim::EntityId, const sim::ResourceNode& n) {
    if (n.kind == sim::kWood && n.amount > 0 && wx < 0) wx = n.tx, wy = n.ty;
  });
  CHECK(wx >= 0);
  const auto u = put(s, "elf_tollas_harcos", 1, wx * 10 + 5, wy * 10 + 15);
  m.run(120);
  CHECK(s.world().get<sim::Stealth>(u)->hidden == 0);   // irány nélkül nem
  s.player_mut(1).ascended = true;
  CHECK(s.choose_path(1, "jaguar"));
  m.run(30);
  const auto& p = *s.world().get<sim::Position>(u);
  std::printf("  fa (%d,%d), harcos (%d,%d)\n", wx, wy, p.x.floor_int(), p.y.floor_int());
  CHECK(s.world().get<sim::Stealth>(u)->hidden == 1);
}

TEST_CASE("adatvezérelt gép: létszámegység (`supply_unit`: a Wullof emut képez) és hős az oltárból (`altar`)") {
  const std::string src = read_file(std::filesystem::path(TOTU_CONTENT_DIR) / "base" / "scripts" / "ai_nep.lua");
  Match m("ECO-ALAP", true, true, {"wullof", "ursan"});
  sim::Sim& s = *m.sim;
  script::ScriptHost ai("ai", 20261113);
  CHECK(ai.run("ai_players", "AI_PLAYERS = {1}"));
  CHECK(ai.run("ai_nep.lua", src));
  s.add_hooks(&ai);
  ai.start(s);
  rich(s, 1);
  m.run(8 * 60 * 30);
  int heroes = 0;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit& u) {
    heroes += R().defs[u.def].hero && hp(s, e).owner == 1 ? 1 : 0;
  });
  const int emus = count_of(s, 1, def_of("wul_emu"));
  int altars = 0, army = 0;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& b) {
    altars += R().defs[b.def].altar && b.complete && hp(s, e).owner == 1 ? 1 : 0;
  });
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit&) { army += hp(s, e).owner == 1 && !s.world().get<sim::Worker>(e) ? 1 : 0; });
  for (const auto& msg : ai.messages()) std::printf("  napló: %s\n", msg.c_str());
  std::printf("  oltár: %d, sereg: %d, arany %d\n", altars, army, s.player(1).resource(sim::kGold));
  std::printf("  emu: %d, hős: %d\n", emus, heroes);
  CHECK(emus > 0);
  CHECK(heroes > 0);
}

TEST_CASE("képesség spóra-ára (`spores`, Micélia-mod: a Spóratüske 2 spórába kerül, kevesebbel nem használható)") {
  content::ContentSet set;
  content::Vfs vfs;
  vfs.add_layer("totu", std::filesystem::path(TOTU_CONTENT_DIR) / "base");
  vfs.add_layer("micelia", std::filesystem::path(TOTU_CONTENT_DIR) / "mods" / "micelia");
  content::ContentLoader(vfs, std::filesystem::path(TOTU_CONTENT_DIR) / "schema").load(set);
  std::vector<std::string> errors;
  map::TerrainTable terrain;
  terrain.load(set, errors);
  const auto game = content::parse(read_file(std::filesystem::path(TOTU_CONTENT_DIR) / "base" / "game.json"));
  auto rules = sim::Rules::load(*game, set, errors);
  CHECK(rules.has_value() && rules->faction("mycel") != nullptr);
  if (!rules || !rules->faction("mycel")) return;
  const auto doc = content::parse(read_file(std::filesystem::path(TOTU_TEST_DATA) / "maps" / "BUILD-ALAP.json"));
  auto mp = map::Map::from_json(*doc, terrain, errors);
  map::NavGrid nav(*mp, terrain);
  map::Pathfinder pf(nav);
  sim::Sim s(7);
  s.attach_navigation(&pf);
  s.attach_rules(&*rules);
  sim::MatchSetup setup;
  setup.factions = {"mycel", "ursan"};
  std::string err;
  CHECK(s.setup_match(*mp, setup, &err));
  const auto u = s.spawn_unit(rules->def("myc_tuskegomba_lovesz"), 1, fx(100, 10), fx(100, 10));
  if (auto* c = s.world().get<sim::Caster>(u)) c->energy = 1000000;
  auto foe = [&](int y10) { return s.spawn_unit(rules->def("urs_berserker"), 2, fx(140, 10), fx(y10, 10)); };
  const auto t = foe(100);
  CHECK(!s.use_ability({u}, "sporatuske", {}, {}, t));   // spóra nélkül nem
  for (int i = 0; i < 2; ++i) {   // két ölés: két spóra
    s.deal_damage(u, 1, rules->def("myc_tuskegomba_lovesz"), foe(140 + i * 10), 100000000, false, true);
    s.step();
  }
  CHECK(s.use_ability({u}, "sporatuske", {}, {}, t));
  for (int i = 0; i < 60; ++i) s.step();   // a lövéskor vonódik le
  if (auto* c = s.world().get<sim::Caster>(u)) c->ready.clear();
  bool again = true;   // az ölésekből jött spóra (2–3) a lövés után már nem elég egy újabbra
  for (int i = 0; i < 3 && again; ++i) {
    again = s.use_ability({u}, "sporatuske", {}, {}, t);
    for (int k = 0; k < 60; ++k) s.step();
    if (auto* c = s.world().get<sim::Caster>(u)) c->ready.clear();
  }
  CHECK(!again);
}
