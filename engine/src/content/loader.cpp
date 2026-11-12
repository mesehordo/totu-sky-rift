// SPDX-License-Identifier: GPL-3.0-or-later
#include "content/loader.hpp"

#include <fstream>
#include <optional>
#include <sstream>

#include "content/schema.hpp"

namespace totu::content {

namespace {

std::optional<Schema> load_schema(const std::filesystem::path& p, std::vector<std::string>& errs) {
  std::ifstream in(p, std::ios::binary);
  if (!in) { errs.push_back("hiányzó séma: " + p.filename().string()); return std::nullopt; }
  std::ostringstream ss;
  ss << in.rdbuf();
  ParseError pe;
  auto v = parse(ss.str(), &pe);
  if (!v) { errs.push_back(p.filename().string() + ": " + pe.message); return std::nullopt; }
  return Schema(std::move(*v));
}

}  // namespace

std::string namespace_of(const std::string& rel) {
  // rel: "templates/units/x.json" vagy "templates/abilities.json"
  std::string rest = rel.rfind("templates/", 0) == 0 ? rel.substr(10) : rel;
  const size_t slash = rest.find('/');
  std::string head = slash == std::string::npos ? rest.substr(0, rest.rfind('.')) : rest.substr(0, slash);
  if (head == "units" || head == "buildings" || head == "common") return "entity";
  if (head == "researches") return "research";
  if (head == "abilities") return "ability";
  if (head == "items") return "item";
  if (head == "statuses") return "status";
  if (head == "factions") return "faction";
  return head;
}

LoadReport ContentLoader::load(ContentSet& out) {
  LoadReport rep;
  for (const auto& layer : vfs_.layers()) {
    Vfs single;
    single.add_layer(layer.first, layer.second);
    for (const auto& rel : single.list("templates", ".json")) {
      auto text = single.read_text(rel);
      if (!text) continue;
      ParseError pe;
      auto doc = parse(*text, &pe);
      if (!doc) {
        rep.errors.push_back(layer.first + "/" + rel + ": " + pe.message + " (" + std::to_string(pe.line) + ". sor, " +
                             std::to_string(pe.column) + ". oszlop)");
        continue;
      }
      out[namespace_of(rel)].add_file(layer.first, layer.first + "/" + rel, *doc);
    }
  }
  auto entity = load_schema(schema_dir_ / "entity.schema.json", rep.errors);
  auto other = load_schema(schema_dir_ / "other.schema.json", rep.errors);
  for (auto& [ns_name, store] : out.ns) {
  for (const auto& id : store.ids()) {
    if (id.rfind("template_", 0) == 0) continue;           // absztrakt alapsablon
    const Value* t = store.get(id);
    if (!t) continue;                                      // a hiba már a tárban van
    const Value* ident = t->find("Identity");
    const Value* kind = ident ? ident->find("kind") : nullptr;
    const std::string k = (kind && kind->is_string()) ? kind->text() : "";
    if (k.empty()) { rep.errors.push_back(ns_name + "/" + id + ": hiányzik az Identity.kind"); continue; }
    ++rep.count_by_kind[k];
    const Schema* sch = (k == "unit" || k == "building") ? (entity ? &*entity : nullptr) : (other ? &*other : nullptr);
    if (!sch) continue;
    for (const auto& e : sch->validate(*t)) rep.errors.push_back(ns_name + "/" + id + ": " + e);
  }
  for (const auto& e : store.errors()) rep.errors.push_back(ns_name + ": " + e);
  }
  return rep;
}

}  // namespace totu::content
