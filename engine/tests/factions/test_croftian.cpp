// SPDX-License-Identifier: GPL-3.0-or-later
// Croftianok (M7, ADR-008): a népmodul viselkedési próbái (src/sim/factions/croftian.cpp) – a mélység terjedése és
// növekedése, építés csak mélységre, mozgás és gyógyulás a mélységen, a fák rothadása, fertőzés, mérgező tüske,
// átváltozás, determinizmus és mentés.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "check.hpp"
#include "match_util.hpp"

namespace totu::sim {
int croftian_creep_tiles(Sim& s, uint8_t owner);
bool croftian_on_creep(Sim& s, uint8_t owner, int x, int y);
}  // namespace totu::sim

using namespace totu;
using namespace totu::testing;
using core::Fixed;

namespace {

const sim::Rules& R() { return *content_once().rules; }
uint16_t def_of(const char* id) { return R().def(id); }
Fixed fx(int n, int d) { return Fixed::from_ratio(n, d); }

Match cro() { return Match("BUILD-ALAP", true, true, {"croftian", "ursan"}); }

sim::EntityId put(sim::Sim& s, const char* id, uint8_t owner, Fixed x, Fixed y) {
  const auto e = s.spawn_unit(def_of(id), owner, x, y);
  if (auto* c = s.world().get<sim::Combat>(e)) c->order = sim::Combat::Hold;
  return e;
}

int count(sim::Sim& s, const char* id, uint8_t owner) {
  int n = 0;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit& u) {
    const auto* h = s.world().get<sim::Health>(e);
    n += u.def == def_of(id) && h->owner == owner && h->hp > 0;
  });
  return n;
}

sim::EntityId hq_of(sim::Sim& s, uint8_t owner) {
  sim::EntityId hq = 0;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& b) {
    if (b.def == def_of("cro_kaptar") && s.world().get<sim::Health>(e)->owner == owner) hq = e;
  });
  return hq;
}

void no_workers(Match& m) {
  for (const auto w : m.workers(1)) m.sim->kill(w);
}

sim::FactionModule* module(sim::Sim& s) {
  for (auto& mod : s.modules_) {
    if (std::string(mod->name()) == "croftian") return mod.get();
  }
  return nullptr;
}

const content::Value* faction_key(const char* key) {
  const sim::FactionDef* f = R().faction("croftian");
  return f && f->raw ? f->raw->find("Faction")->find(key) : nullptr;
}

}  // namespace

TEST_CASE("Croftianok: a népmodul betöltve") {
  Match m("BUILD-ALAP");
  bool found = false;
  for (const auto& mod : m.sim->modules_) found = found || std::string(mod->name()) == "croftian";
  CHECK(found);
}

TEST_CASE("Croftianok mélység: a Kaptár köre (creep_radius + fél méret), csak építhető terepen; az ursan nem kap") {
  Match m = cro();
  sim::Sim& s = *m.sim;
  CHECK(m.ok);
  const auto hq = hq_of(s, 1);
  CHECK(hq != 0);
  const auto& b = *s.world().get<sim::Building>(hq);
  const int cx = b.tx + b.w / 2, cy = b.ty + b.h / 2;
  CHECK(sim::croftian_on_creep(s, 1, cx, cy));
  CHECK(sim::croftian_on_creep(s, 1, cx + 11, cy));      // 10 + 2 sugarú körben
  CHECK(!sim::croftian_on_creep(s, 1, cx + 13, cy));     // azon kívül
  CHECK(!sim::croftian_on_creep(s, 1, 15, 4));           // tó: nem építhető, nincs mélység
  CHECK(!sim::croftian_on_creep(s, 2, cx, cy));          // más játékosé nem
  CHECK(sim::croftian_creep_tiles(s, 1) > 300);
  CHECK(sim::croftian_creep_tiles(s, 2) == 0);           // az ursannak nincs mélysége
}

TEST_CASE("Croftianok mélység: kész épületnél másodpercenként +0,7 a teljes sugárig; pusztuláskor eltűnik") {
  Match m = cro();
  sim::Sim& s = *m.sim;
  const auto e = s.place_building(def_of("cro_tapgomba"), 1, 26, 15, false);
  CHECK(e != 0);
  m.run(1);
  CHECK(sim::croftian_on_creep(s, 1, 27, 16));
  CHECK(!sim::croftian_on_creep(s, 1, 27, 12));          // épülőnél csak 1 + fél méret
  s.world().get<sim::Building>(e)->complete = 1;
  const int before = sim::croftian_creep_tiles(s, 1);
  m.run(30 - static_cast<int>(s.tick() % 30) + 1);
  const content::Value* raw = R().defs[def_of("cro_tapgomba")].raw->find("Mechanics")->find("creep_radius");
  const int full = static_cast<int>(raw->as_int().value_or(0));
  CHECK(full == 8);
  m.run(30 * 10);
  CHECK(sim::croftian_creep_tiles(s, 1) > before);
  CHECK(sim::croftian_on_creep(s, 1, 27, 16 - full));    // a teljes sugár (8 + 1)
  s.kill(e);
  m.run(2);
  CHECK(!sim::croftian_on_creep(s, 1, 27, 16));
}

