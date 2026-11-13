// SPDX-License-Identifier: GPL-3.0-or-later
// Képességek (M6, 8. lépés; ADR-007, KEPESSEGEK.md): az Ursanok 16 képessége és a három hős képességei a régi játék
// szerint – kapcsoló, ének, önerősítés, csapás körben, célzott lövés, területi mező, idézés, aura, gyógyítás, elemek.
#include <algorithm>
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
uint16_t ab_of(const char* id) { return static_cast<uint16_t>(R().ability_index(id)); }

// a pálya nyílt, sík sávja (BUILD-ALAP: y 17–19, x 0–19; a munkások fölötte dolgoznak)
sim::EntityId put(sim::Sim& s, const char* id, uint8_t owner, int x10, int y10, bool hold = true) {
  const sim::EntityId e = s.spawn_unit(def_of(id), owner, Fixed::from_ratio(x10, 10), Fixed::from_ratio(y10, 10));
  if (hold) {
    if (auto* c = s.world().get<sim::Combat>(e)) c->order = sim::Combat::Hold;
  }
  return e;
}

int64_t taken(sim::Sim& s, sim::EntityId e) {
  const auto* h = s.world().get<sim::Health>(e);
  return int64_t{h->max - h->hp} * 1000 + s.world().get<sim::Vital>(e)->deficit;
}

void run(Match& m, int n) { m.run(n); }

bool use(sim::Sim& s, sim::EntityId e, const char* ab, sim::EntityId target = 0, int x10 = 0, int y10 = 0) {
  return s.use_ability({e}, ab, Fixed::from_ratio(x10, 10), Fixed::from_ratio(y10, 10), target);
}

Fixed fx(int num, int den) { return Fixed::from_ratio(num, den); }

bool near_fx(Fixed a, Fixed b) { return std::abs(int64_t{a.raw()} - b.raw()) <= 2; }

int count_units(sim::Sim& s, const char* id, uint8_t owner) {
  int n = 0;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit& u) {
    if (u.def == def_of(id) && s.world().get<sim::Health>(e)->owner == owner && s.world().get<sim::Health>(e)->hp > 0) ++n;
  });
  return n;
}

void set_energy(sim::Sim& s, sim::EntityId e, int v) { s.world().get<sim::Caster>(e)->energy = v * 1000; }

// hős a megadott szinten, a megadott képességekkel (D25: tapasztalat → szint, képességpont → tanulás)
sim::EntityId hero(sim::Sim& s, const char* id, uint8_t owner, int x10, int y10, int level,
                   std::initializer_list<const char*> learn) {
  const auto e = put(s, id, owner, x10, y10);
  if (level > 1) s.add_xp(e, R().heroes.levels[static_cast<size_t>(level - 1)] * 1000);
  for (const char* a : learn) CHECK(s.learn(e, a));
  return e;
}

}  // namespace

