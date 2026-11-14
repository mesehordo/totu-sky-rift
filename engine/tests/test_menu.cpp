// SPDX-License-Identifier: GPL-3.0-or-later
// Menük és beállítások (M8; a régi `rts/menu.py`, `rts/setup_screen.py`, `rts/i18n.py`, `rts/profiles.py` szerint):
// a menü-állapotgép, az egyéni csata beállítása → MatchSetup, a beállításfájl írása és olvasása, a billentyűkiosztás
// ütközése, a nyelvek a locale mappa .po fájljaiból és a nyelvváltás – mind grafikus kártya nélkül.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "check.hpp"
#include "client/i18n.hpp"
#include "client/menu.hpp"
#include "client/session.hpp"
#include "client/settings.hpp"

using namespace totu;
using client::Menu;
using client::MenuAct;
using client::MenuScreen;

namespace {

std::vector<client::MapInfo> test_maps() {
  return client::scan_maps(std::string(TOTU_TEST_DATA) + "/maps");
}

client::Locale& real_locale() {
  static client::Locale loc = [] {
    client::Locale l;
    l.load(TOTU_CONTENT_DIR);
    return l;
  }();
  return loc;
}

// egy widget közepére kattint (a legutóbbi elrendezés szerint)
Menu::Result press(Menu& m, MenuAct act, int arg = 0, int button = 1) {
  const client::MenuWidget* w = m.find(act, arg);
  CHECK(w != nullptr);
  if (!w) return Menu::Result::None;
  const auto r = m.click(w->r.cx(), w->r.cy(), button);
  m.layout(1280, 720);
  return r;
}

// a nyitott lenyíló `index`-edik elemére kattint
void pick(Menu& m, MenuAct act, int arg, int index) {
  press(m, act, arg);
  const auto items = m.dropdown_items();
  CHECK(static_cast<int>(items.size()) > index);
  if (static_cast<int>(items.size()) <= index) return;
  m.click(items[static_cast<size_t>(index)].cx(), items[static_cast<size_t>(index)].cy());
  m.layout(1280, 720);
}

std::filesystem::path temp_dir(const char* name) {
  const auto p = std::filesystem::temp_directory_path() / name;
  std::error_code ec;
  std::filesystem::remove_all(p, ec);
  std::filesystem::create_directories(p, ec);
  return p;
}

}  // namespace

TEST_CASE("M8 felulet – menü: a főmenü a régi pontjaival – egyéni csata, beállítások, kilépés él, a többi hamarosan") {
  client::Settings st;
  Menu m(st, real_locale(), test_maps(), client::scan_factions(TOTU_CONTENT_DIR));
  m.layout(1280, 720);
  CHECK(m.screen() == MenuScreen::Main);
  CHECK(m.sidebar_width() == 320);
  // a régi MAIN_ITEMS: Egyjátékos ▸, Többjátékos, Történetek, Beállítások ▸, Modok, Készítők, Kilépés
  int items = 0, soon = 0;
  for (const auto& w : m.widgets()) {
    if (w.kind != client::MenuWidget::Kind::Item) continue;
    ++items;
    if (w.soon) {
      ++soon;
      CHECK(!w.enabled);
      CHECK(w.tag == "hamarosan");
    }
  }
  CHECK(items == 7);
  CHECK(soon == 4);   // többjátékos, történetek, modok, készítők
  CHECK(m.find(MenuAct::Toggle, 0)->label == "Egyjátékos");
  CHECK(m.find(MenuAct::Toggle, 0)->sub);
  // a letiltott pontra kattintás nem tesz semmit
  const auto* lan = m.find(MenuAct::None, 1);
  CHECK(lan && lan->soon);
  CHECK(m.click(lan->r.cx(), lan->r.cy()) == Menu::Result::None);
  // almenü: kampány, novellák, betöltés hamarosan; egyéni csata él
  press(m, MenuAct::Toggle, 0);
  CHECK(m.open_submenu() == 0);
  CHECK(m.find(MenuAct::Skirmish, 2) && m.find(MenuAct::Skirmish, 2)->enabled);
  CHECK(m.find(MenuAct::None, 0)->soon);   // kampány
  CHECK(m.panels().size() == 1 && m.panels()[0].gold_top);
  // Esc: előbb csak az almenü zárul, utána kilépés (mint a régi játékban)
  CHECK(m.key(27) == Menu::Result::None);
  CHECK(m.open_submenu() == -1);
  m.layout(1280, 720);
  CHECK(m.key(27) == Menu::Result::Quit);
  CHECK(press(m, MenuAct::Quit, 6) == Menu::Result::Quit);
  // az almenün kívüli kattintás bezárja
  press(m, MenuAct::Toggle, 3);
  CHECK(m.open_submenu() == 3);
  m.click(1200, 700);
  CHECK(m.open_submenu() == -1);
}

