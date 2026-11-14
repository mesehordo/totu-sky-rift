// SPDX-License-Identifier: GPL-3.0-or-later
// Hősök, tárgyak, semleges táborok (M7 2. lépés; ADR-008, D25; HOS-1–4, TARGY-1–2, TABOR-1–2).
#include <cstdio>
#include <cstdlib>
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
uint16_t item_of(const char* id) { return static_cast<uint16_t>(R().item_index(id)); }
Fixed fx(int n, int d) { return Fixed::from_ratio(n, d); }

sim::EntityId put(sim::Sim& s, const char* id, uint8_t owner, int x10, int y10) {
  const auto e = s.spawn_unit(def_of(id), owner, fx(x10, 10), fx(y10, 10));
  if (auto* c = s.world().get<sim::Combat>(e)) c->order = sim::Combat::Hold;
  return e;
}

sim::HeroState& hs(sim::Sim& s, sim::EntityId e) { return *s.world().get<sim::HeroState>(e); }
sim::Health& hp(sim::Sim& s, sim::EntityId e) { return *s.world().get<sim::Health>(e); }

void kill_by(sim::Sim& s, sim::EntityId killer, sim::EntityId victim) {
  s.deal_damage(killer, s.world().get<sim::Health>(killer)->owner, sim::kNoDef, victim, 100000000, false, true);
  s.step();
}

// meccs a Vadon táboraival
struct CampMatch {
  Match m;
  explicit CampMatch(const char* map) : m(map, true, false) {
    sim::MatchSetup setup;
    setup.factions = {"ursan", "ursan"};
    setup.camps = true;
    std::string err;
    CHECK(m.sim->setup_match(*m.map, setup, &err));
  }
};

}  // namespace

TEST_CASE("hős: tapasztalat ölésért (képlet, osztozás 12 csempén belül), szintlépés (+pont, növekedés, +25% életerő)") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto a = put(s, "urs_hos_ragna", 1, 20, 180);
  const auto b = put(s, "urs_hos_bjarka", 1, 40, 180);
  const auto far = put(s, "urs_hos_hjalmar", 1, 200, 100);
  CHECK(hs(s, a).level == 1 && hs(s, a).points == 1 && hs(s, a).learned.empty());
  // berserker: 8 + 0,18 × költség (a sablon szerint), a két közeli hős fele-fele; a távoli semmit
  const auto foe = put(s, "urs_berserker", 2, 30, 185);
  int64_t cost = 0;
  for (const int32_t c : R().defs[def_of("urs_berserker")].cost) cost += c;
  const int32_t want = static_cast<int32_t>(((Fixed::from_int(8) + fx(18, 100) * Fixed::from_int(static_cast<int32_t>(cost))).raw() * int64_t{1000}) >> 16);
  kill_by(s, a, foe);
  std::printf("  berserker: %d ezred tapasztalat, fejenként %d\n", want, hs(s, a).xp);
  CHECK(std::abs(hs(s, a).xp - want / 2) <= 1 && hs(s, a).xp == hs(s, b).xp && hs(s, far).xp == 0);
  // a Vadon lénye a saját `Hero.xp`-jét adja
  const auto bug = s.spawn_unit(def_of("vad_kristalybogar"), 0, fx(25, 10), fx(175, 10));
  const int32_t before = hs(s, a).xp;
  kill_by(s, a, bug);
  CHECK(hs(s, a).xp - before == 45000 / 2);
  // szintlépés: 200 tapasztalatnál 2. szint
  const int32_t max1 = hp(s, a).max;
  hp(s, a).hp = 100;
  s.add_xp(a, 200000);
  CHECK(hs(s, a).level == 2 && hs(s, a).points == 2);
  const int32_t grow = R().defs[def_of("urs_hos_ragna")].grow_hp.floor_int();
  CHECK(hp(s, a).max == max1 + grow);
  CHECK(hp(s, a).hp == 100 + grow + hp(s, a).max / 4);
  s.add_xp(a, 10000000);   // a legmagasabb szint felett nem nő
  CHECK(hs(s, a).level == static_cast<int>(R().heroes.levels.size()));
}

TEST_CASE("hős: képességtanulás – az n. szint a 2n − 1. hősszinttől, a végső a 6.-tól; a szintváltozat erősebb") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto r = put(s, "urs_hos_ragna", 1, 20, 180);
  CHECK(!s.learn(r, "h_ragna_hosszutel"));   // végső: 6. szinttől
  CHECK(s.learn(r, "h_ragna_roham"));
  CHECK(!s.learn(r, "h_ragna_roham"));       // nincs pont
  s.add_xp(r, 200000);                        // 2. szint
  CHECK(!s.learn(r, "h_ragna_roham"));       // a 2. képességszint a 3. hősszinttől
  CHECK(s.learn(r, "h_ragna_esku"));
  s.add_xp(r, 300000);                        // 3. szint
  CHECK(s.learn(r, "h_ragna_roham"));
  const auto roham2 = static_cast<uint16_t>(R().ability_index("h_ragna_roham@2"));
  CHECK(s.has_ability(r, roham2) && R().abilities[roham2].damage == Fixed::from_int(80));
  // használat a 2. szinten: 80 sebzés
  const auto foe = put(s, "urs_berserker", 2, 45, 180);
  m.run(6);
  s.world().get<sim::Caster>(r)->energy = 100000;
  CHECK(s.use_ability({r}, "h_ragna_roham", {}, {}, foe));
  m.run(2);
  const auto& fh = hp(s, foe);
  CHECK(int64_t{fh.max - fh.hp} * 1000 + s.world().get<sim::Vital>(foe)->deficit == 80000);
  s.add_xp(r, 10000000);
  CHECK(s.learn(r, "h_ragna_hosszutel"));
}

