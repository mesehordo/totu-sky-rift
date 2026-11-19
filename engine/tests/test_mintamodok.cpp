// SPDX-License-Identifier: GPL-3.0-or-later
// M13: a mintamodok (content/mods/minta_*) – az SDK leírásának példái. Mindegyik hiba nélkül betölt az alap fölé, és
// azt teszi, amit a leírás ígér: új egység (örökléssel, meglévő képpel, képezhető), új képesség (folttal egy meglévő
// egységre), küldetés a modban.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "match_util.hpp"
#include "sim/mission.hpp"
#include "script/host.hpp"
#include "script/victory.hpp"
#include "sim/replay.hpp"

using namespace totu;

namespace {

struct ModRules {
  content::ContentSet set;
  map::TerrainTable terrain;
  std::optional<sim::Rules> rules;
  std::vector<std::string> errors;
  explicit ModRules(const std::vector<std::string>& mods) {
    const auto root = std::filesystem::path(TOTU_CONTENT_DIR);
    const auto avail = content::discover_mods(root, {});
    const auto order = content::select_mods(avail, mods);
    errors = order.errors;
    content::Vfs vfs;
    content::add_mod_layers(vfs, root, order.order);
    const auto rep = content::ContentLoader(vfs, root / "schema").load(set);
    errors.insert(errors.end(), rep.errors.begin(), rep.errors.end());
    terrain.load(set, errors);
    const auto game = content::parse(testing::read_file(root / "base" / "game.json"));
    if (game) rules = sim::Rules::load(*game, set, errors);
    for (size_t i = 0; i < errors.size() && i < 5; ++i) std::printf("  hiba: %s\n", errors[i].c_str());
  }
};

}  // namespace

TEST_CASE("M13 mintamodok: uj egyseg (oroklessel, meglevo keppel, a Harcosok hazaban kepezheto)") {
  ModRules m({"minta_egyseg"});
  CHECK(m.errors.empty() && m.rules.has_value());
  if (!m.rules) return;
  const uint16_t u = m.rules->def("urs_minta_vertes"), base = m.rules->def("urs_pajzshordozo"), house = m.rules->def("urs_harcosok_haza");
  CHECK(u != sim::kNoDef && base != sim::kNoDef && house != sim::kNoDef);
  if (u == sim::kNoDef || house == sim::kNoDef) return;
  const auto& d = m.rules->defs[u];
  CHECK(d.sprite == "urs_pajzshordozo" && d.faction == "ursan");
  CHECK(d.speed == m.rules->defs[base].speed);   // öröklött
  const auto& p = m.rules->defs[house].produces;
  CHECK(std::find(p.begin(), p.end(), u) != p.end());
}

TEST_CASE("M13 mintamodok: uj kepesseg folttal a Berserkerre; a ket mod egyutt is betolt") {
  ModRules m({"minta_kepesseg"});
  CHECK(m.errors.empty() && m.rules.has_value());
  if (!m.rules) return;
  const int ab = m.rules->ability_index("minta_harci_kialtas");
  const uint16_t b = m.rules->def("urs_berserker");
  CHECK(ab >= 0 && b != sim::kNoDef);
  if (ab < 0 || b == sim::kNoDef) return;
  const auto& l = m.rules->defs[b].abilities;
  CHECK(std::find(l.begin(), l.end(), static_cast<uint16_t>(ab)) != l.end());
  CHECK(m.rules->abilities[static_cast<size_t>(ab)].type == sim::AbilityDef::Type::SelfBuff);
  ModRules both({"minta_egyseg", "minta_kepesseg"});
  CHECK(both.errors.empty() && both.rules.has_value());
}

TEST_CASE("M13 mintamodok: a kuldetes a modban betoltheto es ervenyes") {
  const auto root = std::filesystem::path(TOTU_CONTENT_DIR);
  std::string err;
  auto desc = sim::load_mission_package((root / "mods" / "minta_kuldetes" / "missions" / "minta").string(), (root / "base" / "maps").string(), 3, &err);
  if (!desc) std::printf("  hiba: %s\n", err.c_str());
  CHECK(desc.has_value());
  ModRules m({"minta_kuldetes"});
  CHECK(m.errors.empty() && m.rules.has_value());
  if (!m.rules) return;
  std::vector<std::string> errs;
  const auto def = sim::MissionDef::parse(testing::read_file(root / "mods" / "minta_kuldetes" / "missions" / "minta" / "mission.json"), &*m.rules, errs);
  for (const auto& e : errs) std::printf("  hiba: %s\n", e.c_str());
  CHECK(def.has_value() && errs.empty());
  if (def) CHECK(def->objectives.size() == 2 && def->objectives[0].at == "kapu");
  // a küldetés a modok tartalmával indul: a minta_egyseg (függőség) egysége is ott van a pályán
  if (!desc) return;
  std::string serr;
  auto match = sim::start_match(*desc, &m.terrain, &*m.rules, serr);
  if (!match) std::printf("  hiba: %s\n", serr.c_str());
  CHECK(match != nullptr);
  if (!match) return;
  const uint16_t vertes = m.rules->def("urs_minta_vertes");
  bool found = false;
  match->sim->world().pool<sim::Unit>().each([&](sim::EntityId, const sim::Unit& u) { found = found || u.def == vertes; });
  CHECK(vertes != sim::kNoDef && found);
}

