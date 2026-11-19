// SPDX-License-Identifier: GPL-3.0-or-later
// A játék közbeni HUD állapota, nézetmodellje és bemenete (M8; a régi `rts/hud.py` HudMixin és a `rts/ui.py` hozzá
// tartozó részeinek átvétele). Grafikus kártya nélkül fut és tesztelhető: a pillanatképből (Snapshot) és a
// szabályokból (Rules) olvas, a parancsokat a közös parancs-API-n (sim::Command → a futtató sora) adja ki; a
// rajzolás (client/hud.cpp) vékony réteg rá.
//
// Billentyű és kattintás ugyanazt hívja: activate_slot() → a hely saját függvénye → cmd() (a szimuláció dönt a
// jogosultságról, a költségről és az újratöltésről).
#pragma once

#include <algorithm>
#include <array>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "client/hud_layout.hpp"
#include "client/session.hpp"
#include "map/map.hpp"
#include "map/terrain.hpp"

namespace totu::client {

// a parancsrács egy helye (a régi `_slot` szótár)
struct HudSlot {
  enum Kind : uint8_t { None, Unit, Building, Research };
  std::string id, icon, label, ab_id, ally;   // ally: a szövetséges kisnép azonosítója (ikonhoz)
  std::function<void()> fn, rfn;              // bal és jobb kattintás (üres: nincs akció)
  bool active = false, enabled = true, passive = false;
  float cd = 0.0f;                            // újratöltés hátralévő aránya (0..1)
  float prog = -1.0f;                         // gyártás haladása (0..1; -1 = nincs)
  int count = 0;                              // a sorban lévő darab
  std::string cost;                           // sarokcímke (jobb felül)
  std::array<uint8_t, 3> cost_col{255, 255, 255};
  Kind kind = None;
  uint16_t def = sim::kNoDef;                 // egység / épület sablonja
  int research = -1;                          // kutatás sorszáma
  std::string tip_title, tip_text;
};
using CommandCard = std::array<std::optional<HudSlot>, 12>;

// a 4×3-as kijelölés-rács egy cellája
struct SelCell {
  enum Kind : uint8_t { One, Group, Queue, More } kind = One;
  std::vector<sim::EntityId> ents;
  int idx = 0;                                // gyártósor: az elem sorszáma
};

// kattintható / súgós terület (a régi `regions`)
struct HudRegion {
  Rect r;
  std::string kind;                           // "item", "emblem", "cell", "slot", "info"
  std::string data;                           // pl. a fajcímernél a nép neve
  std::function<void()> left, right;
  std::string tip_title, tip_text;
};

// célzási mód (a régi `mode`)
struct HudMode {
  enum Kind : uint8_t { None, Move, Attack, Cast, AbilityPlace, Place, Unpack, GCast } kind = None;
  std::string ability;
  uint16_t def = sim::kNoDef;
  sim::EntityId ent = 0;
  bool operator==(const HudMode&) const = default;
};

// a karakterlap egy sávja a portré alatt / az adatoszlopban (energia, pajzs, harci kedv)
struct CharBar {
  float frac = 0;
  std::array<uint8_t, 3> col{};
  std::string label, tip_title, tip_text;
};

// a minimap egy jele (a minimap-téglalap bal felső sarkától, képpontban)
struct MiniMark {
  enum Kind : uint8_t { Resource, Building, Unit } kind = Unit;
  float x = 0, y = 0, w = 0, h = 0;
  std::array<uint8_t, 3> col{};
};

// a karakterlap átmeneti állapota (név, hátralévő mp, szín, magyarázat)
struct HudEffect {
  std::string name;
  float left = 0;
  std::array<uint8_t, 3> col{};
  std::string desc;
};

class HudModel {
 public:
  using NameFn = std::function<std::string(const std::string&)>;
  using PostFn = std::function<void(const sim::Command&)>;
  static constexpr int kKeyEscape = 27, kKeyF8 = 0x108, kKeyF2 = 0x102, kKeyF3 = 0x103, kKeyF4 = 0x104, kKeySpace = 0x120;

