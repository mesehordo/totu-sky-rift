// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/editor_view.hpp"

#include <cmath>

namespace totu::client {

namespace {

double num(const content::Value* v) {
  if (!v) return 0;
  const auto f = v->as_fixed();
  return f ? f->to_double() : 0;
}
std::string sval(const content::Value& v, const char* k) {
  const content::Value* x = v.find(k);
  return x && x->is_string() ? x->text() : std::string();
}

}  // namespace

void draw_editor(Hud& hud, const EditorUi& ui, const EditorDoc& doc, const Camera& cam, int w, int h,
                 const Translate& tr, const std::vector<std::string>& problems) {
  const float T = World::kTilePx * cam.zoom;
  auto sx = [&](double x) { return static_cast<float>((x * World::kTilePx - cam.x) * cam.zoom); };
  auto sy = [&](double y) { return static_cast<float>((y * World::kTilePx - cam.y) * cam.zoom); };
  const int F = 14, S = 12;
  const content::Value& m = doc.mission();
  // régiók (áttetsző téglalap, kerettel és névvel – a WC3 szerint)
  if (const content::Value* rs = m.find("regions"); rs && rs->is_array()) {
    for (const auto& r : rs->items()) {
      const float x = sx(num(r.find("x"))), y = sy(num(r.find("y")));
      const float rw = static_cast<float>(num(r.find("w"))) * T, rh = static_cast<float>(num(r.find("h"))) * T;
      hud.ui_rect(x, y, rw, rh, 60, 120, 220, 60);
      hud.ui_outline(Rect{static_cast<int>(x), static_cast<int>(y), static_cast<int>(rw), static_cast<int>(rh)}, 90, 160, 255);
      hud.ui_text(S, x + 3, y + 2, sval(r, "id"), 200, 225, 255);
    }
  }
  // pontok és kamerapontok
  for (const char* key : {"points", "cameras"}) {
    const bool cam_pt = key[0] == 'c';
    if (const content::Value* ps = m.find(key); ps && ps->is_array()) {
      for (const auto& p : ps->items()) {
        const float x = sx(num(p.find("x"))), y = sy(num(p.find("y")));
        if (cam_pt) hud.ui_rect(x - 7, y - 5, 14, 10, 230, 200, 80, 220);
        else hud.ui_rect(x - 4, y - 4, 8, 8, 240, 120, 200, 230);
        hud.ui_text(S, x + 8, y - 6, sval(p, "id"), 255, 235, 200);
      }
    }
  }
  // névvel ellátott egységek felirata, a kijelölt kerete
  if (const content::Value* us = m.find("units"); us && us->is_array()) {
    for (size_t i = 0; i < us->items().size(); ++i) {
      const auto& u = us->items()[i];
      const float x = sx(num(u.find("x"))), y = sy(num(u.find("y")));
      const std::string name = sval(u, "name");
      if (!name.empty()) hud.ui_text(S, x - hud.ui_text_w(S, name) / 2, y + 10, name, 255, 255, 180);
      if (static_cast<int>(i) == ui.selected) {
        hud.ui_outline(Rect{static_cast<int>(x - 0.6f * T), static_cast<int>(y - 0.6f * T), static_cast<int>(1.2f * T),
                            static_cast<int>(1.2f * T)}, 120, 255, 120, 2);
      }
    }
  }
  // a húzott régió és az ecset
  if (ui.dragging && ui.tool == EditorUi::Tool::Regions) {
    const double x0 = std::min(ui.drag_x0, ui.drag_x1), y0 = std::min(ui.drag_y0, ui.drag_y1);
    const double x1 = std::max(ui.drag_x0, ui.drag_x1) + 1, y1 = std::max(ui.drag_y0, ui.drag_y1) + 1;
    hud.ui_rect(sx(x0), sy(y0), static_cast<float>(x1 - x0) * T, static_cast<float>(y1 - y0) * T, 255, 255, 255, 50);
  }
  if ((ui.tool == EditorUi::Tool::Terrain || ui.tool == EditorUi::Tool::Trees) && ui.hover_wx >= 0 && !ui.in_panel(ui.hover_sx, ui.hover_sy)) {
    const double cx = std::floor(ui.hover_wx), cy = std::floor(ui.hover_wy);
    const float r = (static_cast<float>(ui.brush) + 0.5f) * T;
    hud.ui_outline(Rect{static_cast<int>(sx(cx + 0.5) - r), static_cast<int>(sy(cy + 0.5) - r), static_cast<int>(2 * r),
                        static_cast<int>(2 * r)}, 255, 255, 255);
  }
  // bal oldali paletta
  const Rect& P = ui.panel;
  hud.ui_rect(static_cast<float>(P.x), static_cast<float>(P.y), static_cast<float>(P.w), static_cast<float>(P.h), 24, 22, 20, 235);
  hud.ui_outline(P, 90, 80, 60);
  auto button = [&](const EditorUi::Item& it, const std::string& label) {
    const Rect& r = it.r;
    const bool hot = r.contains(ui.hover_sx, ui.hover_sy);
    hud.ui_rect(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h),
                it.active ? 90 : hot ? 60 : 40, it.active ? 75 : hot ? 52 : 36, it.active ? 40 : hot ? 36 : 30);
    hud.ui_outline(r, it.active ? 220 : 100, it.active ? 190 : 90, it.active ? 110 : 70);
    hud.ui_text(S, static_cast<float>(r.x + 6), static_cast<float>(r.y) + (static_cast<float>(r.h) - hud.ui_line_h(S)) / 2, label,
                240, 236, 214);
  };
  for (const auto& b : ui.tool_buttons) button(b, tr(b.label));
  const std::string bl = tr("editor.brush") + ": " + std::to_string(ui.brush);
  hud.ui_text(F, static_cast<float>(P.x + P.w / 2) - hud.ui_text_w(F, bl) / 2, static_cast<float>(ui.brush_minus.y + 4), bl, 255, 214, 120);
  button(EditorUi::Item{ui.brush_minus, "-", false}, "-");
  button(EditorUi::Item{ui.brush_plus, "+", false}, "+");
  for (const auto& b : ui.owner_buttons) button(b, b.label);
  for (const auto& it : ui.palette) {
    std::string label = it.label;
    if (ui.tool == EditorUi::Tool::Units) {
      const std::string a = tr("unit." + it.label + ".name"), b = tr("building." + it.label + ".name");
      label = a != "unit." + it.label + ".name" ? a : b != "building." + it.label + ".name" ? b : it.label;
    }
    button(it, label);
  }
  // állapotsor: koordináta, az utolsó művelet, a hibák száma, mentetlen jelzés
  const Rect& B = ui.status_bar;
  hud.ui_rect(static_cast<float>(B.x), static_cast<float>(B.y), static_cast<float>(B.w), static_cast<float>(B.h), 16, 14, 12, 240);
  std::string st = ui.hover_wx >= 0 ? "x " + std::to_string(static_cast<int>(ui.hover_wx)) + ", y " + std::to_string(static_cast<int>(ui.hover_wy)) : std::string();
  st += "   " + doc.dir() + (doc.dirty() ? " *" : "");
  if (!ui.status.empty()) st += "   " + ui.status;
  st += "   " + (problems.empty() ? tr("editor.valid") : tr("editor.problems") + ": " + std::to_string(problems.size()) + " – " + problems.front());
  hud.ui_text(S, static_cast<float>(B.x + 8), static_cast<float>(B.y) + (static_cast<float>(B.h) - hud.ui_line_h(S)) / 2, st,
              problems.empty() ? 200 : 255, problems.empty() ? 230 : 150, problems.empty() ? 200 : 120);
  (void)w;
  (void)h;
}

