// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/trigger_editor.hpp"

#include <algorithm>

#include "sim/rules.hpp"

namespace totu::client {

using content::Value;

namespace {

using K = ParamKind;
const std::vector<const char*> kOps = {"ge", "gt", "eq", "ne", "lt", "le"};

const std::vector<TypeSpec> kEvents = {
    {"start", {}},
    {"time", {{"at", K::Int, true, {}}, {"every", K::Int, true, {}}}},
    {"enter_region", {{"region", K::Region, false, {}}, {"owner", K::Player, true, {}}, {"unit", K::UnitName, true, {}}}},
    {"leave_region", {{"region", K::Region, false, {}}, {"owner", K::Player, true, {}}, {"unit", K::UnitName, true, {}}}},
    {"unit_dies", {{"owner", K::Player, true, {}}, {"unit", K::UnitName, true, {}}, {"def", K::Def, true, {}}}},
    {"count", {{"owner", K::Player, false, {}}, {"def", K::Def, true, {}}, {"op", K::Enum, false, kOps}, {"value", K::Int, false, {}}}},
    {"always", {}},
    {"dialogue_end", {{"id", K::Dialogue, false, {}}}},
    {"choice", {{"id", K::Dialogue, false, {}}, {"value", K::Int, true, {}}}},
    {"objective_done", {{"id", K::Objective, false, {}}}},
    {"timer", {{"id", K::Text, false, {}}}},
    {"cutscene_end", {{"id", K::Cutscene, false, {}}}},
};
const std::vector<TypeSpec> kConds = {
    {"var", {{"var", K::Text, false, {}}, {"op", K::Enum, false, kOps}, {"value", K::Int, false, {}}}},
    {"alive", {{"unit", K::UnitName, false, {}}}},
    {"dead", {{"unit", K::UnitName, false, {}}}},
    {"in_region", {{"unit", K::UnitName, false, {}}, {"region", K::Region, false, {}}}},
    {"count", {{"owner", K::Player, false, {}}, {"def", K::Def, true, {}}, {"op", K::Enum, false, kOps}, {"value", K::Int, false, {}}}},
    {"objective", {{"id", K::Objective, false, {}}, {"state", K::Enum, false, {"done", "active", "failed", "hidden"}}}},
    {"resource", {{"owner", K::Player, false, {}}, {"res", K::Enum, false, {"gold", "wood", "meteor", "faith"}}, {"op", K::Enum, false, kOps}, {"value", K::Int, false, {}}}},
    {"tick", {{"op", K::Enum, false, kOps}, {"value", K::Int, false, {}}}},
};
const std::vector<TypeSpec> kActions = {
    {"dialogue", {{"id", K::Dialogue, false, {}}}},
    {"objective", {{"id", K::Objective, false, {}}, {"state", K::Enum, false, {"show", "done", "fail", "hide"}}}},
    {"spawn", {{"def", K::Def, false, {}}, {"owner", K::Player, false, {}}, {"count", K::Int, true, {}}, {"region", K::Region, true, {}}, {"point", K::Point, true, {}}, {"name", K::Text, true, {}}}},
    {"order", {{"unit", K::UnitName, false, {}}, {"order", K::Enum, false, {"move", "attack_move", "stop", "hold"}}, {"region", K::Region, true, {}}, {"point", K::Point, true, {}}, {"path", K::Path, true, {}}}},
    {"kill", {{"unit", K::UnitName, false, {}}}},
    {"remove", {{"unit", K::UnitName, false, {}}}},
    {"reveal", {{"player", K::Player, false, {}}, {"region", K::Region, false, {}}, {"ticks", K::Int, true, {}}}},
    {"resources", {{"player", K::Player, false, {}}, {"gold", K::Int, true, {}}, {"wood", K::Int, true, {}}, {"meteor", K::Int, true, {}}, {"faith", K::Int, true, {}}}},
    {"set_var", {{"var", K::Text, false, {}}, {"value", K::Int, false, {}}}},
    {"add_var", {{"var", K::Text, false, {}}, {"value", K::Int, true, {}}}},
    {"enable", {{"trigger", K::Trigger, false, {}}}},
    {"disable", {{"trigger", K::Trigger, false, {}}}},
    {"timer", {{"id", K::Text, false, {}}, {"ticks", K::Int, false, {}}}},
    {"cutscene", {{"id", K::Cutscene, false, {}}}},
    {"camera", {{"point", K::Point, false, {}}, {"ticks", K::Int, true, {}}}},
    {"message", {{"text", K::Text, false, {}}, {"ticks", K::Int, true, {}}}},
    {"letterbox", {{"on", K::Bool, false, {}}}},
    {"subtitle", {{"speaker", K::Text, true, {}}, {"text", K::Text, false, {}}, {"ticks", K::Int, true, {}}}},
    {"win", {}},
    {"lose", {}},
};
const char* kSectionKeys[3] = {"event", "conditions", "actions"};
constexpr int kRowH = 22, kPad = 8;

std::string sval(const Value& v, const char* k) {
  const Value* x = v.find(k);
  if (!x) return {};
  if (x->is_string() || x->is_number()) return x->text();
  if (x->is_bool()) return x->as_bool() ? "igen" : "nem";
  return content::dump(*x);
}

void ids_of(const Value* l, std::vector<std::string>& out) {
  if (!l || !l->is_array()) return;
  for (const auto& x : l->items()) {
    const std::string id = sval(x, "id");
    if (!id.empty()) out.push_back(id);
  }
}

}  // namespace

const std::vector<TypeSpec>& trigger_types(int section) { return section == 0 ? kEvents : section == 1 ? kConds : kActions; }

const TypeSpec* trigger_type(int section, const std::string& type) {
  for (const auto& t : trigger_types(section)) {
    if (type == t.type) return &t;
  }
  return nullptr;
}

std::string TriggerEditor::describe(int sec, const Value& v, const std::string& type_label) {
  const TypeSpec* ts = trigger_type(sec, sval(v, "type"));
  std::string s = type_label;
  if (!ts) return s + " " + content::dump(v);   // a szerkesztő nem ismeri (pl. all/any/not): nyersen
  std::string args;
  for (const auto& p : ts->params) {
    const std::string val = sval(v, p.key);
    if (val.empty()) continue;
    if (!args.empty()) args += ", ";
    args += std::string(p.key) + " = " + val;
  }
  return args.empty() ? s : s + " (" + args + ")";
}

std::vector<std::string> TriggerEditor::options_for(const EditorDoc& d, ParamKind kind, const ParamSpec& p) const {
  std::vector<std::string> out;
  const Value& m = d.mission();
  switch (kind) {
    case K::Enum:
      for (const char* o : p.options) out.emplace_back(o);
      break;
    case K::Region: ids_of(m.find("regions"), out); break;
    case K::Point:
      ids_of(m.find("points"), out);
      ids_of(m.find("cameras"), out);
      break;
    case K::Path: ids_of(m.find("paths"), out); break;
    case K::Objective: ids_of(m.find("objectives"), out); break;
    case K::Trigger: ids_of(m.find("triggers"), out); break;
    case K::Dialogue:
    case K::Cutscene:
      if (const Value* o = m.find(kind == K::Dialogue ? "dialogues" : "cutscenes"); o && o->is_object()) {
        for (const auto& [k, v] : o->members()) out.push_back(k);
      }
      break;
    case K::UnitName: {
      if (const Value* us = m.find("units"); us && us->is_array()) {
        for (const auto& u : us->items()) {
          const std::string n = sval(u, "name");
          if (!n.empty()) out.push_back(n);
        }
      }
      if (const Value* ts = m.find("triggers"); ts && ts->is_array()) {   // a spawn akciók nevei
        for (const auto& t : ts->items()) {
          if (const Value* as = t.find("actions"); as && as->is_array()) {
            for (const auto& a : as->items()) {
              if (sval(a, "type") == "spawn" && !sval(a, "name").empty()) out.push_back(sval(a, "name"));
            }
          }
        }
      }
      break;
    }
    case K::Def:
      if (rules_) {
        for (const auto& def : rules_->defs) out.push_back(def.id);
      }
      break;
    case K::Player:
      for (int i = 0; i <= 8; ++i) out.push_back(std::to_string(i));
      break;
    default: break;
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  if (p.optional) out.insert(out.begin(), "—");
  return out;
}

std::vector<std::string> TriggerEditor::filtered() const {
  std::vector<std::string> out;
  for (const auto& o : popup.options) {
    if (popup.filter.empty() || o.find(popup.filter) != std::string::npos) out.push_back(o);
  }
  return out;
}

bool TriggerEditor::edit(EditorDoc& d, const std::function<void(Value&)>& f) {
  Value ts = d.mission().find("triggers") && d.mission().find("triggers")->is_array() ? *d.mission().find("triggers") : Value::array();
  f(ts);
  std::string err;
  if (!d.set_section("triggers", content::dump(ts), &err)) {
    status = err;
    return false;
  }
  return true;
}

Value* TriggerEditor::current(Value& ts, int sec, int idx) const {
  if (trigger < 0 || trigger >= static_cast<int>(ts.items().size())) return nullptr;
  Value& t = ts.items()[static_cast<size_t>(trigger)];
  if (sec == 0) return t.find("event");
  Value* l = t.find(kSectionKeys[sec]);
  if (!l || !l->is_array() || idx < 0 || idx >= static_cast<int>(l->items().size())) return nullptr;
  return &l->items()[static_cast<size_t>(idx)];
}

const Value* TriggerEditor::current_c(const EditorDoc& d) const {
  const Value* ts = d.mission().find("triggers");
  if (!ts || !ts->is_array() || line < 0) return nullptr;
  return current(const_cast<Value&>(*ts), section, line);
}

void TriggerEditor::layout(int w, int h, const EditorDoc& d) {
  window = Rect{std::max(0, w / 2 - 520), std::max(0, h / 2 - 330), std::min(w, 1040), std::min(h, 660)};
  list_box = Rect{window.x + kPad, window.y + 40, 230, window.h - 40 - kPad};
  body_box = Rect{list_box.right() + kPad, window.y + 40, window.right() - list_box.right() - 2 * kPad, window.h - 200};
  params_box = Rect{body_box.x, body_box.bottom() + kPad, body_box.w, window.bottom() - body_box.bottom() - 2 * kPad};
  buttons.clear();
  const char* labels[] = {"editor.trig.new", "editor.trig.delete", "editor.trig.enabled", "editor.trig.repeat", "editor.trig.close"};
  for (int i = 0; i < 5; ++i) buttons.push_back(Button{Rect{window.x + kPad + i * 150, window.y + kPad, 140, 24}, labels[i], i + 1});
  list_items.clear();
  const Value* ts = d.mission().find("triggers");
  const int n = ts && ts->is_array() ? static_cast<int>(ts->items().size()) : 0;
  if (trigger >= n) trigger = n - 1;
  const int room = std::max(1, list_box.h / kRowH);
  list_scroll_ = std::clamp(list_scroll_, 0, std::max(0, n - room));
  for (int i = list_scroll_; i < n && i - list_scroll_ < room; ++i) {
    const Value& t = ts->items()[static_cast<size_t>(i)];
    const Value* en = t.find("enabled");
    const bool on = !en || !en->is_bool() || en->as_bool();
    list_items.push_back(Button{Rect{list_box.x, list_box.y + (i - list_scroll_) * kRowH, list_box.w, kRowH - 2},
                                (on ? "" : "(ki) ") + sval(t, "id"), i});
  }
  rows.clear();
  param_buttons.clear();
  if (trigger >= 0 && ts) {
    const Value& t = ts->items()[static_cast<size_t>(trigger)];
    int y = body_box.y;
    for (int sec = 0; sec < 3; ++sec) {
      rows.push_back(Row{Rect{body_box.x, y, body_box.w, kRowH - 2}, kSectionKeys[sec], sec, -1, false});
      y += kRowH;
      std::vector<const Value*> items;
      if (sec == 0) {
        if (const Value* e = t.find("event")) items.push_back(e);
      } else if (const Value* l = t.find(kSectionKeys[sec]); l && l->is_array()) {
        for (const auto& x : l->items()) items.push_back(&x);
      }
      for (size_t i = 0; i < items.size(); ++i) {
        rows.push_back(Row{Rect{body_box.x + 16, y, body_box.w - 16, kRowH - 2}, sval(*items[i], "type"), sec, static_cast<int>(i),
                           sec == section && static_cast<int>(i) == line});
        y += kRowH;
      }
    }
    // a kijelölt sor paraméterei
    if (const Value* v = current_c(d)) {
      if (const TypeSpec* tsp = trigger_type(section, sval(*v, "type"))) {
        const int cols = 2, bw = (params_box.w - kPad) / cols;
        for (size_t i = 0; i < tsp->params.size(); ++i) {
          const std::string val = sval(*v, tsp->params[i].key);
          param_buttons.push_back(Button{Rect{params_box.x + static_cast<int>(i % cols) * (bw + kPad),
                                              params_box.y + static_cast<int>(i / cols) * (kRowH + 4), bw, kRowH},
                                         std::string(tsp->params[i].key) + ": " + (val.empty() ? "—" : val), static_cast<int>(i)});
        }
      }
    }
  }
  popup_items.clear();
  if (popup.open) {
    popup_box = Rect{window.x + window.w / 2 - 180, window.y + 60, 360, window.h - 120};
    const auto f = filtered();
    const int room2 = std::max(1, (popup_box.h - 40) / kRowH);
    popup.scroll = std::clamp(popup.scroll, 0, std::max(0, static_cast<int>(f.size()) - room2));
    for (int i = popup.scroll; i < static_cast<int>(f.size()) && i - popup.scroll < room2; ++i) {
      popup_items.push_back(Button{Rect{popup_box.x + kPad, popup_box.y + 34 + (i - popup.scroll) * kRowH, popup_box.w - 2 * kPad, kRowH - 2}, f[static_cast<size_t>(i)], i});
    }
  }
}

void TriggerEditor::open_param(EditorDoc& d, int param) {
  const Value* v = current_c(d);
  const TypeSpec* tsp = v ? trigger_type(section, sval(*v, "type")) : nullptr;
  if (!tsp || param < 0 || param >= static_cast<int>(tsp->params.size())) return;
  const ParamSpec& p = tsp->params[static_cast<size_t>(param)];
  if (p.kind == K::Int || p.kind == K::Text) {
    field = Field{true, param, sval(*v, p.key)};
    return;
  }
  if (p.kind == K::Bool) {
    const Value* b = v->find(p.key);
    const bool now = b && b->is_bool() && b->as_bool();
    const int sec = section, ln = line;
    edit(d, [&](Value& ts) {
      if (Value* c = current(ts, sec, ln)) c->set(p.key, Value::boolean(!now));
    });
    return;
  }
  popup = Popup{};
  popup.open = true;
  popup.title = p.key;
  popup.options = options_for(d, p.kind, p);
  popup.target = param;
}

void TriggerEditor::choose(EditorDoc& d, const std::string& value) {
  const Popup pp = popup;
  popup.open = false;
  if (pp.target == -2) {   // új sor (vagy az esemény típusa)
    const TypeSpec* tsp = trigger_type(pp.section, value);
    if (!tsp) return;
    Value nv = Value::object();
    nv.set("type", Value::string(value));
    for (const auto& p : tsp->params) {   // a kötelező paraméterek alapértéke
      if (p.optional) continue;
      const auto opts = options_for(d, p.kind, p);
      if (p.kind == K::Int || p.kind == K::Player) nv.set(p.key, Value::number(p.kind == K::Player ? "1" : "0"));
      else if (p.kind == K::Bool) nv.set(p.key, Value::boolean(true));
      else if (p.kind == K::Text) nv.set(p.key, Value::string("uj"));
      else nv.set(p.key, Value::string(opts.empty() ? std::string() : opts.front()));
    }
    const int sec = pp.section;
    int added = 0;
    edit(d, [&](Value& ts) {
      if (trigger < 0 || trigger >= static_cast<int>(ts.items().size())) return;
      Value& t = ts.items()[static_cast<size_t>(trigger)];
      if (sec == 0) {
        t.set("event", nv);
        return;
      }
      if (!t.find(kSectionKeys[sec]) || !t.find(kSectionKeys[sec])->is_array()) t.set(kSectionKeys[sec], Value::array());
      t.find(kSectionKeys[sec])->push(nv);
      added = static_cast<int>(t.find(kSectionKeys[sec])->items().size()) - 1;
    });
    section = sec;
    line = sec == 0 ? 0 : added;
    return;
  }
  const int sec = section, ln = line;
  if (pp.target == -1) {   // a sor típusának cseréje (a közös paraméterek maradnak)
    const TypeSpec* tsp = trigger_type(sec, value);
    if (!tsp) return;
    edit(d, [&](Value& ts) {
      Value* c = current(ts, sec, ln);
      if (!c) return;
      Value nv = Value::object();
      nv.set("type", Value::string(value));
      for (const auto& p : tsp->params) {
        if (const Value* old = c->find(p.key)) nv.set(p.key, *old);
      }
      *c = nv;
    });
    return;
  }
  const Value* v = current_c(d);
  const TypeSpec* tsp = v ? trigger_type(sec, sval(*v, "type")) : nullptr;
  if (!tsp || pp.target >= static_cast<int>(tsp->params.size())) return;
  const ParamSpec& p = tsp->params[static_cast<size_t>(pp.target)];
  edit(d, [&](Value& ts) {
    Value* c = current(ts, sec, ln);
    if (!c) return;
    if (value == "—") c->erase(p.key);
    else if (p.kind == K::Player) c->set(p.key, Value::number(value));
    else c->set(p.key, Value::string(value));
  });
}

bool TriggerEditor::mouse_down(EditorDoc& d, float x, float y, bool right) {
  if (!open) return false;
  if (popup.open) {
    for (const auto& b : popup_items) {
      if (b.r.contains(x, y)) {
        choose(d, b.label);
        return true;
      }
    }
    if (!popup_box.contains(x, y)) popup.open = false;   // mellékattintás: bezár
    return true;
  }
  if (field.open && !params_box.contains(x, y)) field.open = false;
  for (const auto& b : buttons) {
    if (!b.r.contains(x, y)) continue;
    if (b.id == 1) {   // új trigger
      const std::string id = d.unique_id("triggers", "t");
      edit(d, [&](Value& ts) {
        Value t = Value::object();
        t.set("id", Value::string(id));
        Value ev = Value::object();
        ev.set("type", Value::string("start"));
        t.set("event", ev);
        t.set("conditions", Value::array());
        t.set("actions", Value::array());
        ts.push(t);
      });
      const Value* ts = d.mission().find("triggers");
      trigger = ts ? static_cast<int>(ts->items().size()) - 1 : -1;
      section = 0;
      line = 0;
    } else if (b.id == 2 && trigger >= 0) {
      const int t = trigger;
      edit(d, [&](Value& ts) { ts.items().erase(ts.items().begin() + t); });
      trigger = std::max(-1, t - 1);
      line = -1;
    } else if ((b.id == 3 || b.id == 4) && trigger >= 0) {
      const char* key = b.id == 3 ? "enabled" : "repeat";
      const int t = trigger;
      edit(d, [&](Value& ts) {
        Value& tv = ts.items()[static_cast<size_t>(t)];
        const Value* cur = tv.find(key);
        const bool now = cur && cur->is_bool() ? cur->as_bool() : (b.id == 3);
        tv.set(key, Value::boolean(!now));
      });
    } else if (b.id == 5) {
      open = false;
    }
    return true;
  }
  for (const auto& b : list_items) {
    if (b.r.contains(x, y)) {
      trigger = b.id;
      section = 0;
      line = 0;
      field.open = false;
      return true;
    }
  }
  for (const auto& r : rows) {
    if (!r.r.contains(x, y)) continue;
    if (r.index < 0) {   // fejléc: új sor felvétele (eseménynél: a típus cseréje)
      popup = Popup{};
      popup.open = true;
      popup.title = kSectionKeys[r.section];
      for (const auto& t : trigger_types(r.section)) popup.options.emplace_back(t.type);
      popup.target = -2;
      popup.section = r.section;
    } else if (r.selected || right) {   // a kijelölt sor újra (vagy jobb gomb): a típus választása
      section = r.section;
      line = r.index;
      popup = Popup{};
      popup.open = true;
      popup.title = "type";
      for (const auto& t : trigger_types(r.section)) popup.options.emplace_back(t.type);
      popup.target = -1;
    } else {
      section = r.section;
      line = r.index;
      field.open = false;
    }
    return true;
  }
  for (const auto& b : param_buttons) {
    if (b.r.contains(x, y)) {
      open_param(d, b.id);
      return true;
    }
  }
  return window.contains(x, y);
}

void TriggerEditor::wheel(float x, float y, int dy) {
  if (popup.open && popup_box.contains(x, y)) popup.scroll = std::max(0, popup.scroll - dy * 3);
  else if (list_box.contains(x, y)) list_scroll_ = std::max(0, list_scroll_ - dy * 3);
}

void TriggerEditor::text_input(const std::string& utf8) {
  if (popup.open) {
    popup.filter += utf8;
    popup.scroll = 0;
  } else if (field.open) {
    field.text += utf8;
  }
}

bool TriggerEditor::key(EditorDoc& d, int k) {
  if (!open) return false;
  auto pop_utf8 = [](std::string& s) {
    while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80) s.pop_back();
    if (!s.empty()) s.pop_back();
  };
  if (popup.open) {
    if (k == kEscape) popup.open = false;
    else if (k == kBackspace) pop_utf8(popup.filter);
    else if (k == kEnter) {
      const auto f = filtered();
      if (!f.empty()) choose(d, f.front());
    }
    return true;
  }
  if (field.open) {
    if (k == kEscape) field.open = false;
    else if (k == kBackspace) pop_utf8(field.text);
    else if (k == kEnter) {
      const Value* v = current_c(d);
      const TypeSpec* tsp = v ? trigger_type(section, sval(*v, "type")) : nullptr;
      if (tsp && field.param >= 0 && field.param < static_cast<int>(tsp->params.size())) {
        const ParamSpec& p = tsp->params[static_cast<size_t>(field.param)];
        const std::string text = field.text;
        bool ok = true;
        if (p.kind == K::Int && !text.empty()) {
          ok = text.find_first_not_of("-0123456789") == std::string::npos && text != "-";
          if (!ok) status = "editor.trig.not_number";
        }
        const int sec = section, ln = line;
        if (ok) {
          edit(d, [&](Value& ts) {
            Value* c = current(ts, sec, ln);
            if (!c) return;
            if (text.empty() && p.optional) c->erase(p.key);
            else if (p.kind == K::Int) c->set(p.key, Value::number(text.empty() ? "0" : text));
            else c->set(p.key, Value::string(text));
          });
        }
      }
      field.open = false;
    }
    return true;
  }
  if (k == kEscape) {
    open = false;
    return true;
  }
  if (k == kDelete && trigger >= 0 && line >= 0 && section > 0) {   // a kijelölt feltétel / akció törlése
    const int sec = section, ln = line;
    edit(d, [&](Value& ts) {
      Value& t = ts.items()[static_cast<size_t>(trigger)];
      if (Value* l = t.find(kSectionKeys[sec]); l && l->is_array() && ln < static_cast<int>(l->items().size())) {
        l->items().erase(l->items().begin() + ln);
      }
    });
    line = std::max(-1, line - 1);
    return true;
  }
  if (k == kUp || k == kDown) {   // a kijelölés a sorokon
    std::vector<std::pair<int, int>> all;
    for (const auto& r : rows) {
      if (r.index >= 0) all.emplace_back(r.section, r.index);
    }
    auto it = std::find(all.begin(), all.end(), std::make_pair(section, line));
    if (all.empty()) return true;
    if (it == all.end()) it = all.begin();
    else if (k == kDown && it + 1 != all.end()) ++it;
    else if (k == kUp && it != all.begin()) --it;
    section = it->first;
    line = it->second;
    return true;
  }
  return true;   // a nyitott ablak minden billentyűt elfogyaszt
}

}  // namespace totu::client
