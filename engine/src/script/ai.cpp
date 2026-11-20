// SPDX-License-Identifier: GPL-3.0-or-later
#include "script/ai.hpp"

#include <algorithm>
#include <set>

#include "content/json.hpp"

namespace totu::script {

const std::string* AiPackage::file(const std::string& path) const {
  const auto it = files.find(path);
  return it == files.end() ? nullptr : &it->second;
}

const std::vector<std::string>& ai_difficulties() {
  static const std::vector<std::string> d = {"kezdo", "konnyu", "normal", "nehez", "mester", "orult"};
  return d;
}

std::optional<AiPackage> load_ai_package(const content::Vfs& vfs, const std::string& id, std::string* error) {
  const std::string dir = "ai/" + id;
  if (id.empty() || id.find_first_of("/\\.") != std::string::npos || !vfs.exists(dir + "/ai.json")) {
    if (error) *error = "nincs ilyen AI-csomag: " + id;
    return std::nullopt;
  }
  AiPackage p;
  p.id = id;
  for (const auto& rel : vfs.list(dir, "")) {
    const bool lua = rel.size() > 4 && rel.compare(rel.size() - 4, 4, ".lua") == 0;
    const bool json = rel.size() > 5 && rel.compare(rel.size() - 5, 5, ".json") == 0;
    if (!lua && !json) continue;
    auto text = vfs.read_text(rel);
    if (!text) continue;
    std::string key = rel.substr(dir.size() + 1);
    std::replace(key.begin(), key.end(), '\\', '/');
    p.files[key] = std::move(*text);
  }
  return p;
}

std::vector<std::string> list_ai_packages(const content::Vfs& vfs) {
  std::set<std::string> ids;
  for (const auto& rel : vfs.list("ai", ".json")) {
    // ai/<azonosító>/ai.json
    std::string r = rel;
    std::replace(r.begin(), r.end(), '\\', '/');
    if (r.size() < 11 || r.compare(r.size() - 8, 8, "/ai.json") != 0) continue;
    const std::string mid = r.substr(3, r.size() - 11);
    if (!mid.empty() && mid.find('/') == std::string::npos) ids.insert(mid);
  }
  return {ids.begin(), ids.end()};
}

std::vector<std::string> ai_personalities(const AiPackage& pkg) {
  std::vector<std::string> out;
  const std::string* t = pkg.file("personalities.json");
  if (!t) return out;
  const auto v = content::parse(*t);
  if (!v || !v->is_object()) return out;
  for (const auto& [k, x] : v->members()) {
    if (!k.empty() && k[0] != '_') out.push_back(k);
  }
  return out;
}

std::unique_ptr<ScriptHost> make_ai(const AiPackage& pkg, const sim::AiSlot& slot, uint64_t seed, std::string* error) {
  auto fail = [&](const std::string& m) -> std::unique_ptr<ScriptHost> {
    if (error) *error = m;
    return nullptr;
  };
  if (slot.player < 1 || slot.player > sim::Sim::kMaxPlayers) return fail("érvénytelen gépi játékos");
  const auto& diffs = ai_difficulties();
  if (std::find(diffs.begin(), diffs.end(), slot.difficulty) == diffs.end()) return fail("ismeretlen nehézség: " + slot.difficulty);
  const std::string* info = pkg.file("ai.json");
  const auto info_v = info ? content::parse(*info) : std::nullopt;
  if (!info_v || !info_v->is_object()) return fail("hibás vagy hiányzó ai.json: " + pkg.id);
  auto host = std::make_unique<ScriptHost>("ai" + std::to_string(slot.player), seed);
  host->set_shared("AI_INFO", *info_v);
  for (const auto& [file, name] : {std::pair<const char*, const char*>{"personalities.json", "PERSONALITIES"},
                                   std::pair<const char*, const char*>{"difficulties.json", "DIFFICULTIES"}}) {
    const std::string* t = pkg.file(file);
    if (!t) continue;
    const auto v = content::parse(*t);
    if (!v) return fail(std::string("hibás ") + file + ": " + pkg.id);
    host->set_shared(name, *v);
  }
  const auto pers = ai_personalities(pkg);
  if (!pers.empty() && std::find(pers.begin(), pers.end(), slot.personality) == pers.end()) {
    return fail("ismeretlen személyiség: " + slot.personality);
  }
  auto files = std::make_shared<const std::map<std::string, std::string>>(pkg.files);
  host->set_module_loader([files](const std::string& module) -> std::optional<std::string> {
    std::string path = module;
    std::replace(path.begin(), path.end(), '.', '/');
    const auto it = files->find(path + ".lua");
    if (it == files->end()) return std::nullopt;
    return it->second;
  });
  host->enable_ai(slot.player, slot.difficulty, slot.personality, slot.path, slot.ally);
  std::string entry = "main.lua";
  if (const content::Value* e = info_v->find("entry"); e && e->is_string()) entry = e->text();
  const std::string* src = pkg.file(entry);
  if (!src) return fail("hiányzik a belépési pont: " + entry);
  if (!host->run(pkg.id + "/" + entry, *src)) {
    return fail("a gép szkriptje hibás: " + (host->messages().empty() ? std::string() : host->messages().back()));
  }
  return host;
}

std::vector<std::unique_ptr<ScriptHost>> attach_ais(sim::Sim& s, const std::vector<sim::AiSlot>& slots,
                                                    const std::vector<AiPackage>& packages, uint64_t seed, bool start,
                                                    std::string* error) {
  std::vector<std::unique_ptr<ScriptHost>> out;
  for (const auto& slot : slots) {
    const AiPackage* pkg = nullptr;
    for (const auto& p : packages) {
      if (p.id == slot.ai) pkg = &p;
    }
    if (!pkg) {
      if (error) *error = "nincs ilyen AI-csomag: " + slot.ai;
      return {};
    }
    auto h = make_ai(*pkg, slot, seed, error);
    if (!h) return {};
    out.push_back(std::move(h));
  }
  for (auto& h : out) s.add_hooks(h.get());
  if (start) {
    for (auto& h : out) h->start(s);
  }
  return out;
}

void add_to_replay(sim::Replay& r, const AiPackage& pkg) {
  for (const auto& [path, text] : pkg.files) r.ai_files.push_back(sim::ScriptFile{pkg.id, path, text});
}

}  // namespace totu::script