TEST_CASE("képességek: a tartalom – az Ursanok 16 képessége és a hősöké, típus, szint, elem, idézett egység") {
  using T = sim::AbilityDef::Type;
  const char* ursan[] = {"beszallas", "fegyverbe", "pajzsfal", "dardafal", "roham", "taposas", "harci_kurt", "medveuvoltes",
                         "verszomj", "szellemtest", "gyogyitas", "horgonyszigony", "zuhano_csapas", "runako", "runako_aura",
                         "telepites"};
  for (const char* id : ursan) {
    const int i = R().ability_index(id);
    CHECK(i >= 0);
    if (i >= 0) CHECK(R().abilities[static_cast<size_t>(i)].type != T::Unknown);
  }
  // az egységek listája feloldva
  for (const char* id : {"urs_telepes", "urs_pajzshordozo", "urs_szigonyvezo", "urs_berserker", "urs_runapap", "urs_kardfogu",
                         "urs_viharhollo", "urs_mammut", "urs_dardaor", "urs_ostromszan", "urs_kardfogu_tigris",
                         "urs_runako", "urs_medvealak", "urs_kurtos", "urs_runaorias"}) {
    CHECK(!R().defs[def_of(id)].abilities.empty());
  }
  // a hősök négy-négy képessége az 1. szinten
  for (const char* h : {"urs_hos_ragna", "urs_hos_hjalmar", "urs_hos_bjarka"}) {
    CHECK(R().defs[def_of(h)].abilities.size() == 4);
    CHECK(R().defs[def_of(h)].energy_max >= 150);
  }
  const auto& sz = R().abilities[ab_of("h_hja_szilank")];
  CHECK(sz.type == T::Bolt && sz.damage == Fixed::from_int(75) && sz.energy == 50 && sz.element == sim::Element::Ice);
  CHECK(R().abilities[ab_of("h_ragna_esku")].mods.armor_add == Fixed::from_int(1));
  CHECK(R().abilities[ab_of("medveuvoltes")].element == sim::Element::Fear);
  CHECK(R().abilities[ab_of("runako")].unit == def_of("urs_runako"));
  CHECK(R().abilities[ab_of("h_bja_medve")].unit == def_of("urs_kardfogu_tigris"));
  CHECK(R().abilities[ab_of("h_bja_mamutcsorda")].unit == def_of("urs_mammut") && R().abilities[ab_of("h_bja_mamutcsorda")].count == 2);
  // alapértékek (KEPESSEGEK.md B-2): a csapás 10 s helyett a megadott 18 s, a gyógyítás 0,8 s
  CHECK(R().abilities[ab_of("taposas")].cooldown == 540 && R().abilities[ab_of("gyogyitas")].cooldown == 24);
  // a Fegyverbe! a Népfelkeléshez kötött
  bool gated = false;
  for (const auto& [a, rid] : R().defs[def_of("urs_telepes")].ability_research) gated = gated || (a == ab_of("fegyverbe") && rid == "urs_nepfelkeles");
  CHECK(gated);
}

TEST_CASE("pajzsfal (kapcsoló): fél sebesség, a távolsági sebzés 40%-a, +2 páncél; a közelharc ellen csak a páncél") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto sh = put(s, "urs_pajzshordozo", 1, 160, 180);
  const auto sp = put(s, "urs_szigonyvezo", 2, 120, 180);
  run(m, 6);
  // kikapcsolva: a szigony teljes sebzése
  CHECK(s.order_attack(sp, sh));
  int64_t before = taken(s, sh);
  for (int i = 0; i < 120 && taken(s, sh) == before; ++i) s.step();
  const int64_t off = taken(s, sh) - before;
  CHECK(off > 0);
  CHECK(use(s, sh, "pajzsfal"));
  CHECK(s.toggled(sh, ab_of("pajzsfal")));
  const auto mods = s.unit_mods(sh);
  CHECK(mods.speed_mult == fx(1, 2) && mods.armor_add == Fixed::from_int(2) && near_fx(mods.ranged_taken_mult, fx(2, 5)));
  before = taken(s, sh);
  for (int i = 0; i < 120 && taken(s, sh) == before; ++i) s.step();
  const int64_t on = taken(s, sh) - before;
  // páncél nélkül a nyers sebzés: off + 2000; bekapcsolva × 0,4, majd − 4 páncél (legalább 0,5)
  const int64_t raw = off + 2000;
  const int64_t expect = std::max<int64_t>(500, raw * 2 / 5 - 4000);
  std::printf("  szigony a pajzshordozóra: %lld, pajzsfalban %lld (várt %lld)\n", static_cast<long long>(off),
              static_cast<long long>(on), static_cast<long long>(expect));
  CHECK(std::abs(on - expect) <= 2);
  // újra megnyomva kikapcsol
  CHECK(use(s, sh, "pajzsfal"));
  CHECK(!s.toggled(sh, ab_of("pajzsfal")));
}

