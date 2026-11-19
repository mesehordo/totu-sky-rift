// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/data_editor.hpp"

#include <algorithm>

#include "sim/rules.hpp"

namespace totu::client {

using content::Value;
using K = ParamKind;

namespace {

constexpr int kRowH = 22, kPad = 8;

std::string sval(const Value& v, const char* k) {
  const Value* x = v.find(k);
  if (!x) return {};
  if (x->is_string() || x->is_number()) return x->text();
  if (x->is_bool()) return x->as_bool() ? "igen" : "nem";
  return content::dump(*x);
}

Value* member_at(Value& obj, int i) {
  if (!obj.is_object() || i < 0 || i >= static_cast<int>(obj.members().size())) return nullptr;
  return &obj.members()[static_cast<size_t>(i)].second;
}

Value* arr(Value& owner, const char* key) {
  Value* a = owner.find(key);
  if (!a || !a->is_array()) {
    owner.set(key, Value::array());
    a = owner.find(key);
  }
  return a;
}

std::string unique_member(const Value& obj, const std::string& prefix) {
  for (int i = 1;; ++i) {
    const std::string id = prefix + std::to_string(i);
    if (!obj.find(id)) return id;
  }
}

std::string unique_item(const Value& a, const std::string& prefix) {
  for (int i = 1;; ++i) {
    const std::string id = prefix + std::to_string(i);
    bool used = false;
    for (const auto& x : a.items()) used = used || sval(x, "id") == id;
    if (!used) return id;
  }
}

// pontozott út (pl. "cost.gold") olvasása és írása
const Value* get_path(const Value& v, const std::string& key) {
  const size_t dot = key.find('.');
  if (dot == std::string::npos) return v.find(key);
  const Value* inner = v.find(key.substr(0, dot));
  return inner && inner->is_object() ? inner->find(key.substr(dot + 1)) : nullptr;
}
void set_path(Value& v, const std::string& key, const Value* val) {   // val == nullptr: törlés
  const size_t dot = key.find('.');
  if (dot == std::string::npos) {
    if (val) v.set(key, *val);
    else v.erase(key);
    return;
  }
  const std::string head = key.substr(0, dot), tail = key.substr(dot + 1);
  Value* inner = v.find(head);
  if (!inner || !inner->is_object()) {
    if (!val) return;
    v.set(head, Value::object());
    inner = v.find(head);
  }
  if (val) inner->set(tail, *val);
  else inner->erase(tail);
  if (inner->members().empty()) v.erase(head);
}
bool decimal(const std::string& s) {
  if (s.empty() || s == "-" || s == ".") return false;
  bool dot = false;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '-' && i == 0) continue;
    if (s[i] == '.' && !dot) { dot = true; continue; }
    if (s[i] < '0' || s[i] > '9') return false;
  }
  return true;
}

std::vector<std::string> action_names() {
  std::vector<std::string> out;
  for (const auto& t : trigger_types(2)) out.emplace_back(t.type);
  return out;
}

}  // namespace

const char* DataEditor::section_key() const {
  switch (tab) {
    case kObjectives: return "objectives";
    case kVariables: return "variables";
    case kDialogues: return "dialogues";
    case kObjects: return "objects";
    default: return "cutscenes";
  }
}

bool DataEditor::edit(EditorDoc& d, const std::function<void(Value&)>& f) {
  const Value* cur = d.mission().find(section_key());
  Value sec = cur ? *cur : (tab == kObjectives ? Value::array() : Value::object());
  if (tab == kObjectives && !sec.is_array()) sec = Value::array();
  if (tab != kObjectives && !sec.is_object()) sec = Value::object();
  f(sec);
  std::string err;
  if (!d.set_section(section_key(), content::dump(sec), &err)) {
    status = err;
    return false;
  }
  return true;
}