TEST_CASE("M8 felulet – menü: egyéni csata – pálya, nép, gép, köd, napszak, időjárás, táborok → MatchSetup") {
  client::Settings st;
  const auto factions = client::scan_factions(TOTU_CONTENT_DIR);
  // a főnépek: a kisnépek és a semleges Vadon nélkül
  CHECK(factions.size() == 7);
  for (const auto& f : factions) CHECK(f.id != "vadon" && f.id != "daresh");
  const auto maps = test_maps();
  CHECK(maps.size() == 2);   // csak a kezdőhelyes pályák (BUILD-ALAP, ECO-ALAP)
  CHECK(maps[1].name == "ECO-ALAP" && maps[1].players == 2 && maps[1].width == 40);
  Menu m(st, real_locale(), maps, factions);
  m.layout(1280, 720);
  press(m, MenuAct::Toggle, 0);
  press(m, MenuAct::Skirmish, 2);
  CHECK(m.screen() == MenuScreen::Skirmish);
  press(m, MenuAct::PickMap, 1);
  CHECK(m.skirmish().map == 1);
  CHECK(m.find(MenuAct::PickMap, 1)->selected);
  // az 1. hely a játékosé (nem állítható); a 3–8. hely a 2 fős pályán zárt és letiltott
  CHECK(!m.find(MenuAct::SlotKind, 0)->enabled);
  CHECK(m.find(MenuAct::SlotKind, 1)->enabled);
  CHECK(!m.find(MenuAct::SlotKind, 2)->enabled);
  // nép: az 1. helyen ursan, a gépnek Wullof
  int urs = 0, wul = 0;
  for (size_t i = 0; i < factions.size(); ++i) {
    if (factions[i].id == "ursan") urs = static_cast<int>(i) + 1;
    if (factions[i].id == "wullof") wul = static_cast<int>(i) + 1;
  }
  pick(m, MenuAct::SlotFaction, 0, urs);
  pick(m, MenuAct::SlotFaction, 1, wul);
  CHECK(m.skirmish().slots[0].faction == "ursan");
  CHECK(m.skirmish().slots[1].faction == "wullof");
  pick(m, MenuAct::Option, 0, 2);   // köd: nincs
  pick(m, MenuAct::Option, 1, 1);   // napszak: hangulat
  pick(m, MenuAct::Option, 2, 3);   // időjárás: hatással és pályaváltozással
  pick(m, MenuAct::Option, 3, 1);   // táborok: ki
  pick(m, MenuAct::Option, 4, 1);   // hadtáp: ki
  CHECK(m.find(MenuAct::Option, 0)->current == 2);
  CHECK(press(m, MenuAct::Start) == Menu::Result::Start);
  client::MatchConfig c;
  CHECK(m.match_config(7, &c));
  CHECK(c.map_path == maps[1].path);
  CHECK(c.setup.factions.size() == 2 && c.setup.factions[0] == "ursan" && c.setup.factions[1] == "wullof");
  CHECK(c.ai.size() == 1 && c.ai[0] == 2);
  CHECK(c.setup.fog == sim::MatchSetup::Fog::None);
  CHECK(c.setup.daytime == sim::MatchSetup::Daytime::Mood);
  CHECK(c.setup.weather == sim::MatchSetup::WeatherMode::EffectTerrain);
  CHECK(!c.setup.camps && !c.setup.upkeep);
  // Esc: vissza a főmenübe
  m.key(27);
  CHECK(m.screen() == MenuScreen::Main);
}