TEST_CASE("dárdafal és telepítés (kapcsoló): nem mozdul, erősebben üt; a telepített ostromszán messzebbre lő") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto d = put(s, "urs_dardaor", 1, 30, 180, false);
  const auto o = put(s, "urs_ostromszan", 1, 60, 185, false);
  CHECK(use(s, d, "dardafal") && use(s, o, "telepites"));
  const auto md = s.unit_mods(d), mo = s.unit_mods(o);
  CHECK(md.speed_mult.raw() == 0 && near_fx(md.damage_mult, fx(8, 5)) && md.armor_add == Fixed::from_int(2));
  CHECK(mo.speed_mult.raw() == 0 && mo.range_add == Fixed::from_int(3) && mo.damage_mult == fx(5, 4));
  CHECK(s.order_move(d, Fixed::from_int(12), Fixed::from_int(18)) && s.order_move(o, Fixed::from_int(15), Fixed::from_int(18)));
  run(m, 60);
  CHECK(s.world().get<sim::Position>(d)->x == fx(30, 10));   // áll
  CHECK(s.world().get<sim::Position>(o)->x == fx(60, 10));
  CHECK(use(s, d, "dardafal"));   // kikapcsolva elindul
  run(m, 30);
  CHECK(s.world().get<sim::Position>(d)->x > fx(40, 10));
}

TEST_CASE("vérszomj és roham (önerősítés): ideiglenes, újratöltéssel; a berserker többet üt és többet kap") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto b = put(s, "urs_berserker", 1, 30, 180, false);
  const auto k = put(s, "urs_kardfogu", 1, 30, 190, false);
  CHECK(use(s, b, "verszomj"));
  CHECK(!use(s, b, "verszomj"));   // újratöltés (20 s)
  auto mb = s.unit_mods(b);
  CHECK(mb.damage_mult == fx(3, 2) && mb.speed_mult == fx(6, 5) && mb.armor_add == Fixed::from_int(-2));
  CHECK(use(s, k, "roham"));
  const auto mk = s.unit_mods(k);
  CHECK(near_fx(mk.speed_mult, fx(17, 10)) && near_fx(mk.damage_mult, fx(13, 10)));
  run(m, 119);
  CHECK(s.buffed(k, ab_of("roham")));
  run(m, 2);
  CHECK(!s.buffed(k, ab_of("roham")));   // 4 s
  run(m, 210 - 121 - 1);
  CHECK(s.buffed(b, ab_of("verszomj")));
  run(m, 2);
  CHECK(!s.buffed(b, ab_of("verszomj")));   // 7 s
  CHECK(!use(s, b, "verszomj"));
  run(m, 600 - 211);
  CHECK(use(s, b, "verszomj"));   // 20 s után újra
  // a roham alatt gyorsabb: ugyanannyi idő alatt messzebb jut
  Match m2("BUILD-ALAP");
  const auto k1 = put(*m2.sim, "urs_kardfogu", 1, 10, 175, false);
  const auto k2 = put(*m2.sim, "urs_kardfogu", 1, 10, 190, false);
  CHECK(use(*m2.sim, k2, "roham"));
  m2.sim->order_move(k1, Fixed::from_int(19), fx(175, 10));
  m2.sim->order_move(k2, Fixed::from_int(19), Fixed::from_int(19));
  run(m2, 40);
  const Fixed d1 = m2.sim->world().get<sim::Position>(k1)->x - fx(10, 10);
  const Fixed d2 = m2.sim->world().get<sim::Position>(k2)->x - fx(10, 10);
  std::printf("  40 tick alatt: kardfogú %.2f, rohamban %.2f csempe\n", d1.to_double(), d2.to_double());
  CHECK(d2 > d1 * fx(3, 2));
}

TEST_CASE("fegyverbe (önerősítés munkásnak): csak a Népfelkeléssel; a gyűjtést abbahagyja, a riadó után visszamegy") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  run(m, 90);
  sim::EntityId w = sim::kNoEntity;
  for (const auto e : m.workers(1)) {
    const auto* wk = s.world().get<sim::Worker>(e);
    if (wk->state == sim::Worker::Work || wk->state == sim::Worker::Go || wk->state == sim::Worker::Return) w = e;
  }
  CHECK(w != sim::kNoEntity);
  CHECK(!use(s, w, "fegyverbe"));   // kutatás nélkül nem
  s.add_resources(1, sim::kWood, 2000);
  s.add_resources(1, sim::kStone, 2000);
  s.add_resources(1, sim::kMeteor, 2000);
  std::string err;
  CHECK(s.research(1, "urs_nepfelkeles", &err));
  run(m, R().research("urs_nepfelkeles")->time_ticks + 2);
  CHECK(use(s, w, "fegyverbe"));
  CHECK(s.world().get<sim::Worker>(w)->state == sim::Worker::Idle);
  const auto mw = s.unit_mods(w);
  CHECK(near_fx(mw.damage_mult, fx(8, 5)) && mw.armor_add == Fixed::from_int(2) && near_fx(mw.speed_mult, fx(115, 100)));
  run(m, 45 * 30 + 2);
  CHECK(s.world().get<sim::Worker>(w)->state != sim::Worker::Idle);   // visszament dolgozni
}

