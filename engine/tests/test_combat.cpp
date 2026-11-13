// SPDX-License-Identifier: GPL-3.0-or-later
// Harc (M6, 5. lépés; ADR-007, D33, D40): a régi sebzésképlet (típusszorzó, címkebónusz, páncél, pajzs, tört
// sebzés), a lövedékfajták eltérő viselkedése (követő, talajra érkező, egyenes vonalú, pattanó és visszatérő), a
// magaslat (lőtáv, sebzés, hiba felfelé), célválasztás, visszaütés, póráz, állás, támadó mozgás, halál.
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

struct Arena {
  Match m{"BUILD-ALAP"};
  sim::Sim& s = *m.sim;
  sim::EntityId spawn(const char* id, uint8_t owner, int x10, int y10) {
    return s.spawn_unit(def_of(id), owner, Fixed::from_ratio(x10, 10), Fixed::from_ratio(y10, 10));
  }
  int hp(sim::EntityId e) { return s.world().alive(e) ? s.world().get<sim::Health>(e)->hp : 0; }
  int deficit(sim::EntityId e) { return s.world().get<sim::Vital>(e)->deficit; }
  int shots() {
    int n = 0;
    s.world().pool<sim::Shot>().each([&](sim::EntityId, const sim::Shot&) { ++n; });
    return n;
  }
  // amíg a feltétel nem teljesül (legfeljebb `max` tick); a lefutott tickek száma
  template <typename F>
  int run_until(F f, int max = 600) {
    int i = 0;
    for (; i < max && !f(); ++i) s.step();
    return i;
  }
};

}  // namespace

TEST_CASE("harc: a régi sebzésképlet – típusszorzó, címkebónusz, páncél, tört sebzés") {
  Arena a;
  CHECK(a.m.ok);
  // berserker (közelharcos) → pajzshordozó (lándzsás): 16 × 1,25 − 2 páncél = 18
  const auto b = a.spawn("urs_berserker", 1, 120, 170), p = a.spawn("urs_pajzshordozo", 2, 128, 170);
  CHECK(a.s.order_attack(b, p));
  a.run_until([&] { return a.hp(p) < 157; });
  CHECK(a.hp(p) == 157 - 18);
  // dárdaőr (lándzsás) → kardfogú (lovas, cavalry): 12 × 1,5 + 14 − 2 = 30
  Arena c;
  const auto d = c.spawn("urs_dardaor", 1, 120, 170), k = c.spawn("urs_kardfogu", 2, 133, 170);
  CHECK(c.s.order_attack(d, k));
  c.run_until([&] { return c.hp(k) < 210; });
  CHECK(c.hp(k) == 210 - 30);
  // íjász (lövész) → berserker (közelharcos): 11 × 1,25 − 1 = 12,75 – a tört rész a hiányban marad
  Arena e;
  const auto ij = e.spawn("elf_jade_ijasz", 1, 120, 170), be = e.spawn("urs_berserker", 2, 160, 170);
  CHECK(e.s.order_attack(ij, be));
  e.run_until([&] { return e.hp(be) < 150; });
  CHECK(e.hp(be) == 138 && e.deficit(be) == 750);   // 150 − 12,75 = 137,25
}

TEST_CASE("harc: pajzs előbb nyel, és a késleltetés után visszatöltődik") {
  Arena a;
  const auto h = a.spawn("urs_hos_hjalmar", 2, 160, 170);   // 40 pajzs
  const auto b = a.spawn("urs_berserker", 1, 150, 170);
  CHECK(a.s.order_attack(b, h));
  a.run_until([&] { return a.s.world().get<sim::Vital>(h)->shield < 40000; });
  // berserker (közelharcos) → varázsló: 16 × 1,25 = 20 a pajzsba, az életerő érintetlen
  CHECK(a.s.world().get<sim::Vital>(h)->shield == 20000 && a.hp(h) == 340);
  a.s.kill(b);   // a támadó eltűnik; a hős nyugalomban
  a.s.world().get<sim::Combat>(h)->order = sim::Combat::None;
  a.s.world().get<sim::Combat>(h)->target = 0;
  a.m.run(30 * 10 + 30);
  CHECK(a.s.world().get<sim::Vital>(h)->shield > 20000);
}

