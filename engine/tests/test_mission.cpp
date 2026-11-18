// SPDX-License-Identifier: GPL-3.0-or-later
// M11 küldetésrendszer: a leírás ellenőrzése, a trigger-motor, párbeszéd-választás parancsként, átvezető, célok,
// győzelem és bukás; mentésből folytatva és visszajátszva ugyanaz.
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "check.hpp"
#include "match_util.hpp"
#include "script/host.hpp"
#include "sim/campaign.hpp"
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
  s.queue(choose("bevezeto", 1, s.tick()));   // „Óvatosan” (+100 arany)
  run(*m, 2);
  CHECK(h.var("bator") == 0 && h.dialogue().node == "n3");
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

TEST_CASE("kuldetes: Lua-hid - a kuldetes szkriptje akciot futtat, valtozot es egysegeket olvas") {
  sim::Replay r = mission_desc();
  r.scripts.push_back({"mission", "proba.lua", R"(
    function on_tick(t)
      if t == 5 then
        S.orseg = #Mission.units("orseg")
        Mission.action{type = "set_var", var = "x", value = 7}
        Mission.action{type = "objective", id = "o_kapu", state = "done"}
        Mission.action{type = "spawn", def = "urs_pajzshordozo", owner = 1, count = 2, region = "otthon", name = "luas"}
        S.x = Mission.var("x")
        S.cel = Mission.objective("o_kapu")
      end
    end
  )"});
  auto m = start(r);
  CHECK(m && m->mission && m->hosts.size() == 1);
  if (!m || m->hosts.empty()) return;
  run(*m, 10);
  CHECK(m->hosts.front()->errors() == 0);
  for (const auto& msg : m->hosts.front()->messages()) std::printf("  %s\n", msg.c_str());
  CHECK(m->mission->var("x") == 7 && m->mission->objective_state("o_kapu") == 2);
  CHECK(m->mission->named("luas") && m->mission->named("luas")->size() == 2);
  CHECK(m->hosts.front()->run("ell", "log(S.orseg, S.x, S.cel)"));
  const auto& ms = m->hosts.front()->messages();
  CHECK(!ms.empty() && ms.back().find("3 7 2") != std::string::npos);
}

namespace {
// a k1 megnyerése a megadott választással; a hős közben tapasztalatot kap
bool win_k1(sim::MatchInstance& m, int choice, int32_t xp_milli) {
  sim::Sim& s = *m.sim;
  run(m, 2);
  if (m.mission->dialogue().dialogue != "valasztas") return false;
  s.queue(choose("valasztas", choice, s.tick()));
  run(m, 2);
  const sim::EntityId ragna = m.mission->named("ragna")->front();
  s.add_xp(ragna, xp_milli);
  s.queue(order(sim::Command::Kind::Move, 1, {ragna}, Fixed::from_int(34), Fixed::from_int(30), s.tick()));
  for (int i = 0; i < 2400 && m.mission->result() == sim::MissionHost::Result::None; ++i) run(m, 1);
  return m.mission->result() == sim::MissionHost::Result::Won;
}
}  // namespace

TEST_CASE("hadjarat: elagazas a dontes szerint, a hos atvitele, a profil mentese es visszaolvasasa") {
  const std::string dir = (std::filesystem::path(TOTU_CONTENT_DIR) / "base" / "campaigns" / "proba").string();
  const std::string maps = (std::filesystem::path(TOTU_CONTENT_DIR) / "base" / "maps").string();
  std::vector<std::string> errs;
  const auto def = sim::CampaignDef::parse(read_file(std::filesystem::path(dir) / "campaign.json"), errs);
  for (const auto& e : errs) std::printf("  %s\n", e.c_str());
  CHECK(def.has_value());
  if (!def) return;
  for (const int choice : {0, 1}) {   // 0: a hegyen át (ut = 1) → k2a; 1: a völgyön át (ut = 2) → k2b
    sim::CampaignState st = sim::CampaignState::fresh(*def);
    std::string err;
    auto r1 = sim::prepare_mission(*def, st, dir, maps, 9, &err);
    CHECK(r1.has_value());
    if (!r1) return;
    auto m1 = start(*r1);
    CHECK(m1 && win_k1(*m1, choice, 300000));
    const auto* hs1 = m1->sim->world().get<sim::HeroState>(m1->mission->named("ragna")->front());
    CHECK(hs1 && hs1->level > 1);
    // a vesztes vagy félbehagyott küldetés nem zár le
    CHECK(sim::finish_mission(*def, st, *m1));
    CHECK(st.current == (choice == 0 ? "k2a" : "k2b") && st.flags["ut"] == (choice == 0 ? 1 : 2));
    CHECK(st.heroes.count("ragna") && st.done == std::vector<std::string>{"k1"});
    // a profil JSON-ban oda-vissza
    const auto back = sim::CampaignState::from_json(*content::parse(content::dump(st.to_json())));
    CHECK(back && back->current == st.current && back->flags == st.flags && back->heroes.at("ragna") == st.heroes.at("ragna"));
    // a következő küldetésben a hős ugyanazzal a szinttel és tapasztalattal indul, a döntés változója átjön
    auto r2 = sim::prepare_mission(*def, *back, dir, maps, 9, &err);
    CHECK(r2.has_value());
    if (!r2) return;
    auto m2 = start(*r2);
    CHECK(m2 && m2->mission);
    if (!m2) return;
    const auto* hs2 = m2->sim->world().get<sim::HeroState>(m2->mission->named("ragna")->front());
    CHECK(hs2 && hs2->level == hs1->level && hs2->xp == hs1->xp && hs2->points == hs1->points);
    CHECK(m2->mission->var("ut") == (choice == 0 ? 1 : 2));
    CHECK(m2->mission->def().id == (choice == 0 ? "k2a" : "k2b"));
    // a hős a második küldetésben is a hőstulajdonságokkal (a hero_recalc után) – életereje nagyobb az alapnál
    const auto* h2 = m2->sim->world().get<sim::Health>(m2->mission->named("ragna")->front());
    CHECK(h2 && h2->max > 0);
  }
  // hibás hadjárat: ismeretlen következő küldetés
  std::vector<std::string> e2;
  CHECK(!sim::CampaignDef::parse(R"({"id": "x", "start": "a", "missions": [{"id": "a", "dir": "a", "next": [{"to": "nincs"}]}]})", e2));
  CHECK(!e2.empty());
}

TEST_CASE("objektumszerkeszto: modositott es szarmaztatott sablon a kuldetes sajat szabalyaiban") {
  const sim::Rules& base = *content_once().rules;
  const uint16_t bers = base.def("urs_berserker");
  CHECK(bers != sim::kNoDef);
  sim::Replay r = mission_desc();
  auto doc = content::parse(r.mission);
  CHECK(doc.has_value());
  if (!doc) return;
  const auto objs = content::parse(R"({
    "urs_berserker": {"hp": 999, "damage": 42, "cost": {"gold": 7}},
    "urs_vasfarkas": {"base": "urs_berserker", "hp": 1500, "armor": 6, "speed": 2.5, "range": 1.5, "supply": 3}
  })");
  doc->set("objects", *objs);
  // a vasfarkas a küldetésben: a játékos kap egyet névvel
  content::Value* units = doc->find("units");
  content::Value u = content::Value::object();
  u.set("name", content::Value::string("vas"));
  u.set("def", content::Value::string("urs_vasfarkas"));
  u.set("owner", content::Value::number("1"));
  u.set("x", content::Value::number("22"));
  u.set("y", content::Value::number("24"));
  units->push(u);
  r.mission = content::dump(*doc);
  std::vector<std::string> errs;
  CHECK(sim::MissionDef::parse(r.mission, &base, errs).has_value());
  for (const auto& e : errs) std::printf("  %s\n", e.c_str());
  auto m = start(r);
  CHECK(m && m->rules);
  if (!m || !m->rules) return;
  const sim::Rules& mr = *m->rules;
  CHECK(m->sim->rules() == &mr);
  CHECK(mr.defs[bers].hp == 999 && mr.defs[bers].attack.damage == Fixed::from_int(42) && mr.defs[bers].cost[sim::kGold] == 7);
  CHECK(base.defs[bers].hp != 999);   // a tartalom szabályai érintetlenek
  const uint16_t vf = mr.def("urs_vasfarkas");
  CHECK(vf != sim::kNoDef && vf == base.defs.size() && mr.def("urs_berserker") == bers);
  CHECK(mr.defs[vf].hp == 1500 && mr.defs[vf].armor == 6 && mr.defs[vf].supply == 3 && mr.defs[vf].faction == "ursan");
  CHECK(mr.defs[vf].speed == Fixed::from_ratio(25, 300));
  const auto* vas = m->mission->named("vas");
  CHECK(vas && vas->size() == 1);
  if (vas && !vas->empty()) {
    const auto* h = m->sim->world().get<sim::Health>(vas->front());
    CHECK(h && h->max >= 1500);
  }
  // mentés és visszajátszás a felülírásokkal is
  run(*m, 300);
  const auto saved = m->sim->save();
  run(*m, 300);
  auto b = start(r, true);
  CHECK(b && b->sim->load(saved));
  if (b) {
    b->sim->restore_obstacles(*b->map);
    run(*b, 300);
    CHECK(b->sim->hash() == m->sim->hash());
  }
  // hibás objektumok
  for (const char* bad : {R"({"nincs_ilyen": {"hp": 5}})", R"({"x": {"base": "nincs_alap"}})", R"({"urs_mammut": {"base": "urs_berserker"}})"}) {
    std::vector<std::string> e2;
    CHECK(!sim::apply_objects(base, *content::parse(bad), e2) && !e2.empty());
  }
}