TEST_CASE("taposás és medveüvöltés (csapás körben): varázssebzés a körben, kábítás, illetve félelem") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto mam = put(s, "urs_mammut", 1, 50, 180);
  const auto near = put(s, "urs_berserker", 2, 65, 180);
  const auto far = put(s, "urs_berserker", 2, 86, 180);
  CHECK(use(s, mam, "taposas"));
  CHECK(taken(s, near) == 35000 && taken(s, far) == 0);   // páncél nélkül, csak a 2,2 csempés körben
  CHECK(s.unit_speed_mult(near).raw() == 0);              // kábult (1 s)
  CHECK(!use(s, mam, "taposas"));                         // 18 s újratöltés
  s.order_move(near, Fixed::from_int(2), fx(180, 10));
  run(m, 25);
  CHECK(s.world().get<sim::Position>(near)->x == fx(65, 10));   // kábultan nem mozdul
  // medveüvöltés: 30 sebzés, és az ellenség az üvöltőtől elfelé menekül (nem üt közben)
  Match m2("BUILD-ALAP");
  sim::Sim& s2 = *m2.sim;
  const auto bear = put(s2, "urs_medvealak", 1, 100, 180);
  const auto foe = put(s2, "urs_berserker", 2, 115, 180, false);
  CHECK(use(s2, bear, "medveuvoltes"));
  CHECK(taken(s2, foe) == 30000);
  run(m2, 40);
  std::printf("  félelem: a berserker 40 tick alatt %.2f csempét hátrált\n",
              (s2.world().get<sim::Position>(foe)->x - fx(115, 10)).to_double());
  CHECK(s2.world().get<sim::Position>(foe)->x > fx(125, 10));
  CHECK(taken(s2, bear) == 0);
}

TEST_CASE("harci kürt (ének) és aurák: a körzet saját egységei erősebbek; egyszerre egy ének; az aura nem halmozódik") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto horn = put(s, "urs_kurtos", 1, 50, 180);
  const auto in = put(s, "urs_berserker", 1, 100, 180);
  const auto out = put(s, "urs_berserker", 1, 120, 180);
  const auto foe = put(s, "urs_berserker", 2, 90, 180);
  CHECK(use(s, horn, "harci_kurt"));
  run(m, 1);
  CHECK(near_fx(s.unit_mods(in).damage_mult, fx(115, 100)) && near_fx(s.unit_mods(in).speed_mult, fx(11, 10)));
  CHECK(s.unit_mods(out).damage_mult == Fixed::from_int(1));   // 7 csempe: kívül
  CHECK(s.unit_mods(foe).damage_mult == Fixed::from_int(1));   // ellenségre nem
  CHECK(use(s, horn, "harci_kurt"));   // ki
  run(m, 1);
  CHECK(s.unit_mods(in).damage_mult == Fixed::from_int(1));
  // két azonos aura (két hős Esküje) nem halmozódik: +1 páncél marad
  Match m2("BUILD-ALAP");
  sim::Sim& s2 = *m2.sim;
  hero(s2, "urs_hos_ragna", 1, 40, 180, 1, {"h_ragna_esku"});
  hero(s2, "urs_hos_ragna", 1, 60, 180, 1, {"h_ragna_esku"});
  const auto ally = put(s2, "urs_berserker", 1, 50, 185);
  run(m2, 1);
  CHECK(s2.unit_mods(ally).armor_add == Fixed::from_int(1));
}