Value* DataEditor::target(Value& sec) const {
  if (tab == kObjectives) {
    if (!sec.is_array() || sel1 < 0 || sel1 >= static_cast<int>(sec.items().size())) return nullptr;
    return &sec.items()[static_cast<size_t>(sel1)];
  }
  Value* el = member_at(sec, sel1);
  if (!el || tab == kVariables || tab == kObjects) return el;
  if (sel2 < 0) return el;
  Value* list = el->find(tab == kDialogues ? "nodes" : "steps");
  if (!list || !list->is_array() || sel2 >= static_cast<int>(list->items().size())) return nullptr;
  Value* n = &list->items()[static_cast<size_t>(sel2)];
  if (tab == kDialogues && sel3 >= 0) {
    Value* ch = n->find("choices");
    if (!ch || !ch->is_array() || sel3 >= static_cast<int>(ch->items().size())) return nullptr;
    return &ch->items()[static_cast<size_t>(sel3)];
  }
  return n;
}

const Value* DataEditor::target_c(const EditorDoc& d) const {
  const Value* sec = d.mission().find(section_key());
  return sec ? target(const_cast<Value&>(*sec)) : nullptr;
}

std::vector<ParamSpec> DataEditor::field_specs(const EditorDoc& d) const {
  switch (tab) {
    case kObjectives:
      if (sel1 < 0) return {};
      return {{"id", K::Text, false, {}}, {"kind", K::Enum, false, {"primary", "secondary", "hidden"}}, {"text", K::Text, false, {}},
              {"visible", K::Bool, true, {}}, {"at", K::Text, true, {}}, {"marker", K::Text, true, {}}};
    case kVariables:
      if (sel1 < 0) return {};
      return {{"name", K::Text, false, {}}, {"value", K::Int, false, {}}};
    case kDialogues:
      if (sel1 < 0) return {};
      if (sel2 < 0) return {{"name", K::Text, false, {}}};
      if (sel3 < 0) {
        return {{"id", K::Text, false, {}}, {"speaker", K::Def, true, {}}, {"portrait", K::Def, true, {}}, {"text", K::Text, false, {}},
                {"next", K::Enum, true, {}}, {"ticks", K::Int, true, {}}};
      }
      return {{"text", K::Text, false, {}}, {"next", K::Enum, true, {}}, {"var", K::Text, true, {}}, {"value", K::Int, true, {}}};
    case kObjects:
      if (sel1 < 0) return {};
      // a WC3 objektumszerkesztőjének alapmezői (a tizedes mezők szövegként, számmá alakítva)
      return {{"name", K::Text, false, {}},   {"base", K::Def, true, {}},       {"hp", K::Int, true, {}},
              {"armor", K::Int, true, {}},    {"damage", K::Text, true, {}},    {"range", K::Text, true, {}},
              {"cooldown", K::Int, true, {}}, {"speed", K::Text, true, {}},     {"sight", K::Int, true, {}},
              {"supply", K::Int, true, {}},   {"time", K::Int, true, {}},       {"cost.gold", K::Int, true, {}},
              {"cost.wood", K::Int, true, {}}, {"cost.meteor", K::Int, true, {}}};
    default: {
      if (sel1 < 0) return {};
      if (sel2 < 0) return {{"name", K::Text, false, {}}, {"length", K::Int, true, {}}};
      std::vector<ParamSpec> out = {{"t", K::Int, false, {}}, {"type", K::Enum, false, {}}};
      const Value* v = target_c(d);
      if (const TypeSpec* ts = v ? trigger_type(2, sval(*v, "type")) : nullptr) {
        for (const auto& p : ts->params) out.push_back(p);
      }
      return out;
    }
  }
}

std::vector<std::string> DataEditor::options(const EditorDoc& d, const ParamSpec& p) const {
  if (std::string(p.key) == "next" && tab == kDialogues) {   // a párbeszéd csomópontjai
    std::vector<std::string> out = {"-"};
    const Value* sec = d.mission().find("dialogues");
    const Value* el = sec ? member_at(const_cast<Value&>(*sec), sel1) : nullptr;
    if (const Value* ns = el ? el->find("nodes") : nullptr; ns && ns->is_array()) {
      for (const auto& n : ns->items()) out.push_back(sval(n, "id"));
    }
    return out;
  }
  if (std::string(p.key) == "type" && tab == kCutscenes) return action_names();
  return trig_.options_for(d, p.kind, p);
}