TEST_CASE("lövedékek (D40): a nyíl követi a célt; a szikla a kilövéskori helyre esik, és területre sebez") {
  Arena a;
  const auto ij = a.spawn("elf_jade_ijasz", 1, 110, 170), t = a.spawn("urs_kardfogu", 2, 150, 170);
  CHECK(a.s.order_attack(ij, t));
  a.run_until([&] { return a.shots() > 0; });
  CHECK(a.shots() == 1);
  // a célpont elfut: a követő nyíl utoléri
  sim::Command mv;
  mv.kind = sim::Command::Kind::Move;
  mv.units = {t};
  mv.x = Fixed::from_int(18);
  mv.y = Fixed::from_int(19);
  a.s.queue(std::move(mv));
  a.s.world().get<sim::Combat>(t)->order = sim::Combat::None;
  a.run_until([&] { return a.shots() == 0; });
  CHECK(a.hp(t) < 210);
  // szikla: álló célpont + mellette egy másik → teljes és fél sebzés
  Arena b;
  const auto mam = b.spawn("urs_mammut", 1, 110, 175);
  const auto t1 = b.spawn("urs_berserker", 2, 150, 175), t2 = b.spawn("urs_berserker", 2, 150, 185);
  b.s.world().get<sim::Combat>(t1)->order = sim::Combat::Hold;
  b.s.world().get<sim::Combat>(t2)->order = sim::Combat::Hold;
  CHECK(b.s.order_attack(mam, t1));
  b.run_until([&] { return b.hp(t1) < 150; }, 900);
  // 30 × 1,25 − 1 = 36,5 a célpontnak; 15 × 1,25 − 1 = 17,75 a mellette állónak
  CHECK(b.hp(t1) == 150 - 36 && b.deficit(t1) == 500);
  CHECK(b.hp(t2) == 150 - 17 && b.deficit(t2) == 750);
  // a mozgó célpontot a szikla elhibázhatja: elfut a becsapódás elől
  Arena c;
  const auto m2 = c.spawn("urs_mammut", 1, 110, 175), run = c.spawn("urs_kardfogu", 2, 150, 175);
  c.s.world().get<sim::Combat>(run)->order = sim::Combat::Hold;
  CHECK(c.s.order_attack(m2, run));
  c.run_until([&] { return c.shots() > 0; }, 900);
  c.s.world().get<sim::Combat>(run)->order = sim::Combat::None;
  sim::Command flee;
  flee.kind = sim::Command::Kind::Move;
  flee.units = {run};
  flee.x = Fixed::from_int(15);
  flee.y = Fixed::from_int(10);
  c.s.queue(std::move(flee));
  c.run_until([&] { return c.shots() == 0; });
  // közvetlen találat 30 × 1,25 − 2 = 35,5 lenne; a régi helyére esett szikla legfeljebb fröccsen (17,75 − 2 = 16,75)
  std::printf("  szikla a menekülőre: %d életerő maradt (210-ből)\n", c.hp(run));
  CHECK(c.hp(run) >= 210 - 17);
}

TEST_CASE("lövedékek (D40): a dárda az útjába kerülő első ellenséget találja; a bumeráng továbbpattan, majd visszatér") {
  Arena a;
  const auto sz = a.spawn("urs_szigonyvezo", 1, 110, 175);
  const auto front = a.spawn("urs_berserker", 2, 135, 175), back = a.spawn("urs_berserker", 2, 155, 175);
  a.s.world().get<sim::Combat>(front)->order = sim::Combat::Hold;
  a.s.world().get<sim::Combat>(back)->order = sim::Combat::Hold;
  // a hátsót célozza, de a dárda az elsőbe fúródik
  CHECK(a.s.order_attack(sz, back));
  a.run_until([&] { return a.hp(front) < 150 || a.hp(back) < 150; }, 900);
  CHECK(a.hp(front) < 150 && a.hp(back) == 150);
  // bumeráng: a második célpont 75%-ot kap, a lövedék utána visszarepül a dobóhoz
  Arena b;
  const auto w = b.spawn("wul_ijas_vadasz", 1, 110, 175);
  const auto t1 = b.spawn("urs_kardfogu", 2, 150, 175), t2 = b.spawn("urs_kardfogu", 2, 160, 185);
  b.s.world().get<sim::Combat>(t1)->order = sim::Combat::Hold;
  b.s.world().get<sim::Combat>(t2)->order = sim::Combat::Hold;
  CHECK(b.s.order_attack(w, t1));
  b.run_until([&] { return b.hp(t2) < 210; }, 900);
  // 11 × 1 − 2 = 9 az elsőnek; 11 × 0,75 = 8,25 − 2 = 6,25 a másodiknak
  CHECK(b.hp(t1) == 201 && b.hp(t2) == 204);
  bool returning = false;
  b.s.world().pool<sim::Shot>().each([&](sim::EntityId, const sim::Shot& s) { returning = returning || s.returning; });
  CHECK(returning);
}

