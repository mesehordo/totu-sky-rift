// SPDX-License-Identifier: GPL-3.0-or-later
#include "content/schema.hpp"

namespace totu::content {

namespace {

const char* type_name(const Value& v) {
  switch (v.type()) {
    case Value::Type::Null: return "null";
    case Value::Type::Bool: return "boolean";
    case Value::Type::Number: return "number";
    case Value::Type::String: return "string";
    case Value::Type::Array: return "array";
    case Value::Type::Object: return "object";
  }
  return "?";
}

bool type_matches(const std::string& t, const Value& v) {
  if (t == "integer") return v.is_number() && v.as_int().has_value();
  if (t == "number") return v.is_number() && v.as_fixed().has_value();   // a fixpontos tartományban
  return t == type_name(v);
}

}  // namespace

std::vector<std::string> Schema::validate(const Value& doc) const {
  std::vector<std::string> errs;
  check(root_, doc, "", errs, 0);
  return errs;
}

void Schema::check(const Value& s, const Value& v, const std::string& path, std::vector<std::string>& errs,
                   int depth) const {
  if (depth > 64 || !s.is_object()) return;
  const std::string where = path.empty() ? "(gyökér)" : path;
  if (const Value* ref = s.find("$ref"); ref && ref->is_string()) {
    const std::string& r = ref->text();
    const std::string prefix = "#/$defs/";
    const Value* defs = root_.find("$defs");
    const Value* target = (r.rfind(prefix, 0) == 0 && defs) ? defs->find(r.substr(prefix.size())) : nullptr;
    if (!target) { errs.push_back(where + ": ismeretlen $ref: " + r); return; }
    check(*target, v, path, errs, depth + 1);
    return;
  }
  if (const Value* t = s.find("type")) {
    bool ok = false;
    std::string want;
    if (t->is_string()) { ok = type_matches(t->text(), v); want = t->text(); }
    else if (t->is_array()) {
      for (const auto& x : t->items()) {
        if (!x.is_string()) continue;
        ok |= type_matches(x.text(), v);
        want += (want.empty() ? "" : "|") + x.text();
      }
    }
    if (!ok) {
      errs.push_back(where + ": " + want + " kell, de " + type_name(v) +
                     (v.is_number() ? " (" + v.text() + ")" : std::string()) + " áll");
      return;
    }
  }
  if (const Value* en = s.find("enum"); en && en->is_array()) {
    bool found = false;
    for (const auto& x : en->items()) found |= (x == v);
    if (!found) errs.push_back(where + ": nem megengedett érték: " + dump(v));
  }
  if (v.is_number()) {
    auto cmp_fail = [&](const char* key, bool is_min) {
      const Value* lim = s.find(key);
      if (!lim || !lim->is_number()) return;
      auto a = v.as_fixed(), b = lim->as_fixed();
      if (!a || !b) return;
      if (is_min ? (*a < *b) : (*a > *b)) {
        errs.push_back(where + ": " + v.text() + (is_min ? " kisebb, mint " : " nagyobb, mint ") + lim->text());
      }
    };
    cmp_fail("minimum", true);
    cmp_fail("maximum", false);
  }
  if (v.is_object()) {
    const Value* props = s.find("properties");
    if (const Value* req = s.find("required"); req && req->is_array()) {
      for (const auto& r : req->items()) {
        if (r.is_string() && !v.find(r.text())) errs.push_back(where + ": hiányzik: " + r.text());
      }
    }
    const Value* addl = s.find("additionalProperties");
    for (const auto& m : v.members()) {
      const std::string sub = path.empty() ? m.first : path + "." + m.first;
      const Value* ps = props ? props->find(m.first) : nullptr;
      if (ps) { check(*ps, m.second, sub, errs, depth + 1); continue; }
      if (!m.first.empty() && m.first[0] == '_') continue;
      if (addl && addl->is_bool() && !addl->as_bool()) {
        errs.push_back(where + ": ismeretlen kulcs: " + m.first);
      } else if (addl && addl->is_object()) {
        check(*addl, m.second, sub, errs, depth + 1);
      }
    }
  }
  if (v.is_array()) {
    if (const Value* it = s.find("items"); it && it->is_object()) {
      for (size_t i = 0; i < v.items().size(); ++i) {
        check(*it, v.items()[i], path + "[" + std::to_string(i) + "]", errs, depth + 1);
      }
    }
  }
}

}  // namespace totu::content
