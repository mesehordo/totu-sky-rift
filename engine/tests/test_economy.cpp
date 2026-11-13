// SPDX-License-Identifier: GPL-3.0-or-later
// Gazdaság (M6, 2. lépés; ADR-007): szabályadatok, meccsindítás a pályából, gyűjtés–hordás–leadás, kimerülés,
// Hadtáp, létszám, parancsok, determinizmus és mentés.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "check.hpp"
#include "match_util.hpp"

using namespace totu;
using core::Fixed;

using namespace totu::testing;

TEST_CASE("gazdaság: a szabályadatok a régi játék számai szerint") {
  const auto& r = content_once().rules;
  CHECK(r.has_value());
  if (!r) return;
  CHECK(r->resources[sim::kWood].carry == 8 && r->resources[sim::kStone].carry == 6);
  CHECK(sim::seconds_to_ticks(r->resources[sim::kWood].gather_time) == 66);   // 2,2 s
  CHECK(r->resources[sim::kWood].node_amount == 120 && r->resources[sim::kStone].node_amount == 1500);
  CHECK(r->resources[sim::kFaith].special);
  CHECK(r->start_resources[sim::kWood] == 400 && r->start_resources[sim::kStone] == 250 && r->max_supply == 200);
  const sim::FactionDef* f = r->faction("ursan");
  CHECK(f != nullptr);
  if (!f) return;
  CHECK(f->builder == "enter" && f->start_gather == sim::kWood);
  CHECK(f->hq != sim::kNoDef && r->defs[f->hq].id == "urs_nagyterem");
  const sim::EntityDef& hq = r->defs[f->hq];
  CHECK(hq.building && hq.fw == 4 && hq.fh == 4 && hq.supply_provides == 12 && hq.hp == 1800);
  CHECK(hq.dropoff == ((1u << sim::kWood) | (1u << sim::kStone) | (1u << sim::kMeteor)));
  CHECK(f->start_units.size() == 1 && f->start_units[0].second == 5);
  const sim::EntityDef& w = r->defs[f->worker];
  CHECK(w.id == "urs_telepes" && w.worker && !w.building && w.supply == 1 && w.cost[sim::kWood] == 30);
  CHECK(w.speed == Fixed::from_raw(Fixed::from_ratio(5, 2).raw() / 30 + 0) || w.speed.raw() == (Fixed::from_ratio(5, 2).raw() + 15) / 30);
  CHECK(w.time_ticks == 360);   // 12 s
  // Hadtáp: ≤ 80: 100%, ≤ 140: 70%, különben 40%
  CHECK(r->upkeep_permille(80) == 1000 && r->upkeep_permille(81) == 700 && r->upkeep_permille(140) == 700);
  CHECK(r->upkeep_permille(141) == 400 && r->upkeep_permille(250) == 400);
}

TEST_CASE("gazdaság: meccsindítás – főépület, 5 telepes a pálya közepe felé, gyűjtésre indulnak") {
  Match m;
  CHECK(m.ok);
  if (!m.ok) return;
  int hqs = 0, nodes = 0, trees = 0;
  m.sim->world().pool<sim::Building>().each([&](sim::EntityId, const sim::Building& b) {
    ++hqs;
    CHECK(b.complete && b.w == 4 && b.h == 4);
  });
  m.sim->world().pool<sim::ResourceNode>().each([&](sim::EntityId, const sim::ResourceNode& n) {
    ++nodes;
    if (n.kind == sim::kWood) ++trees;
  });
  CHECK(hqs == 2);
  CHECK(trees == static_cast<int>(m.map->objects().trees.size()) && nodes == trees + 5);
  for (uint8_t owner = 1; owner <= 2; ++owner) {
    const auto ws = m.workers(owner);
    CHECK(ws.size() == 5);
    for (const auto e : ws) CHECK(m.sim->world().get<sim::Worker>(e)->state == sim::Worker::Go);
    const auto& p = m.sim->player(owner);
    CHECK(p.active && p.faction == "ursan" && p.resource(sim::kWood) == 400 && p.resource(sim::kStone) == 250);
    CHECK(p.supply_cap == 12 && p.supply_used == 5);
  }
  // az 1. játékos telepesei a főépülettől jobbra (a pálya közepe felé) állnak
  for (const auto e : m.workers(1)) CHECK(m.sim->world().get<sim::Position>(e)->x > Fixed::from_int(10));
  // a fák akadályok
  const auto& [tx, ty] = m.map->objects().trees.front();
  CHECK(!m.nav->passable(0, 2 * tx, 2 * ty));
}

