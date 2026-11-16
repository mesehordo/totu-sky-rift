// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/editor_ui.hpp"

#include <algorithm>
#include <cmath>

#include "map/terrain.hpp"
#include "sim/rules.hpp"

namespace totu::client {

namespace {
constexpr int kPanelW = 240, kPad = 8, kButtonH = 26, kItemH = 22, kStatusH = 22;
}

const char* EditorUi::tool_key(Tool t) {
  switch (t) {
    case Tool::Select: return "select";
    case Tool::Terrain: return "terrain";
    case Tool::Trees: return "trees";
    case Tool::Units: return "units";
    case Tool::Regions: return "regions";
    case Tool::Points: return "points";
    case Tool::Cameras: return "cameras";
    default: return "?";
  }
}

void EditorUi::init(const map::TerrainTable& terrain, const sim::Rules& rules) {
  rules_ = &rules;
  terrains.clear();
  for (const auto& t : terrain.terrains()) {
    if (t.ch != '?' && t.ch != 'T' && t.ch != '.') terrains.emplace_back(t.ch, t.name);
  }
  if (terrains.empty()) terrains.emplace_back(',', "?");
}

std::vector<std::pair<std::string, std::string>> EditorUi::units_of_owner() const {
  std::vector<std::pair<std::string, std::string>> out;
  if (!rules_) return out;
  const std::string fac = owner >= 1 && static_cast<size_t>(owner) <= owner_faction_.size() ? owner_faction_[static_cast<size_t>(owner - 1)] : std::string();
  for (const auto& d : rules_->defs) {
    if (d.id.empty() || (!fac.empty() && d.faction != fac)) continue;
    out.emplace_back(d.id, d.id);   // a megjelenített nevet a rajzoló fordítja (unit./building.<id>.name)
  }
  return out;
}

void EditorUi::layout(int w, int h) {
  panel = Rect{0, 0, kPanelW, std::max(0, h - kStatusH)};
  status_bar = Rect{0, h - kStatusH, w, kStatusH};
  tool_buttons.clear();
  const int cols = 2, bw = (kPanelW - kPad * 3) / cols;
  for (int i = 0; i < static_cast<int>(Tool::Count); ++i) {
    const Rect r{kPad + (i % cols) * (bw + kPad), kPad + (i / cols) * (kButtonH + 4), bw, kButtonH};
    tool_buttons.push_back(Item{r, std::string("editor.tool.") + tool_key(static_cast<Tool>(i)), static_cast<int>(tool) == i});
  }
  int y = kPad + ((static_cast<int>(Tool::Count) + cols - 1) / cols) * (kButtonH + 4) + kPad;
  brush_minus = Rect{kPad, y, 28, kButtonH};
  brush_plus = Rect{kPanelW - kPad - 28, y, 28, kButtonH};
  y += kButtonH + kPad;
  owner_buttons.clear();
  const int ow = (kPanelW - kPad * 2) / 9;
  for (int p = 0; p <= 8; ++p) {
    owner_buttons.push_back(Item{Rect{kPad + p * ow, y, ow - 2, kItemH}, std::to_string(p), owner == p});
  }
  y += kItemH + kPad;
  palette.clear();
  const int room = std::max(1, (panel.h - y - kPad) / kItemH);
  if (tool == Tool::Terrain) {
    for (size_t i = 0; i < terrains.size() && static_cast<int>(palette.size()) < room; ++i) {
      palette.push_back(Item{Rect{kPad, y + static_cast<int>(palette.size()) * kItemH, kPanelW - kPad * 2, kItemH - 2},
                             terrains[i].second, i == terrain_index});
    }
  } else if (tool == Tool::Units) {
    const auto us = units_of_owner();
    scroll_ = std::clamp(scroll_, 0, std::max(0, static_cast<int>(us.size()) - room));
    for (size_t i = static_cast<size_t>(scroll_); i < us.size() && static_cast<int>(palette.size()) < room; ++i) {
      palette.push_back(Item{Rect{kPad, y + static_cast<int>(palette.size()) * kItemH, kPanelW - kPad * 2, kItemH - 2},
                             us[i].second, i == unit_index});
    }
  }
}

void EditorUi::apply_brush(EditorDoc& d, double wx, double wy) {
  const int x = static_cast<int>(std::floor(wx)), y = static_cast<int>(std::floor(wy));
  bool done = false;
  if (tool == Tool::Terrain && terrain_index < terrains.size()) done = d.paint(x, y, brush, terrains[terrain_index].first);
  if (tool == Tool::Trees) done = d.paint(x, y, brush, 'T');
  if (done) changed();
}

void EditorUi::sync(const EditorDoc& d) {
  last_doc_ = &d;
  owner_faction_.clear();
  if (const content::Value* ps = d.mission().find("players"); ps && ps->is_array()) {
    for (const auto& p : ps->items()) {
      const int slot = static_cast<int>(p.find("slot") ? p.find("slot")->as_int().value_or(0) : 0);
      if (slot < 1 || slot > 8) continue;
      if (owner_faction_.size() < static_cast<size_t>(slot)) owner_faction_.resize(static_cast<size_t>(slot));
      const content::Value* f = p.find("faction");
      owner_faction_[static_cast<size_t>(slot - 1)] = f && f->is_string() ? f->text() : std::string();
    }
  }
}

bool EditorUi::mouse_down(EditorDoc& d, float sx, float sy, double wx, double wy, bool right) {
  sync(d);
  if (panel.contains(sx, sy)) {   // a paletta
    for (size_t i = 0; i < tool_buttons.size(); ++i) {
      if (tool_buttons[i].r.contains(sx, sy)) {
        tool = static_cast<Tool>(i);
        selected = -1;
      }
    }
    if (brush_minus.contains(sx, sy)) brush = std::max(0, brush - 1);
    if (brush_plus.contains(sx, sy)) brush = std::min(12, brush + 1);
    for (size_t i = 0; i < owner_buttons.size(); ++i) {
      if (owner_buttons[i].r.contains(sx, sy)) {
        owner = static_cast<int>(i);
        unit_index = 0;
        scroll_ = 0;
      }
    }
    for (size_t i = 0; i < palette.size(); ++i) {
      if (!palette[i].r.contains(sx, sy)) continue;
      if (tool == Tool::Terrain) terrain_index = i;
      if (tool == Tool::Units) unit_index = static_cast<size_t>(scroll_) + i;
    }
    return true;
  }
  if (status_bar.contains(sx, sy) || wx < 0 || wy < 0 || wx > d.width() || wy > d.height()) return false;
  if (right) {   // jobb gomb: kijelölés törlése / a kijelölt egység törlése (WC3: a kijelölés megszüntetése)
    selected = -1;
    return true;
  }
  switch (tool) {
    case Tool::Terrain:
    case Tool::Trees:
      apply_brush(d, wx, wy);
      dragging = true;
      return true;
    case Tool::Units: {
      const auto us = units_of_owner();
      if (unit_index >= us.size()) return true;
      const double x = std::round(wx * 2) / 2, y = std::round(wy * 2) / 2;   // fél csempére
      selected = d.place(us[unit_index].first, owner, x, y);
      status = selected >= 0 ? us[unit_index].first : std::string("editor.err.place");
      if (selected >= 0) changed();
      return true;
    }
    case Tool::Select:
      selected = d.unit_at(wx, wy, 0.8);
      dragging = selected >= 0;
      return true;
    case Tool::Regions:
      dragging = true;
      drag_x0 = drag_x1 = std::floor(wx);
      drag_y0 = drag_y1 = std::floor(wy);
      return true;
    case Tool::Points:
    case Tool::Cameras: {
      const bool cam = tool == Tool::Cameras;
      const std::string id = d.unique_id(cam ? "cameras" : "points", cam ? "kamera" : "pont");
      if (d.add_point(id, std::round(wx * 2) / 2, std::round(wy * 2) / 2, cam, cam ? 1.5 : 0)) {
        status = id;
        changed();
      }
      return true;
    }
    default: return false;
  }
}

void EditorUi::mouse_move(EditorDoc& d, float sx, float sy, double wx, double wy) {
  hover_sx = sx;
  hover_sy = sy;
  hover_wx = wx;
  hover_wy = wy;
  if (!dragging) return;
  if (tool == Tool::Terrain || tool == Tool::Trees) apply_brush(d, wx, wy);
  if (tool == Tool::Regions) {
    drag_x1 = std::floor(std::clamp(wx, 0.0, static_cast<double>(d.width() - 1)));
    drag_y1 = std::floor(std::clamp(wy, 0.0, static_cast<double>(d.height() - 1)));
  }
}

void EditorUi::mouse_up(EditorDoc& d, double wx, double wy) {
  if (!dragging) return;
  dragging = false;
  if (tool == Tool::Regions) {
    const int x = static_cast<int>(std::min(drag_x0, drag_x1)), y = static_cast<int>(std::min(drag_y0, drag_y1));
    const int w = static_cast<int>(std::fabs(drag_x1 - drag_x0)) + 1, h = static_cast<int>(std::fabs(drag_y1 - drag_y0)) + 1;
    const std::string id = d.unique_id("regions", "regio");
    if (d.add_region(id, x, y, w, h)) {
      status = id;
      changed();
    }
  }
  if (tool == Tool::Select && selected >= 0 && wx >= 0 && wy >= 0) {   // húzással mozgat
    if (d.unit_at(wx, wy, 0.05) != selected && d.move_unit(selected, std::round(wx * 2) / 2, std::round(wy * 2) / 2)) changed();
  }
}

bool EditorUi::key(EditorDoc& d, int k, bool ctrl) {
  if (ctrl && (k == 'z' || k == 'Z')) {
    if (d.undo()) changed();
    selected = -1;
    return true;
  }
  if (ctrl && (k == 'y' || k == 'Y')) {
    if (d.redo()) changed();
    return true;
  }
  if (k == 127 && selected >= 0) {   // Del
    if (d.remove_unit(selected)) changed();
    selected = -1;
    return true;
  }
  if (!ctrl && k >= '1' && k < '1' + static_cast<int>(Tool::Count)) {
    tool = static_cast<Tool>(k - '1');
    return true;
  }
  if (k == '+') {
    brush = std::min(12, brush + 1);
    return true;
  }
  if (k == '-') {
    brush = std::max(0, brush - 1);
    return true;
  }
  return false;
}

}  // namespace totu::client
