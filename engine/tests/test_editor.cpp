// SPDX-License-Identifier: GPL-3.0-or-later
// M11 szerkesztő-modell: kanonikus JSON, változatlan visszamentés, műveletek, visszavonás, ellenőrzés, előnézet.
#include <cstdio>
#include <filesystem>
#include <string>

#include "check.hpp"
#include "client/editor_doc.hpp"
#include "client/editor_ui.hpp"
#include "match_util.hpp"
#include "sim/mission.hpp"

using namespace totu;
using namespace totu::testing;

namespace {

std::string base() { return (std::filesystem::path(TOTU_CONTENT_DIR) / "base").string(); }
std::string tmpdir(const char* name) {
  const auto p = std::filesystem::temp_directory_path() / "totu_szerkeszto" / name;
  std::error_code ec;
  std::filesystem::remove_all(p, ec);
  return p.string();
}

}  // namespace

TEST_CASE("szerkeszto: kanonikus JSON - kiiras, beolvasas, kiiras ugyanaz") {
  const auto v = content::parse(read_file(std::filesystem::path(base()) / "missions" / "proba" / "mission.json"));
  CHECK(v.has_value());
  if (!v) return;
  const std::string a = content::dump_pretty(*v);
  const auto w = content::parse(a);
  CHECK(w.has_value() && *w == *v);
  CHECK(w && content::dump_pretty(*w) == a);
  const auto s = content::parse(R"({"a": "x,y: \"z\"", "b": [1, 2.5, -3], "c": {"d": true, "e": null}})");
  CHECK(s && content::parse(content::dump_pretty(*s)) == s);
}

TEST_CASE("szerkeszto: megnyitas, mentes, ujranyitas - bajtra ugyanaz") {
  client::EditorDoc d;
  std::string err;
  CHECK(d.open(base() + "/missions/proba", base() + "/maps", &err));
  const std::string out = tmpdir("visszamentes");
  CHECK(d.save_as(out, &err));
  client::EditorDoc e;
  CHECK(e.open(out, base() + "/maps", &err));
  CHECK(e.mission_text() == d.mission_text() && e.map_text() == d.map_text());
  CHECK(e.mission() == d.mission());
  CHECK(e.validate(&*content_once().rules, &content_once().terrain).empty());
  const std::string m1 = read_file(std::filesystem::path(out) / "mission.json");
  CHECK(e.save(&err));
  CHECK(read_file(std::filesystem::path(out) / "mission.json") == m1);   // változatlanul visszamenti
}

TEST_CASE("szerkeszto: terep- es faecset, egyseg, regio, pont, visszavonas, ellenorzes") {
  client::EditorDoc d;
  std::string err;
  CHECK(d.open(base() + "/missions/proba", base() + "/maps", &err));
  const std::string before = d.map_text();
  CHECK(d.paint(30, 30, 2, 'w'));   // víz
  CHECK(d.map_text() != before);
  CHECK(!d.paint(30, 30, 2, 'w'));  // nincs változás: nincs visszavonási pont
  CHECK(d.undo() && d.map_text() == before);
  CHECK(d.redo() && d.map_text() != before);
  CHECK(d.paint(10, 60, 1, 'T'));   // fa
  const int u = d.place("urs_berserker", 1, 25.5, 24, "uj");
  CHECK(u >= 0 && d.unit_at(25.4, 24.1, 0.5) == u);
  CHECK(d.move_unit(u, 26, 25) && d.unit_at(26, 25, 0.1) == u);
  CHECK(d.add_region("uj_regio", 10, 10, 4, 4));
  CHECK(!d.add_region("rossz", 86, 86, 5, 5));   // a pályán kívül
  CHECK(d.add_point("uj_kamera", 30, 30, true, 1.4));
  CHECK(d.validate(&*content_once().rules, &content_once().terrain).empty());
  // a hivatkozott régió törlése: az ellenőrzés jelzi
  CHECK(d.remove_named("regions", "kapu"));
  const auto errs = d.validate(&*content_once().rules, &content_once().terrain);
  CHECK(!errs.empty());
  for (const auto& e : errs) std::printf("  várt hiba: %s\n", e.c_str());
  CHECK(d.undo());
  CHECK(d.validate(&*content_once().rules, &content_once().terrain).empty());
  // hibás rész nem kerül be
  CHECK(!d.set_section("triggers", "{\"nem\": \"tömb\"}", &err));
  CHECK(!d.set_section("triggers", "[{", &err));
  CHECK(d.set_section("variables", "{\"a\": 1}", &err));
  // az előnézet meccse felépül, a lerakott egység benne van
  std::string serr;
  auto m = sim::start_match(d.replay(5), &content_once().terrain, &*content_once().rules, serr);
  CHECK(m && m->mission && m->mission->named("uj") && m->mission->named("uj")->size() == 1);
  if (!m) std::printf("  %s\n", serr.c_str());
}