TEST_CASE("Croftianok építés: csak saját mélységre; a no_creep épület (Kaptár) bárhová") {
  Match m = cro();
  sim::Sim& s = *m.sim;
  for (int r = 0; r < sim::kResources; ++r) s.add_resources(1, r, 20000);
  std::string err;
  CHECK(!s.can_place(1, def_of("cro_tapgomba"), 26, 15, &err));
  CHECK(err == "csak saját mélységre építhető");
  err.clear();
  const bool ok = s.can_place(1, def_of("cro_tapgomba"), 10, 15, &err);   // a Kaptár mellett, mélységen
  CHECK(ok || err != "csak saját mélységre építhető");
  err.clear();
  s.can_place(1, def_of("cro_kaptar"), 24, 14, &err);
  CHECK(err != "csak saját mélységre építhető");
  err.clear();
  s.can_place(2, def_of("urs_hosszuhaz"), 26, 15, &err);   // az ursant nem köti
  CHECK(err != "csak saját mélységre építhető");
}

TEST_CASE("Croftianok mélység: földi egység ×1,3 gyors, gyógyulása ×3; a repülő és az idegen nem kap") {
  Match m = cro();
  sim::Sim& s = *m.sim;
  const auto on = put(s, "cro_karmos", 1, fx(105, 10), fx(135, 10));
  const auto off = put(s, "cro_karmos", 1, fx(285, 10), fx(185, 10));
  const auto fly = put(s, "cro_szarnyas_rem", 1, fx(105, 10), fx(135, 10));
  const auto foe = put(s, "urs_berserker", 2, fx(105, 10), fx(125, 10));
  CHECK(s.mod_speed(on) == fx(13, 10));
  CHECK(s.mod_speed(off) == Fixed::from_int(1));
  CHECK(s.mod_speed(fly) == Fixed::from_int(1));
  CHECK(s.mod_speed(foe) == Fixed::from_int(1));
  const Fixed base = R().faction("croftian")->regen + R().defs[def_of("cro_karmos")].regen;
  CHECK(s.mod_regen(on) == base * Fixed::from_int(2));   // (faji + saját) × 3 = alap + 2 × alap
  CHECK(s.mod_regen(off) == Fixed{});
  CHECK(s.mod_regen(foe) == Fixed{});
  // a szorzók a game.json-ból; a Mélységi gyógyulás kutatás (`Rules.creep_regen_mult` 2) a gyógyulást × 2-vel szorozza
  CHECK(R().game_raw->find("creep_speed_mult")->as_fixed() == fx(13, 10));
  CHECK(R().game_raw->find("creep_regen_mult")->as_fixed() == Fixed::from_int(3));
  s.player_mut(1).researched.push_back("cro_melysegi_gyogyulas");
  CHECK(s.mod_regen(on) == base * Fixed::from_int(5));   // × 3 × 2 = alap + 5 × alap
  s.player_mut(1).researched.clear();
  // mérve: a mélységen gyorsabban ér célba
  const auto a = put(s, "cro_karmos", 1, fx(95, 10), fx(165, 10));
  const auto c = put(s, "cro_karmos", 1, fx(235, 10), fx(165, 10));
  s.world().get<sim::Combat>(a)->order = sim::Combat::None;
  s.world().get<sim::Combat>(c)->order = sim::Combat::None;
  s.order_move(a, fx(135, 10), fx(165, 10));
  s.order_move(c, fx(275, 10), fx(165, 10));
  m.run(30);
  const Fixed da = s.world().get<sim::Position>(a)->x - fx(95, 10);
  const Fixed dc = s.world().get<sim::Position>(c)->x - fx(235, 10);
  CHECK(da > dc);
}

