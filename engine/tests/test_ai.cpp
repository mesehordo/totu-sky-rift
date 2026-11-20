// SPDX-License-Identifier: GPL-3.0-or-later
// A gépi ellenfél próbái (M9; GEPI_ELLENFEL.md 4. pont): betöltés, AI-FOG = 0, a személyiségek különbözősége, az
// „Őrült” szint +50% gyűjtése, determinizmus mentéssel és visszajátszással, minden személyiség elakadás nélkül,
// tanulás a meccsen belül.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "client/menu.hpp"
#include "match_util.hpp"
#include "script/ai.hpp"
#include "sim/replay.hpp"

using namespace totu;
using namespace totu::testing;

namespace {

const script::AiPackage& package() {
  static const script::AiPackage p = [] {
    content::Vfs vfs;
    vfs.add_layer("totu", std::filesystem::path(TOTU_CONTENT_DIR) / "base");
    auto pkg = script::load_ai_package(vfs, "alap");
    return pkg ? *pkg : script::AiPackage{};
  }();
  return p;
}

std::string map_text(const std::string& name) {
  return read_file(std::filesystem::path(TOTU_CONTENT_DIR) / "base" / "maps" / (name + ".json"));
}

sim::AiSlot slot(uint8_t p, const std::string& diff, const std::string& pers) {
  sim::AiSlot a;
  a.player = p;
  a.difficulty = diff;
  a.personality = pers;
  return a;
}

// meccs egy csatapályán, a megadott gépi játékosokkal (a többi játékos tétlen)
struct AiMatch {
  std::optional<map::Map> map;
  std::unique_ptr<map::NavGrid> nav;
  std::unique_ptr<map::Pathfinder> pf;
  std::unique_ptr<sim::Sim> sim;
  std::vector<std::unique_ptr<script::ScriptHost>> ais;
  sim::MatchSetup setup;
  bool ok = false;
  AiMatch(const std::string& map_name, std::vector<std::string> factions, std::vector<sim::AiSlot> slots,
          uint64_t seed = 20261115, bool start = true, bool hosts = true) {
    Content& c = content_once();
    std::vector<std::string> errors;
    const auto doc = content::parse(map_text(map_name));
    if (!doc || !c.rules) return;
    map = map::Map::from_json(*doc, c.terrain, errors);
    if (!map) return;
    nav = std::make_unique<map::NavGrid>(*map, c.terrain);
    pf = std::make_unique<map::Pathfinder>(*nav);
    sim = std::make_unique<sim::Sim>(seed);
    sim->attach_navigation(pf.get());
    sim->attach_rules(&*c.rules);
    setup.factions = std::move(factions);
    setup.ai = slots;
    std::string err;
    if (start && !sim->setup_match(*map, setup, &err)) return;
    if (hosts && !slots.empty()) {
      ais = script::attach_ais(*sim, slots, {package()}, seed, start, &err);
      if (ais.empty()) {
        std::printf("  a gép nem indult: %s\n", err.c_str());
        return;
      }
    }
    ok = true;
  }
  void run(int ticks) {
    for (int i = 0; i < ticks; ++i) sim->step();
  }
  size_t errors() const {
    size_t n = 0;
    for (const auto& h : ais) n += h->errors();
    return n;
  }
  void dump_errors() const {
    for (const auto& h : ais) {
      for (const auto& m : h->messages()) {
        if (m.find("hiba") != std::string::npos) std::printf("  %s\n", m.c_str());
      }
    }
  }
};

int count_owned(sim::Sim& s, uint8_t owner, bool buildings) {
  int n = 0;
  s.world().pool<sim::Health>().each([&](sim::EntityId e, const sim::Health& h) {
    if (h.owner != owner || h.hp <= 0) return;
    if (buildings ? s.world().get<sim::Building>(e) != nullptr : s.world().get<sim::Unit>(e) != nullptr) ++n;
  });
  return n;
}

// a gép naplójából egy szám (pl. "elso_tamadas 5400"); -1, ha nincs
int64_t logged(const script::ScriptHost& h, const std::string& key) {
  for (const auto& m : h.messages()) {
    const size_t p = m.find("] " + key + " ");
    if (p != std::string::npos) return std::atoll(m.c_str() + p + key.size() + 3);
  }
  return -1;
}

struct Profile {
  int64_t first_attack = -1;
  int buildings = 0, defense = 0, expansions = 0;
};

Profile profile(AiMatch& m, uint8_t p) {
  Profile r;
  r.first_attack = logged(*m.ais[0], "elso_tamadas");
  const sim::Rules& rules = *content_once().rules;
  const std::string hq = "urs_nagyterem";
  m.sim->world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& b) {
    const sim::Health* h = m.sim->world().get<sim::Health>(e);
    if (!h || h->owner != p || h->hp <= 0) return;
    const sim::EntityDef& d = rules.defs[b.def];
    ++r.buildings;
    if (d.attack.has || d.shape == "wall") ++r.defense;
    if (d.dropoff != 0 && d.supply_provides == 0 && d.id != hq) ++r.expansions;
    if (d.id == hq) ++r.expansions;
  });
  --r.expansions;   // a kezdő főépület nem telep
  return r;
}

}  // namespace

