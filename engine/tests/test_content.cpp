// SPDX-License-Identifier: GPL-3.0-or-later
// A tartalomréteg (JSON, VFS, modok, sablonok) és az entitás–komponens tár tesztjei (M2, 2. lépés).
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "check.hpp"
#include "content/json.hpp"
#include "content/templates.hpp"
#include "content/vfs.hpp"
#include "core/hash.hpp"
#include "sim/world.hpp"

using namespace totu::content;
namespace fs = std::filesystem;

namespace {

Value J(const char* text) {
  ParseError e;
  auto v = parse(text, &e);
  if (!v) std::printf("  JSON-hiba: %s (%d:%d)\n", e.message.c_str(), e.line, e.column);
  return v ? *v : Value{};
}

void write_file(const fs::path& p, const std::string& text) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << text;
}

fs::path temp_root(const char* name) {
  fs::path p = fs::temp_directory_path() / "totu_tests" / name;
  std::error_code ec;
  fs::remove_all(p, ec);
  fs::create_directories(p);
  return p;
}

}  // namespace

TEST_CASE("JSON: elemzés, számok szövegként, kulcssorrend, visszaírás") {
  const char* src = R"({"b": 1, "a": [true, null, "x\u00e9"], "speed": 2.7, "neg": -0.35, "e": 1e-3})";
  Value v = J(src);
  CHECK(v.is_object());
  CHECK(v.members().size() == 5);
  CHECK(v.members()[0].first == "b");                       // a beolvasás sorrendje marad
  CHECK(v.find("speed")->text() == "2.7");                  // a szám szövegként
  CHECK(v.find("speed")->as_fixed()->raw() == 176947);
  CHECK(v.find("neg")->as_fixed()->raw() == -22938);
  CHECK(v.find("b")->as_int() == 1);
  CHECK(!v.find("speed")->as_int());
  CHECK(v.find("a")->items()[2].text() == "x\xc3\xa9");     // \u00e9 → UTF-8
  CHECK(dump(v) == R"({"b":1,"a":[true,null,"xé"],"speed":2.7,"neg":-0.35,"e":1e-3})");
  CHECK(*parse(dump(v)) == v);
}

TEST_CASE("JSON: hibák sor- és oszlopszámmal") {
  ParseError e;
  CHECK(!parse("{\n  \"a\": 1,\n  \"b\" 2\n}", &e));
  CHECK(e.line == 3 && e.column == 7);
  CHECK(!parse(R"({"a": 1, "a": 2})", &e));               // ismétlődő kulcs
  CHECK(!parse("[1, 2,]", &e));
  CHECK(!parse("01x", &e));
  CHECK(!parse("\"nyitott", &e));
  CHECK(parse("\xEF\xBB\xBF{}").has_value());              // BOM megengedett
}

TEST_CASE("modok: betöltési sorrend, függőség, ütközés, kör") {
  std::vector<ModInfo> av = {{"alap", "1", {}, {}, {}}, {"a", "1", {"alap"}, {}, {}},
                             {"b", "1", {"a"}, {"c"}, {}}, {"c", "1", {}, {}, {}},
                             {"x", "1", {"y"}, {}, {}}, {"y", "1", {"x"}, {}, {}}};
  auto r = resolve_mods(av, {"b"});
  CHECK(r.errors.empty());
  CHECK(r.order.size() == 3 && r.order[0].id == "alap" && r.order[1].id == "a" && r.order[2].id == "b");
  CHECK(!resolve_mods(av, {"b", "c"}).errors.empty());     // ütközés
  CHECK(!resolve_mods(av, {"x"}).errors.empty());          // kör
  CHECK(!resolve_mods(av, {"nincs"}).errors.empty());      // hiányzik
}

TEST_CASE("VFS: rétegek, takarás, unió, biztonság") {
  const fs::path root = temp_root("vfs");
  write_file(root / "base/templates/units.json", "alap");
  write_file(root / "base/templates/a.json", "a");
  write_file(root / "mod/templates/units.json", "mod");
  write_file(root / "mod/templates/z.json", "z");
  write_file(root / "mod/mod.json", R"({"id": "probamod", "version": "1.0", "depends": ["alap"]})");
  Vfs v;
  v.add_layer("alap", root / "base");
  v.add_layer("probamod", root / "mod");
  CHECK(*v.read_text("templates/units.json") == "mod");                 // a felső réteg takar
  CHECK(v.read_all("templates/units.json").size() == 2);
  auto files = v.list("templates", ".json");
  CHECK(files.size() == 3 && files[0] == "templates/a.json" && files[2] == "templates/z.json");
  CHECK(!v.read_text("../base/templates/a.json"));                       // kilépés tiltva
  CHECK(!v.exists("C:/Windows/win.ini"));
  std::string err;
  auto mi = read_mod_info(root / "mod", &err);
  CHECK(mi && mi->id == "probamod" && mi->depends.size() == 1);
}