TEST_CASE("M8 felulet – menü: egyéni csata – legalább két résztvevő; a véletlen nép a magból determinisztikus") {
  client::Settings st;
  const auto factions = client::scan_factions(TOTU_CONTENT_DIR);
  Menu m(st, real_locale(), test_maps(), factions);
  m.go(MenuScreen::Skirmish);
  m.layout(1280, 720);
  pick(m, MenuAct::SlotKind, 1, 1);   // a 2. hely zárt
  CHECK(m.skirmish().slots[1].kind == client::SlotSetup::Kind::Closed);
  CHECK(!m.find(MenuAct::SlotFaction, 1)->enabled);
  CHECK(press(m, MenuAct::Start) == Menu::Result::None);
  CHECK(m.message() == "Legalább két résztvevő kell.");
  pick(m, MenuAct::SlotKind, 1, 0);   // újra gép
  CHECK(m.key(13) == Menu::Result::Start);   // Enter: indítás
  client::MatchConfig a, b, c;
  CHECK(m.match_config(11, &a) && m.match_config(11, &b));
  CHECK(a.setup.factions == b.setup.factions);
  for (const auto& f : a.setup.factions) CHECK(!f.empty() && f != "random");
  bool differs = false;   // más maggal (legalább egyszer) más a sorsolás
  for (uint64_t s = 12; s < 40 && !differs; ++s) {
    CHECK(m.match_config(s, &c));
    differs = c.setup.factions != a.setup.factions;
  }
  CHECK(differs);
}

TEST_CASE("M8 felulet – meccs: az egyéni csata beállítása elindul az adatvezérelt géppel (`ai_nep.lua`)") {
  client::Settings st;
  Menu m(st, real_locale(), test_maps(), client::scan_factions(TOTU_CONTENT_DIR));
  m.skirmish().map = 1;
  m.skirmish().slots[0].faction = "imperium";
  m.skirmish().slots[1].faction = "ooni";
  m.skirmish().fog = sim::MatchSetup::Fog::Explored;
  client::MatchConfig c;
  CHECK(m.match_config(5, &c));
  client::Session s;
  std::string err;
  CHECK(s.load(TOTU_CONTENT_DIR, c.map_path, 5, &err));
  CHECK(s.start_match(c.setup, c.ai, "ai_nep.lua", &err));
  std::vector<client::FxEvent> fx;
  for (int i = 0; i < 90; ++i) s.step({}, fx);
  CHECK(s.sim().player(1).faction == "imperium");
  CHECK(s.sim().player(2).faction == "ooni");
}

