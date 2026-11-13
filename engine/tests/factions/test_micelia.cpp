// SPDX-License-Identifier: GPL-3.0-or-later
// Micéliák (M7, ADR-008; MECH-NET-1–2, D28): fonalgráf (kapcsolt / leszakadt), hálózati létszám, lerakás a fonal
// mentén, gyógyulás a mezőn, spórák, osztódás, összeolvadás, a leszakadt épület leállása; determinizmus és mentés.
// A játszható főnép a `micelia` modban van, ezért a próba az alap + mod réteget maga tölti be.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "match_util.hpp"

namespace totu::sim {
std::pair<int, int> micelia_spores(Sim& s, EntityId e);   // micelia.cpp (a próbáknak)
}

using namespace totu;
using namespace totu::testing;
using core::Fixed;

namespace {

struct ModContent {
  content::ContentSet set;
  map::TerrainTable terrain;
  std::optional<sim::Rules> rules;
  ModContent() {
    const auto root = std::filesystem::path(TOTU_CONTENT_DIR);
    content::Vfs vfs;
    vfs.add_layer("totu", root / "base");
    vfs.add_layer("micelia", root / "mods" / "micelia");
    content::ContentLoader(vfs, root / "schema").load(set);
    std::vector<std::string> errors;
    terrain.load(set, errors);
    const auto game = content::parse(read_file(root / "base" / "game.json"));
    if (game) rules = sim::Rules::load(*game, set, errors);
    for (size_t i = 0; i < errors.size() && i < 5; ++i) std::printf("  szabályhiba: %s\n", errors[i].c_str());
  }
};

ModContent& mc() {
  static ModContent c;
  return c;
}

const sim::Rules& R() { return *mc().rules; }
uint16_t def_of(const char* id) { return R().def(id); }
Fixed fx(int n, int d) { return Fixed::from_ratio(n, d); }

// meccs a BUILD-ALAP pályán: 1. játékos Micélia, 2. játékos ursan
struct Myc {
  std::optional<map::Map> map;
  std::unique_ptr<map::NavGrid> nav;
  std::unique_ptr<map::Pathfinder> pf;
  std::unique_ptr<sim::Sim> sim;
  bool ok = false;
  explicit Myc(bool start = true) {
    ModContent& c = mc();
    std::vector<std::string> errors;
    const auto doc = content::parse(read_file(std::filesystem::path(TOTU_TEST_DATA) / "maps" / "BUILD-ALAP.json"));
    if (!doc || !c.rules) return;
    map = map::Map::from_json(*doc, c.terrain, errors);
    if (!map) return;
    nav = std::make_unique<map::NavGrid>(*map, c.terrain);
    pf = std::make_unique<map::Pathfinder>(*nav);
    sim = std::make_unique<sim::Sim>(20261113);
    sim->attach_navigation(pf.get());
    sim->attach_rules(&*c.rules);
    sim::MatchSetup setup;
    setup.factions = {"mycel", "ursan"};
    std::string err;
    ok = !start || sim->setup_match(*map, setup, &err);
    if (!ok) std::printf("  setup: %s\n", err.c_str());
  }
  void run(int ticks) {
    for (int i = 0; i < ticks; ++i) sim->step();
  }
};

sim::EntityId put(sim::Sim& s, const char* id, uint8_t owner, Fixed x, Fixed y) {
  const auto e = s.spawn_unit(def_of(id), owner, x, y);
  if (auto* c = s.world().get<sim::Combat>(e)) c->order = sim::Combat::Hold;
  return e;
}

int count(sim::Sim& s, const char* id, uint8_t owner) {
  int n = 0;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit& u) {
    n += u.def == def_of(id) && s.world().get<sim::Health>(e)->owner == owner && s.world().get<sim::Health>(e)->hp > 0;
  });
  return n;
}

