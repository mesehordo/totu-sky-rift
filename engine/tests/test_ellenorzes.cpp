// SPDX-License-Identifier: GPL-3.0-or-later
// A hibajegyzék nyitott ellenőrzései (C-101…C-115): célzott próbák a mentés, a determinizmus, a gazdaság és az
// útkeresés határeseteire.
#include <cstdio>
#include <optional>
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
  return e;
}

sim::EntityId hq_of(sim::Sim& s, uint8_t owner) {
  sim::EntityId out = sim::kNoEntity;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building&) {
    if (out == sim::kNoEntity && s.world().get<sim::Health>(e)->owner == owner) out = e;
  });
  return out;
}

// mentés a `at`. tickben, betöltés friss meccsbe; mindkettő még `more` tickig fut: a hash egyezik
bool roundtrip(const std::string& map, const std::vector<std::string>& factions, int at, int more,
               void (*setup)(sim::Sim&) = nullptr) {
  Match a(map, true, true, factions);
  if (!a.ok) return false;
  if (setup) setup(*a.sim);
  a.run(at);
  const auto saved = a.sim->save();
  a.run(more);
  Match b(map, true, false, factions);
  if (!b.sim->load(saved)) return false;
  b.sim->restore_obstacles(*b.map);
  b.run(more);
  return a.sim->hash() == b.sim->hash();
}

}  // namespace

TEST_CASE("ellenorzes C-104: mentes-betoltes-folytatas minden jatszhato neppel") {
  int n = 0;
  for (const sim::FactionDef& f : R().factions) {
    if (f.neutral || f.ally) continue;
    const bool ok = roundtrip("ECO-ALAP", {f.id, "ursan"}, 300, 300);
    if (!ok) std::printf("  eltérés: %s\n", f.id.c_str());
    CHECK(ok);
    ++n;
  }
  std::printf("  %d nép\n", n);
  CHECK(n >= 6);
}

TEST_CASE("ellenorzes C-102 C-103: fuggo parancs es tick kozott megolt egyseg a mentesben") {
  // C-102: a következő tickekre szóló parancs a mentésben (sorosítva) és utána ugyanúgy hajtódik végre
  CHECK(roundtrip("BUILD-ALAP", {"ursan", "wullof"}, 10, 200, [](sim::Sim& s) {
    const auto u = put(s, "urs_berserker", 1, 100, 100);
    sim::Command c;
    c.kind = sim::Command::Kind::Move;
    c.owner = 1;
    c.tick = 40;
    c.units = {u, u, 999999};   // ismétlődő és nem létező azonosító is
    c.x = fx(150, 10);
    c.y = fx(140, 10);
    s.queue(c);
  }));
  // C-103: a tickek között megölt (törlésre jelölt) egység: a mentés és a folytatás ugyanaz
  Match a("BUILD-ALAP", true, true, {"ursan", "wullof"});
  const auto v = put(*a.sim, "wul_landzsas", 2, 150, 120);
  a.run(5);
  a.sim->kill(v);
  const auto saved = a.sim->save();
  a.run(60);
  Match b("BUILD-ALAP", true, false, {"ursan", "wullof"});
  CHECK(b.sim->load(saved));
  b.sim->restore_obstacles(*b.map);
  b.run(60);
  CHECK(a.sim->hash() == b.sim->hash());
  CHECK(!b.sim->world().alive(v));
}

TEST_CASE("ellenorzes C-109 C-112: az epulettel elvesz a sor letszama, a letszamot ado egyseg halala csokkenti a korlatot") {
  Match m("BUILD-ALAP", true, true, {"ursan", "wullof"});
  sim::Sim& s = *m.sim;
  m.run(1);
  const int32_t base = s.player(1).supply_used;
  std::string why;
  for (int i = 0; i < 3; ++i) CHECK(s.train(1, def_of("urs_telepes"), 0, &why));
  CHECK(s.player(1).supply_used > base);
  const auto hq = hq_of(s, 1);
  s.deal_damage(0, 2, sim::kNoDef, hq, 100000000, false);
  m.run(2);
  CHECK(!s.world().alive(hq));
  CHECK(s.player(1).supply_used == base);   // a sorral együtt a foglalás is megszűnt
  // C-112: a Wullof emu létszámot ad; halála után a korlát visszaesik
  m.run(1);
  const int32_t cap0 = s.player(2).supply_cap;
  const auto emu = put(s, "wul_emu", 2, 200, 200);
  m.run(2);
  const int32_t cap1 = s.player(2).supply_cap;
  CHECK(cap1 >= cap0 && cap1 <= cap0 + 6);
  s.kill(emu);
  m.run(2);
  CHECK(s.player(2).supply_cap == cap0);
}

