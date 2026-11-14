// SPDX-License-Identifier: GPL-3.0-or-later
// Az Ursanok teljesen (M6, 8. lépés; ADR-007): minden egység képezhető (vagy idézhető), minden épület felépíthető, minden
// kutatás elkészül és hat – a feltételekkel (épület, kutatás, felemelkedés, fejlődési irány) együtt.
#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "match_util.hpp"

using namespace totu;
using namespace totu::testing;
using core::Fixed;

namespace {

const sim::Rules& R() { return *content_once().rules; }
uint16_t def_of(const std::string& id) { return R().def(id); }

const sim::FactionDef& ursan() { return *R().faction("ursan"); }

// a BUILD-ALAP szabad, sík helyei a 3 × 3-as épületeknek (a főépület 7–10 × 13–16, a tó 14–16 × 2–7)
const std::vector<std::pair<int, int>> kSpots = {{12, 9}, {16, 9}, {12, 13}, {16, 13}, {0, 16}, {3, 16}, {12, 17}, {16, 17}};

struct Setup {
  Match m{"BUILD-ALAP"};
  size_t next = 0;
  sim::Sim& s() { return *m.sim; }
  std::pair<int, int> spot() { return kSpots[next++ % kSpots.size()]; }
  bool has_building(const std::string& id) {
    bool found = false;
    s().world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& b) {
      found = found || (b.def == def_of(id) && b.complete && s().world().get<sim::Health>(e)->owner == 1);
    });
    return found;
  }
  sim::EntityId place(const std::string& id) {
    const auto [x, y] = spot();
    return s().place_building(def_of(id), 1, x, y, true);
  }
  void rich() {
    for (int r = 0; r < sim::kResources; ++r) s().add_resources(1, r, 20000);
  }
  void ascend() {
    if (s().player(1).ascended) return;
    for (const char* id : {"urs_hosszuhaz", "urs_harcosok_haza", "urs_runakovacs"}) {
      if (!has_building(id)) place(id);
    }
    m.run(16);
  }
  bool research_done(const std::string& id) {
    const auto& r = s().player(1).researched;
    return std::binary_search(r.begin(), r.end(), id);
  }
  // egy kutatás (a feltételeivel és a kutató épülettel együtt)
  bool do_research(const std::string& id) {
    if (research_done(id)) return true;
    const sim::ResearchDef* rd = R().research(id);
    if (!rd) return false;
    satisfy(rd->reqs);
    for (const auto& d : R().defs) {   // a kutató épület
      if (d.faction != "ursan" || std::find(d.researches.begin(), d.researches.end(), id) == d.researches.end()) continue;
      if (!has_building(d.id)) {
        satisfy(d.reqs);
        place(d.id);
      }
      break;
    }
    rich();
    std::string err;
    if (!s().research(1, id, &err)) {
      std::printf("  kutatás %s: %s\n", id.c_str(), err.c_str());
      return false;
    }
    m.run(rd->time_ticks + 2);
    return research_done(id);
  }
  void satisfy(const std::vector<sim::EntityDef::Req>& reqs) {
    for (const auto& q : reqs) {
      switch (q.kind) {
        case sim::EntityDef::Req::Building: {
          const sim::EntityDef& bd = R().defs[def_of(q.id)];
          satisfy(bd.reqs);
          if (!has_building(q.id)) place(q.id);
          break;
        }
        case sim::EntityDef::Req::Research:
          do_research(q.id);
          break;
        case sim::EntityDef::Req::Path:
          ascend();
          if (s().player(1).path.empty()) s().choose_path(1, q.id);
          break;
        case sim::EntityDef::Req::Ascended:
          ascend();
          break;
        case sim::EntityDef::Req::Ally:   // kisnép: Diplomácia csarnoka és szövetség
          if (!has_building("urs_diplomacia")) place("urs_diplomacia");
          rich();
          s().choose_ally(1, q.id);
          break;
      }
    }
  }
};

int count_owned(sim::Sim& s, uint16_t def) {
  int n = 0;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit& u) {
    if (u.def == def && s.world().get<sim::Health>(e)->owner == 1 && s.world().get<sim::Health>(e)->hp > 0) ++n;
  });
  return n;
}

const std::string* producer_of(const std::string& unit) {
  for (const auto& d : R().defs) {
    if (d.faction != "ursan" || !d.building) continue;
    if (std::find(d.produces.begin(), d.produces.end(), def_of(unit)) != d.produces.end()) return &d.id;
  }
  return nullptr;
}

}  // namespace