void DataEditor::set_field(EditorDoc& d, const ParamSpec& p, const std::string& value) {
  const std::string key = p.key;
  if (key == "name" && tab != kObjectives) {   // átnevezés (a sorrend marad)
    if (value.empty()) return;
    const int i = sel1;
    edit(d, [&](Value& sec) {
      if (i < 0 || i >= static_cast<int>(sec.members().size()) || sec.find(value)) return;
      sec.members()[static_cast<size_t>(i)].first = value;
    });
    return;
  }
  if (tab == kVariables && key == "value") {
    const int i = sel1;
    edit(d, [&](Value& sec) {
      if (Value* v = member_at(sec, i)) *v = Value::number(value.empty() ? "0" : value);
    });
    return;
  }
  edit(d, [&](Value& sec) {
    Value* t = target(sec);
    if (!t) return;
    if (key == "type") {   // lépés típusa: a „t” és a közös paraméterek maradnak
      const TypeSpec* ts = trigger_type(2, value);
      if (!ts) return;
      Value nv = Value::object();
      if (const Value* tt = t->find("t")) nv.set("t", *tt);
      nv.set("type", Value::string(value));
      for (const auto& q : ts->params) {
        if (const Value* old = t->find(q.key)) nv.set(q.key, *old);
      }
      *t = nv;
      return;
    }
    if (value == "-" || (value.empty() && p.optional)) {
      set_path(*t, key, nullptr);
    } else if (p.kind == K::Int || p.kind == K::Player) {
      const Value v = Value::number(value.empty() ? "0" : value);
      set_path(*t, key, &v);
    } else if (p.kind == K::Bool) {
      t->set(key, Value::boolean(value == "igen" || value == "true"));
    } else if (tab == kObjects && (key == "damage" || key == "range" || key == "speed")) {
      if (!decimal(value)) {
        status = "editor.trig.not_number";
        return;
      }
      const Value v = Value::number(value);
      set_path(*t, key, &v);
    } else {
      t->set(key, Value::string(value));
    }
  });
}

