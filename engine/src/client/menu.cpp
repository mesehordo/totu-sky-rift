// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/menu.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "content/json.hpp"
#include "core/platform_info.hpp"

namespace totu::client {

namespace {

std::string read_file(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

uint64_t splitmix(uint64_t& s) {
  uint64_t z = (s += 0x9e3779b97f4a7c15ull);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  return z ^ (z >> 31);
}

// --- a főmenü (a régi MAIN_ITEMS; ami az új motorban még nincs, az „hamarosan”) ---------------------------------
// a feliratok kulcsok a .po fájlokban; a súgó kulcsa a felirat kulcsa + ".hint"
struct MainItem {
  const char* key;
  MenuAct act;
  bool soon;
};
struct MainGroup {
  const char* key;
  MenuAct act;
  bool soon;
  std::vector<MainItem> subs;
};
const std::vector<MainGroup>& main_items() {
  static const std::vector<MainGroup> m = {
      {"ui.main.single", MenuAct::Toggle, false,
       {{"ui.main.campaign", MenuAct::None, true},
        {"ui.main.novellas", MenuAct::None, true},
        {"ui.main.skirmish", MenuAct::Skirmish, false},
        {"ui.main.load", MenuAct::OpenLoad, false}}},
      {"ui.main.multiplayer", MenuAct::None, true, {}},
      {"ui.main.stories", MenuAct::None, true, {}},
      {"ui.main.settings", MenuAct::Toggle, false,
       {{"ui.main.game_settings", MenuAct::Settings, false},
        {"ui.main.profiles", MenuAct::None, true}}},
      {"ui.main.mods", MenuAct::OpenMods, false, {}},
      {"ui.main.credits", MenuAct::OpenCredits, false, {}},
      {"ui.main.quit", MenuAct::Quit, false, {}},
  };
  return m;
}

const char* const kSettingLabels[Menu::kSettingRows] = {
    "ui.settings.language",     "ui.settings.resolution",  "ui.settings.fullscreen", "ui.settings.ui_scale",
    "ui.settings.colorblind",   "ui.settings.scroll_speed", "ui.settings.edge_scroll", "ui.settings.show_fps",
    "ui.settings.fps_limit",    "ui.settings.sound",        "ui.settings.keys",
    "ui.settings.show_clock",   "ui.settings.help_hint",    "ui.settings.shadows",    "ui.settings.ambient",
    "ui.settings.status_fx",    "ui.settings.game_speed"};
// a sorok megjelenítési sorrendje (a sorszám a gomb azonosítója – a régi sorszámok maradnak)
const int kSettingOrder[Menu::kSettingRows] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 11, 12, 16, 13, 14, 15, 9, 10};
const int kSpeeds[] = {50, 75, 100, 150, 200};

const int kScroll[] = {500, 700, 900, 1200, 1600};
const int kFps[] = {30, 60, 120, 0};

template <typename T, size_t N>
int index_of(const T (&arr)[N], T v) {
  for (size_t i = 0; i < N; ++i) {
    if (arr[i] == v) return static_cast<int>(i);
  }
  return -1;
}

int wrap_index(int i, int dir, int n) { return ((i < 0 ? 0 : i + dir) % n + n) % n; }

// a pálya neve / leírása a .po-ból (`map.<fájlnév>.name|desc`); ha nincs, a fájlnév / a `_leiras`
std::string map_text(const Locale& loc, const MapInfo& m, const char* field, const std::string& fallback) {
  const std::string key = "map." + m.name + "." + field;
  std::string s = loc.tr(key);
  return s == key ? fallback : s;
}

}  // namespace

// --- tartalom -------------------------------------------------------------------------------------------------------
std::vector<MapInfo> scan_maps(const std::string& dir, const map::TerrainTable* terrain) {
  std::vector<MapInfo> out;
  std::error_code ec;
  for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
    if (e.path().extension() != ".json") continue;
    const auto doc = content::parse(read_file(e.path()));
    if (!doc || !doc->is_object()) continue;
    const content::Value* ob = doc->find("objects");
    const content::Value* st = ob ? ob->find("starts") : nullptr;
    if (!st || !st->is_array() || st->items().empty()) continue;
    if (terrain) {
      std::vector<std::string> errs;
      if (!map::Map::from_json(*doc, *terrain, errs)) continue;
    }
    MapInfo m;
    m.path = e.path().string();
    m.name = e.path().stem().string();
    m.players = static_cast<int>(std::min<size_t>(st->items().size(), 8));
    if (const content::Value* v = doc->find("width")) m.width = static_cast<int>(v->as_int().value_or(0));
    if (const content::Value* v = doc->find("height")) m.height = static_cast<int>(v->as_int().value_or(0));
    if (const content::Value* v = doc->find("_leiras"); v && v->is_string()) m.desc = v->text();
    if (const content::Value* v = doc->find("order")) m.order = static_cast<int>(v->as_int().value_or(m.order));
    out.push_back(std::move(m));
  }
  std::sort(out.begin(), out.end(), [](const MapInfo& a, const MapInfo& b) {   // a régi sorrend, azon belül név
    return a.order != b.order ? a.order < b.order : a.name < b.name;
  });
  return out;
}

std::vector<FactionInfo> scan_factions(const std::string& content_dir) {
  std::vector<FactionInfo> out;
  std::error_code ec;
  const auto dir = std::filesystem::path(content_dir) / "base" / "templates" / "factions";
  for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
    if (e.path().extension() != ".json") continue;
    const auto doc = content::parse(read_file(e.path()));
    if (!doc || !doc->is_object()) continue;
    for (const auto& [key, val] : doc->members()) {
      if (key.rfind("faction_", 0) != 0 || !val.is_object()) continue;
      bool minor = false;
      std::string name_key = "faction." + key.substr(8) + ".name";
      for (const auto& [comp, cv] : val.members()) {
        if (!cv.is_object()) continue;
        for (const char* flag : {"ally", "neutral"}) {
          if (const content::Value* f = cv.find(flag); f && f->is_bool() && f->as_bool()) minor = true;
        }
        if (comp == "Identity") {
          if (const content::Value* n = cv.find("name"); n && n->is_string()) name_key = n->text();
        }
      }
      if (!minor) out.push_back({key.substr(8), name_key});
    }
  }
  std::sort(out.begin(), out.end(), [](const FactionInfo& a, const FactionInfo& b) { return a.id < b.id; });
  return out;
}

// --- egyéni csata ---------------------------------------------------------------------------------------------------
const std::vector<std::string>& ai_difficulty_ids() {
  static const std::vector<std::string> d = {"kezdo", "konnyu", "normal", "nehez", "mester", "orult"};
  return d;
}

const std::vector<std::string>& ai_personality_ids() {
  static const std::vector<std::string> p = {"vedekezo",     "tamado",   "kiegyenlitett", "gyors_roham",
                                             "terjeszkedo",  "technologus", "portyazo",   "ostromlo",
                                             "csapatjatekos", "alkalmazkodo", "nepe_szerinti"};
  return p;
}

SkirmishSetup::SkirmishSetup() {
  slots[0].kind = SlotSetup::Kind::Human;
  slots[1].kind = SlotSetup::Kind::Ai;
}

