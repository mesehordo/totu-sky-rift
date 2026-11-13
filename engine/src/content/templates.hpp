// SPDX-License-Identifier: GPL-3.0-or-later
// Sablontár (ADR-005): a tartalom nevesített sablonokból áll. Egy sablonfájl egy JSON-objektum, kulcsai a sablonok
// azonosítói. Ha egy későbbi réteg (mod) ugyanazt az azonosítót adja, az **folt**: mélyen összefésülődik az
// eddigivel. Az `"inherits"` (szöveg vagy lista) a szülő(k) feloldott tartalmára fésüli rá a sablont.
//
// Összefésülési szabályok (`merge_patch`):
//   - objektum + objektum: kulcsonként, rekurzívan;
//   - `null` érték: a kulcs törlése (a szülőtől örökölt kulcsé is);
//   - lista + {"+": [...], "-": [...]}: előbb az azonos elemek eltávolítása, aztán a hozzáadás a végére;
//   - minden más: felülírás.
// A `_` kezdetű kulcsok megjegyzések; a feloldott sablonból kimaradnak.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "content/json.hpp"

namespace totu::content {

// `keep_list_patches`: a foltrétegek összefésülésekor igaz – a hiányzó listára vonatkozó `+`/`-` művelet foltként
// megmarad (az öröklődés feloldásakor hajtódik végre a szülő listáján), két egymást követő listafolt pedig
// sorrendhelyesen összevonódik. A végleges feloldáskor hamis: a hiányzó listára csak a `+` érvényes.
void merge_patch(Value& base, const Value& patch, bool keep_list_patches = false);

class TemplateStore {
 public:
  // Egy sablonfájl hozzáadása; `layer` a réteg neve (hibaüzenethez), `origin` a fájl útvonala.
  void add_file(const std::string& layer, const std::string& origin, const Value& doc);
  // A feloldott sablon (öröklődéssel); nullptr, ha nincs ilyen vagy hibás (a hiba az `errors()`-ban).
  const Value* get(const std::string& id);
  std::vector<std::string> ids() const;          // ábécérendben
  const std::vector<std::string>& errors() const { return errors_; }
  // A `_` kezdetű, nem sablon kulcsok (pl. `_hozzarendeles`): a rétegek sorrendjében összefésülve; nullptr, ha nincs.
  const Value* meta(const std::string& key) const;

 private:
  struct Entry {
    Value def;                  // a rétegek foltjaival összefésült saját definíció
    std::string first_origin;
    std::vector<std::string> layers;   // a rétegek, amelyek ezt a sablont adják vagy foltozzák
  };
  bool resolve(const std::string& id, std::vector<std::string>& stack);
  std::map<std::string, Entry> raw_;
  std::map<std::string, Value> meta_;
  std::map<std::string, Value> resolved_;
  std::map<std::string, bool> failed_;
  std::vector<std::string> errors_;
};

}  // namespace totu::content
