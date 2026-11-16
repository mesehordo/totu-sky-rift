// SPDX-License-Identifier: GPL-3.0-or-later
// Trigger-szerkesztő (M11, a WC3 Trigger Editor mintájára), GL nélkül: bal oldalt a triggerek listája, jobbra a
// kijelölt trigger eseménye, feltételei és akciói mondatszerű sorokként; a kijelölt sor paraméterei alul. A típus és a
// hivatkozás (régió, párbeszéd, cél, egységnév, sablon …) felugró, gépeléssel szűrhető listából, a szám és a szöveg
// beírással. Minden változás az EditorDoc-on át (visszavonható, ellenőrzött) megy.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "client/editor_doc.hpp"
#include "client/hud_layout.hpp"

namespace totu::sim {
struct Rules;
}

namespace totu::client {

// egy paraméter fajtája (a választható értékek forrása)
enum class ParamKind : uint8_t { Int, Text, Bool, Player, Enum, Region, Point, Path, Dialogue, Objective, Trigger, Cutscene, UnitName, Def };

struct ParamSpec {
  const char* key;
  ParamKind kind;
  bool optional;
  std::vector<const char*> options;   // Enum
};
struct TypeSpec {
  const char* type;
  std::vector<ParamSpec> params;
};
// a három rész típusai (0 esemény, 1 feltétel, 2 akció)
const std::vector<TypeSpec>& trigger_types(int section);
const TypeSpec* trigger_type(int section, const std::string& type);

class TriggerEditor {
 public:
  bool open = false;
  int trigger = -1;                 // a kijelölt trigger sorszáma
  int section = 0, line = -1;       // a kijelölt sor (0 esemény, 1 feltétel, 2 akció)
  std::string status;               // az utolsó hiba vagy művelet

  // felugró lista (típus vagy hivatkozás választása) és szövegmező
  struct Popup {
    bool open = false;
    std::string title;
    std::vector<std::string> options;   // a teljes lista
    std::string filter;                  // gépeléssel szűr
    int scroll = 0;
    int target = -1;                     // -1: a sor típusa; különben a paraméter sorszáma
    int section = 0;                     // új sor felvétele: a rész (a `target` ekkor -2)
  } popup;
  struct Field {
    bool open = false;
    int param = -1;
    std::string text;
  } field;

  // elrendezés és nézetadat (a rajzolónak)
  struct Row {
    Rect r;
    std::string text;
    int section = 0, index = -1;   // index -1: a rész fejléce (+ gomb)
    bool selected = false;
  };
  struct Button {
    Rect r;
    std::string label;
    int id = 0;
  };
  Rect window, list_box, body_box, params_box, popup_box;
  std::vector<Button> list_items, buttons;   // triggerek; eszközgombok (új, törlés, engedélyezve, ismétlés)
  std::vector<Row> rows;                     // a kijelölt trigger sorai
  std::vector<Button> param_buttons;         // a kijelölt sor paraméterei (id: a paraméter sorszáma)
  std::vector<Button> popup_items;           // id: az opció sorszáma a szűrt listában

  void init(const sim::Rules* rules) { rules_ = rules; }
  void layout(int w, int h, const EditorDoc& d);
  bool mouse_down(EditorDoc& d, float x, float y, bool right);
  void wheel(float x, float y, int dy);
  // billentyű: Enter (mező / választás jóváhagyása), Esc (bezárás), Backspace, Del (sor törlése), fel/le
  enum Key { kEnter = 1, kEscape, kBackspace, kDelete, kUp, kDown };
  bool key(EditorDoc& d, int k);
  void text_input(const std::string& utf8);

  // a mondatszerű sor: a típus (lefordított) neve és a megadott paraméterek
  static std::string describe(int section, const content::Value& v, const std::string& type_label);
  std::vector<std::string> filtered() const;
  std::vector<std::string> options_for(const EditorDoc& d, ParamKind kind, const ParamSpec& p) const;

 private:
  bool edit(EditorDoc& d, const std::function<void(content::Value& triggers)>& f);
  content::Value* current(content::Value& triggers, int sec, int idx) const;   // a kijelölt sor értéke
  const content::Value* current_c(const EditorDoc& d) const;
  void open_param(EditorDoc& d, int param);
  void choose(EditorDoc& d, const std::string& value);
  const sim::Rules* rules_ = nullptr;
  int list_scroll_ = 0;
};

}  // namespace totu::client