  // a forrásadatok (képkockánként); a `names` a megjelenített név a fordításból, a `post` a parancs-API
  void bind(const Snapshot* snap, const sim::Rules* rules, NameFn names, PostFn post);
  void set_map_size(int w, int h) { map_w_ = w; map_h_ = h; }
  void set_screen(int sw, int sh);
  const HudLayout& layout() const { return L_; }
  const Snapshot* snap() const { return snap_; }
  const sim::Rules* rules() const { return rules_; }
  std::string name(const std::string& key) const { return names_ ? names_(key) : key; }
  // a HUD saját felirata a fordításból (`hud.<kulcs>`; a „{}” helyére sorban az `args`)
  std::string tr(const std::string& key, std::initializer_list<std::string> args = {}) const;

  // ---- állapot (a régi UI-mezők)
  uint8_t player = 1;
  std::vector<Rect> mission_choices;   // M11: a párbeszéd válaszgombjai (a rajzolás tölti; a hívó kattintásra parancsot küld)
  std::vector<sim::EntityId> sel;
  bool sel_changed = false;                   // a HUD módosította a kijelölést (a hívó átveszi)
  std::string cmd_page;                       // "" | "build" | "learn"
  int page_no = 0;
  bool learn_mode = false;
  HudMode mode;
  bool text_focus = false, paused = false, path_hidden = false, show_allies = false, quests_open = false;
  bool shift = false;                         // Shift lenyomva (5× képzés)
  bool menu_requested = false;                // a fogaskerék: a szünetmenü (a hívó nyitja)
  // a meccs utáni összesítő (a régi `draw_summary`): fül (0 áttekintés, 1 gazdaság, 2 katonaság, 3 grafikon), a fülek és
  // a „Főmenü” gomb téglalapja (a rajzoló tölti), a kilépés kérése (a hívó zárja a meccset)
  int sum_tab = 0;
  std::vector<Rect> sum_tab_btn;
  Rect sum_menu_btn{}, sum_diag_btn{};
  bool quit_requested = false;
  bool diag_requested = false;                // „Diagnosztikai jelentés mentése” (a hívó menti; DIAGNOSTICS_DESIGN 5.)
  bool match_over() const;                    // vége a meccsnek a helyi játékos szemszögéből (az összesítő látszik)
  // a régi `_score`: összes, katonai, gazdasági, építkezés
  static std::array<int64_t, 4> score(const SnapPlayer& p);
  uint8_t formation = 1;                      // a mozgásparancs alakzata
  double t = 0;                               // a felület órája (másodperc)
  int slot_flash = -1;
  double slot_flash_t = -9;
  std::array<double, sim::kResources> res_flash{-99, -99, -99, -99};
  // AS-03: a legutóbbi elutasított kattintás rövid, lokalizált oka (a HUD 2 mp-ig mutatja); csak a kliensoldalon
  // biztosan ismert okok: hiányzó nyersanyag, betelt létszám
  std::string notice;
  double notice_t = -99;
  // a régi jobb felső sarok (`hud.py` `draw_clock`): óra, sebesség −/+ gombbal, alatta „fps • F1: súgó”
  int speed_pct = 100;          // a kijelzett sebesség (a futtatótól)
  bool speed_locked = false;    // hálózati játék: nem állítható
  int speed_change = 0;         // a kattintás/billentyű kérése (−1/+1); a főciklus alkalmazza és nullázza
  std::array<Rect, 2> speed_btn{};   // [0]: −, [1]: + (a rajzolás tölti ki)
  bool help_open = false;       // F1: a súgó panel
 private:
  std::unordered_map<sim::EntityId, int32_t> seen_hp_;   // a riasztáshoz: az előző pillanatkép életereje
  std::unordered_map<sim::EntityId, SnapUnit> seen_bld_;  // a saját épületek („Elpusztult: …”, a régi `notify`)
  int64_t seen_tick_ = -1;
  double hero_click_t_ = -9;
  sim::EntityId hero_click_id_ = 0;
 public:
  static int next_speed(int pct, int dir);   // a régi lépcsők: 50, 75, 100, 125, 150, 200, 300
  // a kijelzett sor: bal oldalt, az alsó sáv fölött (a régi játék `draw_hud` üzenetsora); a legutóbbi 6, időbélyeggel
  struct Notice {
    std::string text;
    double t = 0;
    std::array<uint8_t, 3> col{255, 160, 90};
  };
  std::vector<Notice> notice_log;
  void post_notice(std::string text, std::array<uint8_t, 3> col = {255, 160, 90});
  // a pillanatkép építési próbája szerint ide nem építhető (`why`: a lefordított ok); ismeretlen helyre: nem tiltott
  bool place_blocked(uint16_t def, int tx, int ty, std::string* why) const;
  // a régi hősportrék (jobb felül, legfeljebb 4): a rajzolás tölti ki; kattintás kijelöl, dupla kattintás odaugrik
  std::array<Rect, 4> hero_btn{};
  std::array<sim::EntityId, 4> hero_ids{};
  std::vector<const SnapUnit*> my_heroes() const;
  void select_hero(size_t i);                 // F2/F3/F4 és a portré: már kijelölve → odaugrik
  // a régi riasztás („Támadás ér!”, 12 mp-enként legfeljebb egy) és a Szóköz: az utolsó riasztás (10 mp-en belül)
  // vagy a bázis; a pillanatképből (a saját egységek és épületek életerejének csökkenése)
  void watch_alerts();
  void jump_home_or_alert();
  int64_t alert_tick = -1000000;
  float alert_x = 0, alert_y = 0;
  bool jump = false;                          // kameraugrás kérése (világ-képpont)
  float jump_x = 0, jump_y = 0;