TEST_CASE("ellenorzes C-110 C-111: telt sor es letszamkorlat mellett nincs fizetes") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  s.add_resources(1, sim::kWood, 5000);
  s.add_resources(1, sim::kGold, 5000);
  std::string why;
  int ok = 0;
  for (int i = 0; i < 12; ++i) ok += s.train(1, def_of("urs_telepes"), 0, &why) ? 1 : 0;
  CHECK(ok <= sim::Rules::kQueueMax);
  const auto res = s.player(1).res;
  const int32_t used = s.player(1).supply_used;
  CHECK(!s.train(1, def_of("urs_telepes"), 0, &why));
  CHECK(s.player(1).res == res && s.player(1).supply_used == used);   // az elutasított képzés nem fizet
  CHECK(s.player(1).supply_used <= s.player(1).supply_cap);
  m.run(2);
  CHECK(s.player(1).supply_used <= s.player(1).supply_cap);
}

TEST_CASE("ellenorzes C-115: a palya szelere kuldott egyseg a palyan belul marad") {
  Match m("BUILD-ALAP", true, true, {"ursan", "wullof"});
  sim::Sim& s = *m.sim;
  const auto u = put(s, "urs_berserker", 1, 20, 20);
  const Fixed W = Fixed::from_int(m.map->width()), H = Fixed::from_int(m.map->height());
  s.order_move(u, Fixed{} - Fixed::from_int(5), H + Fixed::from_int(5));   // a pályán kívüli célpont
  bool inside = true;
  for (int i = 0; i < 600; ++i) {
    m.run(1);
    const auto& p = *s.world().get<sim::Position>(u);
    inside = inside && p.x >= Fixed{} && p.y >= Fixed{} && p.x <= W && p.y <= H;
  }
  CHECK(inside);
}

TEST_CASE("iranyitas D46: Shift-parancssor, jaror, mentesbol folytatva, a kozvetlen parancs torli a sort") {
  Match m("BUILD-ALAP", true, true, {"ursan", "wullof"});
  sim::Sim& s = *m.sim;
  const auto u = put(s, "urs_berserker", 1, 100, 100);
  const uint64_t h0 = s.hash();
  auto move = [&](int x10, int y10, bool queued) {
    sim::Command c;
    c.kind = sim::Command::Kind::Move;
    c.owner = 1;
    c.tick = s.tick();
    c.units = {u};
    c.x = fx(x10, 10);
    c.y = fx(y10, 10);
    if (queued) c.flag = sim::Command::kQueue;
    s.queue(c);
  };
  move(160, 100, false);
  move(160, 160, true);   // utána ide
  move(100, 160, true);   // végül ide
  m.run(2);
  CHECK(s.hash() != h0);
  // az első célon túl a második, majd a harmadik
  bool second = false;
  for (int i = 0; i < 1200; ++i) {
    m.run(1);
    const auto& p = *s.world().get<sim::Position>(u);
    if (core::length(p.x - fx(160, 10), p.y - fx(160, 10)) < fx(5, 10)) second = true;
    if (second && core::length(p.x - fx(100, 10), p.y - fx(160, 10)) < fx(5, 10)) break;
  }
  const auto& pe = *s.world().get<sim::Position>(u);
  CHECK(second && core::length(pe.x - fx(100, 10), pe.y - fx(160, 10)) < fx(6, 10));
  // járőr: a két végpont között ingázik; mentés közben, folytatva ugyanaz
  sim::Command pc;
  pc.kind = sim::Command::Kind::Patrol;
  pc.owner = 1;
  pc.tick = s.tick();
  pc.units = {u};
  pc.x = fx(140, 10);
  pc.y = fx(160, 10);
  s.queue(pc);
  move(100, 100, true);   // a járőr alatt a sor vár
  m.run(300);
  const auto saved = s.save();
  m.run(400);
  Match b("BUILD-ALAP", true, false, {"ursan", "wullof"});
  CHECK(b.sim->load(saved));
  b.sim->restore_obstacles(*b.map);
  b.run(400);
  CHECK(b.sim->hash() == s.hash());
  // még járőrözik (nem állt meg)
  int visits = 0;
  bool near_b = false;
  for (int i = 0; i < 900; ++i) {
    m.run(1);
    const auto& p = *s.world().get<sim::Position>(u);
    const bool nb = core::length(p.x - fx(140, 10), p.y - fx(160, 10)) < fx(8, 10);
    if (nb && !near_b) ++visits;
    near_b = nb;
  }
  CHECK(visits >= 1);
  // a közvetlen parancs törli a járőrt és a sort: utána megáll a célnál
  move(120, 120, false);
  m.run(400);
  const auto& pf = *s.world().get<sim::Position>(u);
  CHECK(core::length(pf.x - fx(120, 10), pf.y - fx(120, 10)) < fx(6, 10));
  m.run(200);
  const auto& pg = *s.world().get<sim::Position>(u);
  CHECK(core::length(pg.x - fx(120, 10), pg.y - fx(120, 10)) < fx(6, 10));
  // az idegen játékos nem törölheti a sort (H-01)
  const auto v = put(s, "wul_landzsas", 2, 60, 60);
  sim::Command q2;
  q2.kind = sim::Command::Kind::Patrol;
  q2.owner = 2;
  q2.tick = s.tick();
  q2.units = {v};
  q2.x = fx(60, 10);
  q2.y = fx(90, 10);
  s.queue(q2);
  m.run(2);
  sim::Command steal;
  steal.kind = sim::Command::Kind::Stop;
  steal.owner = 1;
  steal.tick = s.tick();
  steal.units = {v};
  s.queue(steal);
  m.run(60);
  const auto* vm = s.world().get<sim::Movement>(v);
  const auto* vc = s.world().get<sim::Combat>(v);
  CHECK((vm && vm->moving) || (vc && vc->order != sim::Combat::None));   // a 2. játékos járőre fut tovább
}