TEST_CASE("gyógyítás (önműködő): a rúnapap 0,8 s-onként 10 életerőt ad a legsebesültebb közeli élőlénynek, gépnek nem") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  put(s, "urs_runapap", 1, 50, 180);
  const auto hurt = put(s, "urs_berserker", 1, 80, 180);
  const auto golem = put(s, "urs_runaorias", 1, 50, 160);
  s.world().get<sim::Health>(hurt)->hp = 100;
  s.world().get<sim::Health>(golem)->hp = 500;
  run(m, 1);
  CHECK(s.world().get<sim::Health>(hurt)->hp == 110);
  run(m, 29);
  CHECK(s.world().get<sim::Health>(hurt)->hp == 110);   // 0,5 s-os ütem + 0,8 s újratöltés: 1 s-onként
  run(m, 1);
  CHECK(s.world().get<sim::Health>(hurt)->hp == 120);
  CHECK(s.world().get<sim::Health>(golem)->hp == 500);  // a rúnaóriás gép
}

TEST_CASE("horgonyszigony és zuhanó csapás (célzott lövés): varázssebzés, kábítás, illetve lassítás; újratöltés") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto sp = put(s, "urs_szigonyvezo", 1, 30, 180);
  const auto foe = put(s, "urs_berserker", 2, 95, 180);
  run(m, 6);
  CHECK(use(s, sp, "horgonyszigony", foe));
  for (int i = 0; i < 90 && taken(s, foe) == 0; ++i) s.step();
  CHECK(taken(s, foe) == 30000);   // varázssebzés: páncél nélkül
  CHECK(s.unit_speed_mult(foe).raw() == 0);   // kábult (1,2 s)
  CHECK(!use(s, sp, "horgonyszigony", foe));  // 14 s újratöltés
  // zuhanó csapás: azonnal, 35 sebzés, fél sebesség 1,5 s-ig
  Match m2("BUILD-ALAP");
  sim::Sim& s2 = *m2.sim;
  const auto raven = put(s2, "urs_viharhollo", 1, 30, 180);
  const auto f2 = put(s2, "urs_berserker", 2, 70, 180);
  run(m2, 6);
  CHECK(use(s2, raven, "zuhano_csapas", f2));
  run(m2, 1);
  CHECK(taken(s2, f2) == 35000);
  CHECK(s2.unit_speed_mult(f2) == fx(1, 2));
  run(m2, 45);
  CHECK(s2.unit_speed_mult(f2) == Fixed::from_int(1));
}

TEST_CASE("célzott lövés messziről: a használó előbb odamegy; más parancs megszakítja") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto sp = put(s, "urs_szigonyvezo", 1, 10, 180);
  const auto foe = put(s, "urs_berserker", 2, 95, 180);
  run(m, 6);
  if (!s.is_visible(1, foe)) {   // 8,5 csempe: a látótávon túl lehet – akkor a felderítő hozza látótérbe
    put(s, "urs_berserker", 1, 80, 190);
    run(m, 6);
  }
  CHECK(use(s, sp, "horgonyszigony", foe));
  run(m, 2);
  CHECK(s.world().get<sim::Position>(sp)->x > fx(10, 10));   // közelebb megy (7 + célsugár)
  for (int i = 0; i < 120 && taken(s, foe) == 0; ++i) s.step();
  CHECK(taken(s, foe) == 30000);
  // megszakítás: mozgásparancs után nem lő
  Match m2("BUILD-ALAP");
  sim::Sim& s2 = *m2.sim;
  const auto sp2 = put(s2, "urs_szigonyvezo", 1, 10, 180);
  const auto spot = put(s2, "urs_berserker", 1, 80, 190);
  (void)spot;
  const auto foe2 = put(s2, "urs_berserker", 2, 95, 180);
  run(m2, 6);
  CHECK(use(s2, sp2, "horgonyszigony", foe2));
  sim::Command mv;
  mv.kind = sim::Command::Kind::Move;
  mv.tick = s2.tick();
  mv.units = {sp2};
  mv.x = fx(10, 10);
  mv.y = fx(195, 10);
  s2.queue(mv);
  run(m2, 10);
  CHECK(taken(s2, foe2) == 0);
  CHECK(s2.world().get<sim::Caster>(sp2)->cast == sim::kNoAbility);
  CHECK(s2.world().get<sim::Position>(sp2)->y > fx(183, 10));   // a mozgásparancs szerint megy
}

