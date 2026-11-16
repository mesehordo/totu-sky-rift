// SPDX-License-Identifier: GPL-3.0-or-later
// M11 küldetésrendszer: a leírás ellenőrzése, a trigger-motor, párbeszéd-választás parancsként, átvezető, célok,
// győzelem és bukás; mentésből folytatva és visszajátszva ugyanaz.
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "check.hpp"
#include "match_util.hpp"
#include "sim/mission.hpp"
#include "sim/replay.hpp"

using namespace totu;
using namespace totu::testing;
using core::Fixed;

namespace {

std::string mission_text() {
  return read_file(std::filesystem::path(TOTU_CONTENT_DIR) / "base" / "missions" / "proba" / "mission.json");
}

sim::Replay mission_desc() {
  sim::Replay r;
  r.seed = 1111;
  r.map = read_file(std::filesystem::path(TOTU_CONTENT_DIR) / "base" / "maps" / "kraterfold.json");
  r.mission = mission_text();
  return r;
}

std::unique_ptr<sim::MatchInstance> start(const sim::Replay& r, bool load_state = false) {
  std::string err;
  auto m = sim::start_match(r, &content_once().terrain, &*content_once().rules, err, load_state);
  if (!m) std::printf("  hiba: %s\n", err.c_str());
  return m;
}

void run(sim::MatchInstance& m, int ticks) {
  for (int i = 0; i < ticks; ++i) m.sim->step();
}

sim::Command order(sim::Command::Kind k, uint8_t owner, std::vector<sim::EntityId> us, Fixed x, Fixed y, int64_t tick) {
  sim::Command c;
  c.kind = k;
  c.owner = owner;
  c.units = std::move(us);
  c.x = x;
  c.y = y;
  c.tick = tick;
  return c;
}

sim::Command choose(const std::string& dialogue, int idx, int64_t tick) {
  sim::Command c;
  c.kind = sim::Command::Kind::Mission;
  c.owner = 1;
  c.def = dialogue;
  c.rx = idx;
  c.tick = tick;
  return c;
}

}  // namespace

TEST_CASE("kuldetes: a leiras ellenorzese - a mintakuldetes ervenyes, a hibas hivatkozas hiba") {
  std::vector<std::string> errs;
  const auto ok = sim::MissionDef::parse(mission_text(), &*content_once().rules, errs);
  for (const auto& e : errs) std::printf("  %s\n", e.c_str());
  CHECK(ok.has_value() && errs.empty());
  if (ok) CHECK(ok->triggers.size() == 11 && ok->dialogues.size() == 1 && ok->cutscenes.size() == 1);
  const std::string bad = R"({"id": "x", "players": [{"slot": 1, "faction": "ursan", "human": true}],
    "regions": [{"id": "a", "x": 1, "y": 1}],
    "triggers": [
      {"id": "t1", "event": {"type": "enter_region", "region": "nincs"}, "actions": [{"type": "robban"}]},
      {"id": "t1", "event": {"type": "start"}, "actions": [{"type": "dialogue", "id": "nincs"},
        {"type": "spawn", "def": "nem_letezo", "owner": 1, "region": "a"}]},
      {"id": "t3", "event": {"type": "start"}, "conditions": [{"type": "objective", "id": "nincs"}], "actions": []}
    ]})";
  std::vector<std::string> e2;
  CHECK(!sim::MissionDef::parse(bad, &*content_once().rules, e2).has_value());
  for (const auto& e : e2) std::printf("  várt hiba: %s\n", e.c_str());
  CHECK(e2.size() >= 6);   // ismeretlen régió, akció, ismétlődő azonosító, párbeszéd, sablon, cél
}