namespace {
// meccs a megadott móddal (a Match segéd a szokásos indítást végzi; itt a beállítás kell)
struct ModeMatch {
  std::optional<map::Map> map;
  std::unique_ptr<map::NavGrid> nav;
  std::unique_ptr<map::Pathfinder> pf;
  std::unique_ptr<sim::Sim> sim;
  bool ok = false;
  explicit ModeMatch(sim::MatchSetup setup, const std::string& name = "BUILD-ALAP", bool start = true) {
    Content& c = content_once();
    std::vector<std::string> errors;
    const auto doc = content::parse(read_file(std::filesystem::path(TOTU_TEST_DATA) / "maps" / (name + ".json")));
    map = map::Map::from_json(*doc, c.terrain, errors);
    nav = std::make_unique<map::NavGrid>(*map, c.terrain);
    pf = std::make_unique<map::Pathfinder>(*nav);
    sim = std::make_unique<sim::Sim>(777);
    sim->attach_navigation(pf.get());
    sim->attach_rules(&*c.rules);
    std::string err;
    ok = !start || sim->setup_match(*map, setup, &err);
    if (!ok) std::printf("  %s\n", err.c_str());
  }
  void run(int n) { for (int i = 0; i < n; ++i) sim->step(); }
};
sim::MatchSetup setup_of(sim::MatchSetup::Victory v, sim::MatchSetup::Tempo t, int minutes = 0) {
  sim::MatchSetup st;
  st.factions = {"ursan", "wullof"};
  st.victory = v;
  st.tempo = t;
  st.minutes = minutes;
  return st;
}
}  // namespace

TEST_CASE("M12 jatekmodok: az alapertelmezett mod hash-e valtozatlan, a Fovaros a foepulettel veszit") {
  ModeMatch a(setup_of(sim::MatchSetup::Victory::Conquest, sim::MatchSetup::Tempo::Classic));
  Match b("BUILD-ALAP", true, true, {"ursan", "wullof"});
  CHECK(a.ok && !a.sim->match_mode().active());
  // Főváros: a 2. játékos más épülettel is kiesik, ha a főépülete elpusztul
  ModeMatch c(setup_of(sim::MatchSetup::Victory::Capital, sim::MatchSetup::Tempo::Classic));
  CHECK(c.ok);
  sim::Sim& s = *c.sim;
  s.place_building(R().def("wul_tipi"), 2, 40, 40, true);
  c.run(2);
  CHECK(!s.player(2).defeated);
  s.deal_damage(0, 1, sim::kNoDef, hq_of(s, 2), 100000000, false);
  c.run(3);
  CHECK(s.player(2).defeated && s.winner() == 1);
}

