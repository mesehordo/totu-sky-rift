// SPDX-License-Identifier: GPL-3.0-or-later
// Menük (M8; a régi `rts/menu.py` és `rts/setup_screen.py` szerint): főmenü oldalsávval és almenükkel, egyéni csata
// beállító képernyője (pálya, játékoshelyek népe és gépe, köd, napszak, időjárás, táborok, hadtáp), beállítások
// (nyelv, felbontás, teljes képernyő, felület mérete, színvak-barát színek, görgetés, kijelzés), hang- és
// billentyűkiosztás-alképernyő.
//
// Ez a réteg grafikus kártya nélküli: az állapotgép, az elrendezés (a régi játék méretei 1280×720-ról a képernyőhöz
// arányítva, `ui_scale`) és a bevitel (kattintás, húzás, billentyű). Minden felirat a `Locale` szerint fordul. A
// rajzolás (`menu_view`) csak kirajzolja a `widgets()`, `texts()` és `panels()` elemeit.
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "content/vfs.hpp"
#include <vector>

#include "client/i18n.hpp"
#include "client/settings.hpp"
#include "map/map.hpp"
#include "script/victory.hpp"
#include "sim/sim.hpp"

namespace totu::client {

struct MenuRect {
  float x = 0, y = 0, w = 0, h = 0;
  bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
  float cx() const { return x + w / 2; }
  float cy() const { return y + h / 2; }
};

// --- tartalom (pályák, népek) ---------------------------------------------------------------------------------------
struct MapInfo {
  std::string path, name, desc;   // a név a fájlnévből, a leírás a `_leiras` mezőből
  int players = 0, width = 0, height = 0;
  int order = 1000000;   // a régi menü sorrendje (`order`); ha nincs, a végére, név szerint
};
// a mappa kezdőhelyes pályái (név szerint rendezve); a kezdőhely nélküli (próba)pályák kimaradnak
// H-65: terep-táblával a teljes pályaellenőrzés (Map::from_json) is lefut, a hibás pálya nem választható
std::vector<MapInfo> scan_maps(const std::string& dir, const map::TerrainTable* terrain = nullptr);

struct FactionInfo {
  std::string id;         // pl. "ursan" (a MatchSetup ezt várja)
  std::string name_key;   // pl. "faction.ursan.name" (a Locale adja a nevét)
};
// a főnépek (a kisnépek – `ally` – és a semleges Vadon nélkül), azonosító szerint
std::vector<FactionInfo> scan_factions(const std::string& content_dir);

// --- egyéni csata ---------------------------------------------------------------------------------------------------
struct SlotSetup {
  enum class Kind : uint8_t { Human = 0, Ai = 1, Closed = 2 };
  Kind kind = Kind::Closed;
  std::string faction = "random";
  // gépi helyen (M9): az alap gép nehézsége és személyisége
  std::string difficulty = "normal";
  std::string personality = "kiegyenlitett";
};

// az alap gépi ellenfél (`content/base/ai/alap`) nehézségei és személyiségei a lenyílókhoz (a csomag sorrendjében)
const std::vector<std::string>& ai_difficulty_ids();
const std::vector<std::string>& ai_personality_ids();

struct SkirmishSetup {
  size_t map = 0;
  std::array<SlotSetup, 8> slots{};   // az 1. hely a játékosé
  sim::MatchSetup::Fog fog = sim::MatchSetup::Fog::Normal;
  sim::MatchSetup::Daytime daytime = sim::MatchSetup::Daytime::Effect;
  sim::MatchSetup::WeatherMode weather = sim::MatchSetup::WeatherMode::Mood;
  bool camps = true;    // a Vadon táborai (a régi „Szörnytáborok”)
  bool upkeep = true;   // Hadtáp
  // M12 (D46): győzelmi feltétel és tempóprofil (munkanevek – D20)
  sim::MatchSetup::Victory victory = sim::MatchSetup::Victory::Conquest;
  sim::MatchSetup::Tempo tempo = sim::MatchSetup::Tempo::Classic;
  std::string victory_script;   // M13: Szkript módban a modbeli győzelmi szkript azonosítója
  SkirmishSetup();
};

// a meccs indításához: a pálya, a MatchSetup (a véletlen népek feloldva; a gépi helyek a `setup.ai`-ban, M9) és a
// gépi játékosok (1-től)
struct MatchConfig {
  std::string map_path;
  sim::MatchSetup setup;
  std::vector<int> ai;
};
// false és a hibaüzenet kulcsa (a .po-ban), ha a beállítás nem indítható (pl. kevesebb mint két résztvevő)
bool make_match_config(const SkirmishSetup& s, const std::vector<MapInfo>& maps, const std::vector<FactionInfo>& factions,
                       uint64_t seed, MatchConfig* out, std::string* error_hu);

// --- az elrendezés elemei -------------------------------------------------------------------------------------------
enum class MenuScreen : uint8_t { Main = 0, Skirmish, Settings, Sound, Keys, Mods };

enum class MenuAct : uint8_t {
  None = 0,
  Toggle,        // főmenü: almenü nyitása/zárása (arg: a menüpont)
  Skirmish, Settings, Quit, Back, Start,
  PickMap,       // arg: a pálya indexe
  SlotKind,      // lenyíló; arg: a hely
  SlotFaction,   // lenyíló; arg: a hely
  SlotDifficulty,    // lenyíló; arg: a hely (M9)
  SlotPersonality,   // lenyíló; arg: a hely (M9)
  Option,        // lenyíló; arg: 0 köd, 1 napszak, 2 időjárás, 3 táborok, 4 hadtáp
  Cycle,         // beállítás léptetése; arg: a sor
  OpenSound, OpenKeys,
  SoundToggle,   // arg: a hangkategória
  SoundSlider,   // arg: a hangkategória (a `slider` mező az érték)
  KeyCell,       // arg: a parancsrács helye
  KeysDefault,
  // M13, D50: a modkezelő (a 0 A.D. szerint)
  OpenMods,
  ModPick,       // arg: < 1000 az elérhetők listájában, ≥ 1000 az engedélyezettekében
  ModEnable, ModDisable, ModUp, ModDown, ModSave,
};

struct MenuWidget {
  enum class Kind : uint8_t { Item, Button, Dropdown, Slider, KeyCell, MapRow };
  Kind kind = Kind::Button;
  MenuRect r;
  std::string label;                 // lefordítva
  std::string hint;                  // főmenü: a súgó az oldalsáv alján
  std::string tag;                   // kis jelölés a felirat mellett (lefordított „hamarosan”)
  bool enabled = true;
  bool soon = false;                 // „hamarosan” (a régi játékban megvolt, az új motorban még nincs)
  bool sub = false;                  // almenüt nyit (▸)
  bool selected = false;
  bool small = false;                // kisebb betűs gomb (beállítás-értékek)
  MenuAct act = MenuAct::None;
  int arg = 0;
  std::vector<std::string> options;  // lenyíló: a választható feliratok
  int current = 0;                   // lenyíló: a kijelölt; csúszka: az érték (0..100)
  int team = -1;                     // lenyíló: a csapatszín-négyzet (a hely sorszáma), -1: nincs
};

struct MenuText {
  enum Font : uint8_t { Tiny = 0, Small, Med, Big, Huge };
  enum Color : uint8_t { Text = 0, Gold, Dim, Warn, Light };
  float x = 0, y = 0, w = 0;   // y: a sor teteje; w: a szélesség (a hosszabb szöveg kisebb betűre vált, majd rövidül)
  std::string text;
  Font font = Small;
  Color color = Text;
  bool center = false;
  bool title = false;          // a képernyő címe (árnyékolt, arany, a régi `Screen.title`)
  int max_lines = 1;           // >1: tördelt
};

struct MenuPanel {
  MenuRect r;
  int alpha = 200;
  bool gold_top = false;   // az almenü-panel arany felső vonala
};

class Menu {
 public:
  // változásjelzők (a hívó menti a fájlt, alkalmazza a képernyőt, a nyelvet, a csapatszíneket)
  enum Change : uint32_t { kSave = 1, kDisplay = 2, kLanguage = 4, kColors = 8 };
  enum class Result : uint8_t { None = 0, Start, Quit, Restart };