bool make_match_config(const SkirmishSetup& s, const std::vector<MapInfo>& maps, const std::vector<FactionInfo>& factions,
                       uint64_t seed, MatchConfig* out, std::string* error_hu) {
  auto fail = [&](const char* m) {
    if (error_hu) *error_hu = m;
    return false;
  };
  if (maps.empty() || s.map >= maps.size()) return fail("ui.skirmish.no_map");
  if (factions.empty()) return fail("ui.skirmish.no_faction");
  const MapInfo& m = maps[s.map];
  MatchConfig c;
  c.map_path = m.path;
  uint64_t rng = seed;
  int participants = 0;
  const size_t n = static_cast<size_t>(std::clamp(m.players, 0, 8));
  for (size_t i = 0; i < n; ++i) {
    const SlotSetup& sl = s.slots[i];
    const SlotSetup::Kind kind = i == 0 ? SlotSetup::Kind::Human : sl.kind;
    c.colors.push_back(static_cast<uint8_t>(sl.color >= 0 ? sl.color : static_cast<int8_t>(i)));
    if (kind == SlotSetup::Kind::Closed) {
      c.setup.factions.emplace_back();
      c.setup.teams.push_back(0);
      c.setup.starts.push_back(-1);
      continue;
    }
    c.setup.teams.push_back(sl.team);
    c.setup.starts.push_back(sl.start >= 0 && static_cast<size_t>(sl.start) < n ? sl.start : static_cast<int8_t>(-1));
    std::string f = sl.faction;
    const bool known = std::any_of(factions.begin(), factions.end(), [&](const FactionInfo& fi) { return fi.id == f; });
    if (!known) f = factions[static_cast<size_t>(splitmix(rng) % factions.size())].id;   // véletlen nép
    c.setup.factions.push_back(f);
    ++participants;
    if (kind == SlotSetup::Kind::Ai) {
      c.ai.push_back(static_cast<int>(i) + 1);
      sim::AiSlot a;   // M9: az alap gép a hely nehézségével és személyiségével
      a.player = static_cast<uint8_t>(i + 1);
      a.difficulty = sl.difficulty;
      a.personality = sl.personality;
      c.setup.ai.push_back(std::move(a));
    }
  }
  if (participants < 2) return fail("ui.skirmish.need_two");
  {   // a régi: csapatjátékhoz legalább két különböző oldal kell (a „–” mindenki ellen játszik)
    std::vector<int> sides;
    for (size_t i = 0; i < c.setup.teams.size(); ++i) {
      if (c.setup.factions[i].empty()) continue;
      sides.push_back(c.setup.teams[i] == 0 ? 100 + static_cast<int>(i) : c.setup.teams[i]);
    }
    std::sort(sides.begin(), sides.end());
    if (std::unique(sides.begin(), sides.end()) - sides.begin() < 2) return fail("ui.skirmish.need_two_teams");
  }
  if (std::all_of(c.setup.teams.begin(), c.setup.teams.end(), [](uint8_t t) { return t == 0; })) c.setup.teams.clear();
  {   // a régi `_assign_starts`: a megadott kezdőhely; a többi a szabadokból – kevesebb résztvevőnél egyenletesen
      // szétosztva, teli pályán keverve (a magból, determinisztikusan)
    std::vector<int8_t> free;
    for (size_t k = 0; k < n; ++k) free.push_back(static_cast<int8_t>(k));
    bool dup = false;
    for (size_t k = 0; k < c.setup.starts.size(); ++k) {
      const int8_t st = c.setup.starts[k];
      if (st < 0 || c.setup.factions[k].empty()) continue;
      const auto it = std::find(free.begin(), free.end(), st);
      if (it == free.end()) dup = true;
      else free.erase(it);
    }
    if (dup) return fail("ui.skirmish.start_taken");
    const size_t players = static_cast<size_t>(participants);
    if (n == players) {
      for (size_t k = free.size(); k > 1; --k) std::swap(free[k - 1], free[static_cast<size_t>(splitmix(rng) % k)]);
    }
    for (size_t k = 0, p = 0; k < c.setup.starts.size(); ++k) {
      if (c.setup.factions[k].empty() || c.setup.starts[k] >= 0) continue;
      int8_t pick = free.empty() ? static_cast<int8_t>(k) : free.front();
      if (n > players) {   // egyenletesen a körön
        const int8_t want = static_cast<int8_t>((p * n + players / 2) / players % n);
        if (std::find(free.begin(), free.end(), want) != free.end()) pick = want;
      }
      ++p;
      c.setup.starts[k] = pick;
      free.erase(std::remove(free.begin(), free.end(), pick), free.end());
    }
  }
  c.setup.resources = s.resources;
  c.setup.fog = s.fog;
  c.setup.daytime = s.daytime;
  c.setup.weather = s.weather;
  c.setup.camps = s.camps;
  c.setup.upkeep = s.upkeep;
  c.setup.victory = s.victory;
  c.setup.victory_script = s.victory_script;
  if (s.victory == sim::MatchSetup::Victory::Script && s.victory_script.empty()) c.setup.victory = sim::MatchSetup::Victory::Conquest;
  c.setup.tempo = s.tempo;
  if (out) *out = std::move(c);
  return true;
}

// --- a menü ---------------------------------------------------------------------------------------------------------
Menu::Menu(Settings& settings, Locale& locale, std::vector<MapInfo> maps, std::vector<FactionInfo> factions)
    : st_(settings), loc_(locale), maps_(std::move(maps)), factions_(std::move(factions)) {
  loc_.set_lang(st_.language);
  apply_map();
}

float Menu::base_scale(int w, int h) {
  return std::max(0.8f, std::min({3.0f, static_cast<float>(w) / 1280.0f, static_cast<float>(h) / 720.0f}));
}

float Menu::k(float v) const { return std::round(v * u_); }

float Menu::anim() const {
  const double a = std::clamp((now_ - anim_t_) / 0.14, 0.0, 1.0);
  return static_cast<float>(1.0 - (1.0 - a) * (1.0 - a) * (1.0 - a));
}

void Menu::go(MenuScreen s) {
  if (s != screen_) anim_t_ = now_;
  if (s == MenuScreen::Mods && screen_ != MenuScreen::Mods) {   // a modkezelő a mentett készlettel indul
    mods_on_ = st_.mods;
    mod_sel_.clear();
    message_.clear();
    mods_check();
  }
  screen_ = s;
  open_ = -1;
  open_dd_ = MenuAct::None;
  dragging_ = -1;
  capture_ = -1;
  message_.clear();
  if (s == MenuScreen::Skirmish) apply_map();
}

void Menu::apply_map() {
  if (maps_.empty()) return;
  sk_.map = std::min(sk_.map, maps_.size() - 1);
  const int n = maps_[sk_.map].players;
  for (int i = 0; i < 8; ++i) {
    SlotSetup& sl = sk_.slots[static_cast<size_t>(i)];
    if (i == 0) sl.kind = SlotSetup::Kind::Human;
    else if (i >= n) sl.kind = SlotSetup::Kind::Closed;
    else if (sl.kind == SlotSetup::Kind::Human) sl.kind = SlotSetup::Kind::Ai;
  }
  // legalább egy ellenfél: ha minden más hely zárt, a 2. hely gép lesz
  bool any = false;
  for (int i = 1; i < n; ++i) any = any || sk_.slots[static_cast<size_t>(i)].kind != SlotSetup::Kind::Closed;
  if (!any && n > 1) sk_.slots[1].kind = SlotSetup::Kind::Ai;
}

MenuWidget& Menu::add(MenuWidget::Kind kind, MenuRect r, std::string label, MenuAct act, int arg) {
  MenuWidget w;
  w.kind = kind;
  w.r = r;
  w.label = std::move(label);
  w.act = act;
  w.arg = arg;
  widgets_.push_back(std::move(w));
  return widgets_.back();
}

void Menu::text(float x, float y, float w, std::string s, MenuText::Font f, MenuText::Color c, bool center, int lines) {
  MenuText t;
  t.x = x;
  t.y = y;
  t.w = w;
  t.text = std::move(s);
  t.font = f;
  t.color = c;
  t.center = center;
  t.max_lines = lines;
  texts_.push_back(std::move(t));
}

void Menu::title(const std::string& s) {
  MenuText t;
  t.x = area_w_ > 0 ? area_x_ + area_w_ / 2 : static_cast<float>(w_) / 2;
  t.y = k(14);
  t.w = (area_w_ > 0 ? area_w_ : static_cast<float>(w_)) - k(80);
  t.text = s;
  t.font = MenuText::Huge;
  t.color = MenuText::Gold;
  t.center = true;
  t.title = true;
  texts_.push_back(std::move(t));
}

void Menu::back_button() {
  add(MenuWidget::Kind::Button, {(area_w_ > 0 ? area_x_ : 0) + k(36), static_cast<float>(h_) - k(76), k(190), k(50)},
      tr("ui.back"), MenuAct::Back);
}