TEST_CASE("M13 kuldetes palyaja a mod palyamappajabol") {
  const auto root = std::filesystem::path(TOTU_CONTENT_DIR);
  const auto tmp = std::filesystem::temp_directory_path() / "totu_modpalya";
  std::filesystem::remove_all(tmp);
  std::filesystem::create_directories(tmp / "mod" / "maps");
  std::filesystem::create_directories(tmp / "kuld");
  std::filesystem::copy_file(root / "base" / "maps" / "kraterfold.json", tmp / "mod" / "maps" / "mod_palya.json");
  std::string text = testing::read_file(root / "mods" / "minta_kuldetes" / "missions" / "minta" / "mission.json");
  const auto at = text.find("kraterfold.json");
  CHECK(at != std::string::npos);
  text.replace(at, std::string("kraterfold.json").size(), "mod_palya.json");
  { std::ofstream(tmp / "kuld" / "mission.json", std::ios::binary) << text; }
  std::string err;
  CHECK(!sim::load_mission_package((tmp / "kuld").string(), (root / "base" / "maps").string(), 3, &err).has_value());   // a modmappa nélkül nincs meg
  CHECK(sim::load_mission_package((tmp / "kuld").string(), (root / "base" / "maps").string(), 3, &err, {(tmp / "mod" / "maps").string()}).has_value());
  std::filesystem::remove_all(tmp);
}

TEST_CASE("M13 gyozelmi feltetel modbol: a Kincsvadaszat a modban felfedezheto es betoltheto") {
  const auto root = std::filesystem::path(TOTU_CONTENT_DIR);
  const auto order = content::select_mods(content::discover_mods(root, {}), {"minta_gyozelem"});
  CHECK(order.errors.empty());
  content::Vfs vfs;
  content::add_mod_layers(vfs, root, order.order);
  const auto list = script::list_victories(vfs);
  const auto it = std::find_if(list.begin(), list.end(), [](const script::VictoryInfo& v) { return v.id == "kincsvadaszat"; });
  CHECK(it != list.end());
  if (it != list.end()) CHECK(it->label("hu") == "Kincsvadászat" && it->label("en") == "Treasure hunt" && it->label("de") == "Treasure hunt");
  CHECK(script::load_victory(vfs, "kincsvadaszat").has_value());
  CHECK(!script::load_victory(vfs, "nincs_ilyen").has_value());
  CHECK(!script::load_victory(vfs, "../kincsvadaszat").has_value());
  content::Vfs base_only;   // a mod nélkül nincs ott
  content::add_mod_layers(base_only, root, content::select_mods(content::discover_mods(root, {}), {}).order);
  CHECK(!script::load_victory(base_only, "kincsvadaszat").has_value());
}

TEST_CASE("M13 gyozelmi szkript: a Victory tabla dont, determinisztikus, a visszajatszasban is") {
  ModRules m({"minta_gyozelem"});
  CHECK(m.errors.empty() && m.rules.has_value());
  if (!m.rules) return;
  const auto root = std::filesystem::path(TOTU_CONTENT_DIR);
  sim::Replay base;
  base.seed = 7;
  base.ticks = 150;
  base.map = testing::read_file(root / "base" / "maps" / "kraterfold.json");
  base.factions = {"ursan", "ursan"};
  // saját győzelmi szkript: a 2. másodpercben a 2. játékos nyer (a hódítás szabálya kikapcsolva)
  script::VictoryPackage pkg{{"proba", {}, {}},
                             "function on_start() Victory.conquest(false) end\n"
                             "function on_tick(t) if t == 60 then Victory.win(2); Victory.status('vege') end end\n"};
  script::add_to_replay(base, pkg);
  CHECK(base.victory == 4);
  const sim::Replay rec = sim::record_match(base, m.terrain, *m.rules);
  const auto v = sim::verify(rec, &m.terrain, &*m.rules);
  if (!v.ok) std::printf("  hiba: %s\n", v.error.c_str());
  CHECK(v.ok);
  const auto dec = sim::decode(sim::encode(rec));
  CHECK(dec.has_value() && dec->victory == 4 && dec->scripts.size() == 1);
  std::string err;
  auto match = sim::start_match(base, &m.terrain, &*m.rules, err);
  CHECK(match != nullptr);
  if (!match) return;
  CHECK(match->sim->match_mode().victory == 4 && !match->sim->conquest());
  for (int i = 0; i < 59; ++i) match->sim->step();
  CHECK(match->sim->winner() == 0);
  for (int i = 0; i < 3; ++i) match->sim->step();
  CHECK(match->sim->winner() == 2 && match->sim->player(1).defeated);
  // a Kincsvadászat: indul, és kiírja az állást
  std::string verr;
  content::Vfs vfs;
  content::add_mod_layers(vfs, root, content::select_mods(content::discover_mods(root, {}), {"minta_gyozelem"}).order);
  auto kincs = script::load_victory(vfs, "kincsvadaszat", &verr);
  CHECK(kincs.has_value());
  if (!kincs) return;
  sim::Replay b2 = base;
  b2.scripts.clear();
  script::add_to_replay(b2, *kincs);
  auto m2 = sim::start_match(b2, &m.terrain, &*m.rules, err);
  CHECK(m2 != nullptr);
  if (!m2) return;
  for (int i = 0; i < 31; ++i) m2->sim->step();
  const script::ScriptHost* vh = nullptr;
  for (const auto& h : m2->hosts) {
    if (h->name() == "victory") vh = h.get();
  }
  CHECK(vh && vh->errors() == 0 && vh->victory_status().rfind("Kincs:", 0) == 0 && m2->sim->winner() == 0 && m2->sim->conquest());
  if (vh && vh->errors()) std::printf("  hiba: %s\n", vh->messages().back().c_str());
}