  Menu(Settings& settings, Locale& locale, std::vector<MapInfo> maps, std::vector<FactionInfo> factions);

  // a régi `ui_scale`: 1280×720-on 1, nagyobb képernyőn arányosan nagyobb (0,8 … 3), szorozva a felület-mérettel
  static float base_scale(int w, int h);
  float scale() const { return u_; }
  float k(float v) const;   // méret 1280×720-as alapról

  void set_logo_aspect(float a) { logo_aspect_ = a; }   // a logó szélesség/magasság aránya (0: nincs logó)
  void layout(int w, int h);                            // a widgetek, szövegek, panelek az adott képernyőre

  // bevitel (a legutóbbi `layout` szerint); button: 1 bal, 3 jobb (léptetésnél visszafelé)
  Result click(float x, float y, int button = 1);
  void drag(float x, float y);   // lenyomott bal gombbal mozgatás (csúszka)
  void release();                // a bal gomb felengedése
  // billentyű: 27 Esc, 13 Enter, különben a betű (A–Z, nagybetűvel); a billentyűkiosztás-alképernyő ezt veszi át
  Result key(int key);

  MenuScreen screen() const { return screen_; }
  void go(MenuScreen s);
  int open_submenu() const { return open_; }
  // a becsúszás órája (a menü ciklusa adja minden képkockán; másodperc)
  void set_time(double t) { now_ = t; }
  void set_status(std::string s) { status_ = std::move(s); }   // a lábléc fölötti állapotsor (pl. a grafika előkészítése)
  int capture_slot() const { return capture_; }   // billentyűkiosztás: erre a helyre vár új billentyűt (-1: nincs)
  const std::string& message() const { return message_; }
  uint32_t take_changes() {
    const uint32_t c = changes_;
    changes_ = 0;
    return c;
  }