TEST_CASE("Croftianok rothadás: a mélységen álló fa a Kaptár 9 csempés körében 0,06 × 2,5, azon kívül 0,06 fa / s") {
  Match m = cro();
  sim::Sim& s = *m.sim;
  no_workers(m);
  m.run(30 - static_cast<int>(s.tick() % 30));
  std::vector<std::pair<sim::EntityId, int32_t>> trees;
  s.world().pool<sim::ResourceNode>().each([&](sim::EntityId e, const sim::ResourceNode& n) {
    if (n.kind == sim::kWood) trees.push_back({e, n.amount});
  });
  CHECK(trees.size() == 15u);
  const int64_t wood0 = s.player(1).res[sim::kWood];
  m.run(300);   // 10 rothadás
  const auto& b = *s.world().get<sim::Building>(hq_of(s, 1));
  const Fixed hx = Fixed::from_int(b.tx) + fx(b.w, 2), hy = Fixed::from_int(b.ty) + fx(b.h, 2);
  const Fixed rate = *faction_key("creep_rot")->find("rate")->as_fixed();
  const Fixed mult = *faction_key("creep_rot")->find("hq_mult")->as_fixed();
  const Fixed rad = *faction_key("creep_rot")->find("hq_radius")->as_fixed();
  int64_t used = 0, expect = 0;
  int near = 0, far = 0;
  for (const auto& [e, a0] : trees) {
    const auto& n = *s.world().get<sim::ResourceNode>(e);
    const Fixed px = Fixed::from_int(n.tx) + fx(1, 2), py = Fixed::from_int(n.ty) + fx(1, 2);
    const bool in = core::length(px - hx, py - hy) <= rad;
    const int64_t per = ((in ? rate * mult : rate).raw() * int64_t{1000}) >> 16;
    expect += per * 10;
    used += int64_t{a0 - n.amount} * 1000;   // a tört a modul komponensében marad
    (in ? near : far)++;
  }
  CHECK(near > 0);
  CHECK(far > 0);
  const int64_t got = s.player(1).res[sim::kWood] - wood0;
  CHECK(std::llabs(got - expect) < 1000);   // egészként jóváírva, a tört megmarad
  CHECK(used <= expect);
  CHECK(used >= expect - 15 * 1000);
}

TEST_CASE("Croftianok fertőzés: a croftian ölte szerves ellenség a mélységen 30%-kal 6 s múlva Karmosként kel ki") {
  Match m = cro();
  sim::Sim& s = *m.sim;
  sim::FactionModule* mod = module(s);
  CHECK(mod != nullptr);
  const int k0 = count(s, "cro_karmos", 1);
  constexpr int kN = 60;
  for (int i = 0; i < kN; ++i) {
    const auto v = put(s, "urs_berserker", 2, fx(80 + (i % 10) * 4, 10), fx(140 + (i / 10) * 4, 10));
    mod->on_death(s, v, 0, 1);
    s.kill(v);
  }
  for (int i = 0; i < 20; ++i) {   // mélységen kívül: soha
    const auto v = put(s, "urs_berserker", 2, fx(285, 10), fx(185, 10));
    mod->on_death(s, v, 0, 1);
    s.kill(v);
  }
  for (int i = 0; i < 20; ++i) {   // nem croftian ölte: soha
    const auto v = put(s, "urs_berserker", 2, fx(90, 10), fx(140, 10));
    mod->on_death(s, v, 0, 2);
    s.kill(v);
  }
  m.run(1);
  const int delay = sim::seconds_to_ticks(*faction_key("infest")->find("delay")->as_fixed());
  m.run(delay - 2);
  CHECK(count(s, "cro_karmos", 1) == k0);   // még nem kelt ki
  m.run(3);
  const int hatched = count(s, "cro_karmos", 1) - k0;
  CHECK(hatched >= kN * 15 / 100);
  CHECK(hatched <= kN * 45 / 100);
}

TEST_CASE("Croftianok mérgező tüske: nem akadály; az ellenséget lassítja (×0,6) és mérgezi (4 / s), a croftiant nem") {
  Match m = cro();
  sim::Sim& s = *m.sim;
  const auto sp = s.place_building(def_of("cro_merges_tuskek"), 1, 11, 16, true);
  CHECK(sp != 0);
  const auto foe = put(s, "urs_berserker", 2, fx(115, 10), fx(165, 10));
  const auto own = put(s, "cro_karmos", 1, fx(115, 10), fx(165, 10));
  const auto walker = put(s, "cro_karmos", 1, fx(95, 10), fx(165, 10));
  m.run(16);
  const auto& af = *s.world().get<sim::Afflict>(foe);
  CHECK(af.slow_until > s.tick());
  CHECK(af.slow_mult == fx(3, 5));
  CHECK(af.poison_dps == 4000);
  CHECK(af.poison_until > s.tick());
  const auto& ao = *s.world().get<sim::Afflict>(own);
  CHECK(ao.poison_dps == 0);
  CHECK(ao.slow_until <= s.tick());
  // a tüske csempéje járható: a karmos átmegy rajta
  s.kill(foe);
  s.kill(own);
  m.run(2);
  s.world().get<sim::Combat>(walker)->order = sim::Combat::None;   // a helytartó nem mozdul
  s.order_move(walker, fx(135, 10), fx(165, 10));
  m.run(150);
  CHECK(s.world().get<sim::Position>(walker)->x > fx(125, 10));
}