void DataEditor::add(EditorDoc& d, int level) {
  int created = -1;
  const int s1 = sel1, s2 = sel2;
  edit(d, [&](Value& sec) {
    if (level == 1) {
      if (tab == kObjectives) {
        Value o = Value::object();
        o.set("id", Value::string(unique_item(sec, "o")));
        o.set("kind", Value::string("primary"));
        o.set("text", Value::string("uj_cel"));
        sec.push(o);
        created = static_cast<int>(sec.items().size()) - 1;
      } else if (tab == kVariables) {
        sec.set(unique_member(sec, "valtozo"), Value::number("0"));
        created = static_cast<int>(sec.members().size()) - 1;
      } else if (tab == kObjects) {   // új, származtatott sablon az első harcos egységből (az alap átírható)
        std::string base_id;
        if (rules_) {
          for (const auto& def : rules_->defs) {
            if (!def.building && !def.worker && !def.hero && !def.faction.empty()) {
              base_id = def.id;
              break;
            }
          }
        }
        Value o = Value::object();
        o.set("base", Value::string(base_id));
        sec.set(unique_member(sec, "uj_egyseg"), o);
        created = static_cast<int>(sec.members().size()) - 1;
      } else if (tab == kDialogues) {
        const auto v = content::parse(R"({"nodes": [{"id": "n1", "text": "uj_szoveg"}]})");
        sec.set(unique_member(sec, "parbeszed"), *v);
        created = static_cast<int>(sec.members().size()) - 1;
      } else {
        const auto v = content::parse(R"({"steps": [{"t": 0, "type": "letterbox", "on": true}]})");
        sec.set(unique_member(sec, "atvezeto"), *v);
        created = static_cast<int>(sec.members().size()) - 1;
      }
      return;
    }
    Value* el = member_at(sec, s1);
    if (!el) return;
    if (level == 2) {
      Value* l = arr(*el, tab == kDialogues ? "nodes" : "steps");
      Value n = Value::object();
      if (tab == kDialogues) {
        n.set("id", Value::string(unique_item(*l, "n")));
        n.set("text", Value::string("uj_szoveg"));
      } else {
        int64_t last = 0;
        for (const auto& st : l->items()) last = std::max<int64_t>(last, st.find("t") ? st.find("t")->as_int().value_or(0) : 0);
        n.set("t", Value::number(std::to_string(last + 30)));
        n.set("type", Value::string("message"));
        n.set("text", Value::string("uj_szoveg"));
      }
      l->push(n);
      created = static_cast<int>(l->items().size()) - 1;
      return;
    }
    if (level == 3 && tab == kDialogues) {
      Value* l = el->find("nodes");
      if (!l || !l->is_array() || s2 < 0 || s2 >= static_cast<int>(l->items().size())) return;
      Value* ch = arr(l->items()[static_cast<size_t>(s2)], "choices");
      Value c = Value::object();
      c.set("text", Value::string("uj_valasz"));
      ch->push(c);
      created = static_cast<int>(ch->items().size()) - 1;
    }
  });
  if (created < 0) return;
  if (level == 1) {
    sel1 = created;
    sel2 = sel3 = -1;
  } else if (level == 2) {
    sel2 = created;
    sel3 = -1;
  } else {
    sel3 = created;
  }
}

void DataEditor::remove(EditorDoc& d, int level) {
  const int s1 = sel1, s2 = sel2, s3 = sel3;
  bool ok = false;
  edit(d, [&](Value& sec) {
    if (level == 1) {
      if (tab == kObjectives && s1 >= 0 && s1 < static_cast<int>(sec.items().size())) {
        sec.items().erase(sec.items().begin() + s1);
        ok = true;
      } else if (tab != kObjectives && s1 >= 0 && s1 < static_cast<int>(sec.members().size())) {
        sec.members().erase(sec.members().begin() + s1);
        ok = true;
      }
      return;
    }
    Value* el = member_at(sec, s1);
    Value* l = el ? el->find(tab == kDialogues ? "nodes" : "steps") : nullptr;
    if (!l || !l->is_array() || s2 < 0 || s2 >= static_cast<int>(l->items().size())) return;
    if (level == 2) {
      if (l->items().size() <= 1) return;   // legalább egy csomópont / lépés kell
      l->items().erase(l->items().begin() + s2);
      ok = true;
      return;
    }
    Value* ch = l->items()[static_cast<size_t>(s2)].find("choices");
    if (ch && ch->is_array() && s3 >= 0 && s3 < static_cast<int>(ch->items().size())) {
      ch->items().erase(ch->items().begin() + s3);
      if (ch->items().empty()) l->items()[static_cast<size_t>(s2)].erase("choices");
      ok = true;
    }
  });
  if (!ok) return;
  if (level == 1) sel1 = std::max(-1, sel1 - 1), sel2 = sel3 = -1;
  else if (level == 2) sel2 = std::max(-1, sel2 - 1), sel3 = -1;
  else sel3 = -1;
}