TEST_CASE("Ursanok: a népi lista – 18 egység, 15 épület, 16 kutatás, mind betöltve") {
  const auto* v = content_once().set["faction"].get("faction_ursan");
  CHECK(v != nullptr);
  if (!v) return;
  const auto* roster = v->find("Roster");
  int units = 0, buildings = 0, researches = 0;
  for (const auto& u : roster->find("units")->items()) {
    CHECK(def_of(u.text()) != sim::kNoDef && !R().defs[def_of(u.text())].building);
    ++units;
  }
  for (const auto& b : roster->find("buildings")->items()) {
    CHECK(def_of(b.text()) != sim::kNoDef && R().defs[def_of(b.text())].building);
    ++buildings;
  }
  for (const auto& r : roster->find("researches")->items()) {
    CHECK(R().research(r.text()) != nullptr);
    ++researches;
  }
  CHECK(units == 18 && buildings == 15 && researches == 16);
  CHECK(ursan().paths.size() == 3 && ursan().ascension_buildings == 3);
}

TEST_CASE("Ursanok: minden egység képezhető a saját épületéből a feltételeivel (vagy képességgel idézhető)") {
  const auto* roster = content_once().set["faction"].get("faction_ursan")->find("Roster");
  int trained = 0, summoned = 0;
  for (const auto& uv : roster->find("units")->items()) {
    const std::string id = uv.text();
    const std::string* prod = producer_of(id);
    if (!prod) {   // idézett lény: egy ursan képesség idézi
      bool by_ability = false;
      for (const auto& a : R().abilities) by_ability = by_ability || (a.unit == def_of(id));
      CHECK(by_ability);
      if (by_ability) ++summoned;
      continue;
    }
    Setup st;
    const sim::EntityDef& d = R().defs[def_of(id)];
    std::string err;
    CHECK(!st.s().train(1, def_of(id), 0, &err) || d.reqs.empty());   // feltétel nélkül csak a feltétel nélküli
    st.satisfy(R().defs[def_of(*prod)].reqs);
    if (!st.has_building(*prod)) st.place(*prod);
    st.satisfy(d.reqs);
    st.place("urs_tanya");   // létszám
    st.place("urs_tanya");
    st.m.run(1);
    st.rich();
    const int before = count_owned(st.s(), def_of(id));
    const bool ok = st.s().train(1, def_of(id), 0, &err);
    if (!ok) std::printf("  %s: %s\n", id.c_str(), err.c_str());
    CHECK(ok);
    st.m.run(d.time_ticks + 30);
    const bool out = count_owned(st.s(), def_of(id)) == before + 1;
    if (!out) std::printf("  %s nem jelent meg\n", id.c_str());
    if (!out) {
      st.s().world().pool<sim::Producer>().each([&](sim::EntityId e, const sim::Producer& pr) {
        if (!pr.queue.empty()) std::printf("    sor: épület %u, %zu elem, haladás %d/%d\n", e, pr.queue.size(), pr.queue[0].progress, pr.queue[0].time);
      });
    }
    CHECK(out);
    trained += ok && out ? 1 : 0;
  }
  std::printf("  képezve %d, idézve %d egység\n", trained, summoned);
  CHECK(trained == 16 && summoned == 2);
}

TEST_CASE("Ursanok: minden épület felépíthető (a telepes bentről építi), a feltételeivel; a hős legfeljebb három") {
  const auto* roster = content_once().set["faction"].get("faction_ursan")->find("Roster");
  int built = 0;
  for (const auto& bv : roster->find("buildings")->items()) {
    const std::string id = bv.text();
    const sim::EntityDef& d = R().defs[def_of(id)];
    Setup st;
    st.satisfy(d.reqs);
    st.rich();
    const auto [x, y] = st.spot();
    const auto workers = st.m.workers(1);
    std::string err;
    const bool ok = st.s().order_build(workers.front(), def_of(id), x, y, &err);
    if (!ok) std::printf("  %s: %s\n", id.c_str(), err.c_str());
    CHECK(ok);
    st.m.run(d.time_ticks + 300);
    bool done = false;
    st.s().world().pool<sim::Building>().each([&](sim::EntityId, const sim::Building& b) {
      done = done || (b.def == def_of(id) && b.tx == x && b.ty == y && b.complete);
    });
    if (!done) std::printf("  %s nem készült el\n", id.c_str());
    CHECK(done);
    built += ok && done ? 1 : 0;
  }
  std::printf("  felépítve %d épület\n", built);
  CHECK(built == 15);
  // a hős legfeljebb három, és mindegyik csak egyszer
  Setup st;
  st.place("urs_harcosok_haza");
  st.place("urs_eskufa");
  st.place("urs_tanya");
  st.place("urs_tanya");
  st.m.run(1);
  st.rich();
  std::string err;
  CHECK(st.s().train(1, def_of("urs_hos_ragna"), 0, &err));
  CHECK(!st.s().train(1, def_of("urs_hos_ragna"), 0, &err));
  CHECK(st.s().train(1, def_of("urs_hos_hjalmar"), 0, &err));
  CHECK(st.s().train(1, def_of("urs_hos_bjarka"), 0, &err));
}