void Menu::layout(int w, int h) {
  w_ = std::max(1, w);
  h_ = std::max(1, h);
  // A felület-méret (akadálymentesség) a régi méretarányt szorozza; a menü legfeljebb 1024×576-os alapra nőhet,
  // hogy kisebb képernyőn is elférjen (a játék közbeni felület ettől függetlenül nőhet).
  const float base = base_scale(w_, h_);
  const float fit = std::max(base, std::min(static_cast<float>(w_) / 1024.0f, static_cast<float>(h_) / 576.0f));
  u_ = std::clamp(base * static_cast<float>(st_.ui_scale) / 100.0f, 0.8f, fit);
  widgets_.clear();
  texts_.clear();
  panels_.clear();
  sidebar_w_ = 0;
  area_x_ = area_w_ = 0;
  logo_ = {};
  switch (screen_) {
    case MenuScreen::Main: layout_main(); break;
    case MenuScreen::Skirmish: layout_skirmish(); break;
    case MenuScreen::Settings: layout_settings(); break;
    case MenuScreen::Sound: layout_sound(); break;
    case MenuScreen::Keys: layout_keys(); break;
    case MenuScreen::Mods: layout_mods(); break;
    case MenuScreen::Load: layout_load(); break;
    case MenuScreen::Credits: layout_credits(); break;
  }
}

void Menu::layout_main() {
  layout_sidebar(open_);
}

void Menu::side_area(const char* highlight) {
  // az oldalsáv a főmenü „Beállítások” pontjával kiemelve; mellette a képernyő, balról becsúszva
  int sel = -1;
  const auto& items = main_items();
  for (size_t i = 0; i < items.size(); ++i) {
    if (std::string(items[i].key) == highlight) sel = static_cast<int>(i);
  }
  layout_sidebar(sel);
  const float off = (1.0f - anim()) * k(60);
  area_x_ = sidebar_w_ + k(8) - off;
  area_w_ = std::max(k(200), static_cast<float>(w_) - sidebar_w_ - k(16));
}

void Menu::layout_sidebar(int highlight) {
  const float H = static_cast<float>(h_);
  const float sw = k(320);
  sidebar_w_ = sw;
  float logo_h = k(90);
  if (logo_aspect_ > 0) {
    logo_h = std::min(H * 0.27f, (sw - k(36)) / logo_aspect_);
    logo_ = {sw / 2 - logo_h * logo_aspect_ / 2, k(14), logo_h * logo_aspect_, logo_h};
  } else {
    text(sw / 2, k(14) + k(30), sw - k(30), "TOTU: Sky Rift", MenuText::Big, MenuText::Gold, true);
  }
  float y = k(14) + logo_h + k(48);
  const float ih = k(46), gap = k(6);
  const auto& items = main_items();
  std::vector<float> item_y;
  for (size_t i = 0; i < items.size(); ++i) {
    const MainGroup& g = items[i];
    MenuWidget& mw = add(MenuWidget::Kind::Item, {k(14), y, sw - k(28), ih}, tr(g.key), g.act, static_cast<int>(i));
    mw.hint = loc_.tr(std::string(g.key) + ".hint");
    mw.sub = !g.subs.empty();
    mw.soon = g.soon;
    mw.enabled = !g.soon;
    mw.selected = highlight == static_cast<int>(i);
    if (g.soon) mw.tag = tr("ui.main.soon");
    item_y.push_back(y);
    y += ih + gap;
  }
  // súgó és lábléc az oldalsáv alján (a régi `MainMenu.draw`)
  const float foot = H - k(70);
  if (!status_.empty()) text(sw / 2, foot - k(20), sw - k(30), status_, MenuText::Tiny, MenuText::Gold, true);
  text(sw / 2, foot + k(10), sw - k(30), tr("ui.main.creator"), MenuText::Tiny, MenuText::Dim, true);
  text(sw / 2, foot + k(34), sw - k(30), std::string("v") + core::version(), MenuText::Tiny, MenuText::Dim, true);
  if (screen_ == MenuScreen::Main && open_ >= 0 && static_cast<size_t>(open_) < items.size()) {
    const auto& subs = items[static_cast<size_t>(open_)].subs;
    const float pw = k(300);
    const float ph = static_cast<float>(subs.size()) * (ih + gap) + k(18);
    const float px = sw + k(4) - (1.0f - anim()) * k(40);   // a régi: kicsúszik az oldalsáv mögül
    const float py = std::min(item_y[static_cast<size_t>(open_)] - k(9), H - ph - k(20));
    panels_.push_back({{px, py, pw, ph}, 228, true});
    for (size_t j = 0; j < subs.size(); ++j) {
      const MainItem& it = subs[j];
      MenuWidget& mw = add(MenuWidget::Kind::Item, {px + k(12), py + k(9) + static_cast<float>(j) * (ih + gap), pw - k(24), ih},
                           tr(it.key), it.act, static_cast<int>(j));
      mw.hint = loc_.tr(std::string(it.key) + ".hint");
      mw.soon = it.soon;
      mw.enabled = !it.soon;
      if (it.soon) mw.tag = tr("ui.main.soon");
    }
  }
}