void DataEditor::layout(int w, int h, const EditorDoc& d) {
  trig_.init(rules_);
  window = Rect{std::max(0, w / 2 - 520), std::max(0, h / 2 - 330), std::min(w, 1040), std::min(h, 660)};
  tabs.clear();
  const char* tab_keys[] = {"editor.data.objectives", "editor.data.variables", "editor.data.dialogues", "editor.data.cutscenes",
                            "editor.data.objects"};
  for (int i = 0; i < kTabs; ++i) tabs.push_back(Button{Rect{window.x + kPad + i * 104, window.y + kPad, 100, 24}, tab_keys[i], i, i == tab});
  tools.clear();
  int tx = window.x + kPad + kTabs * 104 + 12;
  auto tool = [&](const char* key, int id) {
    tools.push_back(Button{Rect{tx, window.y + kPad, 88, 24}, key, id});
    tx += 92;
  };
  tool("editor.data.add", 11);
  tool("editor.data.remove", 12);
  if (tab == kDialogues || tab == kCutscenes) {
    tool(tab == kDialogues ? "editor.data.add_node" : "editor.data.add_step", 21);
    tool("editor.data.remove2", 22);
  }
  tools.push_back(Button{Rect{window.right() - kPad - 90, window.y + kPad, 90, 24}, "editor.trig.close5", 99});
  const bool two = tab == kDialogues || tab == kCutscenes;
  list1_box = Rect{window.x + kPad, window.y + 44, 220, window.h - 44 - kPad};
  list2_box = two ? Rect{list1_box.right() + kPad, list1_box.y, 300, list1_box.h} : Rect{list1_box.right(), list1_box.y, 0, list1_box.h};
  fields_box = Rect{list2_box.right() + kPad, list1_box.y, window.right() - list2_box.right() - 2 * kPad, list1_box.h};
  const Value* sec = d.mission().find(section_key());
  list1.clear();
  list2.clear();
  list3.clear();
  fields.clear();
  if (sec) {
    if (tab == kObjectives && sec->is_array()) {
      for (size_t i = 0; i < sec->items().size(); ++i) {
        list1.push_back(Button{Rect{list1_box.x, list1_box.y + static_cast<int>(i) * kRowH, list1_box.w, kRowH - 2}, sval(sec->items()[i], "id"),
                               static_cast<int>(i), static_cast<int>(i) == sel1});
      }
    } else if (sec->is_object()) {
      for (size_t i = 0; i < sec->members().size(); ++i) {
        std::string label = sec->members()[i].first;
        if (tab == kVariables) label += " = " + sec->members()[i].second.text();
        if (tab == kObjects && !sval(sec->members()[i].second, "base").empty()) label += " (" + sval(sec->members()[i].second, "base") + ")";
        list1.push_back(Button{Rect{list1_box.x, list1_box.y + static_cast<int>(i) * kRowH, list1_box.w, kRowH - 2}, label,
                               static_cast<int>(i), static_cast<int>(i) == sel1});
      }
    }
    if (sel1 >= static_cast<int>(list1.size())) sel1 = static_cast<int>(list1.size()) - 1;
    if (two && sel1 >= 0) {
      const Value* el = member_at(const_cast<Value&>(*sec), sel1);
      const Value* l = el ? el->find(tab == kDialogues ? "nodes" : "steps") : nullptr;
      if (l && l->is_array()) {
        for (size_t i = 0; i < l->items().size(); ++i) {
          const Value& n = l->items()[i];
          std::string label = tab == kDialogues ? sval(n, "id") + ": " + sval(n, "text")
                                                : sval(n, "t") + "  " + TriggerEditor::describe(2, n, sval(n, "type"));
          list2.push_back(Button{Rect{list2_box.x, list2_box.y + static_cast<int>(i) * kRowH, list2_box.w, kRowH - 2}, label,
                                 static_cast<int>(i), static_cast<int>(i) == sel2});
        }
        if (sel2 >= static_cast<int>(list2.size())) sel2 = static_cast<int>(list2.size()) - 1;
      }
    }
  }
  // mezők
  const auto specs = field_specs(d);
  const Value* t = target_c(d);
  int fy = fields_box.y;
  for (size_t i = 0; i < specs.size(); ++i) {
    std::string val;
    const std::string key = specs[i].key;
    if (key == "name" && tab != kObjectives && sec && sel1 >= 0 && sel1 < static_cast<int>(sec->members().size())) {
      val = sec->members()[static_cast<size_t>(sel1)].first;
    } else if (tab == kVariables && key == "value" && t) {
      val = t->text();
    } else if (t) {
      const Value* x = get_path(*t, key);
      val = x ? (x->is_string() || x->is_number() ? x->text() : content::dump(*x)) : std::string();
    }
    fields.push_back(Button{Rect{fields_box.x, fy, fields_box.w, kRowH}, key + ": " + (val.empty() ? "-" : val), static_cast<int>(i),
                            field.open && field.param == static_cast<int>(i)});
    fy += kRowH + 4;
  }
  // párbeszéd-csomópont: a válaszok listája és gombjai
  if (tab == kDialogues && sel2 >= 0) {
    const Value* el = sec ? member_at(const_cast<Value&>(*sec), sel1) : nullptr;
    const Value* ns = el ? el->find("nodes") : nullptr;
    const Value* n = ns && ns->is_array() && sel2 < static_cast<int>(ns->items().size()) ? &ns->items()[static_cast<size_t>(sel2)] : nullptr;
    fy += kPad;
    list3.push_back(Button{Rect{fields_box.x, fy, 140, kRowH}, "editor.data.add_choice", 31});
    list3.push_back(Button{Rect{fields_box.x + 150, fy, 140, kRowH}, "editor.data.remove_choice", 32});
    list3.push_back(Button{Rect{fields_box.x + 300, fy, 140, kRowH}, "editor.data.node_fields", 33, sel3 < 0});
    fy += kRowH + 4;
    if (const Value* ch = n ? n->find("choices") : nullptr; ch && ch->is_array()) {
      for (size_t i = 0; i < ch->items().size(); ++i) {
        list3.push_back(Button{Rect{fields_box.x, fy, fields_box.w, kRowH - 2},
                               std::to_string(i + 1) + ". " + sval(ch->items()[i], "text") + " → " + sval(ch->items()[i], "next"),
                               100 + static_cast<int>(i), static_cast<int>(i) == sel3});
        fy += kRowH;
      }
    }
  }
  popup_items.clear();
  if (popup.open) {
    popup_box = Rect{window.x + window.w / 2 - 180, window.y + 60, 360, window.h - 120};
    const auto f = filtered();
    const int room = std::max(1, (popup_box.h - 40) / kRowH);
    popup.scroll = std::clamp(popup.scroll, 0, std::max(0, static_cast<int>(f.size()) - room));
    for (int i = popup.scroll; i < static_cast<int>(f.size()) && i - popup.scroll < room; ++i) {
      popup_items.push_back(Button{Rect{popup_box.x + kPad, popup_box.y + 34 + (i - popup.scroll) * kRowH, popup_box.w - 2 * kPad, kRowH - 2},
                                   f[static_cast<size_t>(i)], i});
    }
  }
}