TEST_CASE("rúnakő (idézés pontra) és aurája: a pap a pont mellé megy, a kő 45 s-ig áll, a körzetben +2 páncél, +1 életerő/s") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto pap = put(s, "urs_runapap", 1, 50, 180);
  const auto ally = put(s, "urs_berserker", 1, 145, 180);   // a kő auráján belül, a pap gyógyításán kívül
  s.world().get<sim::Health>(ally)->hp = 100;
  CHECK(use(s, pap, "runako", 0, 100, 180));
  for (int i = 0; i < 120 && count_units(s, "urs_runako", 1) == 0; ++i) s.step();
  CHECK(count_units(s, "urs_runako", 1) == 1);
  run(m, 1);
  const auto mods = s.unit_mods(ally);
  CHECK(mods.armor_add == Fixed::from_int(2) && mods.regen_add == Fixed::from_int(1));
  run(m, 300);
  std::printf("  rúnakő mellett 10 s alatt: %d → %d életerő\n", 100, s.world().get<sim::Health>(ally)->hp);
  CHECK(s.world().get<sim::Health>(ally)->hp >= 109 && s.world().get<sim::Health>(ally)->hp <= 111);
  run(m, 45 * 30);
  CHECK(count_units(s, "urs_runako", 1) == 0);   // lejárt
  run(m, 1);
  CHECK(s.unit_mods(ally).armor_add.raw() == 0);
}

TEST_CASE("hősök (az M6-ban szint nélkül, az 1. szinten): energia, Ragna, Hjalmar és Bjarka képességei") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto ragna = hero(s, "urs_hos_ragna", 1, 30, 180, 2, {"h_ragna_esku", "h_ragna_roham"});
  const auto hja = hero(s, "urs_hos_hjalmar", 1, 30, 190, 6, {"h_hja_szilank", "h_hja_fagyott_ido"});
  const auto bja = hero(s, "urs_hos_bjarka", 1, 30, 170, 6, {"h_bja_falka", "h_bja_medve", "h_bja_mamutcsorda"});
  CHECK(s.world().get<sim::Caster>(bja)->energy == 50000);   // legfeljebb 50 energiával indul
  run(m, 30);
  const int32_t en = s.world().get<sim::Caster>(bja)->energy;
  CHECK(en >= 50700 && en <= 50900);   // 0,8 / s
  // Ragna: Eskü aura (+1 páncél a körzetben, neki is), Roham (azonnali lövés, 50 sebzés, 1,5 s kábítás)
  CHECK(s.unit_mods(ragna).armor_add >= Fixed::from_int(1));
  const auto foe = put(s, "urs_berserker", 2, 60, 180);
  run(m, 6);
  set_energy(s, ragna, 40);
  CHECK(!use(s, ragna, "h_ragna_roham", foe));   // 60 energia kell
  run(m, 2);
  CHECK(taken(s, foe) == 0);
  set_energy(s, ragna, 60);
  CHECK(use(s, ragna, "h_ragna_roham", foe));
  run(m, 2);
  CHECK(taken(s, foe) == 50000);
  CHECK(s.unit_speed_mult(foe).raw() == 0);
  // Hjalmar: Jégszilánk (75 + jég: lassítás 0,35 3 s-ig), Fagyott idő (120 + 3,5 s kábítás körben)
  const auto f2 = put(s, "urs_berserker", 2, 80, 190);
  run(m, 6);
  set_energy(s, hja, 200);
  CHECK(use(s, hja, "h_hja_szilank", f2));
  for (int i = 0; i < 60 && taken(s, f2) == 0; ++i) s.step();
  CHECK(taken(s, f2) == 75000);
  CHECK(s.unit_speed_mult(f2) == fx(35, 100) || near_fx(s.unit_speed_mult(f2), fx(35, 100)));
  const auto f3 = put(s, "urs_berserker", 2, 45, 195);
  CHECK(use(s, hja, "h_hja_fagyott_ido"));
  CHECK(taken(s, f3) == 120000);
  CHECK(s.world().get<sim::Afflict>(f3)->stun_until == s.tick() + 105);
  // Bjarka: Falka aura (×1,08), Medve-idézés (kardfogú tigris, 45 s), Mamutcsorda (2 mamut, 40 s)
  CHECK(near_fx(s.unit_mods(bja).damage_mult, fx(108, 100)));
  set_energy(s, bja, 75);
  CHECK(use(s, bja, "h_bja_medve"));
  CHECK(count_units(s, "urs_kardfogu_tigris", 1) == 1);
  CHECK(!use(s, bja, "h_bja_medve"));   // energia és újratöltés
  set_energy(s, bja, 150);
  CHECK(use(s, bja, "h_bja_mamutcsorda"));
  CHECK(count_units(s, "urs_mammut", 1) == 2);
  run(m, 40 * 30 + 1);
  CHECK(count_units(s, "urs_mammut", 1) == 0);
  CHECK(count_units(s, "urs_kardfogu_tigris", 1) == 1);
  run(m, 5 * 30);
  CHECK(count_units(s, "urs_kardfogu_tigris", 1) == 0);
}

