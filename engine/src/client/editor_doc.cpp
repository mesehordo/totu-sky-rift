// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/editor_doc.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "map/map.hpp"
#include "sim/mission.hpp"

namespace totu::client {

namespace fs = std::filesystem;
using content::Value;

namespace {

constexpr size_t kMaxUndo = 200;

bool slurp(const fs::path& p, std::string& out) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}

bool spit(const fs::path& p, const std::string& s) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  const fs::path tmp = p.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    out << s;
    if (!out) return false;
  }
  fs::rename(tmp, p, ec);   // a régi csak a sikeres írás után cserélődik
  if (ec) {
    fs::remove(p, ec);
    fs::rename(tmp, p, ec);
  }
  return !ec;
}

Value num(double v) {
  char buf[48];
  if (v == std::floor(v) && std::fabs(v) < 1e9) std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(v));
  else std::snprintf(buf, sizeof buf, "%.3f", v);
  std::string s(buf);
  if (s.find('.') != std::string::npos) {   // a felesleges nullák nélkül (kanonikus)
    while (s.back() == '0') s.pop_back();
    if (s.back() == '.') s.pop_back();
  }
  return Value::number(s);
}

double to_d(const Value* v) {
  if (!v) return 0;
  const auto f = v->as_fixed();
  return f ? f->to_double() : 0;
}

std::string str(const Value& v, std::string_view k) {
  const Value* x = v.find(k);
  return x && x->is_string() ? x->text() : std::string();
}

}  // namespace

int EditorDoc::width() const {
  const Value* w = map_.find("width");
  return w ? static_cast<int>(w->as_int().value_or(0)) : 0;
}

int EditorDoc::height() const {
  const Value* h = map_.find("height");
  return h ? static_cast<int>(h->as_int().value_or(0)) : 0;
}

bool EditorDoc::open(const std::string& mission_dir, const std::string& maps_dir, std::string* error) {
  std::string mt, pt;
  if (!slurp(fs::path(mission_dir) / "mission.json", mt)) {
    if (error) *error = "nincs mission.json: " + mission_dir;
    return false;
  }
  content::ParseError pe;
  auto m = content::parse(mt, &pe);
  if (!m || !m->is_object()) {
    if (error) *error = "mission.json: " + pe.message;
    return false;
  }
  const std::string map = str(*m, "map");
  if (map.empty() || map.find("..") != std::string::npos ||
      (!slurp(fs::path(mission_dir) / map, pt) && !slurp(fs::path(maps_dir) / map, pt))) {
    if (error) *error = "a küldetés pályája nem található: " + map;
    return false;
  }
  auto p = content::parse(pt, &pe);
  if (!p || !p->is_object()) {
    if (error) *error = "a pálya hibás: " + pe.message;
    return false;
  }
  dir_ = mission_dir;
  map_name_ = map;
  mission_ = std::move(*m);
  map_ = std::move(*p);
  undo_.clear();
  redo_.clear();
  dirty_ = false;
  return true;
}

bool EditorDoc::create(const std::string& mission_dir, const std::string& map_path, const std::string& id, std::string* error) {
  std::string pt;
  if (!slurp(map_path, pt)) {
    if (error) *error = "a pálya nem olvasható: " + map_path;
    return false;
  }
  auto p = content::parse(pt);
  if (!p || !p->is_object()) {
    if (error) *error = "a pálya hibás: " + map_path;
    return false;
  }
  const auto m = content::parse(R"({"id": "", "name": "", "description": "", "map": "",
    "players": [{"slot": 1, "faction": "ursan", "human": true, "resources": {"wood": 200, "gold": 200}},
                {"slot": 2, "faction": "wullof"}],
    "regions": [], "points": [], "cameras": [], "paths": [], "units": [], "variables": {}, "objectives": [],
    "dialogues": {}, "cutscenes": {}, "triggers": []})");
  mission_ = *m;
  mission_.set("id", Value::string(id));
  map_name_ = fs::path(map_path).filename().string();
  mission_.set("map", Value::string(map_name_));
  map_ = std::move(*p);
  dir_ = mission_dir;
  undo_.clear();
  redo_.clear();
  dirty_ = true;
  return true;
}

bool EditorDoc::save(std::string* error) const {
  if (dir_.empty()) {
    if (error) *error = "nincs megadva a küldetés mappája";
    return false;
  }
  if (!spit(fs::path(dir_) / "mission.json", mission_text()) || !spit(fs::path(dir_) / map_name_, map_text())) {
    if (error) *error = "a küldetés nem írható: " + dir_;
    return false;
  }
  const_cast<EditorDoc*>(this)->dirty_ = false;
  return true;
}

