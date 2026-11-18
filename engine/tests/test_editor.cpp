// SPDX-License-Identifier: GPL-3.0-or-later
// M11 szerkesztő-modell: kanonikus JSON, változatlan visszamentés, műveletek, visszavonás, ellenőrzés, előnézet.
#include <cstdio>
#include <filesystem>
#include <string>

#include "check.hpp"
#include "client/editor_doc.hpp"
#include "client/editor_ui.hpp"
#include "client/data_editor.hpp"
#include "client/trigger_editor.hpp"
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

TEST_CASE("szerkeszto: trigger-szerkeszto - uj trigger, esemeny, feltetel, akcio, parameter, torles, visszavonas") {
  client::EditorDoc d;
  std::string err;
  CHECK(d.create(tmpdir("trigger"), base() + "/maps/kraterfold.json", "tr", &err));
  CHECK(d.add_region("kapu", 30, 26, 8, 8));
  CHECK(d.place("urs_hos_ragna", 1, 20, 20, "hos") >= 0);
  client::TriggerEditor te;
  te.init(&*content_once().rules);
  te.open = true;
  te.layout(1280, 720, d);
  auto click = [&](const client::Rect& r) {
    CHECK(te.mouse_down(d, static_cast<float>(r.x + 2), static_cast<float>(r.y + 2), false));
    te.layout(1280, 720, d);
  };
  auto pick = [&](const std::string& opt) {   // a felugró listában gépeléssel szűr, Enterrel választ
    CHECK(te.popup.open);
    te.text_input(opt);
    CHECK(te.key(d, client::TriggerEditor::kEnter));
    te.layout(1280, 720, d);
  };
  click(te.buttons[0].r);   // új trigger (esemény: start)
  CHECK(te.trigger == 0 && te.list_items.size() == 1 && te.list_items[0].label == "t1");
  // esemény: belépés régióba
  click(te.rows[0].r);      // az esemény fejléce
  pick("enter_region");
  const content::Value& t0 = d.mission().find("triggers")->items()[0];
  CHECK(t0.find("event")->find("type")->text() == "enter_region" && t0.find("event")->find("region")->text() == "kapu");
  // feltétel: a hős él
  for (const auto& r : te.rows) if (r.index < 0 && r.section == 1) { click(r.r); break; }
  pick("alive");
  CHECK(d.mission().find("triggers")->items()[0].find("conditions")->items()[0].find("unit")->text() == "hos");
  // akció: üzenet, a szöveget beírva
  for (const auto& r : te.rows) if (r.index < 0 && r.section == 2) { click(r.r); break; }
  pick("message");
  CHECK(te.section == 2 && te.line == 0 && !te.param_buttons.empty());
  click(te.param_buttons[0].r);   // text
  CHECK(te.field.open);
  te.field.text.clear();
  te.text_input("Szia, kapu!");
  CHECK(te.key(d, client::TriggerEditor::kEnter));
  te.layout(1280, 720, d);
  CHECK(d.mission().find("triggers")->items()[0].find("actions")->items()[0].find("text")->text() == "Szia, kapu!");
  // szám mező: nem szám → hiba, nem kerül be
  click(te.param_buttons[1].r);   // ticks
  te.text_input("abc");
  te.key(d, client::TriggerEditor::kEnter);
  CHECK(te.status == "editor.trig.not_number");
  // a kész küldetés érvényes, és a motor futtatja
  CHECK(d.validate(&*content_once().rules, &content_once().terrain).empty());
  // az akció törlése és visszavonása
  te.layout(1280, 720, d);
  CHECK(te.key(d, client::TriggerEditor::kDelete));
  CHECK(d.mission().find("triggers")->items()[0].find("actions")->items().empty());
  CHECK(d.undo());
  CHECK(d.mission().find("triggers")->items()[0].find("actions")->items().size() == 1);
  // ismétlés kapcsoló, majd a trigger törlése
  te.layout(1280, 720, d);
  click(te.buttons[3].r);
  CHECK(d.mission().find("triggers")->items()[0].find("repeat")->as_bool());
  click(te.buttons[1].r);
  CHECK(d.mission().find("triggers")->items().empty());
  // mondatszerű leírás
  const auto ev = content::parse(R"({"type": "enter_region", "region": "kapu", "owner": 1})");
  CHECK(client::TriggerEditor::describe(0, *ev, "Belépés") == "Belépés (region = kapu, owner = 1)");
}