void Menu::layout_skirmish() {
  const float W = static_cast<float>(w_), H = static_cast<float>(h_);
  title(tr("ui.skirmish.title"));
  const float top = k(96);
  const float lx = k(24), lw = k(270);
  const float x0 = lx + lw + k(24);
  // ---- pályalista
  const MenuRect lp{lx, top, lw, H - top - k(100)};
  panels_.push_back({lp, 200, false});
  text(lp.x + k(10), lp.y + k(6), lw - k(20), tr("ui.skirmish.map"), MenuText::Med, MenuText::Gold);
  const int nmaps = static_cast<int>(std::min<size_t>(maps_.size(), 10));
  // 10-nél több pályánál görgetett ablak: a kiválasztott pálya körül (a szélső sorra kattintva tovább gördül)
  const int first = std::clamp(static_cast<int>(sk_.map) - nmaps / 2, 0, static_cast<int>(maps_.size()) - nmaps);
  for (int i = 0; i < nmaps; ++i) {
    const int idx = first + i;
    const MapInfo& m = maps_[static_cast<size_t>(idx)];
    MenuWidget& mw = add(MenuWidget::Kind::MapRow, {lx + k(6), top + k(32) + static_cast<float>(i) * k(26), lw - k(12), k(24)},
                         "[" + std::to_string(m.players) + "]  " + map_text(loc_, m, "name", m.name), MenuAct::PickMap, idx);
    mw.selected = static_cast<size_t>(idx) == sk_.map;
  }
  float ty = top + k(32) + static_cast<float>(nmaps) * k(26) + k(10);
  if (maps_.empty()) {
    text(lp.x + k(10), ty, lw - k(20), tr("ui.skirmish.no_map"), MenuText::Small, MenuText::Warn);
  } else {
    const MapInfo& md = maps_[sk_.map];
    text(lp.x + k(10), ty, lw - k(20), map_text(loc_, md, "name", md.name), MenuText::Med, MenuText::Gold);
    ty += k(24);
    text(lp.x + k(10), ty, lw - k(20),
         loc_.tr("ui.skirmish.map_info", {{"n", std::to_string(md.players)}, {"w", std::to_string(md.width)},
                                           {"h", std::to_string(md.height)}}),
         MenuText::Tiny, MenuText::Dim);
    ty += k(22);
    const int lines = std::max(1, static_cast<int>((lp.y + lp.h - ty - k(6)) / k(18)));
    text(lp.x + k(10), ty, lw - k(20), map_text(loc_, md, "desc", md.desc), MenuText::Tiny, MenuText::Text, false, lines);
  }
  // ---- játékoshelyek: #, résztvevő, nép, gépi nehézség és személyiség (M9), csapat, kezdőhely, szín
  const float weights[] = {24, 120, 120, 100, 124, 52, 72, 80};
  constexpr int kCols = 8;
  const float gap = k(4);
  float sum = 0;
  for (float wt : weights) sum += wt;
  const float sc = std::min((W - x0 - k(30) - gap * (kCols - 1)) / sum, 1.6f * u_);
  float cx[kCols], cw[kCols];
  float x = x0;
  for (int i = 0; i < kCols; ++i) {
    cx[i] = x;
    cw[i] = std::floor(weights[i] * sc);
    x += cw[i] + gap;
  }
  const float row_y0 = top + k(36), row_h = k(34), dh = k(27);
  const MenuRect slots{x0 - k(10), top, W - x0 - k(14), row_y0 + 8 * row_h + k(8) - top};
  panels_.push_back({slots, 200, false});
  const char* heads[] = {"ui.skirmish.col.num",        "ui.skirmish.col.participant", "ui.skirmish.col.faction",
                         "ui.skirmish.col.difficulty", "ui.skirmish.col.personality", "ui.skirmish.col.team",
                         "ui.skirmish.col.start",      "ui.skirmish.col.color"};
  for (int i = 0; i < kCols; ++i) text(cx[i], slots.y + k(10), cw[i], tr(heads[i]), MenuText::Small, MenuText::Gold);
  std::vector<std::string> diffs, perss;
  for (const auto& d : ai_difficulty_ids()) diffs.push_back(tr(("ui.ai.diff." + d).c_str()));
  for (const auto& p : ai_personality_ids()) perss.push_back(tr(("ui.ai.pers." + p).c_str()));
  const int n = maps_.empty() ? 0 : maps_[sk_.map].players;
  std::vector<std::string> facs = {tr("ui.skirmish.random")};
  for (const FactionInfo& f : factions_) facs.push_back(loc_.name(f.name_key));
  for (int i = 0; i < 8; ++i) {
    const float y = row_y0 + static_cast<float>(i) * row_h;
    const bool active = i < n;
    const SlotSetup& sl = sk_.slots[static_cast<size_t>(i)];
    text(cx[0] + cw[0] / 2, y + (dh - k(22)) / 2, cw[0], std::to_string(i + 1), MenuText::Med,
         active ? MenuText::Text : MenuText::Dim, true);
    MenuWidget& kind = add(MenuWidget::Kind::Dropdown, {cx[1], y, cw[1], dh}, "", MenuAct::SlotKind, i);
    if (i == 0) {
      kind.options = {tr("ui.skirmish.player")};
      kind.enabled = false;
    } else {
      kind.options = {tr("ui.skirmish.computer"), tr("ui.skirmish.closed")};
      kind.current = sl.kind == SlotSetup::Kind::Ai ? 0 : 1;
      kind.enabled = active;
    }
    if (!active && i > 0) kind.current = 1;
    MenuWidget& fac = add(MenuWidget::Kind::Dropdown, {cx[2], y, cw[2], dh}, "", MenuAct::SlotFaction, i);
    fac.options = facs;
    fac.current = 0;
    for (size_t j = 0; j < factions_.size(); ++j) {
      if (factions_[j].id == sl.faction) fac.current = static_cast<int>(j) + 1;
    }
    fac.enabled = active && (i == 0 || sl.kind != SlotSetup::Kind::Closed);
    const bool ai = active && i > 0 && sl.kind == SlotSetup::Kind::Ai;
    MenuWidget& dif = add(MenuWidget::Kind::Dropdown, {cx[3], y, cw[3], dh}, "", MenuAct::SlotDifficulty, i);
    dif.options = ai ? diffs : std::vector<std::string>{"–"};
    dif.current = 0;
    for (size_t j = 0; ai && j < ai_difficulty_ids().size(); ++j) {
      if (ai_difficulty_ids()[j] == sl.difficulty) dif.current = static_cast<int>(j);
    }
    dif.enabled = ai;
    MenuWidget& per = add(MenuWidget::Kind::Dropdown, {cx[4], y, cw[4], dh}, "", MenuAct::SlotPersonality, i);
    per.options = ai ? perss : std::vector<std::string>{"–"};
    per.current = 0;
    for (size_t j = 0; ai && j < ai_personality_ids().size(); ++j) {
      if (ai_personality_ids()[j] == sl.personality) per.current = static_cast<int>(j);
    }
    per.enabled = ai;
    MenuWidget& team = add(MenuWidget::Kind::Dropdown, {cx[5], y, cw[5], dh}, "", MenuAct::SlotTeam, i);
    team.options = {"–", "1", "2", "3", "4"};
    team.current = std::min<int>(sl.team, 4);
    team.enabled = active && (i == 0 || sl.kind != SlotSetup::Kind::Closed);
    if (!team.enabled) team.current = 0;
    MenuWidget& stw = add(MenuWidget::Kind::Dropdown, {cx[6], y, cw[6], dh}, "", MenuAct::SlotStart, i);
    stw.options = {tr("ui.skirmish.random")};
    for (int k = 0; k < n; ++k) stw.options.push_back(std::to_string(k + 1));
    stw.current = sl.start >= 0 && sl.start < n ? sl.start + 1 : 0;
    stw.enabled = team.enabled;
    const int ci = sl.color >= 0 ? sl.color : i;
    MenuWidget& col = add(MenuWidget::Kind::Dropdown, {cx[7], y, cw[7], dh}, "", MenuAct::SlotColor, i);
    for (int k = 0; k < 8; ++k) col.options.push_back(loc_.tr((st_.colorblind ? "ui.color_safe." : "ui.color.") + std::to_string(k)));
    col.current = ci;
    col.enabled = team.enabled;
    col.team = ci;
  }
  // ---- játékbeállítások (saját panelen)
  const float oy = slots.y + slots.h + k(16);
  const float ow = std::min(k(300), std::floor((W - x0 - k(24)) / 4));   // 4 oszlop (M12: 7 beállítás, 2 sor)
  struct Opt {
    const char* label;
    std::vector<const char*> options;
    int current;
  };
  const Opt opts[] = {
      {"ui.opt.fog", {"ui.opt.fog.normal", "ui.opt.fog.explored", "ui.opt.fog.none"}, static_cast<int>(sk_.fog)},
      {"ui.opt.daytime", {"ui.opt.mode.off", "ui.opt.mode.mood", "ui.opt.mode.effect"}, static_cast<int>(sk_.daytime)},
      {"ui.opt.weather", {"ui.opt.mode.off", "ui.opt.mode.mood", "ui.opt.mode.effect", "ui.opt.mode.effect_terrain"}, static_cast<int>(sk_.weather)},
      {"ui.opt.camps", {"ui.on", "ui.off"}, sk_.camps ? 0 : 1},
      {"ui.opt.upkeep", {"ui.on", "ui.off"}, sk_.upkeep ? 0 : 1},
      {"ui.opt.victory", {"ui.opt.victory.conquest", "ui.opt.victory.capital", "ui.opt.victory.hill", "ui.opt.victory.timed"},
       static_cast<int>(sk_.victory)},
      {"ui.opt.tempo", {"ui.opt.tempo.classic", "ui.opt.tempo.calm", "ui.opt.tempo.rush", "ui.opt.tempo.survival"},
       static_cast<int>(sk_.tempo)},
      {"ui.opt.resources", {"ui.opt.resources.low", "ui.opt.resources.medium", "ui.opt.resources.high"}, static_cast<int>(sk_.resources)},
  };
  constexpr int kOpts = 8;
  panels_.push_back({{x0 - k(10), oy, 4 * ow + k(4), static_cast<float>((kOpts + 3) / 4) * k(62) + k(14)}, 200, false});
  for (int i = 0; i < kOpts; ++i) {
    const float ox = x0 + static_cast<float>(i % 4) * ow;
    const float oy2 = oy + k(10) + static_cast<float>(i / 4) * k(62);
    text(ox, oy2, ow - k(20), tr(opts[i].label), MenuText::Small, MenuText::Gold);
    MenuWidget& d = add(MenuWidget::Kind::Dropdown, {ox, oy2 + k(24), ow - k(20), dh}, "", MenuAct::Option, i);
    for (const char* o : opts[i].options) d.options.push_back(tr(o));
    d.current = opts[i].current;
    if (i == 5) {   // M13: a modok győzelmi szkriptjei a beépítettek után
      for (size_t v = 0; v < victories_.size(); ++v) {
        d.options.push_back(victories_[v].label(st_.language));
        if (sk_.victory == sim::MatchSetup::Victory::Script && victories_[v].id == sk_.victory_script) d.current = 4 + static_cast<int>(v);
      }
      if (sk_.victory == sim::MatchSetup::Victory::Script && d.current >= static_cast<int>(d.options.size())) d.current = 0;
    }
  }
  add(MenuWidget::Kind::Button, {W - k(36 + 240), H - k(76), k(240), k(50)}, tr("ui.skirmish.start"), MenuAct::Start).enabled =
      !maps_.empty();
  back_button();
  if (!message_.empty()) text(W / 2, H - k(124), W - k(80), message_, MenuText::Med, MenuText::Warn, true);
}

