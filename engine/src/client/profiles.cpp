// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/profiles.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

#include "content/json.hpp"

namespace totu::client {

namespace {

using content::Value;

Value num(int64_t v) { return Value::number(std::to_string(v)); }
int64_t get_int(const Value& o, const char* k) {
  const Value* v = o.find(k);
  return v ? v->as_int().value_or(0) : 0;
}

std::optional<Profile> parse_profile(const std::string& text) {
  const auto doc = content::parse(text);
  if (!doc || !doc->is_object()) return std::nullopt;
  Profile p;
  if (const Value* v = doc->find("name"); v && v->is_string()) p.name = v->text();
  if (p.name.empty()) return std::nullopt;
  p.created = get_int(*doc, "created");
  if (const Value* v = doc->find("emblem"); v && v->is_string()) p.emblem = v->text();
  if (const Value* st = doc->find("stats"); st && st->is_object()) {
    p.games = static_cast<int>(get_int(*st, "games"));
    p.wins = static_cast<int>(get_int(*st, "wins"));
    p.losses = static_cast<int>(get_int(*st, "losses"));
    p.kills = static_cast<int>(get_int(*st, "kills"));
    p.play_seconds = get_int(*st, "play_time");
    if (const Value* bf = st->find("by_faction"); bf && bf->is_object()) {
      for (const auto& [f, v] : bf->members()) {
        if (v.is_object()) p.by_faction.push_back({f, static_cast<int>(get_int(v, "games")), static_cast<int>(get_int(v, "wins"))});
      }
    }
  }
  return p;
}

}  // namespace

std::filesystem::path ProfileStore::file_of(const std::string& name) const {
  // fájlnév: az ASCII betűk és számok maradnak, a többi bájt hexában (ütközésmentes, visszafejthető)
  std::string f;
  static const char* kHex = "0123456789abcdef";
  for (const unsigned char c : name) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
      f.push_back(static_cast<char>(c));
    } else {
      f.push_back('_');
      f.push_back(kHex[c >> 4]);
      f.push_back(kHex[c & 15]);
    }
  }
  return dir_ / (f + ".json");
}

std::vector<Profile> ProfileStore::list() const {
  std::vector<Profile> out;
  std::error_code ec;
  for (const auto& e : std::filesystem::directory_iterator(dir_, ec)) {
    if (e.path().extension() != ".json") continue;
    std::ifstream in(e.path(), std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (auto p = parse_profile(text)) out.push_back(std::move(*p));
  }
  std::sort(out.begin(), out.end(), [](const Profile& a, const Profile& b) { return a.name < b.name; });
  return out;
}

std::optional<Profile> ProfileStore::load(const std::string& name) const {
  std::ifstream in(file_of(name), std::ios::binary);
  if (!in) return std::nullopt;
  const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return parse_profile(text);
}

bool ProfileStore::save(const Profile& p) const {
  Value o = Value::object();
  o.set("name", Value::string(p.name));
  o.set("created", num(p.created));
  o.set("emblem", Value::string(p.emblem));
  Value st = Value::object();
  st.set("games", num(p.games));
  st.set("wins", num(p.wins));
  st.set("losses", num(p.losses));
  st.set("kills", num(p.kills));
  st.set("play_time", num(p.play_seconds));
  Value bf = Value::object();
  for (const auto& f : p.by_faction) {
    Value v = Value::object();
    v.set("games", num(f.games));
    v.set("wins", num(f.wins));
    bf.set(f.faction, std::move(v));
  }
  st.set("by_faction", std::move(bf));
  o.set("stats", std::move(st));
  std::error_code ec;
  std::filesystem::create_directories(dir_, ec);
  const auto path = file_of(p.name), tmp = std::filesystem::path(path.string() + ".tmp");
  {
    std::ofstream out(tmp, std::ios::binary);
    out << content::dump_pretty(o);
    if (!out) return false;
  }
  std::filesystem::rename(tmp, path, ec);   // atomi csere (a régi `_replace_file`)
  return !ec;
}

bool ProfileStore::create(const std::string& name, const std::string& emblem, int64_t now, std::string* error) {
  std::string n = name;
  while (!n.empty() && n.back() == ' ') n.pop_back();
  while (!n.empty() && n.front() == ' ') n.erase(n.begin());
  if (n.empty()) {
    if (error) *error = "ui.profiles.err_empty";
    return false;
  }
  if (load(n)) {
    if (error) *error = "ui.profiles.err_taken";
    return false;
  }
  Profile p;
  p.name = n;
  p.created = now;
  p.emblem = emblem.empty() ? std::string("ursan") : emblem;
  if (!save(p)) {
    if (error) *error = "ui.profiles.err_write";
    return false;
  }
  return true;
}

bool ProfileStore::remove(const std::string& name) const {
  std::error_code ec;
  return std::filesystem::remove(file_of(name), ec);
}

void ProfileStore::record_game(const std::string& name, const std::string& faction, bool won, int64_t seconds, int kills) const {
  auto p = load(name);
  if (!p) return;
  ++p->games;
  (won ? p->wins : p->losses) += 1;
  p->kills += std::max(0, kills);
  p->play_seconds += std::max<int64_t>(0, seconds);
  auto it = std::find_if(p->by_faction.begin(), p->by_faction.end(), [&](const Profile::ByFaction& f) { return f.faction == faction; });
  if (it == p->by_faction.end()) {
    p->by_faction.push_back({faction, 0, 0});
    it = p->by_faction.end() - 1;
  }
  ++it->games;
  if (won) ++it->wins;
  save(*p);
}

}  // namespace totu::client