std::vector<sim::EntityId> units_of(sim::Sim& s, const char* id, uint8_t owner) {
  std::vector<sim::EntityId> out;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit& u) {
    if (u.def == def_of(id) && s.world().get<sim::Health>(e)->owner == owner && s.world().get<sim::Health>(e)->hp > 0) out.push_back(e);
  });
  return out;
}

// az 1. játékos Szívgombája (a hálózat magja)
sim::EntityId heart(sim::Sim& s) {
  sim::EntityId h = sim::kNoEntity;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& b) {
    if (b.def == def_of("myc_szivgomba") && s.world().get<sim::Health>(e)->owner == 1) h = e;
  });
  return h;
}

sim::Position pos(sim::Sim& s, sim::EntityId e) { return *s.world().get<sim::Position>(e); }

Fixed num(const content::Value* v) {
  const auto f = v ? v->as_fixed() : std::nullopt;
  return f ? *f : Fixed{};
}

Fixed mech(const char* id, const char* key) {
  const content::Value* m = R().defs[def_of(id)].raw->find("Mechanics");
  return num(m ? m->find(key) : nullptr);
}

int32_t net_supply() {
  return num(R().faction("mycel")->raw->find("Faction")->find("network_supply")).floor_int();
}

void rich(sim::Sim& s) {
  for (int r = 0; r < sim::kResources; ++r) s.add_resources(1, r, 20000);
}

// a fonallánc: A a Szívgomba mellett (≤ 7), B az A mellett (≤ 6, a Szívgombától > 7), C messze (> 6 mindentől)
struct Chain {
  int hx = 0, hy = 0;
};
Chain chain_tiles(sim::Sim& s) {
  const sim::Position hp = pos(s, heart(s));
  return {hp.x.floor_int(), hp.y.floor_int()};
}

}  // namespace

TEST_CASE("Micéliák: a mod betöltve, a népmodul él, a nép kulcsai a nyers JSON-ból") {
  Myc m;
  CHECK(m.ok);
  bool has = false;
  for (const auto& x : m.sim->modules_) has = has || std::string(x->name()) == "micelia";
  CHECK(has);
  CHECK(heart(*m.sim) != sim::kNoEntity);
  CHECK(mech("myc_szivgomba", "link_radius") == Fixed::from_int(7));
  CHECK(mech("myc_sporafonal", "link_radius") == Fixed::from_int(6));
  CHECK(net_supply() == 1);
}

TEST_CASE("Micéliák fonalgráf és létszám: kapcsolt kész épületenként + network_supply; a leszakadt nem ad") {
  Myc m;
  sim::Sim& s = *m.sim;
  const Chain c = chain_tiles(s);
  const uint16_t fon = def_of("myc_sporafonal");
  const int32_t prov = R().defs[fon].supply_provides, ns = net_supply();
  m.run(1);
  const int32_t cap0 = s.player(1).supply_cap;
  // A: a Szívgomba középpontjától ~5,5 (≤ 7)
  const auto a = s.place_building(fon, 1, c.hx + 5, c.hy, true);
  m.run(1);
  CHECK(s.player(1).supply_cap - cap0 == prov + ns);
  // B: A-tól 6 (≤ 6), a Szívgombától > 7 – a láncon át kapcsolt
  const auto b = s.place_building(fon, 1, c.hx + 11, c.hy, true);
  m.run(1);
  CHECK(s.player(1).supply_cap - cap0 == 2 * (prov + ns));
  // C: messze – nem kapcsolt, csak a saját létszámát adja
  const auto cc = s.place_building(fon, 1, c.hx + 18, c.hy + 4, true);
  m.run(1);
  std::printf("  létszám: %d → %d (fonal: +%d, hálózat: +%d)\n", cap0, s.player(1).supply_cap, prov, ns);
  CHECK(s.player(1).supply_cap - cap0 == 3 * prov + 2 * ns);
  // A elpusztul: B leszakad
  s.kill(a);
  m.run(2);
  CHECK(s.player(1).supply_cap - cap0 == 2 * prov);
  (void)b;
  (void)cc;
}