TEST_CASE("szerkeszto: adatszerkeszto - cel, valtozo, parbeszed valaszokkal, atvezeto lepesekkel") {
  client::EditorDoc d;
  std::string err;
  CHECK(d.create(tmpdir("adat"), base() + "/maps/kraterfold.json", "adat", &err));
  CHECK(d.add_point("kp", 20, 20, true, 1.5));
  client::DataEditor de;
  de.init(&*content_once().rules);
  de.open = true;
  auto relayout = [&] { de.layout(1280, 720, d); };
  auto click = [&](const client::Rect& r) {
    CHECK(de.mouse_down(d, static_cast<float>(r.x + 2), static_cast<float>(r.y + 2), false));
    relayout();
  };
  auto tool = [&](int id) {
    for (const auto& b : de.tools) if (b.id == id) { click(b.r); return; }
    CHECK(false);
  };
  auto type_into = [&](int field_index, const std::string& text) {
    click(de.fields[static_cast<size_t>(field_index)].r);
    CHECK(de.field.open);
    de.field.text.clear();
    de.text_input(text);
    de.key(d, client::TriggerEditor::kEnter);
    relayout();
  };
  relayout();
  // célok
  tool(11);
  CHECK(de.sel1 == 0 && d.mission().find("objectives")->items().size() == 1);
  type_into(2, "mission.uj.cel");
  CHECK(d.mission().find("objectives")->items()[0].find("text")->text() == "mission.uj.cel");
  click(de.fields[1].r);   // kind: felugró lista
  de.text_input("second");
  de.key(d, client::TriggerEditor::kEnter);
  relayout();
  CHECK(d.mission().find("objectives")->items()[0].find("kind")->text() == "secondary");
  // változók
  click(de.tabs[client::DataEditor::kVariables].r);
  tool(11);
  type_into(0, "arany_jutalom");
  type_into(1, "250");
  CHECK(d.mission().find("variables")->find("arany_jutalom") && d.mission().find("variables")->find("arany_jutalom")->text() == "250");
  // párbeszéd: két csomópont, válasz a másodikra
  click(de.tabs[client::DataEditor::kDialogues].r);
  tool(11);
  type_into(0, "bevezeto");
  tool(21);
  CHECK(de.sel2 == 1);
  click(de.list2[0].r);
  relayout();
  for (const auto& b : de.list3) if (b.id == 31) { click(b.r); break; }   // új válasz az első csomópontra
  CHECK(de.sel3 == 0);
  click(de.fields[1].r);   // next
  de.text_input("n2");
  de.key(d, client::TriggerEditor::kEnter);
  relayout();
  const content::Value* bev = d.mission().find("dialogues")->find("bevezeto");
  CHECK(bev && bev->find("nodes")->items().size() == 2);
  CHECK(bev->find("nodes")->items()[0].find("choices")->items()[0].find("next")->text() == "n2");
  // átvezető: új lépés, a típusa kamera, a pontja kp
  click(de.tabs[client::DataEditor::kCutscenes].r);
  tool(11);
  tool(21);
  CHECK(de.sel2 == 1);
  click(de.fields[1].r);   // type
  de.text_input("camera");
  de.key(d, client::TriggerEditor::kEnter);
  relayout();
  click(de.fields[2].r);   // point
  de.text_input("kp");
  de.key(d, client::TriggerEditor::kEnter);
  relayout();
  const content::Value* cut = d.mission().find("cutscenes")->find("atvezeto1");
  CHECK(cut && cut->find("steps")->items()[1].find("type")->text() == "camera" && cut->find("steps")->items()[1].find("point")->text() == "kp");
  CHECK(cut->find("steps")->items()[1].find("t")->text() == "30");
  // az egész érvényes, és mentve–betöltve ugyanaz
  const auto errs = d.validate(&*content_once().rules, &content_once().terrain);
  for (const auto& e : errs) std::printf("  %s\n", e.c_str());
  CHECK(errs.empty());
  CHECK(d.save(&err));
  client::EditorDoc e;
  CHECK(e.open(d.dir(), base() + "/maps", &err) && e.mission() == d.mission());
  // törlés és visszavonás
  tool(12);
  CHECK(!d.mission().find("cutscenes")->find("atvezeto1"));
  CHECK(d.undo() && d.mission().find("cutscenes")->find("atvezeto1"));
}

