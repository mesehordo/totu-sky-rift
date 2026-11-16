// SPDX-License-Identifier: GPL-3.0-or-later
// A szerkesztő dokumentuma (M11, ADR-010): a küldetés (mission.json) és a pálya (map.json) együtt, GL nélkül
// (próbálható). Minden művelet visszavonható; a mentés kanonikus (dump_pretty), így a mentett csomag újra megnyitva
// és mentve bájtra ugyanaz. A pálya a küldetés mappájába kerül (a pályák mappájából csak olvas).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "content/json.hpp"
#include "sim/replay.hpp"

namespace totu::map {
class TerrainTable;
}

namespace totu::client {

class EditorDoc {
 public:
  // meglévő küldetés megnyitása (a pálya a küldetés mappájából, különben a `maps_dir`-ből)
  bool open(const std::string& mission_dir, const std::string& maps_dir, std::string* error);
  // új küldetés egy meglévő pályára (két játékos: ursan ember, wullof)
  bool create(const std::string& mission_dir, const std::string& map_path, const std::string& id, std::string* error);
  bool save(std::string* error) const;   // mission.json + a pálya a küldetés mappájába
  bool save_as(const std::string& mission_dir, std::string* error);

  const std::string& dir() const { return dir_; }
  std::string mission_text() const { return content::dump_pretty(mission_); }
  std::string map_text() const { return content::dump_pretty(map_); }
  const content::Value& mission() const { return mission_; }
  const content::Value& map() const { return map_; }
  int width() const;
  int height() const;
  bool dirty() const { return dirty_; }

  // --- műveletek (mind visszavonható) ---
  // terep-ecset: a (x, y) középpontú, `radius` sugarú kör csempéi a `terrain_char` terepre ('T'/'.': fa / irtás)
  bool paint(int x, int y, int radius, char terrain_char);
  // egység vagy épület elhelyezése (csempe-koordináta, tört is lehet); a sorszáma
  int place(const std::string& def, int owner, double x, double y, const std::string& name = {});
  bool move_unit(int index, double x, double y);
  bool remove_unit(int index);
  int unit_at(double x, double y, double radius) const;   // a legközelebbi a sugáron belül (-1: nincs)
  bool add_region(const std::string& id, int x, int y, int w, int h);
  bool add_point(const std::string& id, double x, double y, bool camera, double zoom = 0);
  bool remove_named(const std::string& list, const std::string& id);   // "regions" / "points" / "cameras" / "paths"
  // egy lista (triggers, objectives, players, …) vagy objektum (dialogues, cutscenes, variables) cseréje JSON-szövegből
  bool set_section(const std::string& key, const std::string& json, std::string* error);
  bool set_field(const std::string& key, const std::string& value);   // id, name, description

  bool undo();
  bool redo();
  bool can_undo() const { return !undo_.empty(); }
  bool can_redo() const { return !redo_.empty(); }

  // ellenőrzés (a küldetés és a pálya); üres: rendben
  std::vector<std::string> validate(const sim::Rules* rules, const map::TerrainTable* terrain) const;
  // meccsleírás (előnézet, „kipróbálás innen”)
  sim::Replay replay(uint64_t seed) const;

  std::string unique_id(const std::string& list, const std::string& prefix) const;

 private:
  void touch();   // a művelet előtt: visszavonási pont
  content::Value* list(const std::string& key);   // a küldetés listája (létrehozza, ha nincs)
  std::string dir_, map_name_;
  content::Value mission_, map_;
  std::vector<std::pair<content::Value, content::Value>> undo_, redo_;
  bool dirty_ = false;
};

}  // namespace totu::client