TEST_CASE("Ursanok: minden kutatás elkészül a feltételeivel; a 2. és 3. fokozat sorban, a 3. felemelkedés után") {
  const auto* roster = content_once().set["faction"].get("faction_ursan")->find("Roster");
  int done = 0;
  for (const auto& rv : roster->find("researches")->items()) {
    Setup st;
    const sim::ResearchDef* rd = R().research(rv.text());
    CHECK(!rd->modifiers.empty() || rd->rules.garrison_dmg >= 0 || rd->rules.has_supply_add || rd->rules.has_low_hp_rage ||
          rd->rules.has_no_weak);   // minden kutatásnak van hatása
    const bool ok = st.do_research(rv.text());
    if (!ok) std::printf("  %s nem készült el\n", rv.text().c_str());
    CHECK(ok);
    done += ok ? 1 : 0;
  }
  CHECK(done == 16);
  // fokozatok: a 3. felemelkedés nélkül nem indítható
  Setup st;
  CHECK(st.do_research("urs_runapengek"));
  CHECK(st.do_research("urs_runapengek_2"));
  st.rich();
  std::string err;
  CHECK(!st.s().player(1).ascended);
  CHECK(!st.s().research(1, "urs_runapengek_3", &err));
}

TEST_CASE("Ursanok: kutatás-hatások – Vasszigony, Népfelkelés (a telepesre), Rúnapajzs (épület), Telegi edzés") {
  Setup st;
  sim::Sim& s = st.s();
  const auto sp = s.spawn_unit(def_of("urs_szigonyvezo"), 1, Fixed::from_int(2), Fixed::from_ratio(185, 10));
  const auto sh = s.spawn_unit(def_of("urs_pajzshordozo"), 1, Fixed::from_int(3), Fixed::from_ratio(185, 10));
  s.world().get<sim::Combat>(sh)->order = sim::Combat::Hold;   // ne üssön bele a mérésbe
  const auto worker = st.m.workers(1).front();
  CHECK(st.do_research("urs_vasszigony"));
  CHECK(st.do_research("urs_nepfelkeles"));
  CHECK(st.do_research("urs_runapajzs"));
  CHECK(st.do_research("urs_telegi_edzes"));
  // a telepes életereje 55 + 40 (Népfelkelés), a pajzshordozóé 157 + 25 (Telegi edzés); élőben is
  CHECK(s.world().get<sim::Health>(worker)->max == 95);
  CHECK(s.world().get<sim::Health>(sh)->max == 182);
  CHECK(s.world().get<sim::Health>(sp)->max == 115);   // a szigonyvető is gyalogos
  // a főépület páncélja +2, és gyógyul (+1/s)
  sim::EntityId hq = sim::kNoEntity;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& b) {
    if (b.def == def_of("urs_nagyterem") && s.world().get<sim::Health>(e)->owner == 1) hq = e;
  });
  s.world().get<sim::Health>(hq)->hp = 1000;
  st.m.run(300);
  std::printf("  Rúnapajzs: a Nagyterem 10 s alatt 1000 → %d\n", s.world().get<sim::Health>(hq)->hp);
  CHECK(s.world().get<sim::Health>(hq)->hp >= 1009 && s.world().get<sim::Health>(hq)->hp <= 1011);
  // a sebzések: szigony 14 + 2, a telepes 6 + 5 (egy berserkerre, 1 páncél)
  const auto foe = s.spawn_unit(def_of("urs_berserker"), 2, Fixed::from_ratio(60, 10), Fixed::from_ratio(185, 10));
  s.world().get<sim::Afflict>(foe)->stun_until = 1000000;
  st.m.run(6);
  auto hit = [&](sim::EntityId a) {
    const auto* h = s.world().get<sim::Health>(foe);
    const int64_t before = int64_t{h->max - h->hp} * 1000 + s.world().get<sim::Vital>(foe)->deficit;
    s.stop(a);   // a munkás abbahagyja a gyűjtést
    CHECK(s.order_attack(a, foe));
    for (int i = 0; i < 400; ++i) {
      s.step();
      const int64_t now = int64_t{h->max - h->hp} * 1000 + s.world().get<sim::Vital>(foe)->deficit;
      if (now != before) {
        if (auto* c = s.world().get<sim::Combat>(a)) c->order = sim::Combat::Hold;
        s.stop(a);
        return now - before;
      }
    }
    return int64_t{-1};
  };
  const int64_t spear = hit(sp);
  std::printf("  Vasszigony: %lld; Népfelkelés: ", static_cast<long long>(spear));
  CHECK(spear == 15000);   // 14 + 2 − 1
  s.kill(sp);   // a további lövései ne zavarjanak
  st.m.run(1);
  s.world().get<sim::Afflict>(foe)->stun_until = 1000000;
  const int64_t levy = hit(worker);
  std::printf("%lld\n", static_cast<long long>(levy));
  CHECK(levy == 10000);   // 6 + 5 − 1
}
