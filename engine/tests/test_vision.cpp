// SPDX-License-Identifier: GPL-3.0-or-later
// Látótér és lopakodás (M6, 6. lépés; ADR-007, D33): látótáv, felderítettség, a magaslat szabályai (felfelé csak
// közelről, lefelé messzebbre, a repülő mindent), a ködben lévő ellenség nem célpont, rejtett egység és felfedés,
// a fentről lövő felfedése, a köd beállításai.
#include <string>
#include <utility>

#include "check.hpp"
#include "match_util.hpp"

using namespace totu;
using namespace totu::testing;
using core::Fixed;

namespace {

uint16_t def_of(const char* id) { return content_once().rules->def(id); }

bool vis(sim::Sim& s, uint8_t player, int x, int y, int w = 32) {
  const auto& v = s.player(player).visible;
  return !v.empty() && v[static_cast<size_t>(y * w + x)] != 0;
}
bool expl(sim::Sim& s, uint8_t player, int x, int y, int w = 32) {
  const auto& v = s.player(player).explored;
  return !v.empty() && v[static_cast<size_t>(y * w + x)] != 0;
}

}  // namespace

TEST_CASE("látótér: látótáv, felderítettség, a magaslatra lentről csak közelről, fentről messzebbre, a repülő mindent") {
  Match m("BUILD-ALAP");
  CHECK(m.ok);
  if (!m.ok) return;
  sim::Sim& s = *m.sim;
  // a pálya középső sávja (y = 9) üres; egy pajzshordozó (látótáv 7) a (12, 9) csempén
  const auto u = s.spawn_unit(def_of("urs_pajzshordozo"), 1, Fixed::from_ratio(125, 10), Fixed::from_ratio(95, 10));
  m.run(6);
  CHECK(vis(s, 1, 19, 9) && !vis(s, 1, 21, 9));            // 7 csempe igen, 9 nem
  // elmegy: a hely felderített marad, de már nem látható
  s.order_move(u, Fixed::from_ratio(115, 10), Fixed::from_ratio(185, 10));
  m.run(30 * 6);
  CHECK(expl(s, 1, 19, 9) && !vis(s, 1, 19, 9));
  // a magaslat (x 20–29, y 13–18, 1. szint) széle előtt állva csak 2 csempére lát fel
  Match h("BUILD-ALAP");
  sim::Sim& t = *h.sim;
  t.spawn_unit(def_of("urs_pajzshordozo"), 1, Fixed::from_ratio(185, 10), Fixed::from_ratio(155, 10));
  h.run(6);
  CHECK(vis(t, 1, 20, 15) && !vis(t, 1, 23, 15));
  // fentről lefelé: a látótáv +1 (7 + 1 = 8); a vizsgált csempékre a főépület (7, 13) nem lát el
  Match g("BUILD-ALAP");
  sim::Sim& q = *g.sim;
  q.spawn_unit(def_of("urs_pajzshordozo"), 1, Fixed::from_ratio(275, 10), Fixed::from_ratio(185, 10));
  g.run(6);
  CHECK(vis(q, 1, 19, 18) && !vis(q, 1, 18, 18));
  // a repülő (viharholló, látótáv a sablonból) a szintektől függetlenül lát
  Match f("BUILD-ALAP");
  sim::Sim& r = *f.sim;
  r.spawn_unit(def_of("urs_viharhollo"), 1, Fixed::from_ratio(185, 10), Fixed::from_ratio(155, 10));
  f.run(6);
  CHECK(vis(r, 1, 24, 15));
}

