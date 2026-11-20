// SPDX-License-Identifier: GPL-3.0-or-later
// Játszható szelet (M6, 9. lépés; ADR-007): győzelem és vereség, a szkriptelt ursan ellenfél, meccs-visszajátszás.
#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "match_util.hpp"
#include "render/font.hpp"
#include "script/host.hpp"
#include "sim/replay.hpp"

using namespace totu;
using namespace totu::testing;

namespace {

std::string ai_source() {
  return read_file(std::filesystem::path(TOTU_CONTENT_DIR) / "base" / "scripts" / "ai_ursan.lua");
}

int count_kind(sim::Sim& s, uint8_t owner, bool buildings) {
  int n = 0;
  s.world().pool<sim::Health>().each([&](sim::EntityId e, const sim::Health& h) {
    if (h.owner != owner || h.hp <= 0) return;
    if (buildings ? s.world().get<sim::Building>(e) != nullptr : s.world().get<sim::Unit>(e) != nullptr) ++n;
  });
  return n;
}

}  // namespace

TEST_CASE("győzelem és vereség: akinek minden épülete elpusztult, kiesik; az egyetlen megmaradt nyer") {
  Match m("BUILD-ALAP");
  sim::Sim& s = *m.sim;
  m.run(2);
  CHECK(s.winner() == 0 && !s.player(2).defeated);
  std::vector<sim::EntityId> hq2;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building&) {
    if (s.world().get<sim::Health>(e)->owner == 2) hq2.push_back(e);
  });
  CHECK(hq2.size() == 1);
  for (const auto e : hq2) s.deal_damage(0, 1, sim::kNoDef, e, 100000000, false);
  m.run(2);
  CHECK(s.player(2).defeated && !s.player(1).defeated);   // az egységei még élnek, de kiesett
  CHECK(s.winner() == 1);
  CHECK(count_kind(s, 2, false) > 0);
}

TEST_CASE("szkriptelt ellenfél: két ursan gép egymás ellen – gazdaság, építés, kiképzés, kutatás, felemelkedés, támadás") {
  Match m("ECO-ALAP");
  sim::Sim& s = *m.sim;
  script::ScriptHost ai("ai", 20261113);
  CHECK(ai.run("ai_players", "AI_PLAYERS = {1, 2}"));
  CHECK(ai.run("ai_ursan.lua", ai_source()));
  s.add_hooks(&ai);
  ai.start(s);
  m.run(8 * 60 * 30);   // 8 perc
  for (const auto& msg : ai.messages()) std::printf("  napló: %s\n", msg.c_str());
  for (uint8_t p = 1; p <= 2; ++p) {
    const auto& pl = s.player(p);
    std::printf("  %d. gép: %d épület, %d egység, %zu kutatás, irány: %s, létszám %d/%d\n", p, count_kind(s, p, true),
                count_kind(s, p, false), pl.researched.size(), pl.path.empty() ? "-" : pl.path.c_str(), pl.supply_used,
                pl.supply_cap);
    CHECK(count_kind(s, p, true) >= 5);   // a vesztes gép is épít (a pontos szám a harc kimenetétől függ)
    CHECK(pl.ascended);
  }
  CHECK(s.player(1).researched.size() + s.player(2).researched.size() >= 3);
  CHECK(count_kind(s, 1, false) + count_kind(s, 2, false) > 2 * 14 - 10 || s.winner() != 0);   // kiképzett sereg
  CHECK(std::min(count_kind(s, 1, false), count_kind(s, 2, false)) < 14);   // a támadás elérte az ellenfelet
}

TEST_CASE("meccs-visszajátszás (golden M6 alapja): a két gép meccse rögzítve és újrajátszva ugyanaz") {
  Content& c = content_once();
  sim::Replay base;
  base.seed = 20261113;
  base.ticks = 3 * 60 * 30;
  base.map = read_file(std::filesystem::path(TOTU_TEST_DATA) / "maps" / "ECO-ALAP.json");
  base.factions = {"ursan", "ursan"};
  base.weather = 3;   // hatással és pályaváltozással
  base.scripts = {{"ai", "ai_players", "AI_PLAYERS = {1, 2}"}, {"ai", "ai_ursan.lua", ai_source()}};
  const sim::Replay r = sim::record_match(base, c.terrain, *c.rules);
  CHECK(r.checkpoints.size() == 180);
  const auto bytes = sim::encode(r);
  const auto back = sim::decode(bytes);
  CHECK(back.has_value());
  if (!back) return;
  CHECK(back->factions == r.factions && back->weather == 3);
  const auto v = sim::verify(*back, &c.terrain, &*c.rules);
  CHECK(v.ok);
  std::printf("  meccs-visszajátszás: %zu ellenőrzőpont, végső hash 0x%016llx\n", v.checked,
              static_cast<unsigned long long>(r.checkpoints.back().hash));
}