  // ---- nézetmodell
  std::string selection_kind() const;         // none | multi | resource | building | hero | unit
  std::vector<SelCell> sel_cells() const;
  const SnapUnit* primary() const;
  CommandCard command_card();
  bool input_blocked() const;
  bool path_pending() const;
  bool activate_slot(int i, int button = 1);
  std::vector<HudRegion> middle_regions();    // a 2×3-as rész: hősnél a tárgyak, különben a fajcímer
  std::vector<HudRegion> sel_regions();       // a kijelölés-rács cellái
  std::vector<std::pair<Rect, std::string>> path_cards() const;   // irányválasztó ablak: kártya → irány
  std::vector<sim::EntityId> idle_workers() const;

  // ---- bemenet (igaz: elfogyasztotta, a világ nem kapja meg)
  bool key_down(int key, bool repeat = false, bool mods = false);
  void key_up(int key);
  bool click(float x, float y, int button);
  bool over_hud(float x, float y) const;
  // bal/jobb kattintás a világban célzási módban (`under`: a pont alatti ellenség, 0 = nincs); igaz: elfogyasztotta
  bool world_click(float wx, float wy, int button, sim::EntityId under);
  void place_tile(float wx, float wy, uint16_t def, int* tx, int* ty) const;   // az épület bal felső csempéje