TEST_CASE("Croftianok mérgező tüske: a nem tűz/méreg sebzés × 0,2, a támadót megmérgezi; a croftian támadót nem") {
  Match m = cro();
  sim::Sim& s = *m.sim;
  const auto sp = s.place_building(def_of("cro_merges_tuskek"), 1, 11, 16, true);
  const auto foe = put(s, "urs_berserker", 2, fx(135, 10), fx(165, 10));
  const int hp0 = s.world().get<sim::Health>(sp)->hp;
  CHECK(s.mod_damage_taken(foe, sp, false, true) == fx(1, 5));
  s.deal_damage(foe, 2, def_of("urs_berserker"), sp, 50000, false, true);
  std::printf("  tüske: %d -> %d\n", hp0, s.world().get<sim::Health>(sp)->hp);
  CHECK(std::abs(hp0 - s.world().get<sim::Health>(sp)->hp - 10) <= 1);   // 50 × 0,2 (a tört kerekítése)
  CHECK(s.world().get<sim::Afflict>(foe)->poison_dps == 4000);
  CHECK(s.world().get<sim::Afflict>(foe)->poison_until > s.tick());
  // a varázssebzés is × 0,2; a tüske-immunis (croftian) támadó nem mérgeződik
  const auto own = put(s, "cro_karmos", 1, fx(95, 10), fx(165, 10));
  const int hp1 = s.world().get<sim::Health>(sp)->hp;
  s.deal_damage(own, 1, def_of("cro_karmos"), sp, 10000, false, true);
  CHECK(hp1 - s.world().get<sim::Health>(sp)->hp == 2);
  CHECK(s.world().get<sim::Afflict>(own)->poison_dps == 0);
  // más épület sebzése változatlan
  const auto hq = hq_of(s, 1);
  const int hq0 = s.world().get<sim::Health>(hq)->hp;
  s.deal_damage(0, 2, sim::kNoDef, hq, 10000, false, true);
  CHECK(hq0 - s.world().get<sim::Health>(hq)->hp == 10);
}

TEST_CASE("Croftianok idézés: summons_permanent – az idézés nem jár le; hősönként legfeljebb 4 (a legrégebbi pusztul)") {
  CHECK(faction_key("summons_permanent") && faction_key("summons_permanent")->as_bool());
  Match m = cro();
  sim::Sim& s = *m.sim;
  const auto hero = put(s, "cro_hos_vor", 1, fx(105, 10), fx(165, 10));
  CHECK(s.world().get<sim::HeroState>(hero) != nullptr);
  std::vector<sim::EntityId> sum;
  for (int i = 0; i < 6; ++i) {
    const auto k = put(s, "cro_karmos", 1, fx(105 + (i % 3) * 5, 10), fx(175 + (i / 3) * 5, 10));
    s.world().add(k, sim::Lifetime{s.tick() + 30});
    sum.push_back(k);
    m.run(1);
  }
  m.run(120);
  auto live = [&](sim::EntityId e) {
    const auto* h = s.world().alive(e) ? s.world().get<sim::Health>(e) : nullptr;
    return h && h->hp > 0;
  };
  int alive = 0;
  for (const auto k : sum) alive += live(k);
  CHECK(alive == 4);
  CHECK(!live(sum[0]));   // a két legrégebbi pusztult
  CHECK(!live(sum[1]));
  CHECK(s.world().get<sim::Lifetime>(sum[5]) != nullptr);   // idézettnek számít (nem fertőz, nem ad tapasztalatot)
  // hős nélkül idézett: nem jár le, korlát nélkül
  const auto lone = put(s, "cro_karmos", 1, fx(285, 10), fx(185, 10));
  s.world().add(lone, sim::Lifetime{s.tick() + 30});
  m.run(120);
  CHECK(live(lone));
  // az idegen nép idézése lejár
  const auto foreign = put(s, "urs_berserker", 2, fx(285, 10), fx(105, 10));
  s.world().add(foreign, sim::Lifetime{s.tick() + 30});
  m.run(60);
  CHECK(!live(foreign));
}