TEST_CASE("gazdaság: gyűjtés – a fa fogy, a bevétel nő, semmi sem vész el; a kimerült fa helye járható") {
  Match m;
  CHECK(m.ok);
  if (!m.ok) return;
  const int64_t start_nodes = m.total_node_amount();
  const int64_t start_wood = m.sim->player(1).res[sim::kWood] + m.sim->player(2).res[sim::kWood];
  const size_t trees0 = m.map->objects().trees.size();
  m.run(30 * 90);   // 90 s
  const int64_t wood = m.sim->player(1).res[sim::kWood] + m.sim->player(2).res[sim::kWood] - start_wood;
  CHECK(wood > 0);
  CHECK(wood % 1000 == 0);   // a fa nem esik a Hadtáp alá: egész egységek
  // megmaradás: a lelőhelyekből kivett = bevétel + a munkásoknál lévő rakomány
  CHECK(start_nodes - m.total_node_amount() == wood / 1000 + m.carried());
  // 5 telepes 90 s alatt: egy forduló ~2,2 s rakodás + az út oda-vissza; legalább 8 forduló fejenként
  CHECK(m.sim->player(1).gathered[sim::kWood] >= int64_t{8} * 5 * 8 * 1000);
  std::printf("  ECO-ALAP 90 s: fa 1. játékos %lld, 2. játékos %lld (5-5 telepes)\n",
              static_cast<long long>(m.sim->player(1).gathered[sim::kWood] / 1000),
              static_cast<long long>(m.sim->player(2).gathered[sim::kWood] / 1000));
  // 120 fás fa 15 forduló alatt kimerül: a hosszú futásban fák tűnnek el, a helyük járható
  m.run(30 * 240);
  int trees = 0;
  m.sim->world().pool<sim::ResourceNode>().each([&](sim::EntityId, const sim::ResourceNode& n) { trees += n.kind == sim::kWood; });
  CHECK(trees < static_cast<int>(trees0));
  int opened = 0;
  for (const auto& [x, y] : m.map->objects().trees) opened += m.nav->passable(0, 2 * x, 2 * y) ? 1 : 0;
  CHECK(opened == static_cast<int>(trees0) - trees);
  // a munkások továbbra is dolgoznak (a kimerült fa után a közelben újat találtak)
  int busy = 0;
  m.sim->world().pool<sim::Worker>().each([&](sim::EntityId, const sim::Worker& w) { busy += w.state != sim::Worker::Idle; });
  CHECK(busy == 10);
  std::printf("  ECO-ALAP 330 s: %d fa kivágva, fa 1. játékos %lld\n", static_cast<int>(trees0) - trees,
              static_cast<long long>(m.sim->player(1).gathered[sim::kWood] / 1000));
}

TEST_CASE("gazdaság: aranyhordás a Hadtáppal és nélküle; Gather, Stop és mozgásparancs") {
  for (const bool upkeep : {true, false}) {
    Match m("ECO-ALAP", upkeep);
    CHECK(m.ok);
    if (!m.ok) return;
    // az 1. játékos telepesei aranyra mennek (a legközelebbi lelőhelyre)
    sim::EntityId gold = sim::kNoEntity;
    m.sim->world().pool<sim::ResourceNode>().each([&](sim::EntityId e, const sim::ResourceNode& n) {
      if (n.kind == sim::kStone && gold == sim::kNoEntity) gold = e;
    });
    sim::Command c;
    c.kind = sim::Command::Kind::Gather;
    c.units = m.workers(1);
    c.target = gold;
    m.sim->queue(std::move(c));
    m.run(30 * 60);
    const auto& p = m.sim->player(1);
    CHECK(p.gathered[sim::kStone] > 0);
    // 5 létszámnál a Hadtáp szorzója 100%: a bevétel így is egész
    CHECK(p.gathered[sim::kStone] % 1000 == 0);
    std::printf("  arany 60 s, 5 telepes: %lld\n", static_cast<long long>(p.gathered[sim::kStone] / 1000));
  }
  Match m;
  const auto ws = m.workers(1);
  sim::Command stop;
  stop.kind = sim::Command::Kind::Stop;
  stop.units = {ws[0]};
  m.sim->queue(std::move(stop));
  sim::Command mv;
  mv.kind = sim::Command::Kind::GroupMove;
  mv.units = {ws[1]};
  mv.x = Fixed::from_int(20);
  mv.y = Fixed::from_int(28);
  m.sim->queue(std::move(mv));
  m.run(5);
  CHECK(m.sim->world().get<sim::Worker>(ws[0])->state == sim::Worker::Idle);
  CHECK(m.sim->world().get<sim::Worker>(ws[1])->state == sim::Worker::Idle);
  CHECK(m.sim->world().get<sim::Worker>(ws[2])->state != sim::Worker::Idle);
  m.run(300);
  CHECK(m.sim->world().get<sim::Worker>(ws[0])->state == sim::Worker::Idle);   // nem indul magától újra
}

TEST_CASE("gazdaság: determinisztikus, és mentésből folytatva ugyanoda jut") {
  Match a, b;
  CHECK(a.ok && b.ok);
  if (!a.ok || !b.ok) return;
  a.run(900);
  b.run(900);
  CHECK(a.sim->hash() == b.sim->hash());
  // rögzített állapothash: minden rendszeren ugyanennyi (a gazdaság determinizmusa a CI mind az öt rendszerén)
  std::printf("  gazdasági állapothash (900 tick): 0x%016llx\n", static_cast<unsigned long long>(a.sim->hash()));
  CHECK(a.sim->hash() == 0x77d03dd74528f495ULL);
  const auto saved = a.sim->save();
  a.run(900);
  Match c("ECO-ALAP", true, false);   // ugyanaz a pálya, meccsindítás nélkül
  CHECK(c.sim->load(saved));
  c.sim->restore_obstacles(*c.map);   // a navrács akadályai a betöltött épületekből és lelőhelyekből
  c.run(900);
  CHECK(c.sim->hash() == a.sim->hash());
}