TEST_CASE("betű: a játék TTF-betűje magyar ékezetekkel atlaszba sütve") {
  const auto ttf = read_file(std::filesystem::path(TOTU_ASSETS_DIR) / "fonts" / "egszakadas_rovaskonyv_text.ttf");
  render::FontBake f;
  CHECK(render::bake_font(std::vector<uint8_t>(ttf.begin(), ttf.end()), 16.0f, f));
  for (const char* s : {"Á", "é", "ő", "Ű", "A", "z", "9", "→"}) CHECK(f.find(render::utf8_codepoints(s).front()) != nullptr);
  CHECK(f.width("Fa: 120") > 20.0f && f.width("Fa: 120") < 120.0f);
  CHECK(render::utf8_codepoints("őz").size() == 2);
}

TEST_CASE("gépi szövetség (KIS-1): előírt kisnép, egyébként az `ally_pref` első kettőjéből; csarnok nélkül nem, a véletlen sem fogy") {
  {
    Match m("BUILD-ALAP");
    sim::Sim& s = *m.sim;
    for (int r = 0; r < sim::kResources; ++r) s.add_resources(1, r, 5000);
    const uint64_t h0 = s.hash();
    CHECK(s.ai_choose_ally(1).empty());   // csarnok nélkül
    CHECK(s.hash() == h0);
    s.place_building(content_once().rules->def("urs_diplomacia"), 1, 12, 9, true);
    const std::string pick = s.ai_choose_ally(1);
    std::printf("  választás: %s\n", pick.c_str());
    CHECK(pick == "pinguu" || pick == "tokla" || pick == "ghoblin");
    CHECK(s.player(1).ally == pick);
    CHECK(s.ai_choose_ally(1, "tokla").empty());   // egyszer
  }
  // a szkriptelt gép: kész csarnokkal megköti az előírt szövetséget, és a kisnép épületét is felépíti
  Match m("ECO-ALAP");
  sim::Sim& s = *m.sim;
  script::ScriptHost ai("ai", 20261113);
  CHECK(ai.run("ai_players", "AI_PLAYERS = {1}; AI_ALLY = {[1] = 'tokla'}"));
  CHECK(ai.run("ai_ursan.lua", ai_source()));
  s.add_hooks(&ai);
  ai.start(s);
  for (int r = 0; r < sim::kResources; ++r) s.add_resources(1, r, 20000);
  m.run(8 * 60 * 30);
  for (const auto& msg : ai.messages()) std::printf("  napló: %s\n", msg.c_str());
  CHECK(s.player(1).ally == "tokla");
  bool tokla_building = false;
  s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& b) {
    tokla_building = tokla_building || (s.world().get<sim::Health>(e)->owner == 1 && content_once().rules->defs[b.def].faction == "tokla");
  });
  CHECK(tokla_building);
}

TEST_CASE("adatvezérelt gép (`ai_nep.lua`): minden főnép a saját terve szerint épít, kiképez, kutat") {
  const std::string src = read_file(std::filesystem::path(TOTU_CONTENT_DIR) / "base" / "scripts" / "ai_nep.lua");
  size_t researched = 0;
  for (const auto& f : content_once().rules->factions) {
    if (f.ally || f.neutral) continue;
    Match m("ECO-ALAP", true, true, {f.id, "ursan"});
    sim::Sim& s = *m.sim;
    script::ScriptHost ai("ai", 20261113);
    CHECK(ai.run("ai_players", "AI_PLAYERS = {1}"));
    CHECK(ai.run("ai_nep.lua", src));
    s.add_hooks(&ai);
    ai.start(s);
    m.run(8 * 60 * 30);
    for (const auto& msg : ai.messages()) std::printf("  napló: %s\n", msg.c_str());
    const auto& pl = s.player(1);
    std::printf("  %s: %d épület, %d egység, %zu kutatás, felemelkedett: %d, irány: %s, szövetség: %s\n", f.id.c_str(),
                count_kind(s, 1, true), count_kind(s, 1, false), pl.researched.size(), pl.ascended ? 1 : 0,
                pl.path.empty() ? "-" : pl.path.c_str(), pl.ally.empty() ? "-" : pl.ally.c_str());
    // a kis pályán a kő 8 perc alatt elfogy; a tóásás (Huk'ucha) és a mélységi gyűjtés (Croftian) népi gépi viselkedése
    // az M9-é – itt a közös terv fut minden népen
    CHECK(count_kind(s, 1, true) >= 4);
    CHECK(count_kind(s, 1, false) >= 8);
    researched += pl.researched.size();
  }
  CHECK(researched >= 1);   // a kis pályán az arany hamar elfogy
}

