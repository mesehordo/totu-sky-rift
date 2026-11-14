// SPDX-License-Identifier: GPL-3.0-or-later
// Lefedettség (leltár-kapu): a motor már megvalósított, de eddig próba nélküli viselkedései – földi/légi célzás,
// főépület-képesség életerőből, ikrarakás felső korlátja, kutatási statok (lőtáv, látótáv, sebesség, pajzs, energia,
// teherbírás), Wullof zsákmány aranyban, Ooni Harci dobok, Huk'ucha Tavak útja, energiafőzet és tudás könyve.
#include <algorithm>
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
uint16_t item_of(const char* id) { return static_cast<uint16_t>(R().item_index(id)); }
Fixed fx(int n, int d) { return Fixed::from_ratio(n, d); }

sim::EntityId put(sim::Sim& s, const char* id, uint8_t owner, int x10, int y10) {
  const auto e = s.spawn_unit(def_of(id), owner, fx(x10, 10), fx(y10, 10));
  if (auto* c = s.world().get<sim::Combat>(e)) c->order = sim::Combat::Hold;
  return e;
}

// a kutatás kész, ahogy a gyártósor befejezi (állandó statváltozás + szabálymódosítók)
void learn(sim::Sim& s, uint8_t owner, const char* id) {
  auto& r = s.player_mut(owner).researched;
  r.insert(std::lower_bound(r.begin(), r.end(), std::string(id)), id);
  const sim::ResearchDef* rd = R().research(id);
  CHECK(rd != nullptr);
  if (rd) s.grant_modifiers(owner, rd->modifiers);
  s.rebuild_rules(owner);
}


// életerő + pajzs, ezredben (a hiány levonva)
int64_t life(sim::Sim& s, sim::EntityId e) {
  if (!s.world().alive(e)) return 0;
  const auto* v = s.world().get<sim::Vital>(e);
  return int64_t{s.world().get<sim::Health>(e)->hp} * 1000 - (v ? v->deficit - v->shield : 0);
}

int visible_tiles(sim::Sim& s, uint8_t owner) {
  int n = 0;
  for (const auto v : s.player(owner).visible) n += v ? 1 : 0;
  return n;
}

int count(sim::Sim& s, const char* id, uint8_t owner) {
  int n = 0;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit& u) {
    const auto* h = s.world().get<sim::Health>(e);
    n += u.def == def_of(id) && h->owner == owner && h->hp > 0 ? 1 : 0;
  });
  return n;
}

}  // namespace

// --- földi/légi célzás (attack.targets) --------------------------------------------------------------------------------

TEST_CASE("célzás: csak földre lövő egység nem lő repülőt, a légvédelmi igen; földi egységre és épületre mindkettő") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto bers = put(s, "urs_berserker", 1, 120, 170);   // targets: ground
  const auto archer = put(s, "elf_jade_ijasz", 1, 120, 175);   // targets: ground, air
  const auto bird = put(s, "elf_quetzal", 2, 125, 172);       // repülő
  const auto foot = put(s, "urs_dardaor", 2, 126, 168);
  const auto house = s.place_building(def_of("urs_hosszuhaz"), 2, 14, 18, true);
  CHECK(R().defs[def_of("elf_quetzal")].flying);
  CHECK(!s.can_hit(bers, bird));
  CHECK(s.can_hit(archer, bird));
  CHECK(s.can_hit(bers, foot) && s.can_hit(archer, foot));
  CHECK(s.can_hit(bers, house) && s.can_hit(archer, house));
  CHECK(!s.order_attack(bers, bird));   // a parancsot sem fogadja el
  CHECK(s.order_attack(archer, bird));
}