TEST_CASE("látótér: a ködben lévő ellenség nem célpont; látva igen") {
  Match m("BUILD-ALAP");
  CHECK(m.ok);
  if (!m.ok) return;
  sim::Sim& s = *m.sim;
  const auto archer = s.spawn_unit(def_of("elf_jade_ijasz"), 1, Fixed::from_ratio(185, 10), Fixed::from_ratio(155, 10));
  const auto up = s.spawn_unit(def_of("urs_berserker"), 2, Fixed::from_ratio(235, 10), Fixed::from_ratio(155, 10));
  s.world().get<sim::Combat>(up)->order = sim::Combat::Hold;
  m.run(6);
  CHECK(!s.is_visible(1, up) && s.is_visible(2, up) && s.is_visible(1, archer));
  CHECK(!s.order_attack(archer, up));   // ködben: nem célozható
  m.run(60);
  CHECK(s.world().get<sim::Combat>(archer)->order == sim::Combat::None);   // magától sem támad
  // figyelő a magaslaton: most már látja és lövi
  s.spawn_unit(def_of("urs_telepes"), 1, Fixed::from_ratio(235, 10), Fixed::from_ratio(145, 10));
  m.run(6);
  CHECK(s.is_visible(1, up) && s.order_attack(archer, up));
}

TEST_CASE("lopakodás: a rejtett egységet csak a felfedő vagy a nagyon közeli ellenség látja") {
  Match m("BUILD-ALAP");
  CHECK(m.ok);
  if (!m.ok) return;
  sim::Sim& s = *m.sim;
  const auto egg = s.spawn_unit(def_of("huk_ikra"), 2, Fixed::from_ratio(125, 10), Fixed::from_ratio(95, 10));   // mindig rejtett
  // az őr telepes: a munkás magától nem támad (a felfedett rejtett egység után sem megy)
  const auto watcher = s.spawn_unit(def_of("urs_telepes"), 1, Fixed::from_ratio(155, 10), Fixed::from_ratio(95, 10));
  m.run(15);
  CHECK(vis(s, 1, 12, 9) && !s.is_visible(1, egg));   // a csempe látszik, a rejtett egység nem
  // 2,2 csempén belül felfedi
  s.order_move(watcher, Fixed::from_ratio(140, 10), Fixed::from_ratio(95, 10));
  m.run(30 * 2);
  CHECK(s.is_visible(1, egg));
  // vissza: újra rejtett; egy kész őrtorony (felfedés 7) messzebbről is felfedi
  s.order_move(watcher, Fixed::from_ratio(175, 10), Fixed::from_ratio(95, 10));
  m.run(30 * 2);
  CHECK(!s.is_visible(1, egg));
  s.place_building(def_of("urs_ortorony"), 1, 16, 10, true);
  m.run(15);
  CHECK(s.is_visible(1, egg));
}

TEST_CASE("látótér: a magaslatról lövő rövid időre látható; a köd beállításai") {
  Match m("BUILD-ALAP");
  CHECK(m.ok);
  if (!m.ok) return;
  sim::Sim& s = *m.sim;
  const auto low = s.spawn_unit(def_of("urs_berserker"), 1, Fixed::from_ratio(175, 10), Fixed::from_ratio(155, 10));
  s.world().get<sim::Combat>(low)->order = sim::Combat::Hold;
  const auto high = s.spawn_unit(def_of("elf_jade_ijasz"), 2, Fixed::from_ratio(225, 10), Fixed::from_ratio(155, 10));
  s.world().get<sim::Combat>(high)->cooldown = 1000;   // még nem lő
  m.run(6);
  CHECK(!s.is_visible(1, high));
  s.world().get<sim::Combat>(high)->cooldown = 0;
  CHECK(s.order_attack(high, low));
  bool seen = false;
  for (int i = 0; i < 120 && !seen; ++i) {
    s.step();
    seen = s.is_visible(1, high);
  }
  CHECK(seen);   // lövés után láthatóvá vált (D33)
  // köd nélkül minden látszik; „felderített” módban az egész pálya eleve felderített
  Match none("BUILD-ALAP", true, false);
  sim::MatchSetup st;
  st.factions = {"ursan", "ursan"};
  st.fog = sim::MatchSetup::Fog::None;
  std::string err;
  CHECK(none.sim->setup_match(*none.map, st, &err));
  CHECK(vis(*none.sim, 1, 30, 18) && vis(*none.sim, 1, 0, 0));
  Match ex("BUILD-ALAP", true, false);
  st.fog = sim::MatchSetup::Fog::Explored;
  CHECK(ex.sim->setup_match(*ex.map, st, &err));
  CHECK(expl(*ex.sim, 1, 30, 18) && !vis(*ex.sim, 1, 30, 18));
}