void Menu::layout_settings() {
  const float H = static_cast<float>(h_);
  side_area();
  title(tr("ui.settings.title"));
  const float top = k(96);
  const float pw = std::min(k(720), area_w_ - k(24));
  const float W = 2 * area_x_ + area_w_;   // a panel a terület közepén
  const float step = std::max(k(20), std::min(k(54), std::floor((H - top - k(150)) / kSettingRows)));
  const float bh = std::max(1.0f, step - k(2));
  const MenuRect pr{W / 2 - pw / 2, top, pw, kSettingRows * step + k(60)};
  panels_.push_back({pr, 215, false});
  const float bw = k(270);
  auto onoff = [&](bool v) { return tr(v ? "ui.on" : "ui.off"); };
  for (int row = 0; row < kSettingRows; ++row) {
    const int i = kSettingOrder[row];
    std::string val;
    MenuAct act = MenuAct::Cycle;
    switch (i) {
      case 0: val = loc_.language_name(st_.language); break;
      case 1: val = std::to_string(st_.width) + "×" + std::to_string(st_.height); break;
      case 2: val = onoff(st_.fullscreen); break;
      case 3: val = std::to_string(st_.ui_scale) + "%"; break;
      case 4: val = onoff(st_.colorblind); break;
      case 5: val = std::to_string(st_.scroll_speed); break;
      case 6: val = onoff(st_.edge_scroll); break;
      case 7: val = onoff(st_.show_fps); break;
      case 8: val = st_.fps_limit == 0 ? tr("ui.settings.none") : std::to_string(st_.fps_limit); break;
      case 9: val = tr("ui.settings.sound_button"); act = MenuAct::OpenSound; break;
      case 11: val = onoff(st_.show_clock); break;
      case 12: val = onoff(st_.help_hint); break;
      case 13: val = onoff(st_.shadows); break;
      case 14: val = onoff(st_.ambient); break;
      case 15: val = onoff(st_.status_fx); break;
      case 16: {
        char b[16];
        std::snprintf(b, sizeof b, "×%g", static_cast<double>(st_.game_speed) / 100.0);
        val = b;
        break;
      }
      default: val = tr("ui.settings.keys_button"); act = MenuAct::OpenKeys; break;
    }
    const float y = pr.y + k(16) + static_cast<float>(row) * step;
    MenuWidget& b = add(MenuWidget::Kind::Button, {pr.x + pr.w - k(24) - bw, y, bw, bh}, val, act, i);
    b.small = true;
    text(pr.x + k(24), y + (bh - k(22)) / 2, pr.w - k(24 + 270 + 44), tr(kSettingLabels[i]), MenuText::Med, MenuText::Text);
  }
  text(pr.x + k(24), pr.y + pr.h - k(34), pr.w - k(48), tr("ui.settings.footer"),
       MenuText::Tiny, MenuText::Dim);
  back_button();
}

void Menu::layout_sound() {
  const float H = static_cast<float>(h_);
  side_area();
  title(tr("ui.sound.title"));
  const float top = k(96);
  const float pw = std::min(k(770), area_w_ - k(24));
  const float W = 2 * area_x_ + area_w_;
  const float step = std::max(k(24), std::min(k(61), std::floor((H - top - k(120)) / static_cast<float>(kSoundCount))));
  const MenuRect pr{std::floor((W - pw) / 2), top, pw, step * static_cast<float>(kSoundCount) + k(25) + k(30)};
  panels_.push_back({pr, 215, false});
  for (size_t i = 0; i < kSoundCount; ++i) {
    const float y = pr.y + k(12) + static_cast<float>(i) * step;
    const MenuRect btn{pr.x + pr.w - k(105), y, k(80), step - k(8)};
    const float left = pr.x + std::floor(pw / 2);
    const Settings::Volume& v = st_.volume[i];
    MenuWidget& b = add(MenuWidget::Kind::Button, btn, tr(v.enabled ? "ui.on" : "ui.off"), MenuAct::SoundToggle, static_cast<int>(i));
    b.small = true;
    MenuWidget& s = add(MenuWidget::Kind::Slider, {left, y, btn.x - k(70) - left, btn.h}, "", MenuAct::SoundSlider,
                        static_cast<int>(i));
    s.current = v.value;
    s.selected = v.enabled;
    text(pr.x + k(24), y + (btn.h - k(22)) / 2, left - pr.x - k(42), loc_.tr(sound_label(static_cast<Sound>(i))),
         MenuText::Med, MenuText::Text);
    text(s.r.x + s.r.w + k(12), y + (btn.h - k(20)) / 2, k(60), std::to_string(v.value) + "%", MenuText::Small,
         v.enabled ? MenuText::Gold : MenuText::Dim);
  }
  text(pr.x + k(24), pr.y + pr.h - k(30), pr.w - k(48), tr("ui.sound.footer"),
       MenuText::Tiny, MenuText::Dim);
  back_button();
}

void Menu::layout_keys() {
  const float H = static_cast<float>(h_);
  side_area();
  const float W = 2 * area_x_ + area_w_;
  title(tr("ui.keys.title"));
  const float top = k(96);
  const float cell = k(84), gap = k(12);
  const float gw = 4 * cell + 3 * gap;
  const float pw = std::min(W - k(72), std::max(gw + k(60), k(620)));
  const MenuRect pr{W / 2 - pw / 2, top, pw, k(70) + 3 * cell + 2 * gap + k(90)};
  panels_.push_back({pr, 215, false});
  text(pr.x + k(24), pr.y + k(14), pr.w - k(48),
       tr("ui.keys.info"),
       MenuText::Small, MenuText::Text, false, 2);
  const float gx = W / 2 - gw / 2, gy = pr.y + k(70);
  for (int i = 0; i < Keymap::kSlots; ++i) {
    const float x = gx + static_cast<float>(i % 4) * (cell + gap), y = gy + static_cast<float>(i / 4) * (cell + gap);
    MenuWidget& c = add(MenuWidget::Kind::KeyCell, {x, y, cell, cell}, std::string(1, st_.keys.key(i)), MenuAct::KeyCell, i);
    c.selected = capture_ == i;
  }
  const float my = gy + 3 * cell + 2 * gap + k(18);
  if (!message_.empty()) {
    text(W / 2, my, pr.w - k(48), message_, MenuText::Med, capture_ >= 0 ? MenuText::Light : MenuText::Gold, true, 2);
  }
  add(MenuWidget::Kind::Button, {area_x_ + area_w_ - k(36 + 240), H - k(76), k(240), k(50)}, tr("ui.keys.default"), MenuAct::KeysDefault);
  back_button();
}

void Menu::mods_check() {
  mods_err_.clear();
  std::vector<content::ModInfo> avail = mods_avail_;
  content::ModInfo base;   // az alap mindig megvan
  base.id = "totu";
  avail.insert(avail.begin(), base);
  // a 0 A.D. szerint: a függőségnek engedélyezve kell lennie, és előbb kell betölteni (nem húzzuk be magától)
  for (size_t i = 0; i < mods_on_.size(); ++i) {
    for (const auto& m : mods_avail_) {
      if (m.id != mods_on_[i]) continue;
      for (const auto& d : m.depends) {
        if (d == "totu") continue;
        const auto it = std::find(mods_on_.begin(), mods_on_.end(), d);
        if (it == mods_on_.end()) mods_err_.push_back(loc_.tr("ui.mods.err_missing", {{"m", m.id}, {"d", d}}));
        else if (static_cast<size_t>(it - mods_on_.begin()) > i) mods_err_.push_back(loc_.tr("ui.mods.err_order", {{"m", m.id}, {"d", d}}));
      }
    }
  }
  if (mods_err_.empty()) {
    for (const auto& e : content::select_mods(avail, mods_on_).errors) mods_err_.push_back(e);   // ütközés, ismeretlen mod
  }
}