TEST_CASE("célzás: a légvédelmi torony lövi a repülőt; csak földre lövő torony nem (egységre és épületre is)") {
  const uint16_t td = def_of("urs_ortorony");
  CHECK(R().defs[td].attack.air && R().defs[td].attack.ground);
  {
    Match m("BUILD-ALAP");
    sim::Sim& s = *m.sim;
    const auto tower = s.place_building(td, 1, 2, 17, true);
    const auto bird = put(s, "elf_quetzal", 2, 60, 175);
    CHECK(s.can_hit(tower, bird));
    const int64_t bird0 = life(s, bird);
    m.run(120);
    std::printf("  légvédelmi torony: a repülő %lld → %lld ezred (pajzzsal)\n", static_cast<long long>(bird0),
                static_cast<long long>(life(s, bird)));
    CHECK(life(s, bird) < bird0);   // a torony valóban lő rá
  }
  // ugyanez a torony csak földi célzással: a repülőt nem, a földi egységet és az épületet igen
  auto& at = content_once().rules->defs[td].attack;
  at.air = false;
  int64_t bird0 = 0, bird1 = 0, foot0 = 0, foot1 = 0, house0 = 0, house1 = 0;
  bool air_hit = true, ground_hit = false, house_hit = false;
  {
    Match m("BUILD-ALAP");
    sim::Sim& s = *m.sim;
    const auto tower = s.place_building(td, 1, 2, 17, true);
    const auto bird = put(s, "elf_quetzal", 2, 60, 175);
    air_hit = s.can_hit(tower, bird);
    bird0 = life(s, bird);
    m.run(120);
    bird1 = life(s, bird);
    s.kill(bird);
    const auto foot = put(s, "urs_berserker", 2, 70, 178);
    ground_hit = s.can_hit(tower, foot);
    foot0 = life(s, foot);
    m.run(120);
    foot1 = life(s, foot);
    s.kill(foot);
    const auto house = s.place_building(def_of("urs_hosszuhaz"), 2, 6, 17, true);
    house_hit = s.can_hit(tower, house);
    house0 = life(s, house);
    m.run(120);
    house1 = life(s, house);
  }
  at.air = true;
  std::printf("  csak földi torony: a repülő %lld → %lld, a földi %lld → %lld, az épület %lld → %lld\n",
              static_cast<long long>(bird0), static_cast<long long>(bird1), static_cast<long long>(foot0),
              static_cast<long long>(foot1), static_cast<long long>(house0), static_cast<long long>(house1));
  CHECK(!air_hit && ground_hit && house_hit);
  CHECK(bird1 == bird0);
  CHECK(foot1 < foot0);
  CHECK(house1 < house0);
}

// --- főépület-képesség életerőből (ability:hq_hp) ----------------------------------------------------------------------

TEST_CASE("főépület-képesség: a Mélységi kitörés a főépület életerejéből fizet (250); túl sebesült főépületnél nem") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto hive = s.place_building(def_of("cro_kaptar"), 1, 12, 15, true);
  m.run(6);
  auto& h = *s.world().get<sim::Health>(hive);
  const int before = h.hp;
  CHECK(R().abilities[static_cast<size_t>(R().ability_index("gp_melysegi_kitores"))].hq_hp == 250);
  CHECK(s.use_power(1, hive, "gp_melysegi_kitores", fx(140, 10), fx(170, 10)));
  CHECK(h.hp == before - 250);
  // egy másik főépület 300 életerővel (≤ 250 + 50): elutasítva, az életerő marad
  const auto hive2 = s.place_building(def_of("cro_kaptar"), 1, 20, 12, true);
  m.run(6);
  auto& h2 = *s.world().get<sim::Health>(hive2);
  h2.hp = 300;
  CHECK(!s.use_power(1, hive2, "gp_melysegi_kitores", fx(140, 10), fx(170, 10)));
  CHECK(h2.hp == 300);
  h2.hp = 301;
  CHECK(s.use_power(1, hive2, "gp_melysegi_kitores", fx(140, 10), fx(170, 10)));
  CHECK(h2.hp == 51);
}

// --- ikrarakás felső korlátja (ability:max) ----------------------------------------------------------------------------

TEST_CASE("ikrarakás: egyszerre legfeljebb 12 ikra; ha egy elfogy, újra rakható") {
  Match m("BUILD-ALAP", true, true, {"hukucha", "ursan"});
  sim::Sim& s = *m.sim;
  const auto layer = put(s, "huk_ikrarako", 1, 95, 165);
  s.world().get<sim::Caster>(layer)->energy = 100000;
  std::vector<sim::EntityId> eggs;
  for (int i = 0; i < 12; ++i) eggs.push_back(put(s, "huk_ikra", 1, 150 + i * 5, 120));
  CHECK(count(s, "huk_ikra", 1) == 12);
  CHECK(!s.use_ability({layer}, "ikrarakas", fx(95, 10), fx(155, 10), 0));   // tele
  CHECK(count(s, "huk_ikra", 1) == 12);
  s.kill(eggs.front());
  m.run(1);
  CHECK(s.use_ability({layer}, "ikrarakas", fx(95, 10), fx(155, 10), 0));
  CHECK(count(s, "huk_ikra", 1) == 12);
}