TEST_CASE("hős: az elesett hős állapota megmarad; újraképzése idő × (0,6 + 0,12 × szint), ugyanazzal a szinttel") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  s.place_building(def_of("urs_harcosok_haza"), 1, 12, 9, true);
  const auto altar = s.place_building(def_of("urs_eskufa"), 1, 16, 9, true);
  s.place_building(def_of("urs_tanya"), 1, 12, 13, true);
  const auto r = put(s, "urs_hos_ragna", 1, 20, 180);
  s.add_xp(r, 500000);   // 3. szint
  CHECK(s.learn(r, "h_ragna_esku"));
  CHECK(s.give_item(r, item_of("vedogyuru")));
  kill_by(s, put(s, "urs_berserker", 2, 22, 180), r);
  CHECK(!s.world().alive(r));
  CHECK(s.player(1).fallen.size() == 1);
  m.run(2);
  for (int k = 0; k < sim::kResources; ++k) s.add_resources(1, k, 5000);
  std::string err;
  CHECK(s.train(1, def_of("urs_hos_ragna"), 0, &err));
  const auto& q = s.world().get<sim::Producer>(altar)->queue;
  CHECK(!q.empty());
  const int want = (Fixed::from_int(R().defs[def_of("urs_hos_ragna")].time_ticks) * (fx(3, 5) + fx(3, 25) * Fixed::from_int(3))).floor_int();
  std::printf("  újraélesztés: %d tick (az alap %d)\n", q.front().time, R().defs[def_of("urs_hos_ragna")].time_ticks);
  CHECK(q.front().time == want);
  m.run(want + 30);
  sim::EntityId back = sim::kNoEntity;
  s.world().pool<sim::HeroState>().each([&](sim::EntityId e, const sim::HeroState&) { back = e; });
  CHECK(back != sim::kNoEntity);
  if (back) {
    CHECK(hs(s, back).level == 3 && hs(s, back).learned.size() == 1 && hs(s, back).items[0] == item_of("vedogyuru"));
    CHECK(hp(s, back).hp == hp(s, back).max);
  }
  CHECK(s.player(1).fallen.empty());
}

TEST_CASE("tárgyak: 6 hely; főzet (teljes életerőnél nem), tóm (örökre), viselt (páncél, életerő aránya marad), eldobás, felvétel") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto h = put(s, "urs_hos_bjarka", 1, 30, 180);
  for (int i = 0; i < 6; ++i) CHECK(s.give_item(h, item_of("gyogyfozet")));
  CHECK(!s.give_item(h, item_of("gyogyfozet")));   // tele
  CHECK(!s.use_item(h, 0));                        // teljes életerőnél elutasítva
  hp(s, h).hp -= 300;
  const int32_t low = hp(s, h).hp;
  CHECK(s.use_item(h, 0));
  CHECK(hp(s, h).hp == low + 250 && hs(s, h).items[0] == sim::kNoItem);
  // tóm: +60 életerő örökre
  CHECK(s.give_item(h, item_of("eletertom")));
  const int32_t m0 = hp(s, h).max;
  CHECK(s.use_item(h, 0));
  CHECK(hp(s, h).max == m0 + 60 && hs(s, h).perm_hp == 60);
  // viselt: Hősi pajzs +4 páncél, +100 életerő – az arány marad
  const auto armor0 = s.stat(h, "Health.armor", Fixed::from_int(R().defs[def_of("urs_hos_bjarka")].armor));
  hp(s, h).hp = hp(s, h).max / 2;
  CHECK(s.drop_item(h, 1));   // hely kell
  CHECK(s.give_item(h, item_of("hosipajzs")));
  CHECK(s.stat(h, "Health.armor", Fixed::from_int(R().defs[def_of("urs_hos_bjarka")].armor)) == armor0 + Fixed::from_int(4));
  CHECK(std::abs(hp(s, h).hp * 2 - hp(s, h).max) <= 2);
  // a földre ejtett főzet felvehető
  sim::EntityId ground = sim::kNoEntity;
  s.world().pool<sim::GroundItem>().each([&](sim::EntityId e, const sim::GroundItem&) { ground = e; });
  CHECK(ground != sim::kNoEntity);
  CHECK(s.order_pickup(h, ground));
  m.run(60);
  CHECK(!s.world().alive(ground));
}