TEST_CASE("M9 gep – betoltes: az alap AI-csomag, 6 nehézség, 11 személyiség; Game és kill nélkül") {
  content::Vfs vfs;
  vfs.add_layer("totu", std::filesystem::path(TOTU_CONTENT_DIR) / "base");
  const auto ids = script::list_ai_packages(vfs);
  CHECK(std::find(ids.begin(), ids.end(), "alap") != ids.end());
  CHECK(package().file("ai.json") != nullptr);
  CHECK(package().file("main.lua") != nullptr);
  CHECK(script::ai_difficulties().size() == 6);
  CHECK(script::ai_personalities(package()).size() == 11);
  std::string err;
  auto h = script::make_ai(package(), slot(2, "normal", "kiegyenlitett"), 1, &err);
  CHECK(h != nullptr);
  if (h) {
    CHECK(h->run("probe", "assert(Game == nil); assert(AI ~= nil); assert(AI.kill == nil); assert(AI.me() == 2)"));
    CHECK(h->run("probe2", "assert(AI.difficulty() == 'normal' and AI.personality() == 'kiegyenlitett')"));
    CHECK(!h->run("probe3", "AI.foo = 1"));   // csak olvasható
  }
  CHECK(script::make_ai(package(), slot(2, "isteni", "kiegyenlitett"), 1, &err) == nullptr);
  CHECK(script::make_ai(package(), slot(2, "normal", "nincs_ilyen"), 1, &err) == nullptr);
  for (const auto& p : script::ai_personalities(package())) {
    CHECK(script::make_ai(package(), slot(1, "kezdo", p), 1, &err) != nullptr);
  }
}