TEST_CASE("szerkeszto: objektumok ful - szarmaztatott egyseg, ertekek, ervenyes kuldetes, lerakhato") {
  client::EditorDoc d;
  std::string err;
  CHECK(d.create(tmpdir("objektum"), base() + "/maps/kraterfold.json", "obj", &err));
  client::DataEditor de;
  de.init(&*content_once().rules);
  de.open = true;
  auto relayout = [&] { de.layout(1280, 720, d); };
  auto click = [&](const client::Rect& r) {
    CHECK(de.mouse_down(d, static_cast<float>(r.x + 2), static_cast<float>(r.y + 2), false));
    relayout();
  };
  auto field_index = [&](const std::string& key) {
    for (size_t i = 0; i < de.fields.size(); ++i) if (de.fields[i].label.rfind(key + ": ", 0) == 0) return static_cast<int>(i);
    return -1;
  };
  auto type_into = [&](const std::string& key, const std::string& text) {
    const int i = field_index(key);
    CHECK(i >= 0);
    if (i < 0) return;
    click(de.fields[static_cast<size_t>(i)].r);
    de.field.text.clear();
    de.text_input(text);
    de.key(d, client::TriggerEditor::kEnter);
    relayout();
  };
  relayout();
  click(de.tabs[client::DataEditor::kObjects].r);
  for (const auto& b : de.tools) if (b.id == 11) { click(b.r); break; }
  CHECK(de.sel1 == 0);
  type_into("name", "urs_vasfarkas");
  // az alap: a felugró listából
  click(de.fields[static_cast<size_t>(field_index("base"))].r);
  de.text_input("urs_berserker");
  de.key(d, client::TriggerEditor::kEnter);
  relayout();
  type_into("hp", "1500");
  type_into("speed", "2.5");
  type_into("cost.gold", "90");
  type_into("damage", "nem szam");   // tizedes mező: elutasítva
  CHECK(de.status == "editor.trig.not_number");
  const content::Value* o = d.mission().find("objects")->find("urs_vasfarkas");
  CHECK(o != nullptr);
  if (!o) return;
  CHECK(o && o->find("base")->text() == "urs_berserker" && o->find("hp")->text() == "1500" && o->find("speed")->text() == "2.5");
  CHECK(o->find("cost") && o->find("cost")->find("gold")->text() == "90" && !o->find("damage"));
  // a származtatott egység lerakható, a küldetés érvényes, és a meccs a saját szabályaival indul
  CHECK(d.place("urs_vasfarkas", 1, 20, 20, "vas") >= 0);
  const auto errs = d.validate(&*content_once().rules, &content_once().terrain);
  for (const auto& e : errs) std::printf("  %s\n", e.c_str());
  CHECK(errs.empty());
  std::string serr;
  auto m = sim::start_match(d.replay(4), &content_once().terrain, &*content_once().rules, serr);
  CHECK(m && m->rules && m->mission->named("vas") && m->mission->named("vas")->size() == 1);
  if (m && m->rules) CHECK(m->rules->defs[m->rules->def("urs_vasfarkas")].cost[sim::kGold] == 90);
}