// --- kutatási statok (effect_stat) -------------------------------------------------------------------------------------

TEST_CASE("kutatási stat: lőtáv és látótáv (Csillagfény: +1 lőtáv a lövészeknek, +1 látótáv)") {
  Match m("BUILD-ALAP", true, false);
  sim::Sim& s = *m.sim;
  sim::MatchSetup setup;   // napszak nélkül: a látótáv szorzója 1
  setup.factions = {"elves", "ursan"};
  setup.daytime = sim::MatchSetup::Daytime::Off;
  std::string err;
  CHECK(s.setup_match(*m.map, setup, &err));
  const auto archer = put(s, "elf_jade_ijasz", 1, 175, 105);   // síkon, a főépület látókörén kívül
  const auto foe = put(s, "urs_berserker", 2, 300, 50);
  m.run(6);   // a látótér 0,2 s-onként frissül
  const Fixed r0 = s.attack_range(archer, foe);
  const int vis0 = visible_tiles(s, 1);
  learn(s, 1, "elf_csillagfeny");
  m.run(6);
  std::printf("  lőtáv %.2f → %.2f, látott csempe %d → %d\n", r0.to_double(), s.attack_range(archer, foe).to_double(), vis0,
              visible_tiles(s, 1));
  CHECK(s.attack_range(archer, foe) == r0 + Fixed::from_int(1));
  CHECK(visible_tiles(s, 1) > vis0);
  const int sight = R().defs[def_of("elf_jade_ijasz")].sight;
  CHECK(s.stat(archer, "Vision.sight", Fixed::from_int(sight)) == Fixed::from_int(sight + 1));
}

TEST_CASE("kutatási stat: sebesség (Inas lábak: +30% a raj-egységeknek)") {
  auto travel = [](bool researched) {
    Match m("BUILD-ALAP", true, true, {"croftian", "ursan"});
    sim::Sim& s = *m.sim;
    if (researched) learn(s, 1, "cro_inas_labak");
    const auto u = s.spawn_unit(def_of("cro_karmos"), 1, fx(175, 10), fx(105, 10));
    CHECK(s.order_move(u, fx(305, 10), fx(105, 10)));
    m.run(5);
    const Fixed x0 = s.world().get<sim::Position>(u)->x;
    m.run(30);
    return s.world().get<sim::Position>(u)->x - x0;
  };
  const Fixed base = travel(false), fast = travel(true);
  std::printf("  1 s alatt: %.2f → %.2f csempe\n", base.to_double(), fast.to_double());
  CHECK(base > Fixed::from_int(1));
  CHECK(fast > base * fx(12, 10) && fast < base * fx(14, 10));
}

TEST_CASE("kutatási stat: pajzs (Napégi pajzs: +30) – az élő egységen és az újonnan születőn is") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto hero = put(s, "urs_hos_hjalmar", 1, 200, 180);   // 40 pajzs
  CHECK(s.world().get<sim::Vital>(hero)->shield == 40000);
  learn(s, 1, "elf_napegi_pajzs");
  CHECK(s.world().get<sim::Vital>(hero)->shield == 70000);
  const auto fresh = put(s, "urs_hos_hjalmar", 1, 210, 180);
  CHECK(s.world().get<sim::Vital>(fresh)->shield == 70000);
}

TEST_CASE("kutatási stat: energia (Arcanor csillagai: +25 energia a varázslóknak) – a töltődés plafonja feljebb") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto mage = put(s, "imp_szellemidezo", 1, 200, 180);   // 200 energia
  auto& c = *s.world().get<sim::Caster>(mage);
  c.energy = 200000;
  m.run(60);
  CHECK(c.energy == 200000);   // tele
  learn(s, 1, "imp_arcanor_csillagai");
  m.run(60);
  std::printf("  energia a kutatás után 2 s-mal: %d ezred\n", c.energy);
  CHECK(c.energy > 200000);
  c.energy = 224900;
  m.run(60);
  CHECK(c.energy == 225000);
}