TEST_CASE("M9 gep – AIFOG: a köd mögötti ellenséges állapot és lelőhely nem változtat a gép parancsain") {
  AiMatch a("kraterfold", {"ursan", "ursan"}, {slot(2, "mester", "kiegyenlitett")});
  AiMatch b("kraterfold", {"ursan", "ursan"}, {slot(2, "mester", "kiegyenlitett")});
  CHECK(a.ok && b.ok);
  if (!a.ok || !b.ok) return;
  // a b meccsben az 1. játékos rejtett állapota más: nyersanyag, a bázis életereje
  b.sim->add_resources(1, sim::kGold, 5000);
  b.sim->add_resources(1, sim::kWood, 3000);
  b.sim->world().pool<sim::Health>().each([&](sim::EntityId, sim::Health& h) {
    if (h.owner == 1 && h.hp > 2) h.hp /= 2;
  });
  // N-02/N-03: a b meccsből eltűnik egy, a 2. játékos által fel nem derített aranylelőhely
  sim::EntityId hidden_node = sim::kNoEntity;
  int hx = 0, hy = 0;
  {
    const auto& ex = b.sim->player(2).explored;
    const int w = b.map->width();
    b.sim->world().pool<sim::ResourceNode>().each([&](sim::EntityId e, const sim::ResourceNode& r) {
      if (hidden_node != sim::kNoEntity || r.kind != sim::kGold || r.size < 2) return;
      if (!ex.empty() && ex[static_cast<size_t>(r.ty * w + r.tx)]) return;
      hidden_node = e, hx = r.tx, hy = r.ty;
    });
  }
  CHECK(hidden_node != sim::kNoEntity);
  b.sim->remove_resource(hidden_node);
  auto node_seen = [&]() {
    const auto& ex = a.sim->player(2).explored;
    return !ex.empty() && ex[static_cast<size_t>(hy * a.map->width() + hx)] != 0;
  };
  int64_t seen_at = -1;
  bool same = true;
  for (int t = 0; t < 8 * 60 * 30 && seen_at < 0; ++t) {
    a.sim->step();
    b.sim->step();
    a.sim->world().pool<sim::Health>().each([&](sim::EntityId e, const sim::Health& h) {
      if (seen_at < 0 && h.owner == 1 && h.hp > 0 && a.sim->is_visible(2, e)) seen_at = a.sim->tick();
    });
    if (seen_at < 0 && node_seen()) seen_at = a.sim->tick();   // a rejtett lelőhely helyét felderítette
    if (seen_at < 0 && a.ais[0]->command_digest() != b.ais[0]->command_digest()) same = false;
  }
  std::printf("  az első látott ellenség: %lld. tick; a gép parancsai addig: %llu\n", static_cast<long long>(seen_at),
              static_cast<unsigned long long>(a.ais[0]->command_count()));
  CHECK(same);
  CHECK(a.sim->hash() != b.sim->hash());              // a világ valóban más
  CHECK(a.ais[0]->command_count() >= 30);              // a gép közben dolgozott
  CHECK(seen_at < 0 || seen_at > 60 * 30);
  CHECK(a.errors() == 0 && b.errors() == 0);
}

TEST_CASE("M9 gep – szemelyisegek: az első támadás ideje, a védelmi arány és a terjeszkedés mérhetően különbözik") {
  std::vector<std::pair<std::string, Profile>> res;
  for (const char* p : {"gyors_roham", "kiegyenlitett", "vedekezo", "terjeszkedo"}) {
    AiMatch m("kraterfold", {"ursan", "ursan"}, {slot(2, "normal", p)});
    CHECK(m.ok);
    if (!m.ok) return;
    m.run(14 * 60 * 30);
    m.dump_errors();
    CHECK(m.errors() == 0);
    const Profile pr = profile(m, 2);
    std::printf("  %-14s első támadás: %5lld s, épület: %2d, védelmi: %d, telep: %d\n", p,
                static_cast<long long>(pr.first_attack < 0 ? -1 : pr.first_attack / 30), pr.buildings, pr.defense,
                pr.expansions);
    res.emplace_back(p, pr);
  }
  const Profile& rush = res[0].second;
  const Profile& bal = res[1].second;
  const Profile& def = res[2].second;
  const Profile& exp = res[3].second;
  auto attack_time = [](const Profile& p) { return p.first_attack < 0 ? int64_t{1} << 40 : p.first_attack; };
  CHECK(rush.first_attack > 0);
  CHECK(attack_time(rush) < attack_time(bal));
  CHECK(attack_time(bal) < attack_time(def));
  CHECK(attack_time(rush) < attack_time(exp));
  CHECK(def.defense * 100 / std::max(1, def.buildings) > rush.defense * 100 / std::max(1, rush.buildings));
  CHECK(def.defense > bal.defense);
  CHECK(exp.expansions > rush.expansions);
  CHECK(exp.expansions >= 2);
}

