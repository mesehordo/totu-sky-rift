// SPDX-License-Identifier: GPL-3.0-or-later
#include "map/terrain.hpp"

#include <algorithm>
#include <set>

namespace totu::map {

namespace {

const content::Value* section(const content::Value* t, const char* name) { return t ? t->find(name) : nullptr; }

std::string text_or(const content::Value* v, const char* key, std::string def) {
  const content::Value* x = v ? v->find(key) : nullptr;
  return x && x->is_string() ? x->text() : def;
}

bool bool_or(const content::Value* v, const char* key, bool def) {
  const content::Value* x = v ? v->find(key) : nullptr;
  return x && x->is_bool() ? x->as_bool() : def;
}

Fixed fixed_or(const content::Value* v, const char* key, Fixed def) {
  const content::Value* x = v ? v->find(key) : nullptr;
  if (!x || !x->is_number()) return def;
  auto f = x->as_fixed();
  return f ? *f : def;
}

}  // namespace

bool TerrainTable::load(content::ContentSet& content, std::vector<std::string>& errors) {
  terrains_.clear();
  classes_.clear();
  pass_names_.clear();
  const size_t errors_before = errors.size();
  auto t_it = content.ns.find("terrain");
  auto m_it = content.ns.find("movement");
  if (t_it == content.ns.end()) {
    errors.push_back("nincs terepadat (templates/terrain.json)");
    return false;
  }
  // 1. a járhatósági osztályok neve ábécérendben
  std::set<std::string> names;
  for (const auto& id : t_it->second.ids()) {
    if (id.rfind("template_", 0) == 0) continue;
    if (const content::Value* t = t_it->second.get(id)) names.insert(text_or(section(t, "Terrain"), "pass", "land"));
  }
  pass_names_.assign(names.begin(), names.end());
  if (pass_names_.size() > static_cast<size_t>(kMaxPassClasses)) {
    errors.push_back("túl sok járhatósági osztály (legfeljebb 32)");
    return false;
  }
  // 2. terepfajták
  for (const auto& id : t_it->second.ids()) {
    if (id.rfind("template_", 0) == 0) continue;
    const content::Value* t = t_it->second.get(id);
    if (!t) continue;
    const content::Value* tr = section(t, "Terrain");
    TerrainType x;
    x.id = id;
    x.name = text_or(section(t, "Identity"), "name", id);
    const std::string ch = text_or(tr, "char", "");
    if (ch.size() != 1) {
      errors.push_back("terrain/" + id + ": a `char` pontosan egy ASCII karakter legyen");
      continue;
    }
    x.ch = ch[0];
    x.pass = static_cast<uint8_t>(*pass_class(text_or(tr, "pass", "land")));
    x.speed = fixed_or(tr, "speed", Fixed::from_int(1));
    x.buildable = bool_or(tr, "buildable", true);
    x.hides = bool_or(tr, "hides", false);
    for (const auto& o : terrains_) {
      if (o.ch == x.ch) errors.push_back("terrain/" + id + ": a `" + ch + "` jelet már a(z) " + o.id + " használja");
    }
    terrains_.push_back(std::move(x));
  }
  // 3. mozgásosztályok
  if (m_it != content.ns.end()) {
    for (const auto& id : m_it->second.ids()) {
      if (id.rfind("template_", 0) == 0) continue;
      const content::Value* t = m_it->second.get(id);
      if (!t) continue;
      const content::Value* mc = section(t, "MovementClass");
      MovementClass c;
      c.id = id;
      c.name = text_or(section(t, "Identity"), "name", id);
      c.flying = bool_or(mc, "flying", false);
      c.pass_speed.assign(kMaxPassClasses, Fixed{});
      if (const content::Value* passes = mc ? mc->find("passes") : nullptr; passes && passes->is_object()) {
        for (const auto& [name, val] : passes->members()) {
          const auto pc = pass_class(name);
          if (!pc) continue;   // ismeretlen osztály: egyik terep sem ilyen, nincs hatása
          auto f = val.as_fixed();
          if (!f || f->raw() <= 0) continue;
          c.pass_mask |= 1u << *pc;
          c.pass_speed[*pc] = *f;
        }
      }
      classes_.push_back(std::move(c));
    }
  }
  if (classes_.size() > 255) errors.push_back("túl sok mozgásosztály");
  return errors.size() == errors_before;
}

std::optional<uint16_t> TerrainTable::terrain(const std::string& id) const {
  for (size_t i = 0; i < terrains_.size(); ++i) {
    if (terrains_[i].id == id) return static_cast<uint16_t>(i);
  }
  return std::nullopt;
}

std::optional<uint16_t> TerrainTable::terrain_by_char(char c) const {
  for (size_t i = 0; i < terrains_.size(); ++i) {
    if (terrains_[i].ch == c) return static_cast<uint16_t>(i);
  }
  return std::nullopt;
}

std::optional<uint8_t> TerrainTable::movement_class(const std::string& id) const {
  for (size_t i = 0; i < classes_.size(); ++i) {
    if (classes_[i].id == id) return static_cast<uint8_t>(i);
  }
  return std::nullopt;
}

std::optional<uint8_t> TerrainTable::pass_class(const std::string& name) const {
  const auto it = std::lower_bound(pass_names_.begin(), pass_names_.end(), name);
  if (it == pass_names_.end() || *it != name) return std::nullopt;
  return static_cast<uint8_t>(it - pass_names_.begin());
}

bool TerrainTable::passable(uint8_t movement_class, uint16_t terrain) const {
  const MovementClass& c = classes_[movement_class];
  return c.flying || (c.pass_mask >> terrains_[terrain].pass & 1u) != 0;
}

Fixed TerrainTable::speed(uint8_t movement_class, uint16_t terrain) const {
  const MovementClass& c = classes_[movement_class];
  if (c.flying) return Fixed::from_int(1);
  if (!passable(movement_class, terrain)) return Fixed{};
  return terrains_[terrain].speed * c.pass_speed[terrains_[terrain].pass];
}

}  // namespace totu::map
