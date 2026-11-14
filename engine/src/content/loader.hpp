// SPDX-License-Identifier: GPL-3.0-or-later
// Tartalombetöltő (ADR-005): rétegezett fájlrendszer → sablontárak → séma-ellenőrzés. Minden réteg `templates/`
// mappájának minden JSON-fájlja sablonfájl. Az azonosítók FAJTÁNKÉNTI névtérben élnek (a mai adatokban ugyanaz az
// azonosító lehet épület és kutatás is): `entity` (units/, buildings/, common.json), `research` (researches/),
// `ability` (abilities.json), `item`, `status`, `faction`. A konkrét (nem `template_` kezdetű) sablonok a feloldás
// után a fajtájuk sémáján (`schema/entity.schema.json`, `schema/other.schema.json`) mennek át.
#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "content/templates.hpp"
#include "content/vfs.hpp"

namespace totu::content {

// A sablonfájl névtere a `templates/` alatti útvonalából.
std::string namespace_of(const std::string& rel_path);

struct ContentSet {
  std::map<std::string, TemplateStore> ns;   // névtér → sablontár
  TemplateStore& operator[](const std::string& name) { return ns[name]; }
  const Value* get(const std::string& name, const std::string& id) {
    auto it = ns.find(name);
    return it == ns.end() ? nullptr : it->second.get(id);
  }
};

struct LoadReport {
  std::map<std::string, int> count_by_kind;   // unit, building, research, ability, item, status, faction
  std::vector<std::string> errors;            // fájl-, öröklődés- és sémahibák
};

class ContentLoader {
 public:
  // `schema_dir`: a sémák mappája; a rétegeket a `vfs` adja (alap + modok a betöltési sorrendben).
  ContentLoader(const Vfs& vfs, std::filesystem::path schema_dir) : vfs_(vfs), schema_dir_(std::move(schema_dir)) {}
  LoadReport load(ContentSet& out);

 private:
  const Vfs& vfs_;
  std::filesystem::path schema_dir_;
};

}  // namespace totu::content
