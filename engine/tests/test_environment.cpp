// SPDX-License-Identifier: GPL-3.0-or-later
// Kutatás-hatások, napszak, időjárás és pályaváltozás (M6, 7. lépés; ADR-007, D24, D35).
#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>

#include "check.hpp"
#include "match_util.hpp"

using namespace totu;
using namespace totu::testing;
using core::Fixed;

namespace {

uint16_t def_of(const char* id) { return content_once().rules->def(id); }

sim::EntityId spawn(sim::Sim& s, const char* id, uint8_t owner, int x10, int y10) {
  return s.spawn_unit(def_of(id), owner, Fixed::from_ratio(x10, 10), Fixed::from_ratio(y10, 10));
}

// az eddig elszenvedett sebzés ezredekben (az életerő-hiány és a tört rész együtt)
int64_t taken(sim::Sim& s, sim::EntityId e) {
  const auto* h = s.world().get<sim::Health>(e);
  return int64_t{h->max - h->hp} * 1000 + s.world().get<sim::Vital>(e)->deficit;
}

// az első találat sebzése (ezred): a támadó a célpontra, amíg az meg nem sérül
int64_t first_hit(sim::Sim& s, sim::EntityId attacker, sim::EntityId target) {
  s.world().get<sim::Combat>(target)->order = sim::Combat::Hold;
  for (int i = 0; i < 6; ++i) s.step();   // a frissen lerakott egységek látótere
  const int64_t before = taken(s, target);
  if (!s.order_attack(attacker, target)) return -1;
  for (int i = 0; i < 600 && taken(s, target) == before; ++i) s.step();
  return taken(s, target) - before;
}

sim::EntityId find_building(sim::Sim& s, uint16_t def) {
  sim::EntityId out = sim::kNoEntity;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& b) {
    if (b.def == def && out == sim::kNoEntity) out = e;
  });
  return out;
}

bool vis(sim::Sim& s, int x, int y) {
  const auto& v = s.player(1).visible;
  return !v.empty() && v[static_cast<size_t>(y * 32 + x)] != 0;
}

Fixed moved(sim::Sim& s, sim::EntityId e, int x10, int y10) {
  const auto* p = s.world().get<sim::Position>(e);
  return core::length(p->x - Fixed::from_ratio(x10, 10), p->y - Fixed::from_ratio(y10, 10));
}

}  // namespace

TEST_CASE("környezet: a game.json és a népek adatai (napszak, éghajlat, időjárás-hatások, Ursan hőség)") {
  const auto& r = *content_once().rules;
  CHECK(r.env.day_cycle_ticks == 7200 && r.env.transition_ticks == 900);
  CHECK(r.env.hold_min_ticks == 3600 && r.env.hold_max_ticks == 10800);
  CHECK(r.env.night_sight_mult == Fixed::from_ratio(3, 4));
  CHECK(r.env.climates.count("temperate") && r.env.climates.count("snowy"));
  CHECK(r.env.weather.at("kod").sight_mult == Fixed::from_ratio(1, 2));
  CHECK(r.env.weather.at("eso").range_mult == Fixed::from_ratio(9, 10));
  CHECK(r.faction("ursan")->env_weather.at("hoseg").speed_mult == Fixed::from_ratio(9, 10));
  CHECK(r.faction("imperium")->env_weather.count("hoseg") == 0);
  const auto* rp = r.research("urs_runapengek");
  CHECK(rp && rp->modifiers.size() == 1 && rp->modifiers[0].path == "Attack.damage" &&
        rp->modifiers[0].value == Fixed::from_int(2));
}

TEST_CASE("kutatás (D24): Rúnapengék +2 közelharci sebzés, Medvebunda +1 páncél – élő egységre is azonnal") {
  Match m("BUILD-ALAP");
  CHECK(m.ok);
  if (!m.ok) return;
  sim::Sim& s = *m.sim;
  // berserker → pajzshordozó: 16 × 1,25 − 2 = 18 (lásd test_combat)
  const auto b1 = spawn(s, "urs_berserker", 1, 120, 170);
  CHECK(first_hit(s, b1, spawn(s, "urs_pajzshordozo", 2, 128, 170)) == 18000);
  s.place_building(def_of("urs_runakovacs"), 1, 12, 13, true);
  s.add_resources(1, sim::kWood, 1000);
  s.add_resources(1, sim::kGold, 1000);
  std::string err;
  CHECK(s.research(1, "urs_runapengek", &err));
  m.run(content_once().rules->research("urs_runapengek")->time_ticks + 2);
  CHECK(std::binary_search(s.player(1).researched.begin(), s.player(1).researched.end(), std::string("urs_runapengek")));
  // ugyanaz a (már élő) berserker: (16 + 2) × 1,25 − 2 = 20,5
  CHECK(first_hit(s, b1, spawn(s, "urs_pajzshordozo", 2, 128, 180)) == 20500);

  // páncél: az ellenfél berserkere üt egy ursan berserkert, a Medvebunda előtt és után – pontosan 1 egész kevesebb
  Match a("BUILD-ALAP");
  sim::Sim& t = *a.sim;
  const auto v1 = spawn(t, "urs_berserker", 1, 128, 170);
  const int64_t before = first_hit(t, spawn(t, "urs_berserker", 2, 120, 170), v1);
  CHECK(before > 1500);
  t.place_building(def_of("urs_runakovacs"), 1, 12, 13, true);
  t.add_resources(1, sim::kWood, 1000);
  t.add_resources(1, sim::kGold, 1000);
  CHECK(t.research(1, "urs_medvebunda", &err));
  a.run(content_once().rules->research("urs_medvebunda")->time_ticks + 2);
  const auto v2 = spawn(t, "urs_berserker", 1, 128, 190);
  const int64_t after = first_hit(t, spawn(t, "urs_berserker", 2, 120, 190), v2);
  std::printf("  berserker → berserker: %lld, Medvebunda után %lld (ezred)\n", static_cast<long long>(before),
              static_cast<long long>(after));
  CHECK(before - after == 1000);
}