TEST_CASE("szerkeszto: uj kuldetes egy palyara, mentes es megnyitas") {
  client::EditorDoc d;
  std::string err;
  const std::string out = tmpdir("uj");
  CHECK(d.create(out, base() + "/maps/kraterfold.json", "uj_kuldetes", &err));
  CHECK(d.place("urs_hos_ragna", 1, 20, 20, "hos") >= 0);
  CHECK(d.set_section("objectives", R"([{"id": "o1", "text": "x"}])", &err));
  CHECK(d.set_section("triggers", R"([{"id": "t1", "event": {"type": "unit_dies", "unit": "hos"}, "actions": [{"type": "lose"}]}])", &err));
  CHECK(d.validate(&*content_once().rules, &content_once().terrain).empty());
  CHECK(d.save(&err) && !d.dirty());
  client::EditorDoc e;
  CHECK(e.open(out, base() + "/maps", &err) && e.mission() == d.mission());
  auto desc = sim::load_mission_package(out, base() + "/maps", 3, &err);
  CHECK(desc.has_value());
}

TEST_CASE("szerkeszto: felulet - eszkozvaltas, ecset huzassal, egyseg lerakasa, regio huzasa, visszavonas") {
  client::EditorDoc d;
  std::string err;
  CHECK(d.open(base() + "/missions/proba", base() + "/maps", &err));
  client::EditorUi ui;
  ui.init(content_once().terrain, *content_once().rules);
  ui.sync(d);
  ui.layout(1280, 720);
  CHECK(!ui.terrains.empty() && ui.tool_buttons.size() == 7);
  // eszköz: régió (a gombjára kattintva)
  const auto& rb = ui.tool_buttons[static_cast<size_t>(client::EditorUi::Tool::Regions)].r;
  CHECK(ui.mouse_down(d, static_cast<float>(rb.x + 2), static_cast<float>(rb.y + 2), -1, -1, false));
  CHECK(ui.tool == client::EditorUi::Tool::Regions);
  const int rev = ui.revision;
  CHECK(ui.mouse_down(d, 600, 300, 10.2, 10.7, false));
  ui.mouse_move(d, 620, 320, 13.9, 12.1);
  ui.mouse_up(d, 13.9, 12.1);
  CHECK(ui.revision == rev + 1 && ui.status == "regio1");
  // terep-ecset húzással: egy húzás több változás, de mind visszavonható
  ui.key(d, '2', false);
  CHECK(ui.tool == client::EditorUi::Tool::Terrain);
  ui.layout(1280, 720);
  const std::string map0 = d.map_text();
  ui.mouse_down(d, 600, 300, 60, 20, false);
  ui.mouse_move(d, 610, 300, 63, 20);
  ui.mouse_up(d, 63, 20);
  CHECK(d.map_text() != map0);
  // egység: a 2. játékos népének palettája
  ui.key(d, '4', false);
  ui.layout(1280, 720);
  const auto& ob = ui.owner_buttons[2].r;
  ui.mouse_down(d, static_cast<float>(ob.x + 1), static_cast<float>(ob.y + 1), -1, -1, false);
  const auto us = ui.units_of_owner();
  CHECK(!us.empty() && us.front().first.rfind("wul_", 0) == 0);
  ui.layout(1280, 720);
  ui.mouse_down(d, 600, 300, 40.3, 40.6, false);
  CHECK(ui.selected >= 0);
  const int placed = ui.selected;
  CHECK(ui.key(d, 127, false));   // Del: a kijelölt törlése
  CHECK(d.unit_at(40.5, 40.5, 0.2) == -1);
  CHECK(ui.key(d, 'z', true) && d.unit_at(40.5, 40.5, 0.2) == placed);
  CHECK(d.validate(&*content_once().rules, &content_once().terrain).empty());
}