TEST_CASE("kutatási stat: teherbírás (Tartály: +50% az Aratóknak)") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto& arato = R().defs[def_of("imp_arato")];
  const auto& other = R().defs[def_of("urs_telepes")];
  const int32_t a0 = s.carry_cap(arato, sim::kGold, 1), o0 = s.carry_cap(other, sim::kGold, 1);
  learn(s, 1, "imp_tartaly");
  std::printf("  Arató aranyteher: %d → %d\n", a0, s.carry_cap(arato, sim::kGold, 1));
  CHECK(s.carry_cap(arato, sim::kGold, 1) == (a0 * 3) / 2);
  CHECK(s.carry_cap(other, sim::kGold, 1) == o0);
}

TEST_CASE("irány: az `energy` hatás (Energy.max) átfordul, és a választott irány a varázslók energiáját növeli") {
  // a Légió irány hatását egy próbaréteg az energiára cseréli (+25 a varázslóknak)
  content::ContentSet set = content_once().set;
  const auto patch = content::parse(
      R"({"faction_imperium": {"Paths": {"legio": {"effects": [{"stat": "energy", "tags": ["caster"], "add": 25}]}}}})");
  CHECK(patch.has_value());
  set["faction"].add_file("proba", "proba/imperium.json", *patch);
  const auto game = content::parse(read_file(std::filesystem::path(TOTU_CONTENT_DIR) / "base" / "game.json"));
  std::vector<std::string> errors;
  std::optional<sim::Rules> rules = sim::Rules::load(*game, set, errors);
  CHECK(rules.has_value());
  if (!rules) return;
  const sim::FactionDef* f = rules->faction("imperium");
  const sim::PathDef* pd = f ? f->path("legio") : nullptr;
  CHECK(pd && pd->effects.size() == 1 && pd->effects[0].path == "Energy.max" &&
        pd->effects[0].value == Fixed::from_int(25));
  Match m("BUILD-ALAP", true, false);   // csak a pálya és a navigáció; a szimuláció a foltozott szabályokkal
  sim::Sim s(20261113);
  s.attach_navigation(m.pf.get());
  s.attach_rules(&*rules);
  sim::MatchSetup setup;
  setup.factions = {"imperium", "ursan"};
  std::string err;
  CHECK(s.setup_match(*m.map, setup, &err));
  const auto mage = s.spawn_unit(rules->def("imp_szellemidezo"), 1, fx(200, 10), fx(180, 10));
  const Fixed base = Fixed::from_int(rules->defs[rules->def("imp_szellemidezo")].energy_max);
  auto& c = *s.world().get<sim::Caster>(mage);
  c.energy = base.floor_int() * 1000;
  for (int i = 0; i < 30; ++i) s.step();
  CHECK(c.energy == base.floor_int() * 1000);   // tele
  s.player_mut(1).ascended = true;
  CHECK(s.choose_path(1, "legio"));
  CHECK(s.stat(mage, "Energy.max", base) == base + Fixed::from_int(25));
  c.energy = (base.floor_int() + 25) * 1000 - 100;
  for (int i = 0; i < 60; ++i) s.step();
  std::printf("  energia az irány után: %d ezred (plafon %d)\n", c.energy, (base.floor_int() + 25) * 1000);
  CHECK(c.energy == (base.floor_int() + 25) * 1000);   // a töltődés az új plafonig
}

// --- népi szabályok ----------------------------------------------------------------------------------------------------

TEST_CASE("Wullof Zsákmányszerzés: a vadászzsákmány fele aranyban is (`bounty_gold` 0,5)") {
  auto loot = [](bool researched) {
    Match m("BUILD-ALAP", true, true, {"wullof", "ursan"});
    sim::Sim& s = *m.sim;
    if (researched) learn(s, 1, "wul_zsakmanyszerzes");
    const auto w = put(s, "wul_landzsas", 1, 50, 180);
    const auto foe = put(s, "urs_berserker", 2, 55, 180);
    const int32_t gold0 = s.player(1).resource(sim::kGold);
    s.deal_damage(w, 1, def_of("wul_landzsas"), foe, 100000000, false);
    m.run(1);
    return s.player(1).resource(sim::kGold) - gold0;
  };
  int64_t cost = 0;
  for (const int32_t c : R().defs[def_of("urs_berserker")].cost) cost += c;
  const int32_t without = loot(false), with = loot(true);
  std::printf("  arany az ölésért: %d → %d (a berserker költsége %lld)\n", without, with, static_cast<long long>(cost));
  CHECK(without == 0);
  CHECK(with == static_cast<int32_t>(cost / 20));   // költség × 0,2 × 0,5 × 0,5
}