TEST_CASE("magaslat (D33): lefelé +10% sebzés és nagyobb lőtáv; felfelé a nyíl hibázhat, a mágia nem") {
  Arena a;
  // az íjász a magaslaton (1. szint), a célpont alatta: 12,75 × 1,1 = 14,025 − 1… (a szorzó a kilövéskor)
  const auto up = a.spawn("elf_jade_ijasz", 1, 225, 155), low = a.spawn("urs_berserker", 2, 175, 155);
  a.s.world().get<sim::Combat>(low)->order = sim::Combat::Hold;
  CHECK(a.s.level_at(up) == 1 && a.s.level_at(low) == 0);
  CHECK(a.s.order_attack(up, low));
  a.run_until([&] { return a.hp(low) < 150; }, 900);
  // 11 × 1,1 = 12,1 → × 1,25 = 15,125 − 1 = 14,125
  CHECK(a.hp(low) == 136 && a.deficit(low) == 125);
  // felfelé: sok lövésből a nyilak egy része célt téveszt, a mágia mind talál
  auto hits = [](const char* shooter, int shots_wanted) {
    Arena b;
    const auto sh = b.spawn(shooter, 1, 175, 155);
    const auto tgt = b.s.place_building(def_of("urs_hosszuhaz"), 2, 21, 15, true);   // a magaslaton; nem lő vissza
    b.spawn("urs_telepes", 1, 245, 145);   // figyelő a magaslaton: lentről csak így látni fel (D33)
    b.m.run(6);
    CHECK(b.s.level_at(tgt) == 1 && b.s.level_at(sh) == 0);
    b.s.order_attack(sh, tgt);
    int fired = 0, landed = 0, last_hp = b.hp(tgt), last_cd = 0;
    for (int i = 0; i < 30 * 200 && fired < shots_wanted; ++i) {
      b.s.step();
      const int cd = b.s.world().get<sim::Combat>(sh)->cooldown;
      if (cd > last_cd) ++fired;   // az újratöltés újraindult: lőtt
      last_cd = cd;
      if (b.hp(tgt) < last_hp) {
        ++landed;
        last_hp = b.hp(tgt);
      }
    }
    b.m.run(60);   // az utolsó lövés is becsapódik
    if (b.hp(tgt) < last_hp) ++landed;
    return std::pair<int, int>{fired, landed};
  };
  const auto [af, al] = hits("elf_jade_ijasz", 40);
  const auto [mf, ml] = hits("urs_hos_hjalmar", 20);
  std::printf("  felfelé: nyíl %d/%d talált, mágia %d/%d\n", al, af, ml, mf);
  CHECK(al < af - 3 && al > af / 2);   // ~25% hiba
  CHECK(ml == mf);                     // a mágia nem hibáz
}