bool EditorDoc::save_as(const std::string& mission_dir, std::string* error) {
  dir_ = mission_dir;
  return save(error);
}

void EditorDoc::touch() {
  undo_.emplace_back(mission_, map_);
  if (undo_.size() > kMaxUndo) undo_.erase(undo_.begin());
  redo_.clear();
  dirty_ = true;
}

bool EditorDoc::undo() {
  if (undo_.empty()) return false;
  redo_.emplace_back(mission_, map_);
  std::tie(mission_, map_) = std::move(undo_.back());
  undo_.pop_back();
  dirty_ = true;
  return true;
}

bool EditorDoc::redo() {
  if (redo_.empty()) return false;
  undo_.emplace_back(mission_, map_);
  std::tie(mission_, map_) = std::move(redo_.back());
  redo_.pop_back();
  dirty_ = true;
  return true;
}

Value* EditorDoc::list(const std::string& key) {
  Value* v = mission_.find(key);
  if (!v || !v->is_array()) {
    mission_.set(key, Value::array());
    v = mission_.find(key);
  }
  return v;
}

std::string EditorDoc::unique_id(const std::string& key, const std::string& prefix) const {
  const Value* l = mission_.find(key);
  for (int i = 1;; ++i) {
    const std::string id = prefix + std::to_string(i);
    bool used = false;
    if (l && l->is_array()) {
      for (const auto& x : l->items()) used = used || str(x, "id") == id;
    }
    if (!used) return id;
  }
}

bool EditorDoc::paint(int x, int y, int radius, char c) {
  const int w = width(), h = height();
  if (w <= 0 || h <= 0 || radius < 0 || radius > 64) return false;
  const bool tree = c == 'T' || c == '.';
  Value* layer = nullptr;
  if (tree) {
    Value* ob = map_.find("objects");
    Value* tr = ob ? ob->find("trees") : nullptr;
    layer = tr ? tr->find("rows") : nullptr;
  } else {
    Value* tv = map_.find("terrain");
    layer = tv ? tv->find("rows") : nullptr;
  }
  if (!layer || !layer->is_array() || static_cast<int>(layer->items().size()) != h) return false;
  bool changed = false;
  for (int yy = std::max(0, y - radius); yy <= std::min(h - 1, y + radius) && !changed; ++yy) {
    const std::string& row = layer->items()[static_cast<size_t>(yy)].text();
    for (int xx = std::max(0, x - radius); xx <= std::min(w - 1, x + radius); ++xx) {
      if ((xx - x) * (xx - x) + (yy - y) * (yy - y) <= radius * radius && row[static_cast<size_t>(xx)] != c) changed = true;
    }
  }
  if (!changed) return false;
  touch();
  for (int yy = std::max(0, y - radius); yy <= std::min(h - 1, y + radius); ++yy) {
    std::string row = layer->items()[static_cast<size_t>(yy)].text();
    for (int xx = std::max(0, x - radius); xx <= std::min(w - 1, x + radius); ++xx) {
      if ((xx - x) * (xx - x) + (yy - y) * (yy - y) <= radius * radius) row[static_cast<size_t>(xx)] = c;
    }
    layer->items()[static_cast<size_t>(yy)] = Value::string(row);
  }
  return true;
}

int EditorDoc::place(const std::string& def, int owner, double x, double y, const std::string& name) {
  if (def.empty() || x < 0 || y < 0 || x > width() || y > height() || owner < 0 || owner > 8) return -1;
  touch();
  Value u = Value::object();
  if (!name.empty()) u.set("name", Value::string(name));
  u.set("def", Value::string(def));
  u.set("owner", num(owner));
  u.set("x", num(x));
  u.set("y", num(y));
  Value* l = list("units");
  l->push(std::move(u));
  return static_cast<int>(l->items().size()) - 1;
}

bool EditorDoc::move_unit(int index, double x, double y) {
  Value* l = list("units");
  if (index < 0 || index >= static_cast<int>(l->items().size()) || x < 0 || y < 0 || x > width() || y > height()) return false;
  touch();
  l = list("units");
  Value& u = l->items()[static_cast<size_t>(index)];
  u.set("x", num(x));
  u.set("y", num(y));
  return true;
}

bool EditorDoc::remove_unit(int index) {
  Value* l = list("units");
  if (index < 0 || index >= static_cast<int>(l->items().size())) return false;
  touch();
  l = list("units");
  l->items().erase(l->items().begin() + index);
  return true;
}