TEST_CASE("napszak (D35): éjjel −25% látótáv és −10% építési sebesség; a meccs délelőtt indul") {
  const int cyc = content_once().rules->env.day_cycle_ticks;
  Match day("BUILD-ALAP"), night("BUILD-ALAP");
  CHECK(day.sim->night_permille() == 0);
  night.run(cyc * 6 / 10);
  CHECK(night.sim->night_permille() == 1000);
  // építés: ugyanaz a hosszúház ugyanott, egyszer nappal, egyszer éjjel
  auto start_build = [](Match& m) {
    const auto w = m.workers(1).front();
    CHECK(m.sim->order_build(w, def_of("urs_hosszuhaz"), 12, 14));
    sim::EntityId house = sim::kNoEntity;
    for (int i = 0; i < 600 && (house == sim::kNoEntity || m.sim->world().get<sim::Worker>(w)->inside != house); ++i) {
      m.sim->step();
      house = find_building(*m.sim, def_of("urs_hosszuhaz"));
    }
    CHECK(house != sim::kNoEntity && m.sim->world().get<sim::Worker>(w)->inside == house);
    return house;
  };
  const auto hd = start_build(day), hn = start_build(night);
  CHECK(day.sim->night_permille() == 0 && night.sim->night_permille() == 1000);
  const int32_t pd = day.sim->world().get<sim::Building>(hd)->progress, pn = night.sim->world().get<sim::Building>(hn)->progress;
  day.run(300);
  night.run(300);
  const int32_t gd = day.sim->world().get<sim::Building>(hd)->progress - pd;
  const int32_t gn = night.sim->world().get<sim::Building>(hn)->progress - pn;
  std::printf("  építés 300 tick alatt: nappal %d, éjjel %d\n", gd, gn);
  CHECK(gd > 0 && gn * 100 >= gd * 89 && gn * 100 <= gd * 91);

  // látótáv: pajzshordozó (7) a (12, 9) csempén – nappal 7 csempére lát, éjjel 7 × 0,75 = 5,25
  Match vd("BUILD-ALAP"), vn("BUILD-ALAP");
  vn.run(cyc * 6 / 10);
  spawn(*vd.sim, "urs_pajzshordozo", 1, 125, 95);
  spawn(*vn.sim, "urs_pajzshordozo", 1, 125, 95);
  vd.run(6);
  vn.run(6);
  CHECK(vis(*vd.sim, 19, 9));
  CHECK(vis(*vn.sim, 17, 9) && !vis(*vn.sim, 19, 9));
}