TEST_CASE("harc: célválasztás, visszaütés, a munkás nem üt vissza, póráz, állás, támadó mozgás, halál") {
  Arena a;
  // tétlen harcos a látótávon belüli ellenségre magától indul
  const auto g = a.spawn("urs_berserker", 1, 120, 175), e = a.spawn("urs_pajzshordozo", 2, 175, 175);
  a.run_until([&] { return a.s.world().get<sim::Combat>(g)->order == sim::Combat::Attack; }, 60);
  CHECK(a.s.world().get<sim::Combat>(g)->target == e && a.s.world().get<sim::Combat>(g)->autonomous);
  a.run_until([&] { return !a.s.world().alive(e) || !a.s.world().alive(g); }, 30 * 60);
  CHECK(!a.s.world().alive(e) || !a.s.world().alive(g));   // valaki elesett, és eltűnt
  // a munkás nem üt vissza
  Arena b;
  const auto settler = b.spawn("urs_telepes", 1, 120, 175), atk = b.spawn("urs_berserker", 2, 128, 175);
  b.s.order_attack(atk, settler);
  b.run_until([&] { return b.hp(settler) < 55; });
  b.m.run(5);
  CHECK(b.s.world().get<sim::Combat>(settler)->order == sim::Combat::None);
  // állás: nem mozdul, a lőtávon kívülit nem üldözi
  Arena c;
  const auto hold = c.spawn("urs_berserker", 1, 120, 175), far = c.spawn("urs_pajzshordozo", 2, 160, 175);
  c.s.world().get<sim::Combat>(far)->order = sim::Combat::Hold;
  CHECK(c.s.order_hold(hold));
  const auto p0 = *c.s.world().get<sim::Position>(hold);
  c.m.run(90);
  const auto p1 = *c.s.world().get<sim::Position>(hold);
  CHECK(p0.x == p1.x && p0.y == p1.y && c.hp(far) == 157);
  // támadó mozgás: útközben harcol, utána megy tovább a célba
  Arena d;
  const auto am = d.spawn("urs_kardfogu", 1, 110, 175), mid = d.spawn("urs_telepes", 2, 150, 175);
  d.s.world().get<sim::Combat>(mid)->order = sim::Combat::Hold;
  CHECK(d.s.order_attack_move(am, Fixed::from_int(18), Fixed::from_int(17)));
  d.run_until([&] { return !d.s.world().alive(mid); }, 30 * 30);
  CHECK(!d.s.world().alive(mid));
  d.run_until([&] { return d.s.world().get<sim::Combat>(am)->order == sim::Combat::None; }, 30 * 20);
  const auto& pe = *d.s.world().get<sim::Position>(am);
  CHECK(core::length(pe.x - Fixed::from_int(18), pe.y - Fixed::from_int(17)) < Fixed::from_int(2));
  // épület halála: a hely járható lesz
  Arena f;
  const auto house = f.s.place_building(def_of("urs_hosszuhaz"), 2, 15, 16, true);
  const auto ram = f.spawn("urs_runaorias", 1, 130, 170);
  CHECK(f.s.order_attack(ram, house));
  f.run_until([&] { return !f.s.world().alive(house); }, 30 * 90);
  CHECK(!f.s.world().alive(house) && f.m.nav->passable(0, 2 * 15 + 1, 2 * 16 + 1));
}

TEST_CASE("harc: determinisztikus csata (rögzített hash) és mentésből folytatva ugyanaz") {
  auto battle = [](Match& m) {
    sim::Sim& s = *m.sim;
    const char* kinds[] = {"urs_berserker", "urs_pajzshordozo", "urs_szigonyvezo", "urs_mammut", "wul_ijas_vadasz"};
    for (int i = 0; i < 10; ++i) {
      s.spawn_unit(content_once().rules->def(kinds[i % 5]), 1, Fixed::from_ratio(110 + (i % 5) * 10, 10), Fixed::from_ratio(160 + (i / 5) * 15, 10));
      s.spawn_unit(content_once().rules->def(kinds[(i + 2) % 5]), 2, Fixed::from_ratio(185 - (i % 5) * 0, 10) + Fixed::from_ratio((i % 5) * 5, 10),
                   Fixed::from_ratio(150 + (i / 5) * 25 + (i % 5) * 4, 10));
    }
  };
  Match a("BUILD-ALAP"), b("BUILD-ALAP");
  battle(a);
  battle(b);
  a.run(450);
  b.run(450);
  CHECK(a.sim->hash() == b.sim->hash());
  const auto saved = a.sim->save();
  a.run(450);
  Match c("BUILD-ALAP", true, false);
  CHECK(c.sim->load(saved));
  c.sim->restore_obstacles(*c.map);
  c.run(450);
  CHECK(c.sim->hash() == a.sim->hash());
  std::printf("  harci állapothash (900 tick): 0x%016llx\n", static_cast<unsigned long long>(a.sim->hash()));
  CHECK(a.sim->hash() == 0x77c5f9554d753975ULL);   // minden rendszeren ugyanez
}