void draw_trigger_editor(Hud& hud, const TriggerEditor& te, const EditorDoc& doc, float mx, float my, const Translate& tr) {
  if (!te.open) return;
  // a típus neve; az akcióknak saját nevük lehet (pl. timer: esemény „lejár”, akció „indítás”)
  auto type_name = [&](int section, const std::string& type) {
    if (section == 2) {
      const std::string a = tr("editor.t.a." + type);
      if (a != "editor.t.a." + type) return a;
    }
    return tr("editor.t." + type);
  };
  const int S = 12, F = 14;
  auto box = [&](const Rect& r, uint8_t g, uint8_t a) {
    hud.ui_rect(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h), g, g, static_cast<uint8_t>(g * 9 / 10), a);
  };
  auto label = [&](const Rect& r, const std::string& s, uint8_t cr, uint8_t cg, uint8_t cb) {
    hud.ui_text(S, static_cast<float>(r.x + 6), static_cast<float>(r.y) + (static_cast<float>(r.h) - hud.ui_line_h(S)) / 2, s, cr, cg, cb);
  };
  auto button = [&](const Rect& r, const std::string& s, bool active) {
    const bool hot = r.contains(mx, my);
    box(r, active ? 80 : hot ? 56 : 38, 255);
    hud.ui_outline(r, active ? 220 : 100, active ? 190 : 90, active ? 110 : 70);
    label(r, s, 240, 236, 214);
  };
  box(te.window, 20, 245);
  hud.ui_outline(te.window, 200, 170, 110, 2);
  // eszközgombok (az engedélyezés és az ismétlés a kijelölt trigger állapotát mutatja)
  const content::Value* ts = doc.mission().find("triggers");
  const content::Value* cur = ts && ts->is_array() && te.trigger >= 0 && te.trigger < static_cast<int>(ts->items().size())
                                  ? &ts->items()[static_cast<size_t>(te.trigger)] : nullptr;
  for (const auto& b : te.buttons) {
    bool active = false;
    if (cur && b.id == 3) {
      const content::Value* e = cur->find("enabled");
      active = !e || !e->is_bool() || e->as_bool();
    }
    if (cur && b.id == 4) {
      const content::Value* e = cur->find("repeat");
      active = e && e->is_bool() && e->as_bool();
    }
    button(b.r, tr(b.label), active);
  }
  box(te.list_box, 30, 255);
  for (const auto& it : te.list_items) button(it.r, it.label, it.id == te.trigger);
  box(te.body_box, 26, 255);
  for (const auto& r : te.rows) {
    if (r.index < 0) {   // fejléc + jel
      label(r.r, tr("editor.trig.section." + r.text) + "   [+]", 255, 214, 120);
      continue;
    }
    const char* sec_key = r.section == 0 ? "event" : r.section == 1 ? "conditions" : "actions";
    const content::Value* v = nullptr;
    if (cur) {
      if (r.section == 0) v = cur->find("event");
      else if (const content::Value* l = cur->find(sec_key); l && l->is_array() && r.index < static_cast<int>(l->items().size())) {
        v = &l->items()[static_cast<size_t>(r.index)];
      }
    }
    const std::string text = v ? TriggerEditor::describe(r.section, *v, type_name(r.section, r.text)) : r.text;
    if (r.selected) box(r.r, 70, 255);
    else if (r.r.contains(mx, my)) box(r.r, 44, 255);
    label(r.r, text, 230, 230, 220);
  }
  box(te.params_box, 30, 255);
  for (const auto& b : te.param_buttons) {
    std::string s = b.label;
    if (te.field.open && te.field.param == b.id) s = b.label.substr(0, b.label.find(':')) + ": " + te.field.text + "_";
    button(b.r, s, te.field.open && te.field.param == b.id);
  }
  if (!te.status.empty()) {
    hud.ui_text(S, static_cast<float>(te.params_box.x + 6), static_cast<float>(te.params_box.bottom() - 18), tr(te.status), 255, 150, 120);
  }
  if (te.popup.open) {   // felugró lista szűrővel
    box(te.popup_box, 16, 250);
    hud.ui_outline(te.popup_box, 230, 200, 130, 2);
    hud.ui_text(F, static_cast<float>(te.popup_box.x + 8), static_cast<float>(te.popup_box.y + 8),
                te.popup.title + (te.popup.filter.empty() ? std::string() : "   [" + te.popup.filter + "]"), 255, 214, 120);
    for (const auto& it : te.popup_items) {
      std::string s = it.label;
      const int psec = te.popup.target == -2 ? te.popup.section : te.section;
      const std::string t = te.popup.target < 0 ? type_name(psec, it.label) : std::string();
      if (!t.empty() && t != "editor.t." + it.label) s = t + "  (" + it.label + ")";
      button(it.r, s, false);
    }
  }
}

}  // namespace totu::client
