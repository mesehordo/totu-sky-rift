// SPDX-License-Identifier: GPL-3.0-or-later
// Rétegezett fájlrendszer (ADR-005, 0 A.D.-szerű VFS): az alapadat és a modok egymásra rétegzett mappák. Egy
// relatív útvonal a legfelső rétegből olvasódik, amelyben létezik; a könyvtárlista az összes réteg uniója,
// rendezett sorrendben (determinisztikus). A szimuláció és a szkriptek csak ezen át érik el a fájlokat.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "content/json.hpp"

namespace totu::content {

struct ModInfo {
  std::string id;
  std::string version;
  std::vector<std::string> depends;    // kötelező előtte betöltendő modok
  std::vector<std::string> conflicts;  // ezekkel együtt nem tölthető be
  std::filesystem::path root;
};

class Vfs {
 public:
  // Réteg hozzáadása (a később hozzáadott takarja a korábbiakat).
  void add_layer(const std::string& name, const std::filesystem::path& root);
  const std::vector<std::pair<std::string, std::filesystem::path>>& layers() const { return layers_; }

  bool exists(const std::string& rel) const;
  std::optional<std::string> read_text(const std::string& rel) const;
  // A fájl összes példánya alulról felfelé (a modfoltokhoz: alap → mod1 → mod2 …).
  std::vector<std::pair<std::string, std::string>> read_all(const std::string& rel) const;
  // A mappa fájljai (relatív útvonal, a rétegek uniója, ábécérendben), adott kiterjesztéssel.
  std::vector<std::string> list(const std::string& rel_dir, const std::string& extension) const;

 private:
  static bool safe(const std::string& rel);   // nincs „..”, abszolút út vagy meghajtó
  std::vector<std::pair<std::string, std::filesystem::path>> layers_;
};

struct ModLoadResult {
  std::vector<ModInfo> order;          // betöltési sorrend (függőségek előbb)
  std::vector<std::string> errors;     // érthető hibaüzenetek; ha nem üres, a modkészlet nem tölthető be
};

// `mod.json` beolvasása: {"id", "version", "depends": [...], "conflicts": [...]}.
std::optional<ModInfo> read_mod_info(const std::filesystem::path& mod_root, std::string* error);
// A kért modok betöltési sorrendje: a függőségek előbb, egyébként a kérés sorrendje; hiányzó függőség,
// ütközés és körkörös függőség hiba.
ModLoadResult resolve_mods(const std::vector<ModInfo>& available, const std::vector<std::string>& requested);

}  // namespace totu::content
