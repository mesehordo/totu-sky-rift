// SPDX-License-Identifier: GPL-3.0-or-later
#include "script/victory.hpp"

#include <algorithm>
#include <set>

#include "content/json.hpp"

namespace totu::script {

namespace {

bool valid_id(const std::string& id) {
  if (id.empty() || id.size() > 64) return false;
  return std::all_of(id.begin(), id.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

// "szöveg" vagy {"hu": "…", "en": "…"}
void read_text(const content::Value* v, std::map<std::string, std::string>& out) {
  if (!v) return;
  if (v->is_string()) {
    out["hu"] = out["en"] = v->text();
    return;
  }
  if (!v->is_object()) return;
  for (const auto& [k, x] : v->members()) {
    if (x.is_string()) out[k] = x.text();
  }
}

std::optional<VictoryInfo> parse_info(const std::string& id, const std::string& text) {
  const auto v = content::parse(text);
  if (!v || !v->is_object()) return std::nullopt;
  VictoryInfo i;
  i.id = id;
  read_text(v->find("name"), i.name);
  read_text(v->find("description"), i.description);
  return i;
}

}  // namespace

std::string VictoryInfo::label(const std::string& lang) const {
  for (const std::string& l : {lang, std::string("en"), std::string("hu")}) {
    const auto it = name.find(l);
    if (it != name.end() && !it->second.empty()) return it->second;
  }
  return id;
}

std::vector<VictoryInfo> list_victories(const content::Vfs& vfs) {
  std::set<std::string> ids;
  for (const auto& rel : vfs.list("victory", ".json")) {
    std::string r = rel;   // victory/<azonosító>/victory.json
    std::replace(r.begin(), r.end(), '\\', '/');
    const std::string head = "victory/", tail = "/victory.json";
    if (r.size() <= head.size() + tail.size() || r.compare(r.size() - tail.size(), tail.size(), tail) != 0) continue;
    const std::string mid = r.substr(head.size(), r.size() - head.size() - tail.size());
    if (valid_id(mid)) ids.insert(mid);
  }
  std::vector<VictoryInfo> out;
  for (const auto& id : ids) {
    const auto text = vfs.read_text("victory/" + id + "/victory.json");
    if (!text || !vfs.exists("victory/" + id + "/victory.lua")) continue;
    if (auto i = parse_info(id, *text)) out.push_back(std::move(*i));
  }
  return out;
}

std::optional<VictoryPackage> load_victory(const content::Vfs& vfs, const std::string& id, std::string* error) {
  auto fail = [&](const std::string& m) -> std::optional<VictoryPackage> {
    if (error) *error = m;
    return std::nullopt;
  };
  if (!valid_id(id)) return fail("érvénytelen győzelmi szkript: " + id);
  const auto json = vfs.read_text("victory/" + id + "/victory.json");
  const auto lua = vfs.read_text("victory/" + id + "/victory.lua");
  if (!json || !lua) return fail("nincs ilyen győzelmi szkript: " + id);
  auto info = parse_info(id, *json);
  if (!info) return fail("hibás victory.json: " + id);
  return VictoryPackage{std::move(*info), *lua};
}

std::unique_ptr<ScriptHost> attach_victory(sim::Sim& s, const VictoryPackage& pkg, uint64_t seed, bool start, std::string* error) {
  auto h = std::make_unique<ScriptHost>("victory", seed);
  h->enable_victory();
  if (!h->run("victory.lua", pkg.source)) {
    if (error) *error = "a győzelmi szkript hibás (" + pkg.info.id + ")" + (h->messages().empty() ? "" : ": " + h->messages().back());
    return nullptr;
  }
  s.add_hooks(h.get());   // csatolva marad: a hívó birtokolja (indítási hibánál is, mert a szimuláció hivatkozik rá)
  if (start) {
    h->start(s);
    if (h->errors() > 0 && error) *error = "a győzelmi szkript hibát jelzett az indításkor" + (h->messages().empty() ? "" : ": " + h->messages().back());
  }
  return h;
}

void add_to_replay(sim::Replay& r, const VictoryPackage& pkg) {
  r.victory = static_cast<uint8_t>(sim::MatchSetup::Victory::Script);
  r.scripts.push_back(sim::ScriptFile{"victory", "victory.lua", pkg.source});
}

}  // namespace totu::script
