// SPDX-License-Identifier: GPL-3.0-or-later
// Küldetéssor (M11, ADR-010 8. pont): a küldetések láncolata elágazással, és a küldetéseken átívelő állapot (profil):
// az átvitt hősök (szint, tapasztalat, képességek, tárgyak), a megtartott változók (döntések), a teljesített küldetések.
// Az átvitel a következő küldetés leírásába kerül (a névvel adott egység `hero` mezője, a változók kezdőértéke), így a
// küldetés determinisztikus marad – a visszajátszás és a hálózat is viszi.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "content/json.hpp"
#include "sim/replay.hpp"

namespace totu::sim {

struct ChainMission {
  std::string id, dir;                     // a küldetés mappája a küldetéssor mappájához képest
  std::vector<std::string> carry;          // az átvitt hősök neve (a küldetés névvel adott egységei)
  std::vector<std::string> keep;           // a megtartott változók
  std::vector<std::pair<std::string, const content::Value*>> next;   // következő küldetés + feltétel (nullptr: mindig)
  // M12 küldetésválasztó (D46 prototípus): a térképen a helye (0–1) és a megnyílás feltétele (a felsoroltak teljesítve)
  std::string name;
  float mx = 0.5f, my = 0.5f;
  std::vector<std::string> needs;
};

struct ChainDef {
  std::string id, name, start;
  bool pick_mode = false;   // M12: küldetésválasztó – a megnyílt küldetések közül a játékos választ (`requires` szerint)
  std::vector<ChainMission> missions;
  std::shared_ptr<const content::Value> doc;
  const ChainMission* mission(const std::string& id) const;
  static std::optional<ChainDef> parse(const std::string& text, std::vector<std::string>& errors);
};

struct ChainState {
  std::string chain, current;           // a következő (a most játszandó) küldetés; üres: a küldetéssor véget ért
  std::vector<std::string> done;
  std::map<std::string, int64_t> flags;
  std::map<std::string, content::Value> heroes;
  content::Value to_json() const;
  static std::optional<ChainState> from_json(const content::Value& v);
  static ChainState fresh(const ChainDef& d);
};

// a soron következő küldetés meccsleírása: a csomag + az átvitt hősök és változók
std::optional<Replay> prepare_mission(const ChainDef& def, const ChainState& st, const std::string& chain_dir,
                                      const std::string& maps_dir, uint64_t seed, std::string* error);
// a megnyert küldetés lezárása: a hősök és a változók mentése, a következő küldetés kiválasztása (a visszaadott
// azonosító; üres: vége); a nem megnyert küldetés nem változtat (false)
bool finish_mission(const ChainDef& def, ChainState& st, MatchInstance& m);
// küldetésválasztó (M12): a most választható (megnyílt, még nem teljesített) küldetések, és a választás
std::vector<std::string> available_missions(const ChainDef& def, const ChainState& st);
bool choose_mission(const ChainDef& def, ChainState& st, const std::string& id);

}  // namespace totu::sim
