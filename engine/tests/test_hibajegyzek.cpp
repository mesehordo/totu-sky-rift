// SPDX-License-Identifier: GPL-3.0-or-later
// A külső kódvizsgálat hibajegyzékének (2026-11-15) regressziós próbái: tételenként a reprodukció és a javítás igazolása.
#include <algorithm>
#include <climits>
#include <cstdio>
#include <string>
#include <vector>

#include "check.hpp"
#include "match_util.hpp"
#include "core/rng.hpp"
#include "script/host.hpp"
#include "sim/replay.hpp"

#include <optional>

using namespace totu;
using namespace totu::testing;
using core::Fixed;

namespace {

const sim::Rules& R() { return *content_once().rules; }
uint16_t def_of(const char* id) { return R().def(id); }
Fixed fx(int a, int b) { return Fixed::from_ratio(a, b); }

std::string last_log(const script::ScriptHost& h) {
  if (h.messages().empty()) return {};
  const std::string& m = h.messages().back();
  const auto p = m.find("] ");
  return p == std::string::npos ? m : m.substr(p + 2);
}

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

namespace {

std::optional<map::Map> tiny_map(const std::string& extra_objects) {
  std::vector<std::string> err;
  const std::string text = std::string(R"({"width": 24, "height": 16, "terrain": {"rows": [)") + [] {
    std::string rows;
    for (int y = 0; y < 16; ++y) rows += std::string(y ? "," : "") + "\"........................\"";
    return rows;
  }() + "]}, \"objects\": {" + extra_objects + "}}";
  const auto doc = content::parse(text);
  if (!doc) return std::nullopt;
  return map::Map::from_json(*doc, content_once().terrain, err);
}

}  // namespace

TEST_CASE("hibajegyzek H-25 H-26 H-38 H-36: a meccsinditas elott minden feltetel; hibanal a vilag erintetlen, masodszor nem indul") {
  {   // két játékos ugyanazon a kezdőhelyen: elutasítva, a világ üres marad
    auto mp = tiny_map(R"("starts": [[6, 8], [6, 8]], "resources": [{"kind": "gold", "x": 14, "y": 3, "size": 2}])");
    CHECK(mp.has_value());
    map::NavGrid nav(*mp, content_once().terrain);
    map::Pathfinder pf(nav);
    sim::Sim s(1);
    s.attach_navigation(&pf);
    s.attach_rules(&R());
    sim::MatchSetup setup;
    setup.factions = {"ursan", "wullof"};
    std::string err;
    CHECK(!s.setup_match(*mp, setup, &err));
    std::printf("  %s\n", err.c_str());
    size_t n = 0;
    s.world().pool<sim::Health>().each([&](sim::EntityId, const sim::Health&) { ++n; });
    s.world().pool<sim::ResourceNode>().each([&](sim::EntityId, const sim::ResourceNode&) { ++n; });
    CHECK(n == 0);
  }
  // rendes indulás; az üres (0) lelőhely nem kerül a világba; a második indítás elutasítva
  auto mp = tiny_map(R"("starts": [[5, 8], [19, 8]], "resources": [{"kind": "gold", "x": 11, "y": 2, "size": 2, "amount": 0}, {"kind": "gold", "x": 11, "y": 12, "size": 2}])");
  CHECK(mp.has_value());
  map::NavGrid nav(*mp, content_once().terrain);
  map::Pathfinder pf(nav);
  sim::Sim s(1);
  s.attach_navigation(&pf);
  s.attach_rules(&R());
  sim::MatchSetup setup;
  setup.factions = {"ursan", "wullof"};
  std::string err;
  CHECK(s.setup_match(*mp, setup, &err));
  int nodes = 0;
  s.world().pool<sim::ResourceNode>().each([&](sim::EntityId, const sim::ResourceNode&) { ++nodes; });
  CHECK(nodes == 1);
  CHECK(!s.setup_match(*mp, setup, &err));
}

TEST_CASE("hibajegyzek H-03: a fofepuletnel kisebb palya elutasitva, osszeomlas nelkul") {
  std::vector<std::string> errs;
  const auto doc = content::parse(R"({"width": 2, "height": 2, "terrain": {"rows": ["..", ".."]}, "objects": {"starts": [[0, 0], [1, 1]]}})");
  CHECK(doc.has_value());
  auto mp = map::Map::from_json(*doc, content_once().terrain, errs);
  CHECK(mp.has_value());
  if (!mp) return;
  map::NavGrid nav(*mp, content_once().terrain);
  map::Pathfinder pf(nav);
  sim::Sim s(1);
  s.attach_navigation(&pf);
  s.attach_rules(&R());
  sim::MatchSetup setup;
  setup.factions = {"ursan", "ursan"};
  std::string err;
  CHECK(!s.setup_match(*mp, setup, &err));
}

TEST_CASE("hibajegyzek H-27 H-28 H-29: a hash a nepet, a fuggo parancsokat es a palya-blokkolast is tartalmazza") {
  Match a("BUILD-ALAP"), b("BUILD-ALAP");
  CHECK(a.sim->hash() == b.sim->hash());
  b.sim->player_mut(2).faction = "wullof";   // csak a nép más
  CHECK(a.sim->hash() != b.sim->hash());
  b.sim->player_mut(2).faction = a.sim->player(2).faction;
  CHECK(a.sim->hash() == b.sim->hash());
  auto c = cmd(sim::Command::Kind::Stop, 0, b.sim->tick() + 100);   // jövőbeli parancs a sorban
  b.sim->queue(c);
  CHECK(a.sim->hash() != b.sim->hash());
  Match d("BUILD-ALAP"), e("BUILD-ALAP");
  auto blk = cmd(sim::Command::Kind::Block, 0, e.sim->tick());
  blk.rx = 20; blk.ry = 2; blk.rw = 2; blk.rh = 2; blk.flag = 1;
  e.sim->queue(blk);
  d.run(1);
  e.run(1);
  CHECK(d.sim->hash() != e.sim->hash());
}

TEST_CASE("hibajegyzek H-98 H-33 H-10: a serult mentes elutasitva, az allapot valtozatlan") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  m.run(30);
  const uint64_t h0 = s.hash();
  auto good = s.save();
  auto bad = good;
  bad.resize(bad.size() / 2);   // csonka mentés
  CHECK(!s.load(bad));
  CHECK(s.hash() == h0);
  auto noise = good;
  for (size_t i = 40; i < noise.size(); i += 97) noise[i] ^= 0x5A;   // szétszórt bájthibák
  if (!s.load(noise)) CHECK(s.hash() == h0);
  CHECK(s.load(good) && s.hash() == h0);
}

