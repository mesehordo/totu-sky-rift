// SPDX-License-Identifier: GPL-3.0-or-later
// Hadjárat (M11, ADR-010 8. pont): a küldetések láncolata elágazással, és a küldetéseken átívelő állapot (profil):
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

struct CampaignMission {
  std::string id, dir;                     // a küldetés mappája a hadjárat mappájához képest
  std::vector<std::string> carry;          // az átvitt hősök neve (a küldetés névvel adott egységei)
  std::vector<std::string> keep;           // a megtartott változók
  std::vector<std::pair<std::string, const content::Value*>> next;   // következő küldetés + feltétel (nullptr: mindig)
};

struct CampaignDef {
  std::string id, name, start;
  std::vector<CampaignMission> missions;
  std::shared_ptr<const content::Value> doc;
  const CampaignMission* mission(const std::string& id) const;
  static std::optional<CampaignDef> parse(const std::string& text, std::vector<std::string>& errors);
};

struct CampaignState {
  std::string campaign, current;           // a következő (a most játszandó) küldetés; üres: a hadjárat véget ért
  std::vector<std::string> done;
  std::map<std::string, int64_t> flags;
  std::map<std::string, content::Value> heroes;
  content::Value to_json() const;
  static std::optional<CampaignState> from_json(const content::Value& v);
  static CampaignState fresh(const CampaignDef& d);
};

// a soron következő küldetés meccsleírása: a csomag + az átvitt hősök és változók
std::optional<Replay> prepare_mission(const CampaignDef& def, const CampaignState& st, const std::string& campaign_dir,
                                      const std::string& maps_dir, uint64_t seed, std::string* error);
// a megnyert küldetés lezárása: a hősök és a változók mentése, a következő küldetés kiválasztása (a visszaadott
// azonosító; üres: vége); a nem megnyert küldetés nem változtat (false)
bool finish_mission(const CampaignDef& def, CampaignState& st, MatchInstance& m);

}  // namespace totu::sim