TEST_CASE("Ooni Harci dobok: a dobos harci kedv-töltése × 2 (`drum_mult`) – a megtört harcos hamarabb helyreáll") {
  auto recovered = [](bool researched) {
    Match m("BUILD-ALAP", true, true, {"ooni", "ursan"});
    sim::Sim& s = *m.sim;
    const auto u = put(s, "oon_oni_harcos", 1, 185, 185);
    m.run(1);
    s.deal_damage(0, 2, sim::kNoDef, u, int64_t{s.world().get<sim::Health>(u)->max / 2} * 1000, false, true);
    for (int i = 0; i < 7; ++i) {   // megtörés a társak elestével
      const auto a = put(s, "oon_oni_harcos", 1, 175 + i * 3, 178);
      m.run(1);
      s.deal_damage(0, 2, sim::kNoDef, a, 100000000, false, true);
      m.run(1);
    }
    m.run(1);
    CHECK(s.mod_speed(u) != Fixed::from_int(1));   // megtört
    if (researched) learn(s, 1, "oon_harci_dobok");
    put(s, "oon_dobos", 1, 195, 185);
    m.run(4 * 30);   // dob nélkül 8/s (+2/s 3 s után): 4 s alatt < 50; × 2-vel ≥ 50
    return s.mod_speed(u) == Fixed::from_int(1);
  };
  CHECK(!recovered(false));
  CHECK(recovered(true));
}

TEST_CASE("Huk'ucha Tavak útja: a saját tavon álló saját egység sebzése × 1,2 (`pond_dmg`), szárazon nem") {
  Match m("BUILD-ALAP", true, true, {"hukucha", "ursan"});
  sim::Sim& s = *m.sim;
  const auto wet = put(s, "huk_csuzlis", 1, 95, 155);    // a főépület taván
  const auto dry = put(s, "huk_csuzlis", 1, 200, 185);
  const auto foe = put(s, "urs_berserker", 2, 300, 50);
  const Fixed w0 = s.mod_damage(wet, foe), d0 = s.mod_damage(dry, foe);
  learn(s, 1, "huk_tavak_utja");
  std::printf("  tavon: ×%.3f → ×%.3f\n", w0.to_double(), s.mod_damage(wet, foe).to_double());
  CHECK(s.mod_damage(wet, foe) == w0 * fx(12, 10));
  CHECK(s.mod_damage(dry, foe) == d0);
}

// --- fogyasztható tárgyak (item:energy, item:xp) -----------------------------------------------------------------------

TEST_CASE("tárgyak: energiafőzet (+100 energia, teli energiánál elutasítva) és tudás könyve (+300 tapasztalat)") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto h = put(s, "urs_hos_hjalmar", 1, 30, 180);   // 200 energia
  auto& c = *s.world().get<sim::Caster>(h);
  const int32_t cap = s.stat(h, "Energy.max", Fixed::from_int(R().defs[def_of("urs_hos_hjalmar")].energy_max)).floor_int() * 1000;
  CHECK(s.give_item(h, item_of("eterfozet")));
  c.energy = cap;
  CHECK(!s.use_item(h, 0));   // teli energiánál nem
  c.energy = 30000;
  CHECK(s.use_item(h, 0));
  CHECK(c.energy == 130000);
  CHECK(s.world().get<sim::HeroState>(h)->items[0] == sim::kNoItem);
  CHECK(s.give_item(h, item_of("eterfozet")));
  c.energy = cap - 20000;
  CHECK(s.use_item(h, 0));
  CHECK(c.energy == cap);   // a plafonig
  // tudás könyve
  auto& hs = *s.world().get<sim::HeroState>(h);
  CHECK(hs.level == 1 && hs.xp == 0);
  CHECK(s.give_item(h, item_of("emlekkonyv")));
  CHECK(s.use_item(h, 0));
  std::printf("  könyv után: %d. szint, %d ezred tapasztalat\n", hs.level, hs.xp);
  CHECK(hs.xp == 300000 && hs.level == 2);
  CHECK(hs.items[0] == sim::kNoItem);
}
