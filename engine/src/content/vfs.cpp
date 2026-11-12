// SPDX-License-Identifier: GPL-3.0-or-later
#include "content/vfs.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

namespace totu::content {

namespace fs = std::filesystem;

namespace {
// UTF-8 szöveg ↔ útvonal (C++20: a u8path elavult, a u8string char8_t-t ad).
fs::path to_path(const std::string& s) {
  std::u8string u;
  u.reserve(s.size());
  for (const char c : s) u.push_back(static_cast<char8_t>(c));
  return fs::path(u);
}
std::string to_utf8(const fs::path& p) {
  const std::u8string u = p.generic_u8string();
  std::string s;
  s.reserve(u.size());
  for (const char8_t c : u) s.push_back(static_cast<char>(c));
  return s;
}
}  // namespace

void Vfs::add_layer(const std::string& name, const fs::path& root) { layers_.emplace_back(name, root); }

bool Vfs::safe(const std::string& rel) {
  if (rel.empty() || rel[0] == '/' || rel[0] == '\\' || rel.find(':') != std::string::npos) return false;
  size_t start = 0;
  while (start <= rel.size()) {
    const size_t end = rel.find_first_of("/\\", start);
    const std::string part = rel.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (part == "..") return false;
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return true;
}

namespace {
std::optional<std::string> slurp(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return std::nullopt;
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}
}  // namespace

bool Vfs::exists(const std::string& rel) const {
  if (!safe(rel)) return false;
  for (const auto& l : layers_) {
    std::error_code ec;
    if (fs::is_regular_file(l.second / to_path(rel), ec)) return true;
  }
  return false;
}

std::optional<std::string> Vfs::read_text(const std::string& rel) const {
  if (!safe(rel)) return std::nullopt;
  for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
    std::error_code ec;
    const fs::path p = it->second / to_path(rel);
    if (fs::is_regular_file(p, ec)) return slurp(p);
  }
  return std::nullopt;
}

std::vector<std::pair<std::string, std::string>> Vfs::read_all(const std::string& rel) const {
  std::vector<std::pair<std::string, std::string>> out;
  if (!safe(rel)) return out;
  for (const auto& l : layers_) {
    std::error_code ec;
    const fs::path p = l.second / to_path(rel);
    if (fs::is_regular_file(p, ec)) {
      if (auto t = slurp(p)) out.emplace_back(l.first, std::move(*t));
    }
  }
  return out;
}

std::vector<std::string> Vfs::list(const std::string& rel_dir, const std::string& extension) const {
  std::set<std::string> names;   // rendezett, ismétlés nélkül
  if (!safe(rel_dir)) return {};
  for (const auto& l : layers_) {
    std::error_code ec;
    const fs::path dir = l.second / to_path(rel_dir);
    if (!fs::is_directory(dir, ec)) continue;
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
      if (!it->is_regular_file(ec)) continue;
      const fs::path& p = it->path();
      if (!extension.empty() && to_utf8(p.extension()) != extension) continue;
      std::string rel = to_utf8(fs::relative(p, l.second, ec));
      if (!ec) names.insert(std::move(rel));
    }
  }
  return {names.begin(), names.end()};
}

std::optional<ModInfo> read_mod_info(const fs::path& mod_root, std::string* error) {
  auto text = slurp(mod_root / "mod.json");
  if (!text) { if (error) *error = "hiányzik a mod.json"; return std::nullopt; }
  ParseError pe;
  auto v = parse(*text, &pe);
  if (!v || !v->is_object()) {
    if (error) *error = "mod.json: " + pe.message + " (" + std::to_string(pe.line) + ". sor, " +
                        std::to_string(pe.column) + ". oszlop)";
    return std::nullopt;
  }
  ModInfo m;
  m.root = mod_root;
  const Value* id = v->find("id");
  if (!id || !id->is_string() || id->text().empty()) { if (error) *error = "mod.json: hiányzik az \"id\""; return std::nullopt; }
  m.id = id->text();
  if (const Value* ver = v->find("version"); ver && ver->is_string()) m.version = ver->text();
  auto strings = [](const Value* arr, std::vector<std::string>& out) {
    if (!arr || !arr->is_array()) return;
    for (const auto& it : arr->items()) if (it.is_string()) out.push_back(it.text());
  };
  strings(v->find("depends"), m.depends);
  strings(v->find("conflicts"), m.conflicts);
  return m;
}

ModLoadResult resolve_mods(const std::vector<ModInfo>& available, const std::vector<std::string>& requested) {
  ModLoadResult r;
  auto find = [&](const std::string& id) -> const ModInfo* {
    for (const auto& m : available) if (m.id == id) return &m;
    return nullptr;
  };
  std::set<std::string> done, visiting;
  // mélységi bejárás: előbb a függőségek, a kérés sorrendjében
  auto visit = [&](auto&& self, const std::string& id, const std::string& by) -> void {
    if (done.count(id)) return;
    if (visiting.count(id)) { r.errors.push_back("körkörös függőség: " + id); return; }
    const ModInfo* m = find(id);
    if (!m) {
      r.errors.push_back(by.empty() ? "a mod nem található: " + id : by + " függősége hiányzik: " + id);
      return;
    }
    visiting.insert(id);
    for (const auto& d : m->depends) self(self, d, id);
    visiting.erase(id);
    done.insert(id);
    r.order.push_back(*m);
  };
  for (const auto& id : requested) visit(visit, id, "");
  for (const auto& m : r.order) {
    for (const auto& c : m.conflicts) {
      if (done.count(c)) r.errors.push_back(m.id + " ütközik ezzel: " + c);
    }
  }
  if (!r.errors.empty()) r.order.clear();
  return r;
}

}  // namespace totu::content