int EditorDoc::unit_at(double x, double y, double radius) const {
  const Value* l = mission_.find("units");
  if (!l || !l->is_array()) return -1;
  int best = -1;
  double bd = radius * radius;
  for (size_t i = 0; i < l->items().size(); ++i) {
    const Value& u = l->items()[i];
    const double dx = to_d(u.find("x")) - x, dy = to_d(u.find("y")) - y, d = dx * dx + dy * dy;
    if (d <= bd) {
      bd = d;
      best = static_cast<int>(i);
    }
  }
  return best;
}

bool EditorDoc::add_region(const std::string& id, int x, int y, int w, int h) {
  if (id.empty() || w < 1 || h < 1 || x < 0 || y < 0 || x + w > width() || y + h > height()) return false;
  touch();
  Value r = Value::object();
  r.set("id", Value::string(id));
  r.set("x", num(x));
  r.set("y", num(y));
  r.set("w", num(w));
  r.set("h", num(h));
  list("regions")->push(std::move(r));
  return true;
}

bool EditorDoc::add_point(const std::string& id, double x, double y, bool camera, double zoom) {
  if (id.empty() || x < 0 || y < 0 || x > width() || y > height()) return false;
  touch();
  Value p = Value::object();
  p.set("id", Value::string(id));
  p.set("x", num(x));
  p.set("y", num(y));
  if (camera && zoom > 0) p.set("zoom", num(zoom));
  list(camera ? "cameras" : "points")->push(std::move(p));
  return true;
}

bool EditorDoc::remove_named(const std::string& key, const std::string& id) {
  Value* l = mission_.find(key);
  if (!l || !l->is_array()) return false;
  const auto it = std::find_if(l->items().begin(), l->items().end(), [&](const Value& v) { return str(v, "id") == id; });
  if (it == l->items().end()) return false;
  const size_t at = static_cast<size_t>(it - l->items().begin());
  touch();
  l = mission_.find(key);
  l->items().erase(l->items().begin() + static_cast<std::ptrdiff_t>(at));
  return true;
}

bool EditorDoc::set_section(const std::string& key, const std::string& json, std::string* error) {
  static const char* kLists[] = {"players", "regions", "points", "cameras", "paths", "units", "objectives", "triggers"};
  static const char* kObjects[] = {"variables", "dialogues", "cutscenes", "objects"};
  const bool is_list = std::any_of(std::begin(kLists), std::end(kLists), [&](const char* k) { return key == k; });
  const bool is_obj = std::any_of(std::begin(kObjects), std::end(kObjects), [&](const char* k) { return key == k; });
  if (!is_list && !is_obj) {
    if (error) *error = "ismeretlen rész: " + key;
    return false;
  }
  content::ParseError pe;
  auto v = content::parse(json, &pe);
  if (!v || (is_list && !v->is_array()) || (is_obj && !v->is_object())) {
    if (error) *error = key + ": " + (v ? std::string(is_list ? "tömb kell" : "objektum kell") : pe.message + " (" + std::to_string(pe.line) + ". sor)");
    return false;
  }
  touch();
  mission_.set(key, std::move(*v));
  return true;
}

bool EditorDoc::set_field(const std::string& key, const std::string& value) {
  if (key != "id" && key != "name" && key != "description") return false;
  touch();
  mission_.set(key, Value::string(value));
  return true;
}

std::vector<std::string> EditorDoc::validate(const sim::Rules* rules, const map::TerrainTable* terrain) const {
  std::vector<std::string> errors;
  if (terrain) {
    std::vector<std::string> me;
    if (!map::Map::from_json(map_, *terrain, me)) {
      for (auto& e : me) errors.push_back("pálya: " + e);
    }
  }
  sim::MissionDef::parse(content::dump(mission_), rules, errors);
  // a régiók és a pontok a pályán belül
  const int w = width(), h = height();
  if (const Value* rs = mission_.find("regions"); rs && rs->is_array()) {
    for (const auto& r : rs->items()) {
      const double x = to_d(r.find("x")), y = to_d(r.find("y")), rw = to_d(r.find("w")), rh = to_d(r.find("h"));
      if (x < 0 || y < 0 || x + rw > w || y + rh > h) errors.push_back("regions." + str(r, "id") + ": a pályán kívül");
    }
  }
  return errors;
}

sim::Replay EditorDoc::replay(uint64_t seed) const {
  sim::Replay r;
  r.seed = seed;
  r.map = content::dump(map_);
  r.mission = content::dump(mission_);
  return r;
}

}  // namespace totu::client