TEST_CASE("Hjalmar hóvihara (területi mező): 5 s-ig, 0,5 s-onként 8 varázssebzés és lassítás a körben") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  const auto hja = hero(s, "urs_hos_hjalmar", 1, 20, 180, 1, {"h_hja_vihar"});
  const auto foe = put(s, "urs_berserker", 2, 95, 180);
  const auto spot = put(s, "urs_berserker", 1, 70, 190);
  (void)spot;
  run(m, 6);
  set_energy(s, hja, 80);
  CHECK(use(s, hja, "h_hja_vihar", 0, 95, 180));
  run(m, 1);
  s.world().get<sim::Combat>(hja)->order = sim::Combat::Hold;
  run(m, 200);
  std::printf("  hóvihar: %lld sebzés (10 × 8 várt)\n", static_cast<long long>(taken(s, foe) / 1000));
  CHECK(taken(s, foe) == 80000);
  run(m, 60);
  CHECK(taken(s, foe) == 80000);   // a mező elmúlt
}

TEST_CASE("képességek: determinisztikus, és mentésből folytatva ugyanaz (kapcsoló, buff, mező, idézett, kábult)") {
  auto setup = [](Match& m) {
    sim::Sim& s = *m.sim;
    const auto sh = put(s, "urs_pajzshordozo", 1, 30, 180, false);
    const auto b = put(s, "urs_berserker", 1, 40, 185, false);
    const auto hja = hero(s, "urs_hos_hjalmar", 1, 20, 190, 1, {"h_hja_vihar"});
    const auto pap = put(s, "urs_runapap", 1, 30, 195);
    put(s, "urs_berserker", 2, 110, 180, false);
    put(s, "urs_szigonyvezo", 2, 120, 185, false);
    s.step();
    use(s, sh, "pajzsfal");
    use(s, b, "verszomj");
    set_energy(s, hja, 200);
    use(s, hja, "h_hja_vihar", 0, 100, 182);
    use(s, pap, "runako", 0, 60, 190);
  };
  Match a("BUILD-ALAP"), b("BUILD-ALAP");
  setup(a);
  setup(b);
  a.run(150);
  b.run(150);
  CHECK(a.sim->hash() == b.sim->hash());
  const auto saved = a.sim->save();
  a.run(450);
  Match c("BUILD-ALAP", true, false);
  CHECK(c.sim->load(saved));
  c.sim->restore_obstacles(*c.map);
  c.run(450);
  CHECK(c.sim->hash() == a.sim->hash());
  std::printf("  képesség-állapothash (600 tick): 0x%016llx\n", static_cast<unsigned long long>(a.sim->hash()));
  CHECK(a.sim->hash() == 0xa933264baacc7c2aULL);   // minden rendszeren ugyanez
}