TEST_CASE("Micéliák létszám: a szabály network_supply_add (az Óriásliget iránya: +2) kapcsolt épületenként hozzáadódik") {
  Myc m;
  sim::Sim& s = *m.sim;
  const Chain c = chain_tiles(s);
  const uint16_t fon = def_of("myc_sporafonal");
  const int32_t prov = R().defs[fon].supply_provides, ns = net_supply();
  const content::Value* rules = R().faction("mycel")->raw->find("Paths")->find("liget")->find("rules");
  const int32_t add = num(rules->find("network_supply_add")).floor_int();
  CHECK(add == 2);
  m.run(1);
  const int32_t cap0 = s.player(1).supply_cap;
  s.place_building(fon, 1, c.hx + 5, c.hy, true);
  m.run(1);
  CHECK(s.player(1).supply_cap - cap0 == prov + ns);
  s.player_mut(1).path = "liget";
  m.run(1);
  // a Szívgomba és a fonal is kapcsolt: mindkettő + add
  CHECK(s.player(1).supply_cap - cap0 == prov + ns + 2 * add);
}

TEST_CASE("Micéliák lerakás: a nem mag épület csak egy kapcsolt saját épület link_radius-án belül") {
  Myc m;
  sim::Sim& s = *m.sim;
  const Chain c = chain_tiles(s);
  rich(s);
  m.run(1);
  const uint16_t fon = def_of("myc_sporafonal");
  std::string err;
  const bool near = s.can_place(1, fon, c.hx + 5, c.hy, &err);
  if (!near) std::printf("  közel: %s\n", err.c_str());
  CHECK(near);
  err.clear();
  CHECK(!s.can_place(1, fon, c.hx + 11, c.hy, &err));   // 11,5 > 7
  std::printf("  messze: %s\n", err.c_str());
  CHECK(err.find("hálózat") != std::string::npos);
  s.place_building(fon, 1, c.hx + 5, c.hy, true);   // a fonal kinyújtja a hálózatot
  m.run(1);
  CHECK(s.can_place(1, fon, c.hx + 11, c.hy, &err));
  // a nem kész épület nem kapcsol
  s.place_building(fon, 1, c.hx + 11, c.hy, false);
  m.run(1);
  CHECK(!s.can_place(1, fon, c.hx + 17, c.hy, &err));
}

TEST_CASE("Micéliák mező: a kapcsolt épületek field_radius-án belül a faji gyógyulás háromszoros") {
  Myc m;
  sim::Sim& s = *m.sim;
  const sim::Position hp = pos(s, heart(s));
  const auto in = put(s, "myc_pofeteg", 1, hp.x, hp.y + Fixed::from_int(3));
  const auto out = put(s, "myc_pofeteg", 1, Fixed::from_int(28) + fx(1, 2), Fixed::from_int(18) + fx(1, 2));
  const Fixed regen = R().faction("mycel")->regen;
  CHECK(num(R().game_raw->find("creep_regen_mult")) == Fixed::from_int(3));   // a szorzó a game.json-ból
  m.run(1);
  CHECK(s.mod_regen(in) == regen * Fixed::from_int(2));
  CHECK(s.mod_regen(out) == Fixed{});
  s.world().get<sim::Health>(in)->hp = 10;
  s.world().get<sim::Health>(out)->hp = 10;
  m.run(300);   // 10 s
  const int gin = s.world().get<sim::Health>(in)->hp - 10, gout = s.world().get<sim::Health>(out)->hp - 10;
  const int want_out = (regen * Fixed::from_int(10)).floor_int();
  std::printf("  10 s alatt: mezőn +%d, kívül +%d (regen %d ezrelék/s)\n", gin, gout, (regen * Fixed::from_int(1000)).floor_int());
  CHECK(std::abs(gout - want_out) <= 1);
  CHECK(std::abs(gin - 3 * want_out) <= 1);
}