void Menu::layout_credits() {
  // a régi `CreditsMenu`: a cím arannyal, alatta a készítők és a felhasznált eszközök egy panelen, középre igazítva
  side_area("ui.main.credits");
  title(tr("ui.credits.title"));
  const float top = k(96), lh = k(34);
  const float pw = std::min(k(1000), area_w_ - k(48)), cx = area_x_ + area_w_ / 2;
  std::vector<std::string> lines;
  for (int i = 1; i <= 12; ++i) {
    const std::string key = "ui.credits." + std::to_string(i);
    const std::string s = loc_.tr(key);
    if (s == key) break;
    lines.push_back(s == "-" ? std::string() : s);
  }
  panels_.push_back({{cx - pw / 2, top + k(10), pw, static_cast<float>(lines.size()) * lh + k(40)}, 200, false});
  float y = top + k(30);
  for (size_t i = 0; i < lines.size(); ++i) {
    if (!lines[i].empty()) text(cx, y, pw - k(40), lines[i], i == 0 ? MenuText::Med : MenuText::Small, i == 0 ? MenuText::Gold : MenuText::Text, true);
    y += lh;
  }
  back_button();
}

void Menu::layout_load() {
  // a régi `LoadMenu`: a mentések listája (legújabb elöl), alatta a betöltés és a törlés
  const float H = static_cast<float>(h_);
  side_area("ui.main.single");
  title(tr("ui.load.title"));
  const float top = k(96);
  const float lw = std::min(k(720), area_w_ - k(48)), lx = area_x_ + k(24);
  const float lh = std::max(k(200), H - top - k(180));
  panels_.push_back({{lx, top, lw, lh}, 215, false});
  const float rh = k(44);
  float y = top + k(10);
  const int fit = std::max(1, static_cast<int>((lh - k(20)) / (rh + k(4))));
  for (int i = 0; i < static_cast<int>(saves_.size()) && i < fit; ++i) {
    const SaveRow& s = saves_[static_cast<size_t>(i)];
    MenuWidget& w = add(MenuWidget::Kind::MapRow, {lx + k(8), y, lw - k(16), rh}, s.title + "   " + s.detail, MenuAct::SavePick, i);
    w.selected = i == save_sel_;
    y += rh + k(4);
  }
  if (saves_.empty()) text(lx + k(16), top + k(16), lw - k(32), tr("ui.load.none"), MenuText::Med, MenuText::Dim);
  const float by = top + lh + k(16), bw = k(200), bh = k(48);
  add(MenuWidget::Kind::Button, {lx, by, bw, bh}, tr("ui.load.load"), MenuAct::SaveLoad).enabled = save_sel_ >= 0;
  add(MenuWidget::Kind::Button, {lx + bw + k(16), by, bw, bh}, tr("ui.load.delete"), MenuAct::SaveDelete).enabled = save_sel_ >= 0;
  back_button();
}

void Menu::layout_mods() {
  // a 0 A.D. modkezelője szerint (D50): balra az elérhető, jobbra az engedélyezett modok (a betöltési sorrendben),
  // alatta a kijelölt mod adatai és a hibák; a mentés újraindítja a játékot
  const float H = static_cast<float>(h_);
  side_area("ui.main.mods");
  title(tr("ui.mods.title"));
  const float top = k(96);
  const float gap = k(16);
  const float lw = std::min(k(340), (area_w_ - k(48) - k(150)) / 2);
  const float lx = area_x_ + k(24), rx = lx + lw + k(150);
  const float lh = std::max(k(180), H * 0.42f);
  panels_.push_back({{lx, top, lw, lh}, 215, false});
  panels_.push_back({{rx, top, lw, lh}, 215, false});
  text(lx + k(10), top + k(8), lw - k(20), tr("ui.mods.available"), MenuText::Med, MenuText::Gold);
  text(rx + k(10), top + k(8), lw - k(20), tr("ui.mods.enabled"), MenuText::Med, MenuText::Gold);
  const float rh = k(30);
  float y = top + k(44);
  int idx = 0;
  for (const auto& m : mods_avail_) {
    if (std::find(mods_on_.begin(), mods_on_.end(), m.id) != mods_on_.end()) continue;
    MenuWidget& w = add(MenuWidget::Kind::MapRow, {lx + k(6), y, lw - k(12), rh}, m.name + "  " + m.version, MenuAct::ModPick, idx++);
    w.selected = m.id == mod_sel_;
    y += rh + k(2);
  }
  if (idx == 0) text(lx + k(12), y, lw - k(24), tr("ui.mods.none"), MenuText::Small, MenuText::Dim);
  y = top + k(44);
  for (size_t i = 0; i < mods_on_.size(); ++i) {
    std::string label = std::to_string(i + 1) + ". " + mods_on_[i];
    for (const auto& m : mods_avail_) {
      if (m.id == mods_on_[i]) label = std::to_string(i + 1) + ". " + m.name + "  " + m.version;
    }
    MenuWidget& w = add(MenuWidget::Kind::MapRow, {rx + k(6), y, lw - k(12), rh}, label, MenuAct::ModPick, 1000 + static_cast<int>(i));
    w.selected = mods_on_[i] == mod_sel_;
    y += rh + k(2);
  }
  // a két lista között a gombok
  const float bx = lx + lw + k(15), bw = k(120), bh = k(40);
  float by = top + k(50);
  for (const auto& [key, act] : {std::pair<const char*, MenuAct>{"ui.mods.enable", MenuAct::ModEnable}, {"ui.mods.disable", MenuAct::ModDisable},
                                 {"ui.mods.up", MenuAct::ModUp}, {"ui.mods.down", MenuAct::ModDown}}) {
    MenuWidget& b = add(MenuWidget::Kind::Button, {bx, by, bw, bh}, tr(key), act);
    b.small = true;
    by += bh + gap / 2;
  }
  // a kijelölt mod adatai
  const float dy = top + lh + gap;
  const float dw = rx + lw - lx;
  panels_.push_back({{lx, dy, dw, H - dy - k(96)}, 200, false});
  const content::ModInfo* sel = nullptr;
  for (const auto& m : mods_avail_) {
    if (m.id == mod_sel_) sel = &m;
  }
  float ty = dy + k(10);
  if (sel) {
    text(lx + k(12), ty, dw - k(24), sel->name + "  (" + sel->id + ", " + sel->version + ")", MenuText::Med, MenuText::Gold);
    ty += k(30);
    if (!sel->description.empty()) {
      text(lx + k(12), ty, dw - k(24), sel->description, MenuText::Small, MenuText::Text, false, 3);
      ty += k(60);
    }
    auto join = [](const std::vector<std::string>& v) {
      std::string s;
      for (const auto& x : v) s += (s.empty() ? "" : ", ") + x;
      return s.empty() ? std::string("–") : s;
    };
    text(lx + k(12), ty, dw - k(24), loc_.tr("ui.mods.depends", {{"x", join(sel->depends)}}), MenuText::Small, MenuText::Dim);
    ty += k(24);
    text(lx + k(12), ty, dw - k(24), loc_.tr("ui.mods.conflicts", {{"x", join(sel->conflicts)}}), MenuText::Small, MenuText::Dim);
    ty += k(28);
  } else {
    text(lx + k(12), ty, dw - k(24), tr("ui.mods.pick"), MenuText::Small, MenuText::Dim);
    ty += k(30);
  }
  for (const auto& e : mods_err_) {   // a hibák a mentés előtt (a 0 A.D. szerint)
    text(lx + k(12), ty, dw - k(24), e, MenuText::Small, MenuText::Warn);
    ty += k(22);
  }
  MenuWidget& save = add(MenuWidget::Kind::Button, {area_x_ + area_w_ - k(36 + 300), H - k(76), k(300), k(50)}, tr("ui.mods.save"), MenuAct::ModSave);
  save.enabled = mods_err_.empty();
  back_button();
}

