// SPDX-License-Identifier: GPL-3.0-or-later
// A küldetés adatainak szerkesztője (M11, F5): fülek – célok, változók, párbeszédek, átvezetők. Bal oldalt az elemek
// listája, középen (párbeszédnél a csomópontok, átvezetőnél a lépések) a második szint, jobbra a kijelölt elem mezői
// (párbeszéd-csomópontnál a válaszok is). A mezők szerkesztése a trigger-szerkesztőével azonos: felugró, szűrhető lista
// vagy beírás. GL nélkül (próbálható); minden változás az EditorDoc-on át (visszavonható, ellenőrzött).
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "client/editor_doc.hpp"
#include "client/hud_layout.hpp"
#include "client/trigger_editor.hpp"

namespace totu::client {

class DataEditor {
 public:
  enum Tab { kObjectives = 0, kVariables, kDialogues, kCutscenes, kTabs };
  bool open = false;
  int tab = kObjectives;
  int sel1 = -1, sel2 = -1, sel3 = -1;   // elem; csomópont / lépés; válasz
  std::string status;

  struct Button {
    Rect r;
    std::string label;
    int id = 0;
    bool active = false;
  };
  Rect window, list1_box, list2_box, fields_box, popup_box;
  std::vector<Button> tabs, list1, list2, list3, fields, tools, popup_items;

  TriggerEditor::Popup popup;   // a cél: a mező sorszáma (-1: lépés típusa)
  TriggerEditor::Field field;

  void init(const sim::Rules* rules) { rules_ = rules; }
  void layout(int w, int h, const EditorDoc& d);
  bool mouse_down(EditorDoc& d, float x, float y, bool right);
  void wheel(float x, float y, int dy);
  bool key(EditorDoc& d, int k);   // TriggerEditor::Key
  void text_input(const std::string& utf8);
  std::vector<std::string> filtered() const;

  // a kijelölt elem mezői: kulcs, fajta, elhagyható, lehetőségek
  std::vector<ParamSpec> field_specs(const EditorDoc& d) const;

 private:
  bool edit(EditorDoc& d, const std::function<void(content::Value& section)>& f);
  const char* section_key() const;
  content::Value* target(content::Value& section) const;   // a mezők gazdája a szakaszon belül
  const content::Value* target_c(const EditorDoc& d) const;
  std::vector<std::string> options(const EditorDoc& d, const ParamSpec& p) const;
  void set_field(EditorDoc& d, const ParamSpec& p, const std::string& value);
  void add(EditorDoc& d, int level);
  void remove(EditorDoc& d, int level);
  const sim::Rules* rules_ = nullptr;
  TriggerEditor trig_;   // a sablon- és hivatkozáslisták (options_for) és a lépésleírás
};

}  // namespace totu::client