TEST_CASE("Micéliák spórák: 10 s-onként +1 a hálózat 8 csempéjén belül, legfeljebb 6; ölésért +1") {
  Myc m;
  sim::Sim& s = *m.sim;
  const sim::Position hp = pos(s, heart(s));
  const auto near = put(s, "myc_pofeteg", 1, hp.x, hp.y + Fixed::from_int(3));
  const auto far = put(s, "myc_pofeteg", 1, Fixed::from_int(28) + fx(1, 2), Fixed::from_int(18) + fx(1, 2));
  const auto ursa = put(s, "urs_berserker", 2, Fixed::from_int(25), Fixed::from_int(10));
  CHECK(sim::micelia_spores(s, near).first == 0);
  CHECK(sim::micelia_spores(s, ursa).first == -1);   // csak a spórás népé
  m.run(300);
  CHECK(sim::micelia_spores(s, near).first == 1);
  CHECK(sim::micelia_spores(s, far).first == 0);
  m.run(300 * 7);
  CHECK(sim::micelia_spores(s, near).first == 6);
  // ölés: +1 spóra (a távolinak), +1 étkezés
  s.deal_damage(far, 1, def_of("myc_pofeteg"), ursa, 100000000, false);
  m.run(1);
  CHECK(sim::micelia_spores(s, far) == std::make_pair(1, 1));
}

TEST_CASE("Micéliák osztódás: split_kills ölés után kettéválik (fél-fél életerő, az iker a spórák felét kapja)") {
  Myc m;
  sim::Sim& s = *m.sim;
  const sim::Position hp = pos(s, heart(s));
  const auto p = put(s, "myc_pofeteg", 1, hp.x + Fixed::from_int(3), hp.y + Fixed::from_int(3));
  const int maxhp = s.world().get<sim::Health>(p)->max;
  m.run(1);
  const int used0 = s.player(1).supply_used;
  const int s0 = sim::micelia_spores(s, p).first;   // a 0. tick spórája (a hálózat mellett)
  for (int i = 0; i < 3; ++i) {   // osztodas3
    const auto foe = put(s, "urs_berserker", 2, Fixed::from_int(25), Fixed::from_int(10 + i));
    s.deal_damage(p, 1, def_of("myc_pofeteg"), foe, 100000000, false);
    m.run(1);
    const auto sp = sim::micelia_spores(s, p);
    std::printf("  %d. ölés: spóra %d, étkezés %d, pöfeteg %d\n", i + 1, sp.first, sp.second, count(s, "myc_pofeteg", 1));
    CHECK(sp == std::make_pair(s0 + i + 1, i + 1));
  }
  m.run(2);
  CHECK(count(s, "myc_pofeteg", 1) == 2);
  const auto both = units_of(s, "myc_pofeteg", 1);
  for (const auto e : both) {   // fél életerő (+ legfeljebb 1 a mező gyógyulásából)
    const int h = s.world().get<sim::Health>(e)->hp;
    CHECK(h >= maxhp / 2 && h <= maxhp / 2 + 1);
  }
  const auto twin = both[0] == p ? both[1] : both[0];
  CHECK(sim::micelia_spores(s, p) == std::make_pair(s0 + 3, 0));
  CHECK(sim::micelia_spores(s, twin) == std::make_pair((s0 + 3) / 2, 0));   // a spórák fele
  CHECK(s.player(1).supply_used == used0 + R().defs[def_of("myc_pofeteg")].supply);
}