  // ---- a karakterlap adatai (a rajzolónak)
  const SnapUnit* unit(sim::EntityId id) const { return snap_ ? snap_->find(id) : nullptr; }
  const sim::EntityDef* def_of(const SnapUnit& u) const;
  const sim::FactionDef* faction_of(uint8_t owner) const;
  std::string faction_name(const std::string& fid) const { return name("faction." + fid + ".name"); }
  std::string unit_name(const SnapUnit& u) const;
  std::string unit_type(const SnapUnit& u) const;                      // a típus kulcsa (pl. kozelharcos; épület: epulet)
  std::pair<std::string, std::string> type_info(const std::string& key) const;   // név, magyarázat
  // erős ez ellen / gyenge ez ellen: (típus, szorzó) csökkenő szorzó szerint
  std::pair<std::vector<std::pair<std::string, float>>, std::vector<std::pair<std::string, float>>> counters(
      const SnapUnit& u, const std::string& key) const;
  std::vector<HudEffect> active_effects(const SnapUnit& u) const;
  // M9: a második sáv(ok) sorrendben: energia, pajzs, Ooni harci kedv (az első a portré alá, a többi az adatoszlopba)
  std::vector<CharBar> char_bars(const SnapUnit& u) const;
  std::string rank_name(const SnapUnit& u) const;                     // előléptetés: "" | Veterán | Elit
  float power_cd(const SnapUnit& b, uint16_t ability) const;          // épületképesség újratöltésének aránya (0..1)
  std::pair<int, int> pray_slots(const SnapUnit& b) const;            // fohászhelyek: (foglalt, összes); összes 0: nincs
  // minimap (a régi `ui.draw_minimap`): a terep színei csempénként (sorfolytonosan), a jelek (lelőhely, épület,
  // egység csapatszínnel) és a látott rész kerete a minimapre vágva (képernyő-koordinátában)
  static std::vector<std::array<uint8_t, 3>> minimap_terrain(const map::Map& m, const map::TerrainTable& t);
  std::vector<MiniMark> minimap_marks() const;
  Rect minimap_view(float cam_x, float cam_y, float cam_w, float cam_h) const;
  std::string status_text(const SnapUnit& u) const;
  std::string res_name(int k) const;
  std::array<uint8_t, 3> res_color(int k) const;
  std::array<uint8_t, 3> player_color(uint8_t owner) const;
  std::array<uint8_t, 3> faction_color(const std::string& fid, bool accent) const;
  bool has_faith() const;

 private:
  const SnapPlayer& pl(size_t i) const { return snap_->players[std::min<size_t>(i, sim::Sim::kMaxPlayers)]; }
  const SnapPlayer& self() const { return pl(player); }
  bool can_afford(const std::array<int32_t, sim::kResources>& cost) const;
  bool reqs_met(const std::vector<sim::EntityDef::Req>& reqs, std::vector<std::string>* miss = nullptr) const;
  bool path_visible(const sim::EntityDef& d) const;
  bool def_hidden(const sim::EntityDef& d) const;
  std::vector<std::string> roster(const std::string& fid, const char* key) const;   // Roster.units / buildings
  std::vector<std::string> ally_pack(const std::string& fid, const char* key) const;
  std::tuple<int, int, std::string> type_order(const SnapUnit& u) const;
  std::vector<const SnapUnit*> own_sel_units() const;
  std::vector<const SnapUnit*> own_sel_buildings() const;
  const SnapUnit* sel_hero() const;
  std::vector<uint16_t> unit_abilities(const SnapUnit& u) const;
  std::vector<uint16_t> sel_abilities() const;
  HudSlot make_slot(std::string id, std::string icon, std::string label, std::function<void()> fn) const;
  HudSlot entry_slot(HudSlot::Kind kind, uint16_t def, int research);
  CommandCard paged(std::vector<HudSlot> items, std::optional<HudSlot> tail);
  void fill_ctx(CommandCard& slots, std::vector<HudSlot> ctx);
  HudSlot back_slot();
  std::vector<HudSlot> ally_slots();
  void open_page(const std::string& page);
  void use_ability(uint16_t ab);
  void train_from(uint16_t def, std::vector<sim::EntityId> blds);
  void cancel_type(HudSlot::Kind kind, uint16_t def, int research);
  void sel_cell_click(const SelCell& c, int button);
  void select_idle_worker();
  void minimap_click(float x, float y, int button);
  void cmd(sim::Command c);
  sim::Command base_cmd(sim::Command::Kind k) const;

  const Snapshot* snap_ = nullptr;
  const sim::Rules* rules_ = nullptr;
  NameFn names_;
  PostFn post_;
  HudLayout L_;
  int sw_ = 1280, sh_ = 720;
  int map_w_ = 1, map_h_ = 1;
  std::set<int> held_;
};

}  // namespace totu::client