std::vector<std::string> DataEditor::filtered() const {
  std::vector<std::string> out;
  for (const auto& o : popup.options) {
    if (popup.filter.empty() || o.find(popup.filter) != std::string::npos) out.push_back(o);
  }
  return out;
}

bool DataEditor::mouse_down(EditorDoc& d, float x, float y, bool) {
  if (!open) return false;
  const auto specs = field_specs(d);
  if (popup.open) {
    for (const auto& b : popup_items) {
      if (b.r.contains(x, y)) {
        popup.open = false;
        if (popup.target >= 0 && popup.target < static_cast<int>(specs.size())) set_field(d, specs[static_cast<size_t>(popup.target)], b.label);
        return true;
      }
    }
    if (!popup_box.contains(x, y)) popup.open = false;
    return true;
  }
  field.open = false;
  for (const auto& b : tabs) {
    if (b.r.contains(x, y)) {
      tab = b.id;
      sel1 = sel2 = sel3 = -1;
      return true;
    }
  }
  for (const auto& b : tools) {
    if (!b.r.contains(x, y)) continue;
    if (b.id == 99) open = false;
    else if (b.id == 11) add(d, 1);
    else if (b.id == 12) remove(d, 1);
    else if (b.id == 21 && sel1 >= 0) add(d, 2);
    else if (b.id == 22 && sel2 >= 0) remove(d, 2);
    return true;
  }
  for (const auto& b : list1) {
    if (b.r.contains(x, y)) {
      sel1 = b.id;
      sel2 = sel3 = -1;
      return true;
    }
  }
  for (const auto& b : list2) {
    if (b.r.contains(x, y)) {
      sel2 = b.id;
      sel3 = -1;
      return true;
    }
  }
  for (const auto& b : list3) {
    if (!b.r.contains(x, y)) continue;
    if (b.id == 31) add(d, 3);
    else if (b.id == 32 && sel3 >= 0) remove(d, 3);
    else if (b.id == 33) sel3 = -1;
    else if (b.id >= 100) sel3 = b.id - 100;
    return true;
  }
  for (const auto& b : fields) {
    if (!b.r.contains(x, y) || b.id >= static_cast<int>(specs.size())) continue;
    const ParamSpec& p = specs[static_cast<size_t>(b.id)];
    if (p.kind == K::Int || p.kind == K::Text) {
      field = TriggerEditor::Field{true, b.id, b.label.substr(b.label.find(": ") + 2)};
      if (field.text == "-") field.text.clear();
    } else if (p.kind == K::Bool) {
      const Value* t = target_c(d);
      const Value* cur = t ? t->find(p.key) : nullptr;
      set_field(d, p, cur && cur->is_bool() && cur->as_bool() ? "nem" : "igen");
    } else {
      popup = TriggerEditor::Popup{};
      popup.open = true;
      popup.title = p.key;
      popup.options = options(d, p);
      if (p.optional && (popup.options.empty() || popup.options.front() != "-")) popup.options.insert(popup.options.begin(), "-");
      popup.target = b.id;
    }
    return true;
  }
  return window.contains(x, y);
}