TEST_CASE("Micéliák osztódás: létszám nélkül vár, a hálózat bővülésekor kettéválik") {
  Myc m;
  sim::Sim& s = *m.sim;
  const sim::Position hp = pos(s, heart(s));
  const Chain c = chain_tiles(s);
  m.run(1);
  std::vector<sim::EntityId> filler;
  while (s.player(1).supply_used < s.player(1).supply_cap - 1) {   // a pöfeteg tölti be az utolsó helyet
    filler.push_back(put(s, "myc_sporaszedo", 1, Fixed::from_int(2 + static_cast<int>(filler.size())), Fixed::from_int(18)));
    m.run(1);
  }
  const auto p = put(s, "myc_pofeteg", 1, hp.x + Fixed::from_int(3), hp.y + Fixed::from_int(3));
  m.run(1);
  std::printf("  létszám: %d / %d\n", s.player(1).supply_used, s.player(1).supply_cap);
  for (int i = 0; i < 3; ++i) {
    const auto foe = put(s, "urs_berserker", 2, Fixed::from_int(25), Fixed::from_int(10 + i));
    s.deal_damage(p, 1, def_of("myc_pofeteg"), foe, 100000000, false);
    m.run(1);
  }
  m.run(5);
  CHECK(count(s, "myc_pofeteg", 1) == 1);
  CHECK(sim::micelia_spores(s, p).second == 3);
  s.place_building(def_of("myc_sporafonal"), 1, c.hx + 5, c.hy, true);   // + network_supply
  m.run(3);
  CHECK(count(s, "myc_pofeteg", 1) == 2);
  CHECK(sim::micelia_spores(s, p).second == 0);
}

TEST_CASE("Micéliák összeolvadás: 3 pöfeteg → óriáspöfeteg, átlagolt életerő-aránnyal; messziről előbb összegyűlnek") {
  Myc m;
  sim::Sim& s = *m.sim;
  const auto a = put(s, "myc_pofeteg", 1, Fixed::from_int(4), Fixed::from_int(18));
  const auto b = put(s, "myc_pofeteg", 1, Fixed::from_int(5), Fixed::from_int(18));
  const auto c = put(s, "myc_pofeteg", 1, Fixed::from_int(6), Fixed::from_int(18));
  const int mx = s.world().get<sim::Health>(a)->max;
  s.world().get<sim::Health>(c)->hp = mx / 2;
  m.run(1);
  const int ha = s.world().get<sim::Health>(a)->hp, hb = s.world().get<sim::Health>(b)->hp, hc = s.world().get<sim::Health>(c)->hp;
  const int64_t frac = (int64_t{ha} * 1000 / mx + int64_t{hb} * 1000 / mx + int64_t{hc} * 1000 / mx) / 3;
  CHECK(s.use_ability({a, b, c}, "osszeolvadas", Fixed{}, Fixed{}, sim::kNoEntity));
  m.run(2);
  CHECK(count(s, "myc_pofeteg", 1) == 0);
  CHECK(count(s, "myc_oriaspofeteg", 1) == 1);
  const auto giant = units_of(s, "myc_oriaspofeteg", 1).front();
  const int gmax = R().defs[def_of("myc_oriaspofeteg")].hp;
  std::printf("  óriáspöfeteg: %d / %d (arány %lld ezrelék)\n", s.world().get<sim::Health>(giant)->hp, gmax, static_cast<long long>(frac));
  CHECK(s.world().get<sim::Health>(giant)->hp >= static_cast<int>(int64_t{gmax} * frac / 1000));
  CHECK(s.world().get<sim::Health>(giant)->hp <= static_cast<int>(int64_t{gmax} * frac / 1000) + 1);   // + 1 tick gyógyulás
  // messziről: előbb összegyűlnek, aztán olvadnak
  const auto d = put(s, "myc_pofeteg", 1, Fixed::from_int(1), Fixed::from_int(18));
  const auto e = put(s, "myc_pofeteg", 1, Fixed::from_int(10), Fixed::from_int(18));
  const auto f = put(s, "myc_pofeteg", 1, Fixed::from_int(19), Fixed::from_int(18));
  CHECK(s.use_ability({d, e, f}, "osszeolvadas", Fixed{}, Fixed{}, sim::kNoEntity));
  m.run(2);
  CHECK(count(s, "myc_pofeteg", 1) == 3);
  m.run(240);
  CHECK(core::length(pos(s, d).x - pos(s, e).x, pos(s, d).y - pos(s, e).y) < Fixed::from_int(4));
  CHECK(s.use_ability({d, e, f}, "osszeolvadas", Fixed{}, Fixed{}, sim::kNoEntity));
  m.run(2);
  CHECK(count(s, "myc_pofeteg", 1) == 0);
  CHECK(count(s, "myc_oriaspofeteg", 1) == 2);
  // kevés egység: nem olvad
  const auto g = put(s, "myc_pofeteg", 1, Fixed::from_int(4), Fixed::from_int(17));
  CHECK(!s.use_ability({g}, "osszeolvadas", Fixed{}, Fixed{}, sim::kNoEntity));
}