TEST_CASE("Csapatok (a regi szerint): a csapattars nem ellenseg, a gyozelem a csapate, mentes es visszajatszas") {
  Content& c = content_once();
  sim::Replay base;
  base.seed = 11;
  base.ticks = 60;
  base.map = read_file(std::filesystem::path(TOTU_CONTENT_DIR) / "base" / "maps" / "het_ut.json");
  base.factions = {"ursan", "ursan", "ursan"};
  base.teams = {1, 1, 2};
  std::string err;
  auto m = sim::start_match(base, &c.terrain, &*c.rules, err);
  if (!m) std::printf("  hiba: %s\n", err.c_str());
  CHECK(m != nullptr);
  if (!m) return;
  sim::Sim& s = *m->sim;
  CHECK(s.player(1).team == 1 && s.player(2).team == 1 && s.player(3).team == 2);
  CHECK(s.allied(1, 2) && s.allied(2, 1) && !s.allied(1, 3) && !s.allied(0, 1) && s.allied(3, 3));
  CHECK(s.winner() == 0);
  // közös látótér: a csapattárs egységei mindig látszanak, az ellenfél kezdőbázisa (messze) nem
  for (int i = 0; i < 3; ++i) s.step();
  sim::EntityId of2 = sim::kNoEntity, of3 = sim::kNoEntity;
  s.world().pool<sim::Health>().each([&](sim::EntityId e, const sim::Health& h) {
    if (h.owner == 2 && of2 == sim::kNoEntity) of2 = e;
    if (h.owner == 3 && of3 == sim::kNoEntity) of3 = e;
  });
  CHECK(of2 != sim::kNoEntity && s.is_visible(1, of2) && !s.is_visible(1, of3));
  // mentés–betöltés: a csapat megmarad
  const auto saved = s.save();
  auto copy = sim::start_match(base, &c.terrain, &*c.rules, err, true);
  CHECK(copy && copy->sim->load(saved) && copy->sim->player(2).team == 1 && copy->sim->allied(1, 2));
  s.defeat_player(3);
  CHECK(s.winner() == 1 && !s.player(2).defeated);   // az 1-es csapat nyert (a 2. játékos is)
  // egyetlen csapat: nincs két oldal, nincs győztes
  base.teams = {2, 2, 2};
  auto one = sim::start_match(base, &c.terrain, &*c.rules, err);
  CHECK(one && one->sim->winner() == 0);
  // visszajátszás: a 9-es változat viszi a csapatokat, és újrajátszva ugyanaz
  base.teams = {1, 1, 2};
  const sim::Replay rec = sim::record_match(base, c.terrain, *c.rules);
  const auto dec = sim::decode(sim::encode(rec));
  CHECK(dec.has_value() && dec->teams == base.teams);
  CHECK(sim::verify(rec, &c.terrain, &*c.rules).ok);
}

TEST_CASE("Csata beallitas (a regi): a valasztott kezdohely es a kezdo nyersanyag") {
  Content& c = content_once();
  const auto map = read_file(std::filesystem::path(TOTU_CONTENT_DIR) / "base" / "maps" / "het_ut.json");
  auto start = [&](std::vector<int8_t> starts, uint8_t res) {
    sim::Replay r;
    r.seed = 3;
    r.map = map;
    r.factions = {"ursan", "ursan"};
    r.starts = std::move(starts);
    r.resources = res;
    std::string err;
    auto m = sim::start_match(r, &c.terrain, &*c.rules, err);
    if (!m) std::printf("  hiba: %s\n", err.c_str());
    return m;
  };
  auto hq_pos = [&](sim::Sim& s, uint8_t owner) {
    std::pair<int, int> out{-1, -1};
    s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& b) {
      if (s.world().get<sim::Health>(e)->owner == owner) out = {b.tx, b.ty};
    });
    return out;
  };
  auto a = start({}, 1), b = start({4, 2}, 0), d = start({0, 0}, 1);
  CHECK(a && b && !d);   // ugyanaz a kezdőhely kétszer: hiba
  if (!a || !b) return;
  CHECK(hq_pos(*a->sim, 1) != hq_pos(*b->sim, 1) && hq_pos(*a->sim, 2) != hq_pos(*b->sim, 2));
  CHECK(b->sim->player(1).resource(sim::kWood) * 10 == a->sim->player(1).resource(sim::kWood) * 6);
  auto h = start({}, 2);
  CHECK(h && h->sim->player(1).resource(sim::kWood) == a->sim->player(1).resource(sim::kWood) * 2 &&
        h->sim->player(1).resource(sim::kMeteor) == a->sim->player(1).resource(sim::kMeteor) * 2 + 150);
  const auto dec = sim::decode(sim::encode(sim::record_match([&] { sim::Replay r; r.seed = 3; r.ticks = 30; r.map = map; r.factions = {"ursan", "ursan"}; r.starts = {4, 2}; r.resources = 0; return r; }(), c.terrain, *c.rules)));
  CHECK(dec && dec->starts == std::vector<int8_t>({4, 2}) && dec->resources == 0);
}