// --- bevitel --------------------------------------------------------------------------------------------------------
const MenuWidget* Menu::find(MenuAct act, int arg) const {
  for (const MenuWidget& w : widgets_) {
    if (w.act == act && w.arg == arg) return &w;
  }
  return nullptr;
}

const MenuWidget* Menu::open_dropdown() const {
  if (open_dd_ == MenuAct::None) return nullptr;
  const MenuWidget* w = find(open_dd_, open_dd_arg_);
  return w && w->kind == MenuWidget::Kind::Dropdown ? w : nullptr;
}

std::vector<MenuRect> Menu::dropdown_items() const {
  std::vector<MenuRect> out;
  const MenuWidget* d = open_dropdown();
  if (!d) return out;
  const float ih = k(26);
  const float n = static_cast<float>(d->options.size());
  float y = d->r.y + d->r.h;
  if (y + n * ih > static_cast<float>(h_) - 4) y = d->r.y - n * ih;
  for (size_t i = 0; i < d->options.size(); ++i) out.push_back({d->r.x, y + static_cast<float>(i) * ih, d->r.w, ih});
  return out;
}

Menu::Result Menu::click(float x, float y, int button) {
  if (const MenuWidget* d = open_dropdown()) {
    const MenuAct act = d->act;
    const int arg = d->arg;
    const auto items = dropdown_items();
    open_dd_ = MenuAct::None;
    for (size_t i = 0; i < items.size(); ++i) {
      if (items[i].contains(x, y)) {
        choose(act, arg, static_cast<int>(i));
        break;
      }
    }
    return Result::None;
  }
  open_dd_ = MenuAct::None;
  for (const MenuWidget& w : widgets_) {
    if (w.enabled && w.r.contains(x, y)) {
      const MenuWidget copy = w;   // az aktiválás átrendezhet
      return activate(copy, button, x);
    }
  }
  if (screen_ == MenuScreen::Main) open_ = -1;   // kattintás az almenün kívül: bezárul
  return Result::None;
}

Menu::Result Menu::activate(const MenuWidget& w, int button, float x) {
  switch (w.act) {
    case MenuAct::Toggle:
      if (screen_ != MenuScreen::Main) go(MenuScreen::Main);   // oldalsó képernyőről: vissza, az almenü nyitva
      open_ = open_ == w.arg ? -1 : w.arg;
      anim_t_ = now_;
      break;
    case MenuAct::Skirmish: go(MenuScreen::Skirmish); break;
    case MenuAct::Settings: go(MenuScreen::Settings); break;
    case MenuAct::Quit: return Result::Quit;
    case MenuAct::Back:
      go(screen_ == MenuScreen::Sound || screen_ == MenuScreen::Keys ? MenuScreen::Settings : MenuScreen::Main);
      break;
    case MenuAct::Start: {
      std::string err;
      if (!make_match_config(sk_, maps_, factions_, 0, nullptr, &err)) {
        message_ = loc_.tr(err);
        return Result::None;
      }
      message_.clear();
      return Result::Start;
    }
    case MenuAct::PickMap:
      sk_.map = static_cast<size_t>(w.arg);
      apply_map();
      break;
    case MenuAct::SlotKind:
    case MenuAct::SlotFaction:
    case MenuAct::SlotDifficulty:
    case MenuAct::SlotPersonality:
    case MenuAct::SlotTeam:
    case MenuAct::SlotStart:
    case MenuAct::SlotColor:
    case MenuAct::Option:
      open_dd_ = w.act;
      open_dd_arg_ = w.arg;
      break;
    case MenuAct::Cycle: cycle(w.arg, button == 3 ? -1 : 1); break;
    case MenuAct::OpenSound: go(MenuScreen::Sound); break;
    case MenuAct::OpenMods: go(MenuScreen::Mods); break;
    case MenuAct::OpenLoad: go(MenuScreen::Load); break;
    case MenuAct::OpenCredits: go(MenuScreen::Credits); break;
    case MenuAct::SavePick:
      save_sel_ = w.arg;
      if (button == 1 && w.arg >= 0 && static_cast<size_t>(w.arg) < saves_.size() && last_pick_ == w.arg && now_ - last_pick_t_ < 0.4) {
        load_choice_ = saves_[static_cast<size_t>(w.arg)].path;   // dupla kattintás: betöltés
        return Result::Load;
      }
      last_pick_ = w.arg;
      last_pick_t_ = now_;
      break;
    case MenuAct::SaveLoad:
      if (save_sel_ >= 0 && static_cast<size_t>(save_sel_) < saves_.size()) {
        load_choice_ = saves_[static_cast<size_t>(save_sel_)].path;
        return Result::Load;
      }
      break;
    case MenuAct::SaveDelete:
      if (save_sel_ >= 0 && static_cast<size_t>(save_sel_) < saves_.size()) delete_req_ = saves_[static_cast<size_t>(save_sel_)].path;
      break;
    case MenuAct::ModPick: {
      const bool on = w.arg >= 1000;
      const size_t i = static_cast<size_t>(on ? w.arg - 1000 : w.arg);
      if (on && i < mods_on_.size()) mod_sel_ = mods_on_[i];
      if (!on) {
        size_t k = 0;
        for (const auto& m : mods_avail_) {
          if (std::find(mods_on_.begin(), mods_on_.end(), m.id) != mods_on_.end()) continue;
          if (k++ == i) mod_sel_ = m.id;
        }
      }
      break;
    }
    case MenuAct::ModEnable:
      if (!mod_sel_.empty() && std::find(mods_on_.begin(), mods_on_.end(), mod_sel_) == mods_on_.end()) mods_on_.push_back(mod_sel_);
      mods_check();
      break;
    case MenuAct::ModDisable:
      mods_on_.erase(std::remove(mods_on_.begin(), mods_on_.end(), mod_sel_), mods_on_.end());
      mods_check();
      break;
    case MenuAct::ModUp:
    case MenuAct::ModDown: {
      auto it = std::find(mods_on_.begin(), mods_on_.end(), mod_sel_);
      if (it == mods_on_.end()) break;
      const auto pos = static_cast<size_t>(it - mods_on_.begin());
      if (w.act == MenuAct::ModUp && pos > 0) std::swap(mods_on_[pos], mods_on_[pos - 1]);
      if (w.act == MenuAct::ModDown && pos + 1 < mods_on_.size()) std::swap(mods_on_[pos], mods_on_[pos + 1]);
      mods_check();
      break;
    }
    case MenuAct::ModSave:
      mods_check();
      if (!mods_err_.empty()) break;
      st_.mods = mods_on_;
      changes_ |= kSave;
      return Result::Restart;
    case MenuAct::OpenKeys: go(MenuScreen::Keys); break;
    case MenuAct::SoundToggle: {
      Settings::Volume& v = st_.volume[static_cast<size_t>(w.arg)];
      v.enabled = !v.enabled;
      changes_ |= kSave;
      break;
    }
    case MenuAct::SoundSlider:
      dragging_ = w.arg;
      set_slider(w.arg, x);
      break;
    case MenuAct::KeyCell:
      capture_ = w.arg;
      message_ = loc_.tr("ui.keys.press", {{"n", std::to_string(w.arg + 1)}});
      break;
    case MenuAct::KeysDefault:
      st_.keys.reset();
      capture_ = -1;
      changes_ |= kSave;
      message_ = tr("ui.keys.restored");
      break;
    case MenuAct::None: break;
  }
  return Result::None;
}

