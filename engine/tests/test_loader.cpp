// SPDX-License-Identifier: GPL-3.0-or-later
// A valódi, konvertált tartalom betöltése (M2, 3. lépés): a 22 faj minden eleme átmegy a sémán, és a számok
// determinisztikusan olvashatók. A tartalmat a tools/konverter_motor.py állítja elő a Python-játék adataiból.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "check.hpp"
#include "content/loader.hpp"
#include "content/schema.hpp"

using namespace totu::content;

TEST_CASE("séma: típus, kötelező, ismeretlen kulcs, felsorolás, határok, $ref") {
  Schema s(*parse(R"({"type": "object", "required": ["a"], "additionalProperties": false,
    "properties": {"a": {"type": "integer", "minimum": 1}, "b": {"enum": ["x", "y"]},
                   "c": {"$ref": "#/$defs/pos"}},
    "$defs": {"pos": {"type": "number", "minimum": 0, "maximum": 2.5}}})"));
  CHECK(s.validate(*parse(R"({"a": 3, "b": "x", "c": 2.5, "_megj": 1})")).empty());
  CHECK(s.validate(*parse(R"({"b": "z"})")).size() == 2);           // hiányzó a + rossz enum
  CHECK(s.validate(*parse(R"({"a": 1.5})")).size() == 1);           // nem egész
  CHECK(s.validate(*parse(R"({"a": 0})")).size() == 1);             // minimum alatt
  CHECK(s.validate(*parse(R"({"a": 1, "c": 2.6})")).size() == 1);   // maximum felett (fixpontos összevetés)
  CHECK(s.validate(*parse(R"({"a": 1, "zz": 1})")).size() == 1);    // ismeretlen kulcs
}

TEST_CASE("tartalom: az alaptartalom (21 faj) betölthető és érvényes") {
  Vfs vfs;
  vfs.add_layer("totu", std::filesystem::path(TOTU_CONTENT_DIR) / "base");
  ContentSet store;
  ContentLoader loader(vfs, std::filesystem::path(TOTU_CONTENT_DIR) / "schema");
  const LoadReport rep = loader.load(store);
  for (size_t i = 0; i < rep.errors.size() && i < 10; ++i) std::printf("  tartalomhiba: %s\n", rep.errors[i].c_str());
  CHECK(rep.errors.empty());
  auto n = [&](const char* k) { auto it = rep.count_by_kind.find(k); return it == rep.count_by_kind.end() ? 0 : it->second; };
  std::printf("  egység %d, épület %d, kutatás %d, képesség %d, tárgy %d, állapot %d, faj %d\n", n("unit"),
              n("building"), n("research"), n("ability"), n("item"), n("status"), n("faction"));
  CHECK(n("unit") == 206);
  CHECK(n("building") == 142);
  CHECK(n("research") == 164);
  CHECK(n("ability") == 271);
  CHECK(n("item") == 15);
  CHECK(n("status") == 13);
  CHECK(n("faction") == 21);  // a Micélia főnép modként (D28)
  // mintaértékek: a Csápszolga (konverter + öröklődés + determinisztikus számok)
  const Value* u = store.get("entity", "cro_csapszolga");
  CHECK(u != nullptr);
  if (u) {
    CHECK(u->find("Movement")->find("speed")->as_fixed()->raw() == 176947);   // 2.7
    CHECK(u->find("Health")->find("max")->as_int() == 40);
    CHECK(!u->find("Movement")->find("flying")->as_bool());                    // az alapsablonból
  }
  const Value* hero = store.get("entity", "cro_hos_keth");
  CHECK(hero && hero->find("Identity")->find("hero")->as_bool());        // template_hero öröklődés
  // az azonos azonosító két névtérben is élhet (épület és kutatás)
  CHECK(store.get("entity", "imp_nagy_alapozas") && store.get("research", "imp_nagy_alapozas"));
  CHECK(namespace_of("templates/units/elves.json") == "entity" && namespace_of("templates/abilities.json") == "ability");
  const Value* hq = store.get("entity", "cro_kaptar");
  CHECK(hq && hq->find("Footprint")->find("size")->items()[0].as_int() == 4);
}

TEST_CASE("tartalom: a Micélia-mod réteggel a 22 faj teljes tartalma") {
  const auto root = std::filesystem::path(TOTU_CONTENT_DIR);
  std::string err;
  auto mod = read_mod_info(root / "mods" / "micelia", &err);
  CHECK(mod && mod->id == "micelia" && mod->depends.size() == 1 && mod->depends[0] == "totu");
  Vfs vfs;
  vfs.add_layer("totu", root / "base");
  vfs.add_layer("micelia", root / "mods" / "micelia");
  ContentSet store;
  const LoadReport rep = ContentLoader(vfs, root / "schema").load(store);
  for (size_t i = 0; i < rep.errors.size() && i < 10; ++i) std::printf("  tartalomhiba: %s\n", rep.errors[i].c_str());
  CHECK(rep.errors.empty());
  auto n = [&](const char* k) { auto it = rep.count_by_kind.find(k); return it == rep.count_by_kind.end() ? 0 : it->second; };
  CHECK(n("unit") == 220 && n("building") == 152 && n("research") == 168 && n("faction") == 22);
  CHECK(store.get("faction", "faction_mycel") != nullptr);
  CHECK(store.get("faction", "faction_micelia") != nullptr);   // a kisnép-változat az alapban
}


TEST_CASE("M13 modok: felderites (alap, beepitett, felhasznaloi), sorrend, hianyzo, dupla azonosito, retegek") {
  const auto root = std::filesystem::path(TOTU_CONTENT_DIR);
  const auto user = std::filesystem::temp_directory_path() / "totu_mod_proba";
  std::filesystem::remove_all(user);
  std::filesystem::create_directories(user / "sajat");
  std::filesystem::create_directories(user / "masolat");
  std::filesystem::create_directories(user / "rossz");
  {
    std::ofstream(user / "sajat" / "mod.json") << R"({"id": "sajat", "version": "1.2.0", "depends": ["micelia"]})";
    std::ofstream(user / "masolat" / "mod.json") << R"({"id": "micelia", "version": "9.9.9"})";
    std::ofstream(user / "rossz" / "mod.json") << "{ nem json";
  }
  std::vector<std::string> warns;
  const auto avail = discover_mods(root, {user}, &warns);
  CHECK(avail.size() == 3 && avail[0].id == "totu" && avail[1].id == "micelia" && avail[1].version == "0.1.0" && avail[2].id == "sajat");
  CHECK(warns.size() == 2);   // a dupla azonosító (a beépített nyer) és a hibás mod.json
  // a függőség előbb, az alap mindig az első
  auto r = select_mods(avail, {"sajat"});
  CHECK(r.errors.empty() && r.order.size() == 3 && r.order[0].id == "totu" && r.order[1].id == "micelia" && r.order[2].id == "sajat");
  r = select_mods(avail, {});
  CHECK(r.errors.empty() && r.order.size() == 1 && r.order[0].id == "totu");
  r = select_mods(avail, {"nincsilyen"});
  CHECK(!r.errors.empty() && r.order.empty());
  // a rétegek: a mod felül
  Vfs vfs;
  add_mod_layers(vfs, root, select_mods(avail, {"micelia"}).order);
  CHECK(vfs.layers().size() == 2 && vfs.layers()[1].first == "micelia");
  Vfs base;
  add_mod_layers(base, root, {});
  CHECK(base.layers().size() == 1 && base.layers()[0].first == "totu");
  std::filesystem::remove_all(user);
}
