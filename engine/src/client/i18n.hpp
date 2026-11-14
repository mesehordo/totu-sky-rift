// SPDX-License-Identifier: GPL-3.0-or-later
// Nyelvek (M8; a régi `rts/i18n.py` utódja). Minden nyelv egy .po fájl a `locale/` mappában (az alaptartalomé és a
// modrétegeké): a fájlnév a nyelv kódja (`hu.po`, `en.po`, …), a megjelenített nevet a fejléc `Language-Name:` sora
// adja. Új nyelvhez elég egy új .po fájl – a nyelvlista a talált fájlokból épül.
//
// Minden szöveg kulcs szerint kerül elő (msgctxt = kulcs): a felület saját szövegei (`ui.*`) és a játékadatok
// (egységek, épületek, népek…) is. A szöveg az msgstr; a forrásnyelv (magyar) fájljában az üres msgstr helyett az msgid.
// Ha a kulcs hiányzik az adott nyelvből, az angolra esik vissza, ha abból is, a kulcs maga jelenik meg.
// Grafikus kártya nélkül tesztelhető.
#pragma once

#include <functional>
#include <initializer_list>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace totu::client {

// egy .po-bejegyzés
struct PoEntry {
  std::string ctx, id, str;
};
// .po-szöveg elemzése (többsoros szövegek, \" \n \\ \t kódolás); a fejléc (üres msgid) a `header`-be kerül
std::vector<PoEntry> parse_po(std::string_view text, std::string* header = nullptr);

class Locale {
 public:
  static constexpr std::string_view kSource = "hu";     // a forrásnyelv (az msgid nyelve; az alapnyelv)
  static constexpr std::string_view kFallback = "en";   // a hiányzó szöveg ebből jön

  struct Language {
    std::string code, name;
  };

  // az alaptartalom (`<content>/base/locale`) és a modrétegek (`<content>/mods/*/locale`) nyelvei;
  // false, ha egy nyelv sincs
  bool load(const std::string& content_dir);
  // egy mappa minden `*.po` fájlja (a későbbi mappa kiegészíti és felülírja a korábbit)
  void add_dir(const std::string& locale_dir);
  // egy nyelv .po-szövege (tesztekhez is); a név a fejlécből, ha van
  void add_po(const std::string& code, std::string_view text);

  const std::vector<Language>& languages() const { return langs_; }   // kód szerint rendezve
  bool has(std::string_view code) const;
  std::string language_name(std::string_view code) const;   // a .po fejléce szerint; ha nincs, a kód

  // nyelvváltás; ismeretlen nyelvnél a forrásnyelv
  void set_lang(const std::string& code);
  const std::string& lang() const { return lang_; }

  // a kulcs szövege az aktuális nyelven (→ angol → a kulcs); a {mező}-ket a `fields` tölti ki
  std::string tr(std::string_view key) const;
  std::string tr(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string>> fields) const;
  // a játékadatok neve (ugyanaz a keresés, pl. "faction.ursan.name")
  std::string name(std::string_view key) const { return tr(key); }

 private:
  const std::string* find(std::string_view code, std::string_view key) const;
  std::map<std::string, std::map<std::string, std::string, std::less<>>, std::less<>> tables_;
  std::vector<Language> langs_;
  std::string lang_ = std::string(kSource);
};

}  // namespace totu::client
