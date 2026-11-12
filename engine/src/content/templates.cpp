// SPDX-License-Identifier: GPL-3.0-or-later
#include "content/templates.hpp"

#include <algorithm>

namespace totu::content {

namespace {

bool is_list_patch(const Value& v) {
  if (!v.is_object() || v.members().empty()) return false;
  for (const auto& m : v.members()) {
    if ((m.first != "+" && m.first != "-") || !m.second.is_array()) return false;
  }
  return true;
}

void strip_comments(Value& v) {
  if (v.is_object()) {
    auto& ms = v.members();
    for (size_t i = ms.size(); i-- > 0;) {
      if (!ms[i].first.empty() && ms[i].first[0] == '_') ms.erase(ms.begin() + static_cast<std::ptrdiff_t>(i));
      else strip_comments(ms[i].second);
    }
  } else if (v.is_array()) {
    for (auto& it : v.items()) strip_comments(it);
  }
}

// Két listafolt összevonása úgy, hogy az eredmény egyenértékű legyen az egymás utáni alkalmazással:
// „-” = a.„-” + b.„-”;  „+” = (a.„+” a b.„-” elemei nélkül) + b.„+”.
void combine_list_patches(Value& a, const Value& b) {
  Value minus = Value::array(), plus = Value::array();
  if (const Value* am = a.find("-")) minus.items() = am->items();
  if (const Value* bm = b.find("-")) for (const auto& x : bm->items()) minus.push(x);
  if (const Value* ap = a.find("+")) {
    for (const auto& x : ap->items()) {
      bool removed = false;
      if (const Value* bm = b.find("-")) for (const auto& r : bm->items()) removed |= (r == x);
      if (!removed) plus.push(x);
    }
  }
  if (const Value* bp = b.find("+")) for (const auto& x : bp->items()) plus.push(x);
  a = Value::object();
  if (!minus.items().empty()) a.set("-", std::move(minus));
  if (!plus.items().empty()) a.set("+", std::move(plus));
}

}  // namespace

void merge_patch(Value& base, const Value& patch, bool keep_list_patches) {
  if (keep_list_patches && is_list_patch(base) && is_list_patch(patch)) {
    combine_list_patches(base, patch);
    return;
  }
  if (patch.is_object() && base.is_array() && is_list_patch(patch)) {
    auto& items = base.items();
    if (const Value* rem = patch.find("-")) {
      for (const auto& r : rem->items()) {
        for (size_t i = items.size(); i-- > 0;) {
          if (items[i] == r) items.erase(items.begin() + static_cast<std::ptrdiff_t>(i));
        }
      }
    }
    if (const Value* add = patch.find("+")) {
      for (const auto& a : add->items()) items.push_back(a);
    }
    return;
  }
  if (!patch.is_object() || !base.is_object()) {
    base = patch;
    return;
  }
  for (const auto& m : patch.members()) {
    if (m.second.is_null()) {
      // foltrétegben törlésjelző marad (a szülőtől örökölt kulcsot is törli a feloldáskor); feloldáskor törlés
      if (keep_list_patches) base.set(m.first, Value::null());
      else base.erase(m.first);
      continue;
    }
    Value* cur = base.find(m.first);
    if (cur && (cur->is_object() || (cur->is_array() && is_list_patch(m.second)))) {
      merge_patch(*cur, m.second, keep_list_patches);
    } else if (!cur && is_list_patch(m.second)) {
      if (keep_list_patches) {
        base.set(m.first, m.second);            // foltként marad: a szülő listáján hajtódik végre
      } else {
        Value arr = Value::array();             // nem létező listára: csak a hozzáadás érvényes
        merge_patch(arr, m.second);
        base.set(m.first, std::move(arr));
      }
    } else {
      base.set(m.first, m.second);
    }
  }
}

void TemplateStore::add_file(const std::string& layer, const std::string& origin, const Value& doc) {
  resolved_.clear();
  failed_.clear();
  if (!doc.is_object()) {
    errors_.push_back(origin + ": a sablonfájl gyökere objektum kell legyen");
    return;
  }
  for (const auto& m : doc.members()) {
    if (!m.first.empty() && m.first[0] == '_') continue;
    if (!m.second.is_object()) {
      errors_.push_back(origin + ": a(z) \"" + m.first + "\" sablon nem objektum");
      continue;
    }
    auto it = raw_.find(m.first);
    if (it == raw_.end()) {
      raw_.emplace(m.first, Entry{m.second, origin, {layer}});
    } else if (std::find(it->second.layers.begin(), it->second.layers.end(), layer) != it->second.layers.end()) {
      errors_.push_back(origin + ": a(z) \"" + m.first + "\" sablon ebben a rétegben már létezik (" +
                        it->second.first_origin + ")");
    } else {
      merge_patch(it->second.def, m.second, true);   // folt egy későbbi rétegből
      it->second.layers.push_back(layer);
    }
  }
}

bool TemplateStore::resolve(const std::string& id, std::vector<std::string>& stack) {
  if (resolved_.count(id)) return true;
  if (failed_.count(id)) return false;
  auto it = raw_.find(id);
  if (it == raw_.end()) {
    errors_.push_back("ismeretlen sablon: " + id + (stack.empty() ? "" : " (szülőként: " + stack.back() + ")"));
    failed_[id] = true;
    return false;
  }
  for (const auto& s : stack) {
    if (s == id) {
      errors_.push_back("körkörös öröklődés: " + id);
      failed_[id] = true;
      return false;
    }
  }
  stack.push_back(id);
  Value result = Value::object();
  bool ok = true;
  std::vector<std::string> parents;
  if (const Value* inh = it->second.def.find("inherits")) {
    if (inh->is_string()) parents.push_back(inh->text());
    else if (inh->is_array()) for (const auto& p : inh->items()) if (p.is_string()) parents.push_back(p.text());
  }
  for (const auto& p : parents) {
    if (!resolve(p, stack)) { ok = false; break; }
    merge_patch(result, resolved_[p]);
  }
  stack.pop_back();
  if (!ok) { failed_[id] = true; return false; }
  Value own = it->second.def;
  own.erase("inherits");
  merge_patch(result, own);
  strip_comments(result);
  resolved_[id] = std::move(result);
  return true;
}

const Value* TemplateStore::get(const std::string& id) {
  std::vector<std::string> stack;
  if (!resolve(id, stack)) return nullptr;
  return &resolved_[id];
}

std::vector<std::string> TemplateStore::ids() const {
  std::vector<std::string> out;
  for (const auto& kv : raw_) out.push_back(kv.first);
  return out;
}

}  // namespace totu::content