void DataEditor::wheel(float x, float y, int dy) {
  if (popup.open && popup_box.contains(x, y)) popup.scroll = std::max(0, popup.scroll - dy * 3);
}

void DataEditor::text_input(const std::string& utf8) {
  if (popup.open) {
    popup.filter += utf8;
    popup.scroll = 0;
  } else if (field.open) {
    field.text += utf8;
  }
}

bool DataEditor::key(EditorDoc& d, int k) {
  if (!open) return false;
  auto pop_utf8 = [](std::string& s) {
    while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80) s.pop_back();
    if (!s.empty()) s.pop_back();
  };
  const auto specs = field_specs(d);
  if (popup.open) {
    if (k == TriggerEditor::kEscape) popup.open = false;
    else if (k == TriggerEditor::kBackspace) pop_utf8(popup.filter);
    else if (k == TriggerEditor::kEnter) {
      const auto f = filtered();
      popup.open = false;
      if (!f.empty() && popup.target >= 0 && popup.target < static_cast<int>(specs.size())) set_field(d, specs[static_cast<size_t>(popup.target)], f.front());
    }
    return true;
  }
  if (field.open) {
    if (k == TriggerEditor::kEscape) field.open = false;
    else if (k == TriggerEditor::kBackspace) pop_utf8(field.text);
    else if (k == TriggerEditor::kEnter) {
      field.open = false;
      if (field.param >= 0 && field.param < static_cast<int>(specs.size())) {
        const ParamSpec& p = specs[static_cast<size_t>(field.param)];
        if (p.kind == K::Int && !field.text.empty() &&
            (field.text.find_first_not_of("-0123456789") != std::string::npos || field.text == "-")) {
          status = "editor.trig.not_number";
        } else {
          set_field(d, p, field.text);
        }
      }
    }
    return true;
  }
  if (k == TriggerEditor::kEscape) open = false;
  return true;
}

}  // namespace totu::client