TEST_CASE("időjárás (D35): eső −10% lőtáv, hó −10% sebesség, hőség (Ursan) −10% sebesség, köd fele látótáv; hangulat módban semmi") {
  using WM = sim::MatchSetup::WeatherMode;
  // eső: a szigonyvető 5,5-ös lőtávja 4,95 – a 5,18 csempés (széltől szélig) résnyire álló célhoz közelebb kell
  // mennie; derült égen
  // (és hangulat módban) helyből lő
  auto approach = [](const char* weather, bool effect) {
    Match m("BUILD-ALAP");
    if (effect) m.sim->set_weather_mode(WM::Effect);
    const auto sp = spawn(*m.sim, "urs_szigonyvezo", 1, 110, 170);
    const auto far = spawn(*m.sim, "urs_berserker", 2, 170, 170);
    m.sim->world().get<sim::Combat>(far)->order = sim::Combat::Hold;
    m.sim->set_weather(weather);
    m.run(6);
    CHECK(m.sim->order_attack(sp, far));
    m.run(20);
    return moved(*m.sim, sp, 110, 170);
  };
  CHECK(approach("eso", true) > Fixed::from_ratio(1, 10));
  CHECK(approach("", true) == Fixed{});
  CHECK(approach("eso", false) == Fixed{});

  // hó: a kardfogú 60 tick alatt kb. 10%-kal rövidebb utat tesz meg
  auto walk = [](const char* weather, const char* unit, std::vector<std::string> factions, uint8_t owner) {
    Match m("BUILD-ALAP", true, true, std::move(factions));
    m.sim->set_weather_mode(WM::Effect);
    m.sim->set_weather(weather);
    const auto k = spawn(*m.sim, unit, owner, 110, 170);
    m.sim->order_move(k, Fixed::from_int(30), Fixed::from_ratio(170, 10));
    m.run(60);
    return moved(*m.sim, k, 110, 170);
  };
  const Fixed clear = walk("", "urs_kardfogu", {"ursan", "ursan"}, 1);
  const Fixed snow = walk("ho", "urs_kardfogu", {"ursan", "ursan"}, 1);
  std::printf("  60 tick alatt: derült %.2f, hó %.2f csempe\n", clear.to_double(), snow.to_double());
  CHECK(snow.raw() * 100 >= clear.raw() * 88 && snow.raw() * 100 <= clear.raw() * 92);
  // hőség: általánosan csak a gyűjtést lassítja; az Ursanok faji hatása a sebességet is, az Aurethiekét nem
  const Fixed heat_urs = walk("hoseg", "urs_kardfogu", {"ursan", "ursan"}, 1);
  CHECK(heat_urs.raw() * 100 >= clear.raw() * 88 && heat_urs.raw() * 100 <= clear.raw() * 92);
  const Fixed imp_clear = walk("", "imp_legios", {"imperium", "ursan"}, 1);
  const Fixed imp_heat = walk("hoseg", "imp_legios", {"imperium", "ursan"}, 1);
  CHECK(imp_heat == imp_clear);

  // köd: a pajzshordozó 7-es látótávja 3,5
  Match d("BUILD-ALAP");
  d.sim->set_weather_mode(WM::Effect);
  d.sim->set_weather("kod");
  spawn(*d.sim, "urs_pajzshordozo", 1, 125, 95);
  d.run(6);
  CHECK(vis(*d.sim, 15, 9) && !vis(*d.sim, 17, 9));
}

TEST_CASE("pályaváltozás (D35): eső sarat hagy a parton, a hó befagyasztja a tavat (átkelhető), olvadáskor a partra") {
  Match m("BUILD-ALAP");
  CHECK(m.ok);
  if (!m.ok) return;
  sim::Sim& s = *m.sim;
  s.set_weather_mode(sim::MatchSetup::WeatherMode::EffectTerrain);
  const uint16_t mud = *content_once().terrain.terrain("mud");
  const uint16_t grass = *content_once().terrain.terrain("grass");
  const uint16_t deep = *content_once().terrain.terrain("deep_water");
  const uint16_t ice = *content_once().terrain.terrain("ice");
  const int land = m.nav->grid_of(*content_once().terrain.movement_class("land"));
  // a tó: x 14–16, y 2–7 (mély víz); a (13, 4) fű a partján
  CHECK(m.map->terrain(13, 4) == grass && m.map->terrain(15, 4) == deep);
  CHECK(!m.nav->passable(land, 2 * 15 + 1, 2 * 4 + 1));
  s.set_weather("eso");
  m.run(700);
  CHECK(m.map->terrain(13, 4) == mud && m.map->terrain(15, 4) == deep);
  s.set_weather("");
  m.run(700);
  CHECK(m.map->terrain(13, 4) == grass);
  // hó: befagy, és a szárazföldi egység átkelhet rajta
  s.set_weather("ho");
  m.run(700);
  CHECK(m.map->terrain(15, 4) == ice && m.nav->passable(land, 2 * 15 + 1, 2 * 4 + 1));
  const auto walker = spawn(s, "urs_telepes", 1, 155, 45);
  s.set_weather("");
  m.run(700);
  CHECK(m.map->terrain(15, 4) == deep && !m.nav->passable(land, 2 * 15 + 1, 2 * 4 + 1));
  const auto* p = s.world().get<sim::Position>(walker);
  CHECK(!(p->x >= Fixed::from_int(14) && p->x < Fixed::from_int(17) && p->y >= Fixed::from_int(2) && p->y < Fixed::from_int(8)));
}

TEST_CASE("napszak/időjárás: determinisztikus, és mentésből folytatva ugyanaz (a pályaváltozás is)") {
  auto setup = [](Match& m) {
    m.sim->set_weather_mode(sim::MatchSetup::WeatherMode::EffectTerrain);
    m.sim->set_weather("ho");
    spawn(*m.sim, "urs_berserker", 1, 120, 170);
  };
  Match a("BUILD-ALAP"), b("BUILD-ALAP");
  setup(a);
  setup(b);
  a.run(650);
  b.run(650);
  CHECK(a.sim->hash() == b.sim->hash());
  CHECK(a.sim->weather_info().kind == "ho");
  const auto saved = a.sim->save();
  a.run(4000);   // közben időjárás-váltás és olvadás is lehet
  Match c("BUILD-ALAP", true, false);
  CHECK(c.sim->load(saved));
  c.sim->restore_obstacles(*c.map);
  c.run(4000);
  CHECK(c.sim->hash() == a.sim->hash());
  CHECK(c.map->terrain(15, 4) == a.map->terrain(15, 4));
  std::printf("  napszak/időjárás állapothash (4650 tick): 0x%016llx\n", static_cast<unsigned long long>(a.sim->hash()));
}