TEST_CASE("sablonok: öröklődés, foltok, törlés, listaműveletek, hibák") {
  TemplateStore t;
  t.add_file("alap", "base/units.json", J(R"({
    "_leiras": "megjegyzés",
    "template_unit": {"Health": {"max": 100, "regen": 0}, "tags": ["unit"], "speed": 2.5},
    "template_infantry": {"inherits": "template_unit", "tags": {"+": ["infantry"]}, "Attack": {"damage": 8}},
    "urs_harcos": {"inherits": "template_infantry", "Health": {"max": 180}, "_doc": "x"},
    "kor_a": {"inherits": "kor_b"}, "kor_b": {"inherits": "kor_a"},
    "arva": {"inherits": "nincs_ilyen"}
  })"));
  const Value* u = t.get("urs_harcos");
  CHECK(u != nullptr);
  CHECK(u->find("Health")->find("max")->text() == "180");
  CHECK(u->find("Health")->find("regen")->text() == "0");               // a szülőtől
  CHECK(u->find("tags")->items().size() == 2);
  CHECK(u->find("Attack")->find("damage")->text() == "8");
  CHECK(!u->find("inherits") && !u->find("_doc"));
  // mod-folt: érték felülírása, kulcs törlése, lista módosítása
  t.add_file("probamod", "mod/units.json", J(R"({
    "urs_harcos": {"Health": {"max": 200}, "speed": 3, "Attack": null, "tags": {"-": ["unit"], "+": ["elite"]}},
    "template_unit": {"Health": {"regen": 1}}
  })"));
  u = t.get("urs_harcos");
  CHECK(u->find("Health")->find("max")->text() == "200");
  CHECK(u->find("Health")->find("regen")->text() == "1");               // a szülő foltja is érvényes
  CHECK(!u->find("Attack"));
  const auto& tags = u->find("tags")->items();
  CHECK(tags.size() == 2 && tags[0].text() == "infantry" && tags[1].text() == "elite");
  CHECK(t.get("kor_a") == nullptr);
  CHECK(t.get("arva") == nullptr);
  CHECK(t.errors().size() >= 2);
  // egy második mod listafoltja az elsőé után: sorrendhelyes összevonás
  t.add_file("masodikmod", "mod2/units.json", J(R"({"urs_harcos": {"tags": {"-": ["elite"], "+": ["veteran"]}}})"));
  u = t.get("urs_harcos");
  const auto& tags2 = u->find("tags")->items();
  CHECK(tags2.size() == 2 && tags2[0].text() == "infantry" && tags2[1].text() == "veteran");
  CHECK(!u->find("Attack"));                                            // az első mod törlése megmaradt
  t.add_file("probamod", "mod/masik.json", J(R"({"urs_harcos": {"speed": 4}})"));
  CHECK(t.errors().back().find("ebben a rétegben") != std::string::npos);  // azonos réteg kétszer
}

TEST_CASE("entitás–komponens tár: rendezett bejárás, késleltetett törlés") {
  struct Pos { int x = 0; };
  struct Hp { int v = 0; };
  totu::sim::World w;
  w.register_component<Pos>("Pos");
  w.register_component<Hp>("Hp");
  const auto a = w.create(), b = w.create(), c = w.create();
  w.add(c, Pos{3});
  w.add(a, Pos{1});
  w.add(b, Pos{2});
  w.add(b, Hp{50});
  std::vector<totu::sim::EntityId> order;
  w.pool<Pos>().each([&](totu::sim::EntityId id, Pos&) { order.push_back(id); });
  CHECK(order.size() == 3 && order[0] == a && order[1] == b && order[2] == c);   // azonosító szerint
  w.destroy(b);
  CHECK(w.alive(b) && w.get<Pos>(b) != nullptr);          // a tick végéig él
  w.flush();
  CHECK(!w.alive(b) && w.get<Pos>(b) == nullptr && w.get<Hp>(b) == nullptr);
  CHECK(w.entity_count() == 2);
  const auto d = w.create();
  CHECK(d == 4);                                          // az azonosító nem használódik újra
}