TEST_CASE("M8 felulet – beállítások: JSON oda-vissza, hibatűrés, a fájl mentése és betöltése") {
  client::Settings st;
  st.language = "en";
  st.volume[1].value = 35;
  st.volume[6].enabled = false;
  st.width = 1920;
  st.height = 1080;
  st.fullscreen = true;
  st.ui_scale = 150;
  st.colorblind = true;
  CHECK(st.keys.assign(0, 'Q') == -1);
  st.fps_limit = 0;
  const std::string js = client::settings_to_json(st);
  client::Settings back;
  CHECK(client::settings_from_json(js, &back));
  CHECK(back == st);
  CHECK(back.keys.str() == "QERTSDFGXCVB");
  // hibás értékek: alapérték és figyelmeztetés; ismeretlen kulcs kimarad
  std::vector<std::string> warn;
  client::Settings bad;
  CHECK(client::settings_from_json(
      R"({"language":"x y","ui_scale":"nagy","grid_keys":"WWRTSDFGXCVB","resolution":"10x10","jovo":1,"show_fps":false})",
      &bad, &warn));
  CHECK(bad.language == "hu" && bad.ui_scale == 100 && bad.keys.str() == "WERTSDFGXCVB" && bad.width == 1280);
  CHECK(!bad.show_fps);
  CHECK(warn.size() == 4);
  CHECK(!client::settings_from_json("[1, 2]", &bad));
  // fájl: nincs → alapértékek; mentés után ugyanaz jön vissza
  const auto dir = temp_dir("totu_menu_proba");
  const std::string path = client::settings_path((dir / "TOTU" / "").string());
  client::Settings loaded;
  loaded.width = 1;
  CHECK(client::load_settings(path, &loaded));
  CHECK(loaded == client::Settings{});
  CHECK(client::save_settings(path, st));
  CHECK(client::save_settings(path, st));   // felülírás
  CHECK(client::load_settings(path, &loaded));
  CHECK(loaded == st);
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << "{ hibás";
  }
  std::string err;
  CHECK(!client::load_settings(path, &loaded, &err));
  CHECK(loaded == client::Settings{} && !err.empty());
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

TEST_CASE("M8 felulet – billentyűkiosztás: a parancsrács W E R T / S D F G / X C V B, ütközés-ellenőrzéssel") {
  client::Keymap k;
  CHECK(k.str() == "WERTSDFGXCVB");
  CHECK(k.slot_of('S') == 4 && k.slot_of('B') == 11 && k.slot_of('Q') == -1);
  CHECK(k.assign(0, 'E') == 1);   // ütközik az 1-es hellyel: nem állít át
  CHECK(k.key(0) == 'W');
  CHECK(k.assign(0, 'P') == client::Keymap::kInvalid);   // P: szünet
  CHECK(k.assign(0, '1') == client::Keymap::kInvalid);   // számjegy: csapat
  CHECK(k.assign(0, 'w') == -1);                         // ugyanaz (kisbetűvel) siker
  CHECK(k.assign(0, 'q') == -1 && k.key(0) == 'Q');
  k.assign_swap(1, 'Q');                                 // cserével: a 0. hely kapja az E-t
  CHECK(k.key(1) == 'Q' && k.key(0) == 'E');
  CHECK(k.conflicts().empty());
  CHECK(!k.parse("WWRTSDFGXCVB"));   // ütközés
  CHECK(!k.parse("WERT"));
  CHECK(k.parse("qwerasdfzxcv") && k.str() == "QWERASDFZXCV");
  k.reset();
  CHECK(k.str() == "WERTSDFGXCVB");
}