  const std::vector<MenuWidget>& widgets() const { return widgets_; }
  const std::vector<MenuText>& texts() const { return texts_; }
  const std::vector<MenuPanel>& panels() const { return panels_; }
  const MenuWidget* find(MenuAct act, int arg = 0) const;   // a próbáknak és a rajzolónak
  // a nyitott lenyíló (nullptr: nincs) és a listája (képernyő-téglalapok, a választható feliratok sorrendjében)
  const MenuWidget* open_dropdown() const;
  std::vector<MenuRect> dropdown_items() const;
  int sidebar_width() const { return static_cast<int>(sidebar_w_); }   // főmenü: az oldalsáv (0: nincs)
  MenuRect logo_rect() const { return logo_; }

  SkirmishSetup& skirmish() { return sk_; }
  // M13: a választható modok (az alap nélkül) – a modkezelő listái; a szerkesztett lista mentéskor a beállításokba
  void set_mods(std::vector<content::ModInfo> available) { mods_avail_ = std::move(available); }
  // M13: a modok győzelmi szkriptjei – a győzelmi feltétel lenyílójában a beépítettek után
  void set_victories(std::vector<script::VictoryInfo> v) { victories_ = std::move(v); }
  const std::vector<script::VictoryInfo>& victories() const { return victories_; }
  const std::vector<std::string>& mods_enabled() const { return mods_on_; }
  const std::vector<std::string>& mods_errors() const { return mods_err_; }
  const std::vector<MapInfo>& maps() const { return maps_; }
  const std::vector<FactionInfo>& factions() const { return factions_; }
  // az indításhoz (a `Start` eredmény után)
  bool match_config(uint64_t seed, MatchConfig* out);

  // a beállítás-sorok száma és felirata (a próbáknak)
  static constexpr int kSettingRows = 11;

 private:
  void layout_main();
  void layout_skirmish();
  void layout_settings();
  void layout_sound();
  void layout_keys();
  void layout_mods();
  void mods_check();   // a szerkesztett készlet hibái (függőség, ütközés) a mentés előtt
  void title(const std::string& text);
  void back_button();
  MenuWidget& add(MenuWidget::Kind kind, MenuRect r, std::string label, MenuAct act, int arg = 0);
  void text(float x, float y, float w, std::string s, MenuText::Font f, MenuText::Color c, bool center = false,
            int lines = 1);
  Result activate(const MenuWidget& w, int button, float x);
  void choose(MenuAct act, int arg, int index);   // lenyíló választása
  void cycle(int row, int dir);
  void set_slider(int cat, float x);
  void apply_map();
  std::string tr(const char* hu) const { return loc_.tr(hu); }

  Settings& st_;
  Locale& loc_;
  std::vector<MapInfo> maps_;
  std::vector<content::ModInfo> mods_avail_;
  std::vector<script::VictoryInfo> victories_;
  std::vector<std::string> mods_on_, mods_err_;
  std::string mod_sel_;
  std::vector<FactionInfo> factions_;
  SkirmishSetup sk_;
  MenuScreen screen_ = MenuScreen::Main;
  int open_ = -1;                     // főmenü: a nyitott almenü
  MenuAct open_dd_ = MenuAct::None;   // a nyitott lenyíló
  int open_dd_arg_ = 0;
  int dragging_ = -1;                 // a húzott hangcsúszka
  int capture_ = -1;
  std::string message_;
  uint32_t changes_ = 0;
  float logo_aspect_ = 0;
  int w_ = 1280, h_ = 720;
  float u_ = 1;
  float sidebar_w_ = 0;
  // a tulajdonos kérése: a menük oldalt – a beállítás-képernyők az oldalsáv mellett, balról becsúszva; a terület
  // (x, szélesség), ahová az oldalsáv melletti képernyő rajzol (0: a teljes ablak)
  float area_x_ = 0, area_w_ = 0;
  std::string status_;
  double now_ = 0, anim_t_ = -9;   // az utolsó nyitás ideje (almenü, oldalsó képernyő)
  float anim() const;              // 0..1, lassuló (0,14 mp – a régi `MainMenu` almenüje)
  void layout_sidebar(int highlight);
  void side_area(const char* highlight = "ui.main.settings");
  MenuRect logo_;
  std::vector<MenuWidget> widgets_;
  std::vector<MenuText> texts_;
  std::vector<MenuPanel> panels_;
};

}  // namespace totu::client