TEST_CASE("táborok: a Vadon táborai a kezdőhelyektől távol; a tábor együtt riad, pórázon tér vissza, kiirtva zsákmányt dob") {
  CampMatch cm("ECO-ALAP");
  sim::Sim& s = *cm.m.sim;
  std::vector<sim::EntityId> camps;
  s.world().pool<sim::Camp>().each([&](sim::EntityId e, const sim::Camp&) { camps.push_back(e); });
  std::printf("  ECO-ALAP: %zu tábor\n", camps.size());
  CHECK(camps.size() >= 2);   // a kis pályán a kezdőhelyektől 12 csempén túl csak ennyi fér el
  const auto& starts = cm.m.map->objects().starts;
  for (const auto c : camps) {
    const auto& p = *s.world().get<sim::Position>(c);
    for (const auto& [sx, sy] : starts) CHECK(core::length(p.x - Fixed::from_int(sx + 2), p.y - Fixed::from_int(sy + 2)) >= Fixed::from_int(11));
  }
  // egy tábor: a tagjai
  const auto camp = camps.front();
  std::vector<sim::EntityId> mem;
  s.world().pool<sim::CampMember>().each([&](sim::EntityId e, const sim::CampMember& cmb) {
    if (cmb.camp == camp) mem.push_back(e);
  });
  CHECK(!mem.empty());
  const auto& cp = *s.world().get<sim::Position>(camp);
  // a 8 csempére álló ellenséget nem támadják, az 4 csempére állót igen – és az egész tábor
  const auto bait = s.spawn_unit(def_of("urs_berserker"), 1, cp.x + Fixed::from_int(8), cp.y);
  s.world().get<sim::Combat>(bait)->order = sim::Combat::Hold;
  cm.m.run(30);
  for (const auto e : mem) CHECK(s.world().get<sim::Combat>(e)->order == sim::Combat::None);
  s.world().get<sim::Position>(bait)->x = cp.x + Fixed::from_int(4);
  cm.m.run(30);
  int attacking = 0;
  for (const auto e : mem) attacking += s.world().alive(e) && s.world().get<sim::Combat>(e)->order == sim::Combat::Attack ? 1 : 0;
  CHECK(attacking == static_cast<int>(mem.size()));
  // a csali elfut: a 7 csempén túl üldöző tagok visszatérnek és gyógyulnak
  s.world().get<sim::Combat>(bait)->order = sim::Combat::None;
  s.order_move(bait, cp.x + Fixed::from_int(20), cp.y);
  cm.m.run(300);
  bool returned = false;
  for (const auto e : mem) {
    if (!s.world().alive(e)) continue;
    const auto& cmb = *s.world().get<sim::CampMember>(e);
    const auto& p = *s.world().get<sim::Position>(e);
    returned = returned || core::length(p.x - cmb.hx, p.y - cmb.hy) < Fixed::from_int(2);
  }
  CHECK(returned);
  // kiirtva: zsákmány a tábor helyén
  for (const auto e : mem) {
    if (s.world().alive(e)) s.deal_damage(0, 1, sim::kNoDef, e, 100000000, false, true);
  }
  cm.m.run(2);
  int loot = 0;
  s.world().pool<sim::GroundItem>().each([&](sim::EntityId e, const sim::GroundItem&) {
    const auto& p = *s.world().get<sim::Position>(e);
    loot += core::length(p.x - cp.x, p.y - cp.y) < Fixed::from_int(2) ? 1 : 0;
  });
  CHECK(loot >= 1);
  CHECK(s.world().get<sim::Camp>(camp)->looted == 1);
}

TEST_CASE("hősök és táborok: determinisztikus, és mentésből folytatva ugyanaz") {
  auto setup = [](CampMatch& cm) {
    sim::Sim& s = *cm.m.sim;
    const auto r = s.spawn_unit(def_of("urs_hos_ragna"), 1, fx(200, 10), fx(200, 10));
    s.add_xp(r, 900000);
    s.learn(r, "h_ragna_esku");
    s.learn(r, "h_ragna_roham");
    s.give_item(r, item_of("csizma"));
    sim::EntityId camp = sim::kNoEntity;
    s.world().pool<sim::Camp>().each([&](sim::EntityId e, const sim::Camp&) { if (!camp) camp = e; });
    const auto& cp = *s.world().get<sim::Position>(camp);
    s.order_attack_move(r, cp.x, cp.y);
  };
  CampMatch a("ECO-ALAP"), b("ECO-ALAP");
  setup(a);
  setup(b);
  a.m.run(300);
  b.m.run(300);
  CHECK(a.m.sim->hash() == b.m.sim->hash());
  const auto saved = a.m.sim->save();
  a.m.run(600);
  Match c("ECO-ALAP", true, false);
  CHECK(c.sim->load(saved));
  c.sim->restore_obstacles(*c.map);
  c.run(600);
  CHECK(c.sim->hash() == a.m.sim->hash());
  std::printf("  hős- és tábor-állapothash (900 tick): 0x%016llx\n", static_cast<unsigned long long>(a.m.sim->hash()));
}