TEST_CASE("kuldetes: vegigjatszas - atvezeto, parbeszed-valasztas, regio, erosites, celok, gyozelem") {
  auto m = start(mission_desc());
  CHECK(m && m->mission);
  if (!m || !m->mission) return;
  sim::Sim& s = *m->sim;
  sim::MissionHost& h = *m->mission;
  CHECK(s.player(1).resource(sim::kGold) == 200);
  CHECK(h.named("ragna") && h.named("ragna")->size() == 1 && h.named("orseg")->size() == 3);
  CHECK(s.player(1).active && !s.player(1).defeated);
  run(*m, 2);
  CHECK(h.cutscene().id == "nyitany" && h.cutscene().letterbox && h.cutscene().camera == "nyito");
  run(*m, 50);
  CHECK(h.cutscene().camera == "kozep");
  run(*m, 45);   // ~97: az átvezető vége, a párbeszéd indul
  CHECK(h.cutscene().id.empty() && !h.cutscene().letterbox);
  CHECK(h.dialogue().dialogue == "bevezeto" && h.dialogue().node == "n1");
  run(*m, 62);
  CHECK(h.dialogue().node == "n2");   // a válaszra vár (nem lép tovább magától)
  run(*m, 200);
  CHECK(h.dialogue().node == "n2");
  const int32_t gold = s.player(1).resource(sim::kGold);
  s.queue(choose("bevezeto", 0, s.tick()));   // „Előre!”
  run(*m, 2);
  CHECK(h.var("bator") == 1 && h.dialogue().node == "n3");
  CHECK(s.player(1).resource(sim::kGold) == gold + 100 && !h.messages().empty());
  run(*m, 50);
  CHECK(h.dialogue().dialogue.empty());
  // a hős és a csapat a kapuhoz
  std::vector<sim::EntityId> mine = *h.named("ragna");
  for (const auto e : *h.named("csapat")) mine.push_back(e);
  s.queue(order(sim::Command::Kind::Move, 1, mine, Fixed::from_int(34), Fixed::from_int(30), s.tick()));
  for (int i = 0; i < 1800 && h.objective_state("o_kapu") != 2; ++i) run(*m, 1);
  CHECK(h.objective_state("o_kapu") == 2);
  CHECK(h.named("erosites") && h.named("erosites")->size() == 2);
  CHECK(h.result() == sim::MissionHost::Result::None);
  // az őrség és a hullámok (a próba egyszerűsítése: a szkript öli meg őket)
  for (const auto e : *h.named("orseg")) s.kill(e);
  run(*m, 3);
  CHECK(h.objective_state("o_orseg") == 2);
  CHECK(h.result() == sim::MissionHost::Result::Won);
  CHECK(h.errors() == 0);
  std::printf("  győzelem a %lld. tickben, hullámok: %lld\n", static_cast<long long>(s.tick()), static_cast<long long>(h.var("hullamok")));
}

TEST_CASE("kuldetes: bukas, ha a hos elesik; a rejtett cel csak felfedezve latszik") {
  auto m = start(mission_desc());
  if (!m) return;
  sim::MissionHost& h = *m->mission;
  CHECK(h.objective_state("o_titok") == 0 && h.objective_state("o_kapu") == 1);
  run(*m, 5);
  m->sim->kill(h.named("ragna")->front());
  run(*m, 3);
  CHECK(h.result() == sim::MissionHost::Result::Lost);
}

TEST_CASE("kuldetes: mentesbol folytatva es visszajatszva ugyanaz (valasztassal, atvezetovel)") {
  sim::Replay r = mission_desc();
  auto a = start(r);
  if (!a) return;
  r.commands.push_back(choose("bevezeto", 1, 200));
  r.commands.push_back(order(sim::Command::Kind::Move, 1, *a->mission->named("csapat"), Fixed::from_int(60), Fixed::from_int(60), 210));
  for (const auto& c : r.commands) a->sim->queue(c);
  run(*a, 400);
  const auto saved = a->sim->save();
  run(*a, 1500);
  auto b = start(r, true);
  CHECK(b && b->sim->load(saved));
  if (!b) return;
  b->sim->restore_obstacles(*b->map);
  run(*b, 1500);
  CHECK(a->sim->hash() == b->sim->hash());
  CHECK(a->mission->var("hullamok") == b->mission->var("hullamok") && a->mission->var("hullamok") >= 2);
  // visszajátszás (7-es változat a küldetéssel)
  r.ticks = 1200;
  r.checkpoint_every = 300;
  const sim::Replay rec = sim::record_match(r, content_once().terrain, *content_once().rules);
  const auto dec = sim::decode(sim::encode(rec));
  CHECK(dec.has_value() && dec->mission == r.mission);
  if (dec) {
    const auto v = sim::verify(*dec, &content_once().terrain, &*content_once().rules);
    if (!v.ok) std::printf("  %s\n", v.error.c_str());
    CHECK(v.ok && v.checked == 4);
  }
}