TEST_CASE("M9 gep – orult: a csaló szint +50%-ot gyűjt, a többi nem csal") {
  AiMatch a("kraterfold", {"ursan", "ursan"}, {slot(2, "mester", "kiegyenlitett")}, 7, true, false);
  AiMatch b("kraterfold", {"ursan", "ursan"}, {slot(2, "orult", "kiegyenlitett")}, 7, true, false);
  CHECK(a.ok && b.ok);
  if (!a.ok || !b.ok) return;
  CHECK(!a.sim->player(2).cheat_gather);
  CHECK(b.sim->player(2).cheat_gather);
  a.run(2 * 60 * 30);
  b.run(2 * 60 * 30);
  auto total = [](const sim::Sim& s, int p) {
    int64_t t = 0;
    for (int r = 0; r < sim::kResources; ++r) t += s.player(p).gathered[static_cast<size_t>(r)];
    return t;
  };
  std::printf("  gyűjtés: mester %lld, őrült %lld\n", static_cast<long long>(total(*a.sim, 2)),
              static_cast<long long>(total(*b.sim, 2)));
  CHECK(total(*a.sim, 1) == total(*b.sim, 1));
  CHECK(total(*a.sim, 2) > 0);
  CHECK(total(*b.sim, 2) * 100 >= total(*a.sim, 2) * 149);
  CHECK(total(*b.sim, 2) * 100 <= total(*a.sim, 2) * 151);
  CHECK(a.sim->hash() != b.sim->hash());
}

TEST_CASE("M9 gep – determinizmus: két gép, mentésből folytatva és visszajátszva ugyanaz") {
  const std::vector<sim::AiSlot> slots = {slot(1, "nehez", "tamado"), slot(2, "mester", "vedekezo")};
  AiMatch a("kraterfold", {"ursan", "wullof"}, slots);
  AiMatch b("kraterfold", {"ursan", "wullof"}, slots);
  CHECK(a.ok && b.ok);
  if (!a.ok || !b.ok) return;
  a.run(3 * 60 * 30);
  b.run(3 * 60 * 30);
  CHECK(a.sim->hash() == b.sim->hash());
  const auto saved = a.sim->save();
  a.run(2 * 60 * 30);
  // folytatás mentésből: új szimuláció, meccsindítás nélkül, ugyanazokkal a gépekkel (on_start nélkül)
  // a gépek beállítása magából a mentésből (betöltés előtt kiolvasva)
  const auto saved_slots = sim::Sim::saved_ai_slots(saved);
  CHECK(saved_slots == slots);
  AiMatch c("kraterfold", {"ursan", "wullof"}, saved_slots, 20261115, false, true);
  CHECK(c.ok);
  if (!c.ok) return;
  CHECK(c.sim->load(saved));
  c.sim->restore_obstacles(*c.map);
  c.run(2 * 60 * 30);
  CHECK(c.sim->hash() == a.sim->hash());
  CHECK(c.errors() == 0 && a.errors() == 0);
  // visszajátszás: a gépek beállítása és kódja a fájlban
  sim::Replay base;
  base.seed = 20261115;
  base.ticks = 90 * 30;
  base.map = map_text("kraterfold");
  base.factions = {"ursan", "wullof"};
  base.ai = slots;
  script::add_to_replay(base, package());
  const sim::Replay rec = sim::record_match(base, content_once().terrain, *content_once().rules);
  CHECK(!rec.checkpoints.empty());
  const auto dec = sim::decode(sim::encode(rec));
  CHECK(dec.has_value());
  if (!dec) return;
  CHECK(dec->ai == slots);
  const auto v = sim::verify(*dec, &content_once().terrain, &*content_once().rules);
  if (!v.ok) std::printf("  %s\n", v.error.c_str());
  CHECK(v.ok);
}

// C-105, C-106: a gépi (Lua) meccs állapothash-e rögzített – a CI minden rendszeren (Windows, Linux, macOS) ugyanezt várja
TEST_CASE("M9 gep – platformhash: ket gep 90 s meccse rogzitett hash-sel") {
  const std::vector<sim::AiSlot> slots = {slot(1, "nehez", "tamado"), slot(2, "mester", "vedekezo")};
  AiMatch a("kraterfold", {"ursan", "wullof"}, slots);
  CHECK(a.ok);
  if (!a.ok) return;
  a.run(90 * 30);
  std::printf("  gépi meccs hash (90 s): 0x%016llx\n", static_cast<unsigned long long>(a.sim->hash()));
  CHECK(a.sim->hash() == 0x2436175581eca23aULL);
  CHECK(a.errors() == 0);
}