TEST_CASE("M8 felulet – menü: beállítások – léptetés, változásjelzők, hang- és billentyű-alképernyő") {
  client::Settings st;
  Menu m(st, real_locale(), test_maps(), client::scan_factions(TOTU_CONTENT_DIR));
  m.go(MenuScreen::Settings);
  m.layout(1280, 720);
  int buttons = 0;
  for (const auto& w : m.widgets()) buttons += w.act == MenuAct::Cycle || w.act == MenuAct::OpenSound || w.act == MenuAct::OpenKeys;
  CHECK(buttons == Menu::kSettingRows);
  press(m, MenuAct::Cycle, 1);   // felbontás: 1280×720 → 1280×760
  CHECK(st.width == 1280 && st.height == 760);
  CHECK(m.take_changes() == (Menu::kDisplay | Menu::kSave));
  press(m, MenuAct::Cycle, 1, 3);   // jobb gomb: visszafelé
  CHECK(st.height == 720);
  m.take_changes();
  press(m, MenuAct::Cycle, 2);
  CHECK(st.fullscreen);
  press(m, MenuAct::Cycle, 4);
  CHECK(st.colorblind);
  CHECK((m.take_changes() & Menu::kColors) != 0);
  press(m, MenuAct::Cycle, 3);
  CHECK(st.ui_scale == 125);
  press(m, MenuAct::Cycle, 8);
  CHECK(st.fps_limit == 120);
  CHECK(m.find(MenuAct::Cycle, 8)->label == "120");
  // hang: a csúszka a kattintás helyére áll, húzásra követ, felengedéskor ment
  press(m, MenuAct::OpenSound, 9);
  CHECK(m.screen() == MenuScreen::Sound);
  m.take_changes();
  const auto* sl = m.find(MenuAct::SoundSlider, 1);
  CHECK(sl != nullptr);
  m.click(sl->r.x, sl->r.cy());
  CHECK(st.volume[1].value == 0);
  m.drag(sl->r.x + sl->r.w, sl->r.cy());
  CHECK(st.volume[1].value == 100);
  CHECK(m.take_changes() == 0);
  m.release();
  CHECK(m.take_changes() == Menu::kSave);
  press(m, MenuAct::SoundToggle, 0);
  CHECK(!st.volume[0].enabled);
  CHECK(m.find(MenuAct::SoundToggle, 0)->label == "Ki");
  m.key(27);
  CHECK(m.screen() == MenuScreen::Settings);
  // billentyűk: kijelölés, ütközés, érvénytelen, siker, Esc, alapértelmezés
  m.layout(1280, 720);
  press(m, MenuAct::OpenKeys, 10);
  CHECK(m.screen() == MenuScreen::Keys);
  int cells = 0;
  for (const auto& w : m.widgets()) cells += w.kind == client::MenuWidget::Kind::KeyCell;
  CHECK(cells == 12);
  CHECK(m.find(MenuAct::KeyCell, 4)->label == "S");
  press(m, MenuAct::KeyCell, 0);
  CHECK(m.capture_slot() == 0);
  m.key('E');
  CHECK(m.capture_slot() == 0 && st.keys.key(0) == 'W');
  CHECK(m.message() == "Ütközés: a(z) E már a(z) 2. hely billentyűje.");
  m.key('P');
  CHECK(m.message() == "Ez a billentyű nem használható (foglalt: P, számjegyek).");
  m.key('Q');
  CHECK(m.capture_slot() == -1 && st.keys.key(0) == 'Q');
  CHECK((m.take_changes() & Menu::kSave) != 0);
  m.layout(1280, 720);
  press(m, MenuAct::KeyCell, 1);
  m.key(27);   // mégse: a képernyőn marad
  CHECK(m.capture_slot() == -1 && m.screen() == MenuScreen::Keys);
  press(m, MenuAct::KeysDefault);
  CHECK(st.keys.str() == "WERTSDFGXCVB");
  m.key(27);
  m.key(27);
  CHECK(m.screen() == MenuScreen::Main);
}

TEST_CASE("M8 felulet – menü: a régi méretarány (`ui_scale`) és a felület mérete (akadálymentesség)") {
  CHECK(Menu::base_scale(1280, 720) == 1.0f);
  CHECK(Menu::base_scale(1920, 1080) == 1.5f);
  CHECK(Menu::base_scale(800, 600) == 0.8f);
  CHECK(Menu::base_scale(1920, 720) == 1.0f);
  client::Settings st;
  Menu m(st, real_locale(), test_maps(), client::scan_factions(TOTU_CONTENT_DIR));
  m.layout(1920, 1080);
  CHECK(m.scale() == 1.5f);
  CHECK(m.sidebar_width() == 480);
  st.ui_scale = 125;
  m.layout(1920, 1080);
  CHECK(m.scale() == 1.875f);
  st.ui_scale = 200;   // a menü legfeljebb 1024×576-os alapra nő, hogy elférjen
  m.layout(1920, 1080);
  CHECK(m.scale() == 1.875f);
  // minden widget a képernyőn belül marad
  for (MenuScreen s : {MenuScreen::Main, MenuScreen::Skirmish, MenuScreen::Settings, MenuScreen::Sound, MenuScreen::Keys}) {
    m.go(s);
    m.layout(1280, 720);
    for (const auto& w : m.widgets()) {
      CHECK(w.r.x >= 0 && w.r.y >= 0 && w.r.x + w.r.w <= 1280.5f && w.r.y + w.r.h <= 720.5f);
    }
  }
}