TEST_CASE("Micéliák: a leszakadt kész épület termelése nem halad") {
  Myc m;
  sim::Sim& s = *m.sim;
  const Chain c = chain_tiles(s);
  rich(s);
  const uint16_t tt = def_of("myc_termotest");
  const auto on = s.place_building(tt, 1, c.hx + 3, c.hy - 4, true);
  const auto off = s.place_building(tt, 1, 24, 14, true);
  m.run(1);
  std::string e1, e2;
  const bool t1 = s.train(1, def_of("myc_pofeteg"), on, &e1), t2 = s.train(1, def_of("myc_pofeteg"), off, &e2);
  if (!t1 || !t2) std::printf("  képzés: %s | %s\n", e1.c_str(), e2.c_str());
  CHECK(t1 && t2);
  m.run(60);
  const int p_on = s.world().get<sim::Producer>(on)->queue.front().progress;
  const int p_off = s.world().get<sim::Producer>(off)->queue.front().progress;
  std::printf("  haladás 2 s alatt: kapcsolt %d, leszakadt %d tick\n", p_on, p_off);
  CHECK(p_on >= 59);
  CHECK(p_off == 0);
}

TEST_CASE("Micéliák: determinizmus és mentésből folytatás (spórák, étkezések, fonalgráf)") {
  auto setup = [](Myc& m) {
    sim::Sim& s = *m.sim;
    const Chain c = chain_tiles(s);
    rich(s);
    s.place_building(def_of("myc_sporafonal"), 1, c.hx + 5, c.hy, true);
    const auto p = put(s, "myc_pofeteg", 1, Fixed::from_int(c.hx + 3), Fixed::from_int(c.hy + 3));
    for (int i = 0; i < 2; ++i) {
      const auto foe = put(s, "urs_berserker", 2, Fixed::from_int(25), Fixed::from_int(10 + i));
      s.deal_damage(p, 1, def_of("myc_pofeteg"), foe, 100000000, false);
    }
    put(s, "myc_pofeteg", 1, Fixed::from_int(4), Fixed::from_int(18));
    put(s, "myc_pofeteg", 1, Fixed::from_int(8), Fixed::from_int(18));
  };
  Myc a, b;
  setup(a);
  setup(b);
  a.run(400);
  b.run(400);
  CHECK(a.sim->hash() == b.sim->hash());
  const auto saved = a.sim->save();
  const auto more = [](Myc& m) {
    sim::Sim& s = *m.sim;
    const auto ps = units_of(s, "myc_pofeteg", 1);
    s.use_ability(ps, "osszeolvadas", Fixed{}, Fixed{}, sim::kNoEntity);
    m.run(600);
  };
  more(a);
  Myc c(false);
  CHECK(c.sim->load(saved));
  c.sim->restore_obstacles(*c.map);
  more(c);
  CHECK(c.sim->hash() == a.sim->hash());
  CHECK(count(*a.sim, "myc_oriaspofeteg", 1) == 1);
}