void Menu::choose(MenuAct act, int arg, int index) {
  if (act == MenuAct::SlotKind) {
    if (arg > 0 && arg < 8) sk_.slots[static_cast<size_t>(arg)].kind = index == 0 ? SlotSetup::Kind::Ai : SlotSetup::Kind::Closed;
  } else if (act == MenuAct::SlotFaction) {
    if (arg >= 0 && arg < 8) {
      sk_.slots[static_cast<size_t>(arg)].faction =
          index <= 0 || static_cast<size_t>(index) > factions_.size() ? "random" : factions_[static_cast<size_t>(index - 1)].id;
    }
  } else if (act == MenuAct::SlotStart) {
    if (arg >= 0 && arg < 8) sk_.slots[static_cast<size_t>(arg)].start = static_cast<int8_t>(std::clamp(index - 1, -1, 7));
  } else if (act == MenuAct::SlotColor) {
    if (arg >= 0 && arg < 8) sk_.slots[static_cast<size_t>(arg)].color = static_cast<int8_t>(std::clamp(index, 0, 7));
  } else if (act == MenuAct::SlotTeam) {
    if (arg >= 0 && arg < 8) sk_.slots[static_cast<size_t>(arg)].team = static_cast<uint8_t>(std::clamp(index, 0, 4));
  } else if (act == MenuAct::SlotDifficulty || act == MenuAct::SlotPersonality) {
    const auto& ids = act == MenuAct::SlotDifficulty ? ai_difficulty_ids() : ai_personality_ids();
    if (arg > 0 && arg < 8 && index >= 0 && static_cast<size_t>(index) < ids.size()) {
      SlotSetup& sl = sk_.slots[static_cast<size_t>(arg)];
      (act == MenuAct::SlotDifficulty ? sl.difficulty : sl.personality) = ids[static_cast<size_t>(index)];
    }
  } else if (act == MenuAct::Option) {
    switch (arg) {
      case 0: sk_.fog = static_cast<sim::MatchSetup::Fog>(std::clamp(index, 0, 2)); break;
      case 1: sk_.daytime = static_cast<sim::MatchSetup::Daytime>(std::clamp(index, 0, 2)); break;
      case 2: sk_.weather = static_cast<sim::MatchSetup::WeatherMode>(std::clamp(index, 0, 3)); break;
      case 3: sk_.camps = index == 0; break;
      case 4: sk_.upkeep = index == 0; break;
      case 5:
        if (index >= 4 && static_cast<size_t>(index - 4) < victories_.size()) {   // M13: modbeli győzelmi szkript
          sk_.victory = sim::MatchSetup::Victory::Script;
          sk_.victory_script = victories_[static_cast<size_t>(index - 4)].id;
        } else {
          sk_.victory = static_cast<sim::MatchSetup::Victory>(std::clamp(index, 0, 3));
          sk_.victory_script.clear();
        }
        break;
      case 6: sk_.tempo = static_cast<sim::MatchSetup::Tempo>(std::clamp(index, 0, 3)); break;
      case 7: sk_.resources = static_cast<uint8_t>(std::clamp(index, 0, 2)); break;
      default: break;
    }
  }
  message_.clear();
}

void Menu::cycle(int row, int dir) {
  switch (row) {
    case 0: {
      const auto& langs = loc_.languages();   // a locale mappák .po fájljai
      if (langs.empty()) return;
      int i = -1;
      for (size_t j = 0; j < langs.size(); ++j) {
        if (langs[j].code == st_.language) i = static_cast<int>(j);
      }
      st_.language = langs[static_cast<size_t>(wrap_index(i, dir, static_cast<int>(langs.size())))].code;
      loc_.set_lang(st_.language);
      changes_ |= kLanguage;
      break;
    }
    case 1: {
      const auto& res = Settings::resolutions();
      int i = -1;
      for (size_t j = 0; j < res.size(); ++j) {
        if (res[j].first == st_.width && res[j].second == st_.height) i = static_cast<int>(j);
      }
      const auto& r = res[static_cast<size_t>(wrap_index(i, dir, static_cast<int>(res.size())))];
      st_.width = r.first;
      st_.height = r.second;
      changes_ |= kDisplay;
      break;
    }
    case 2: st_.fullscreen = !st_.fullscreen; changes_ |= kDisplay; break;
    case 3: {
      const auto& sc = Settings::ui_scales();
      const auto it = std::find(sc.begin(), sc.end(), st_.ui_scale);
      const int i = it == sc.end() ? -1 : static_cast<int>(it - sc.begin());
      st_.ui_scale = sc[static_cast<size_t>(wrap_index(i, dir, static_cast<int>(sc.size())))];
      break;
    }
    case 4: st_.colorblind = !st_.colorblind; changes_ |= kColors; break;
    case 5: st_.scroll_speed = kScroll[wrap_index(index_of(kScroll, st_.scroll_speed), dir, 5)]; break;
    case 6: st_.edge_scroll = !st_.edge_scroll; break;
    case 7: st_.show_fps = !st_.show_fps; break;
    case 8: st_.fps_limit = kFps[wrap_index(index_of(kFps, st_.fps_limit), dir, 4)]; break;
    case 11: st_.show_clock = !st_.show_clock; break;
    case 12: st_.help_hint = !st_.help_hint; break;
    case 13: st_.shadows = !st_.shadows; break;
    case 14: st_.ambient = !st_.ambient; break;
    case 15: st_.status_fx = !st_.status_fx; break;
    case 16: st_.game_speed = kSpeeds[wrap_index(index_of(kSpeeds, st_.game_speed), dir, 5)]; break;
    default: return;
  }
  changes_ |= kSave;
}

void Menu::set_slider(int cat, float x) {
  const MenuWidget* s = find(MenuAct::SoundSlider, cat);
  if (!s || cat < 0 || static_cast<size_t>(cat) >= kSoundCount) return;
  const float t = s->r.w > 1 ? (x - s->r.x) / (s->r.w - 1) : 0;
  st_.volume[static_cast<size_t>(cat)].value = static_cast<int>(std::lround(std::clamp(t, 0.0f, 1.0f) * 100.0f));
}

void Menu::drag(float x, float) {
  if (dragging_ >= 0) set_slider(dragging_, x);
}

void Menu::release() {
  if (dragging_ >= 0) {
    dragging_ = -1;
    changes_ |= kSave;   // a húzás végén ment (mint a régi `_finish_volume_drag`)
  }
}

Menu::Result Menu::key(int key) {
  if (capture_ >= 0) {
    if (key == 27) {
      capture_ = -1;
      message_.clear();
      return Result::None;
    }
    if (key == 13) return Result::None;
    const char c = static_cast<char>(key >= 'a' && key <= 'z' ? key - 'a' + 'A' : key);
    const int slot = capture_;
    const int r = st_.keys.assign(slot, c);
    if (r == -1) {
      capture_ = -1;
      changes_ |= kSave;
      message_ = loc_.tr("ui.keys.done", {{"n", std::to_string(slot + 1)}, {"k", std::string(1, c)}});
    } else if (r == Keymap::kInvalid) {
      message_ = tr("ui.keys.invalid");
    } else {
      message_ = loc_.tr("ui.keys.conflict",
                         {{"k", std::string(1, c)}, {"n", std::to_string(r + 1)}});
    }
    return Result::None;
  }
  if (key == 27) {
    if (open_dd_ != MenuAct::None) {
      open_dd_ = MenuAct::None;
      return Result::None;
    }
    switch (screen_) {
      case MenuScreen::Mods: go(MenuScreen::Main); return Result::None;
      case MenuScreen::Main:
        if (open_ >= 0) {   // nyitott almenünél az Esc csak az almenüt zárja be
          open_ = -1;
          return Result::None;
        }
        return Result::Quit;
      case MenuScreen::Sound:
      case MenuScreen::Keys: go(MenuScreen::Settings); return Result::None;
      default: go(MenuScreen::Main); return Result::None;
    }
  }
  if (key == 13 && screen_ == MenuScreen::Skirmish && open_dd_ == MenuAct::None) {
    if (const MenuWidget* s = find(MenuAct::Start); s && s->enabled) return activate(*s, 1, s->r.cx());
  }
  return Result::None;
}

bool Menu::match_config(uint64_t seed, MatchConfig* out) {
  std::string err;
  if (make_match_config(sk_, maps_, factions_, seed, out, &err)) return true;
  message_ = loc_.tr(err);
  return false;
}

}  // namespace totu::client