TEST_CASE("M9 gep – mindenszemelyiseg: minden személyiség és nehézség elakadás és hiba nélkül (rövidített)") {
  const auto pers = script::ai_personalities(package());
  const auto& diffs = script::ai_difficulties();
  const char* factions[] = {"ursan", "wullof", "imperium", "croftian", "hukucha", "elves", "ooni"};
  for (size_t i = 0; i < pers.size(); ++i) {
    const std::string f1 = factions[i % 7], f2 = factions[(i + 3) % 7];
    AiMatch m("kraterfold", {f1, f2},
              {slot(1, diffs[i % diffs.size()], pers[i]), slot(2, diffs[(i + 3) % diffs.size()], pers[(i + 5) % pers.size()])},
              100 + i);
    CHECK(m.ok);
    if (!m.ok) continue;
    m.run(5 * 60 * 30);
    m.dump_errors();
    CHECK(m.errors() == 0);
    for (uint8_t p = 1; p <= 2; ++p) {
      const int b = count_owned(*m.sim, p, true), u = count_owned(*m.sim, p, false);
      std::printf("  %-14s %-7s %-9s: %2d épület, %2d egység, %llu parancs\n", pers[i].c_str(),
                  m.setup.ai[static_cast<size_t>(p - 1)].difficulty.c_str(), m.setup.factions[static_cast<size_t>(p - 1)].c_str(), b, u,
                  static_cast<unsigned long long>(m.ais[static_cast<size_t>(p - 1)]->command_count()));
      if (m.sim->player(p).defeated) continue;   // a másik gép legyőzte (nem elakadás)
      CHECK(b >= 3);
      CHECK(u >= 6);
    }
    CHECK(!m.sim->player(1).defeated || !m.sim->player(2).defeated);
  }
}

TEST_CASE("M9 gep – tanulas: a látott lovasság ellen a lándzsás súlya nő; a Könnyű nem tanul") {
  auto learned = [](const std::string& diff) {
    AiMatch m("kraterfold", {"ursan", "ursan"}, {slot(2, diff, "kiegyenlitett")});
    if (!m.ok) return -1000;
    m.run(30);
    const sim::Rules& rules = *content_once().rules;
    const auto& st = m.map->objects().starts[1];
    for (int i = 0; i < 6; ++i) {   // ellenséges lovasság a gép bázisa mellett (látja)
      m.sim->spawn_unit(rules.def("urs_kardfogu"), 1, core::Fixed::from_int(st.first - 6 + i % 3),
                        core::Fixed::from_int(st.second - 6 + i / 3));
    }
    m.run(40 * 30);
    m.ais[0]->run("probe", "log('tanult', S.learn and S.learn.shift and S.learn.shift.landzsas or 0)");
    return static_cast<int>(logged(*m.ais[0], "tanult"));
  };
  const int normal = learned("mester"), easy = learned("konnyu");
  std::printf("  a lándzsás súlyának eltolása: mester %d%%, könnyű %d%%\n", normal, easy);
  CHECK(normal > 0);
  CHECK(easy == 0);
}

TEST_CASE("M9 gep – egyeni csata: a gépi hely nehézsége és személyisége a MatchSetup-ba kerül") {
  client::SkirmishSetup s;
  s.slots[1].difficulty = "nehez";
  s.slots[1].personality = "portyazo";
  client::MapInfo mi;
  mi.path = (std::filesystem::path(TOTU_CONTENT_DIR) / "base" / "maps" / "kraterfold.json").string();
  mi.players = 2;
  client::MatchConfig c;
  std::string err;
  CHECK(client::make_match_config(s, {mi}, {{"ursan", "faction.ursan.name", {}}}, 1, &c, &err));
  CHECK(c.setup.ai.size() == 1);
  if (c.setup.ai.size() == 1) {
    CHECK(c.setup.ai[0].player == 2);
    CHECK(c.setup.ai[0].ai == "alap");
    CHECK(c.setup.ai[0].difficulty == "nehez");
    CHECK(c.setup.ai[0].personality == "portyazo");
  }
  CHECK(client::ai_personality_ids() == script::ai_personalities(package()));
  CHECK(client::ai_difficulty_ids() == script::ai_difficulties());
}
