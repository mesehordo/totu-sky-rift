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
    CHECK(count_kind(s, p, true) >= 6);
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