TEST_CASE("M8 felulet – nyelvek: a locale mappa .po fájljaiból; hiányzó szöveg → angol → kulcs; nyelvváltás a menüben") {
  // a valódi tartalom: hu és en, a nevük a .po fejlécéből
  const client::Locale& real = real_locale();
  CHECK(real.has("hu") && real.has("en"));
  CHECK(real.language_name("hu") == "Magyar");
  CHECK(real.language_name("en") == "English");
  // .po elemzés: többsoros szöveg, kódolás, fejléc
  std::string header;
  const auto e = client::parse_po("msgid \"\"\nmsgstr \"\"\n\"Language-Name: Teszt\\n\"\n\n# megjegyzés\r\n"
                                  "msgctxt \"a.b\"\nmsgid \"Első \\\"sor\\\"\"\n\"folytatás\"\nmsgstr \"\"\n",
                                  &header);
  CHECK(e.size() == 1 && e[0].ctx == "a.b" && e[0].id == "Első \"sor\"folytatás" && e[0].str.empty());
  CHECK(header == "Language-Name: Teszt\n");
  // egy ideiglenes harmadik nyelv (tesztmappában): megjelenik a listában és betölthető
  const auto dir = temp_dir("totu_nyelv_proba");
  std::filesystem::create_directories(dir / "base" / "locale");
  std::filesystem::copy_file(std::filesystem::path(TOTU_CONTENT_DIR) / "base" / "locale" / "hu.po", dir / "base" / "locale" / "hu.po");
  std::filesystem::copy_file(std::filesystem::path(TOTU_CONTENT_DIR) / "base" / "locale" / "en.po", dir / "base" / "locale" / "en.po");
  {
    std::ofstream out(dir / "base" / "locale" / "xx.po", std::ios::binary);
    out << "msgid \"\"\nmsgstr \"\"\n\"Content-Type: text/plain; charset=UTF-8\\n\"\n\"Language-Name: Próbanyelv\\n\"\n\n"
           "msgctxt \"ui.main.quit\"\nmsgid \"Kilépés\"\nmsgstr \"Ki-ki\"\n\n"
           "msgctxt \"ui.csak.magyar\"\nmsgid \"Csak magyarul\"\nmsgstr \"\"\n";
  }
  {
    std::ofstream out(dir / "base" / "locale" / "hu.po", std::ios::binary | std::ios::app);
    out << "\nmsgctxt \"ui.csak.magyar\"\nmsgid \"Csak magyarul\"\nmsgstr \"\"\n";
  }
  client::Locale loc;
  CHECK(loc.load(dir.string()));
  CHECK(loc.languages().size() == 3);
  CHECK(loc.has("xx") && loc.language_name("xx") == "Próbanyelv");
  loc.set_lang("xx");
  CHECK(loc.lang() == "xx");
  CHECK(loc.tr("ui.main.quit") == "Ki-ki");                // a saját fordítás
  CHECK(loc.tr("ui.back") == "Back");                     // hiányzik → angol
  CHECK(loc.tr("ui.csak.magyar") == "ui.csak.magyar");    // az angolból is hiányzik → a kulcs
  CHECK(loc.tr("ui.nincs.ilyen") == "ui.nincs.ilyen");
  loc.set_lang("hu");
  CHECK(loc.tr("ui.csak.magyar") == "Csak magyarul");     // a forrásnyelv az msgid-ből
  CHECK(loc.tr("ui.keys.press", {{"n", "3"}}) == "Nyomd le az új billentyűt a(z) 3. helyhez (Esc: mégse).");
  loc.set_lang("zz");
  CHECK(loc.lang() == "hu");   // ismeretlen nyelv: a forrásnyelv
  // a menü nyelvlistája a talált nyelvekből: a léptetés mindhármon végigmegy, és a feliratok azonnal váltanak
  client::Settings st;
  Menu m(st, loc, test_maps(), client::scan_factions(TOTU_CONTENT_DIR));
  m.go(MenuScreen::Settings);
  m.layout(1280, 720);
  CHECK(m.find(MenuAct::Cycle, 0)->label == "Magyar");
  CHECK(m.find(MenuAct::Back)->label == "Vissza");
  press(m, MenuAct::Cycle, 0);
  CHECK(st.language == "xx");
  CHECK(m.find(MenuAct::Cycle, 0)->label == "Próbanyelv");
  CHECK(m.find(MenuAct::Back)->label == "Back");   // angol tartalék
  CHECK((m.take_changes() & Menu::kLanguage) != 0);
  press(m, MenuAct::Cycle, 0);   // a sor kód szerint: en, hu, xx → vissza az elejére
  CHECK(st.language == "en");
  CHECK(m.find(MenuAct::Cycle, 0)->label == "English");
  CHECK(m.texts()[0].text == "SETTINGS");
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

TEST_CASE("M8 felulet – nyelvek: minden menüfelirat megvan magyarul és angolul (nincs kulcs a képernyőn)") {
  client::Locale loc = real_locale();
  for (const char* lang : {"hu", "en"}) {
    client::Settings st;
    st.language = lang;
    Menu m(st, loc, test_maps(), client::scan_factions(TOTU_CONTENT_DIR));
    for (MenuScreen s : {MenuScreen::Main, MenuScreen::Skirmish, MenuScreen::Settings, MenuScreen::Sound, MenuScreen::Keys}) {
      m.go(s);
      if (s == MenuScreen::Main) m.click(0, 0);
      m.layout(1280, 720);
      if (s == MenuScreen::Main) {   // mindkét almenü feliratai is
        for (int g : {0, 3}) {
          const auto* t = m.find(MenuAct::Toggle, g);
          m.click(t->r.cx(), t->r.cy());
          m.layout(1280, 720);
          for (const auto& w : m.widgets()) {
            CHECK(w.label.rfind("ui.", 0) != 0);
            CHECK(w.hint.rfind("ui.", 0) != 0);
          }
        }
      }
      for (const auto& w : m.widgets()) {
        CHECK(w.label.rfind("ui.", 0) != 0);
        CHECK(w.hint.rfind("ui.", 0) != 0);
        if (w.act != MenuAct::SlotFaction) {
          for (const auto& o : w.options) CHECK(o.rfind("ui.", 0) != 0);
        }
      }
      for (const auto& t : m.texts()) {
        if (t.text.rfind("ui.", 0) == 0) std::printf("  hiányzó szöveg (%s): %s\n", lang, t.text.c_str());
        CHECK(t.text.rfind("ui.", 0) != 0);
      }
    }
  }
}

TEST_CASE("M8 felulet – csapatszínek: a színvak-barát paletta 8 jól elkülönülő színe") {
  const auto& n = client::team_colors(false);
  const auto& c = client::team_colors(true);
  CHECK(n[0][0] == 220 && n[1][2] == 230);   // a megszokott: piros, kék …
  CHECK(c != n);
  for (size_t i = 0; i < 8; ++i) {
    for (size_t j = i + 1; j < 8; ++j) {
      const int d = std::abs(c[i][0] - c[j][0]) + std::abs(c[i][1] - c[j][1]) + std::abs(c[i][2] - c[j][2]);
      CHECK(d >= 60);
    }
  }
}