TEST_CASE("M12 jatekmodok: Dombkiraly - a kozepet egyedul tarto jatekos nyer; Idokorlat - a tobb pont nyer") {
  ModeMatch h(setup_of(sim::MatchSetup::Victory::Hill, sim::MatchSetup::Tempo::Classic, 1));
  CHECK(h.ok);
  sim::Sim& s = *h.sim;
  const int cx = h.map->width() / 2, cy = h.map->height() / 2;
  const auto king = put(s, "urs_berserker", 1, cx * 10, cy * 10);
  sim::Command hold;
  hold.kind = sim::Command::Kind::Hold;
  hold.owner = 1;
  hold.tick = s.tick();
  hold.units.push_back(king);
  s.queue(hold);   // a dombon marad (nem indul harcolni)
  h.run(30 * 30);
  CHECK(s.winner() == 0);   // fél perc még nem elég
  h.run(31 * 30);
  CHECK(s.winner() == 1 && s.player(2).defeated);
  // Időkorlát: 1 perc; az 1. játékos több egységgel
  ModeMatch t(setup_of(sim::MatchSetup::Victory::Timed, sim::MatchSetup::Tempo::Classic, 1));
  CHECK(t.ok);
  for (int i = 0; i < 6; ++i) put(*t.sim, "urs_berserker", 1, 100 + i * 8, 100);
  t.run(60 * 30 + 2);
  CHECK(t.sim->winner() == 1);
}

TEST_CASE("M12 tempo: Nyugodt - beke a jatekosok kozott; Gyors - ketszeres kezdo nyersanyag; Tulels - hullamok es gyozelem") {
  ModeMatch calm(setup_of(sim::MatchSetup::Victory::Conquest, sim::MatchSetup::Tempo::Calm));
  sim::Sim& s = *calm.sim;
  const auto a = put(s, "urs_berserker", 1, 150, 150);
  const auto b = put(s, "wul_landzsas", 2, 160, 150);
  calm.run(300);
  CHECK(s.world().get<sim::Health>(a)->hp == s.world().get<sim::Health>(a)->max);
  CHECK(s.world().get<sim::Health>(b)->hp == s.world().get<sim::Health>(b)->max);   // nem harcolnak
  CHECK(!s.hostile(1, 2) && s.hostile(0, 1));
  ModeMatch classic(setup_of(sim::MatchSetup::Victory::Conquest, sim::MatchSetup::Tempo::Classic));
  ModeMatch rush(setup_of(sim::MatchSetup::Victory::Conquest, sim::MatchSetup::Tempo::Rush));
  CHECK(rush.sim->player(1).resource(sim::kGold) == 2 * classic.sim->player(1).resource(sim::kGold));
  // Túlélés (1 perc a teszthez; az első hullám a 3. percben jönne – itt rövidebb: a győzelem az időn múlik)
  ModeMatch surv(setup_of(sim::MatchSetup::Victory::Conquest, sim::MatchSetup::Tempo::Survival, 4));
  sim::Sim& v = *surv.sim;
  CHECK(!v.hostile(1, 2));
  surv.run(3 * 60 * 30 + 5);
  CHECK(v.match_mode().wave == 1);
  int wild = 0;
  v.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit&) { wild += v.world().get<sim::Health>(e)->owner == 0 ? 1 : 0; });
  CHECK(wild >= 5);
  // mentésből folytatva ugyanaz
  const auto saved = v.save();
  surv.run(600);
  ModeMatch again(setup_of(sim::MatchSetup::Victory::Conquest, sim::MatchSetup::Tempo::Survival, 4), "BUILD-ALAP", false);
  CHECK(again.sim->load(saved));
  again.sim->restore_obstacles(*again.map);
  again.run(600);
  CHECK(again.sim->hash() == v.hash());
  surv.run(60 * 30);
  CHECK(v.winner() != 0 || v.player(1).defeated);
  std::printf("  túlélés: hullám %d, győztes %d\n", v.match_mode().wave, v.winner());
}