TEST_CASE("Croftianok varázserő: spell_resource hp – a képesség energiája életerőből fizet (legalább 1 marad)") {
  CHECK(faction_key("spell_resource")->text() == "hp");
  Match m = cro();
  sim::Sim& s = *m.sim;
  const auto hero = put(s, "cro_hos_keth", 1, fx(105, 10), fx(165, 10));
  m.run(1);
  auto& h = *s.world().get<sim::Health>(hero);
  auto& c = *s.world().get<sim::Caster>(hero);
  CHECK(c.energy == (h.hp - 1) * 1000);   // a felhasználható energia az életerő − 1
  const int hp0 = h.hp;
  c.energy -= 75000;   // egy 75 energiás képesség elköltése
  m.run(1);
  CHECK(hp0 - s.world().get<sim::Health>(hero)->hp == 75);
  CHECK(s.world().get<sim::Caster>(hero)->energy == (s.world().get<sim::Health>(hero)->hp - 1) * 1000);
  // túlköltés: legalább 1 életerő marad
  s.world().get<sim::Caster>(hero)->energy = 0;
  m.run(1);
  // (a közös energiatöltés a modul után fut: a tickenkénti töltés a következő elszámolásban beszámít)
  CHECK(s.world().get<sim::Health>(hero)->hp <= 2);
  CHECK(s.world().get<sim::Caster>(hero)->energy < 2000);
}

TEST_CASE("Croftianok átváltozás: a Karmos a páncél irányon a költségért Gubóvá, 18 s múlva Páncélbogárrá lesz") {
  Match m = cro();
  sim::Sim& s = *m.sim;
  for (int r = 0; r < sim::kResources; ++r) s.add_resources(1, r, 2000);
  const auto k = put(s, "cro_karmos", 1, fx(105, 10), fx(165, 10));
  CHECK(!s.use_ability({k}, "atvaltozas", Fixed{}, Fixed{}, 0));   // irány nélkül nem
  s.player_mut(1).path = "pancel";
  const auto res0 = s.player(1).res;
  CHECK(s.use_ability({k}, "atvaltozas", Fixed{}, Fixed{}, 0));
  const content::Value* cost = R().abilities[static_cast<size_t>(R().ability_index("atvaltozas"))].spec->find("cost");
  for (int r = 0; r < sim::kResources; ++r) {
    const content::Value* c = cost->find(sim::resource_id(r));
    const int64_t want = c ? *c->as_int() * 1000 : 0;
    CHECK(res0[static_cast<size_t>(r)] - s.player(1).res[static_cast<size_t>(r)] == want);
  }
  m.run(2);
  CHECK(count(s, "cro_guboj", 1) == 1);
  CHECK(count(s, "cro_pancelbogar", 1) == 0);
  m.run(18 * 30);
  CHECK(count(s, "cro_guboj", 1) == 0);
  CHECK(count(s, "cro_pancelbogar", 1) == 1);
}

TEST_CASE("Croftianok: determinizmus és mentés (mélység, rothadás, fertőzés, gubó)") {
  auto setup = [](Match& m) {
    sim::Sim& s = *m.sim;
    for (int r = 0; r < sim::kResources; ++r) s.add_resources(1, r, 2000);
    s.place_building(def_of("cro_tapgomba"), 1, 10, 15, false);
    s.place_building(def_of("cro_merges_tuskek"), 1, 11, 10, true);
    put(s, "urs_berserker", 2, fx(115, 10), fx(105, 10));
    s.player_mut(1).path = "pancel";
    const auto k = put(s, "cro_karmos", 1, fx(105, 10), fx(165, 10));
    s.use_ability({k}, "atvaltozas", Fixed{}, Fixed{}, 0);
    sim::FactionModule* mod = module(s);
    for (int i = 0; i < 10; ++i) {
      const auto v = put(s, "urs_berserker", 2, fx(80 + i * 4, 10), fx(140, 10));
      mod->on_death(s, v, 0, 1);
      s.kill(v);
    }
  };
  Match a = cro(), b = cro();
  setup(a);
  setup(b);
  a.run(100);
  b.run(100);
  CHECK(a.sim->hash() == b.sim->hash());
  const auto saved = a.sim->save();
  a.run(600);
  Match c("BUILD-ALAP", true, false, {"croftian", "ursan"});
  CHECK(c.sim->load(saved));
  c.sim->restore_obstacles(*c.map);
  c.run(600);
  CHECK(c.sim->hash() == a.sim->hash());
  CHECK(sim::croftian_creep_tiles(*c.sim, 1) == sim::croftian_creep_tiles(*a.sim, 1));
}