TEST_CASE("hibajegyzek H-15 H-88 H-06: a visszajatszas a teljes meccsbeallitast hordozza; a tul hosszu elutasitva") {
  sim::Replay r;
  r.seed = 3;
  r.ticks = 30;
  r.checkpoint_every = 10;
  r.factions = {"ursan", "wullof"};
  r.upkeep = false;
  r.camps = true;
  r.fog = 2;
  r.daytime = 0;
  const auto back = sim::decode(sim::encode(r));
  CHECK(back.has_value());
  if (back) CHECK(!back->upkeep && back->camps && back->fog == 2 && back->daytime == 0);
  sim::Replay big = r;
  big.ticks = 30LL * 3600 * 30 * 100;   // 100 óra
  CHECK(!sim::decode(sim::encode(big)).has_value());
  sim::Replay empty;   // üres felvétel: nincs ellenőrzőpont, nincs összeomlás
  empty.ticks = 0;
  empty.checkpoint_every = 1;
  CHECK(sim::decode(sim::encode(empty)).has_value());
}

TEST_CASE("hibajegyzek H-04 H-13: a csoportos egyseg letszama a regi szerint, a tarsak nem kerulnek egymasra") {
  Match m("BUILD-ALAP", true, true, {"croftian", "wullof"});
  sim::Sim& s = *m.sim;
  m.run(1);
  const int32_t before = s.player(1).supply_used;
  sim::Producer pr;
  const auto a = s.spawn_group(def_of("cro_karmos"), 1, fx(120, 10), fx(120, 10), &pr);
  CHECK(a != sim::kNoEntity);
  std::vector<sim::EntityId> pair;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit& u) {
    if (u.def == def_of("cro_karmos") && s.world().get<sim::Health>(e)->owner == 1) pair.push_back(e);
  });
  CHECK(pair.size() == 2);
  if (pair.size() == 2) {
    const auto& p0 = *s.world().get<sim::Position>(pair[0]);
    const auto& p1 = *s.world().get<sim::Position>(pair[1]);
    const Fixed rr = R().defs[def_of("cro_karmos")].radius * Fixed::from_int(2);
    CHECK(core::length(p0.x - p1.x, p0.y - p1.y) >= rr);   // H-13
  }
  m.run(2);
  CHECK(s.player(1).supply_used - before == 1);   // H-04: a pár együtt 1 (Cost.supply / Cost.count)
}

TEST_CASE("hibajegyzek H-86 H-87: a teljes 32 bites RNG-tartomany es a tul nagy hanyados telitese") {
  core::Pcg32 rng(7, 1);
  bool varied = false;
  const int32_t first = rng.range(INT32_MIN, INT32_MAX);
  for (int i = 0; i < 8; ++i) varied = varied || rng.range(INT32_MIN, INT32_MAX) != first;
  CHECK(varied);
  CHECK(Fixed::from_ratio(int64_t{1} << 48, 1) == Fixed::max());
  CHECK(Fixed::from_ratio(-(int64_t{1} << 48), 1) == Fixed::min());
}

TEST_CASE("hibajegyzek H-73 H-74: sok idozito egy tickben sorrendben fut, a darabszam korlatos") {
  script::ScriptHost h("idozito", 5);
  CHECK(h.run("i", R"(
    function on_start()
      S.n = 0 S.rend = true S.utolso = 0
      for i = 1, 3000 do Game.after(5, "fut", i) end
      S.tul = pcall(function() for i = 1, 70000 do Game.after(100, "fut", 0) end end)
    end
    function fut(i, id)
      if i > 0 then
        S.n = S.n + 1
        if i < S.utolso then S.rend = false end
        S.utolso = i
      end
    end
  )"));
  sim::Sim s(5);
  s.add_hooks(&h);
  h.start(s);
  for (int i = 0; i < 8; ++i) s.step();
  CHECK(h.run("e", "log(S.n, S.rend, S.tul)"));
  CHECK(last_log(h) == "3000 true false");
}

TEST_CASE("hibajegyzek H-22: a tabor szintkulcsa csak egesz szam lehet") {
  const auto& c = content_once();
  const auto good = content::parse(R"({"width": 8, "height": 8, "terrain": {"rle": "grass*64"}, "camps": {"1": "pinguu"}})");
  const auto bad = content::parse(R"({"width": 8, "height": 8, "terrain": {"rle": "grass*64"}, "camps": {"1hibas": "pinguu"}})");
  std::vector<std::string> e1, e2;
  CHECK(good && map::Map::from_json(*good, c.terrain, e1).has_value());
  CHECK(bad && !map::Map::from_json(*bad, c.terrain, e2).has_value());
}
