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
        {"ui.main.load", MenuAct::None, true}}},
      {"ui.main.multiplayer", MenuAct::None, true, {}},
      {"ui.main.stories", MenuAct::None, true, {}},
      {"ui.main.settings", MenuAct::Toggle, false,
       {{"ui.main.game_settings", MenuAct::Settings, false},
        {"ui.main.profiles", MenuAct::None, true}}},
      {"ui.main.mods", MenuAct::None, true, {}},
      {"ui.main.credits", MenuAct::None, true, {}},
      {"ui.main.quit", MenuAct::Quit, false, {}},
  };
  return m;
}

const char* const kSettingLabels[Menu::kSettingRows] = {
    "ui.settings.language",     "ui.settings.resolution",  "ui.settings.fullscreen", "ui.settings.ui_scale",
    "ui.settings.colorblind",   "ui.settings.scroll_speed", "ui.settings.edge_scroll", "ui.settings.show_fps",
    "ui.settings.fps_limit",    "ui.settings.sound",        "ui.settings.keys"};

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

}  // namespace

// --- tartalom -------------------------------------------------------------------------------------------------------
std::vector<MapInfo> scan_maps(const std::string& dir) {
  std::vector<MapInfo> out;
  std::error_code ec;
  for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
    if (e.path().extension() != ".json") continue;
    const auto doc = content::parse(read_file(e.path()));
    if (!doc || !doc->is_object()) continue;
    const content::Value* ob = doc->find("objects");
    const content::Value* st = ob ? ob->find("starts") : nullptr;
    if (!st || !st->is_array() || st->items().empty()) continue;
    MapInfo m;
    m.path = e.path().string();
    m.name = e.path().stem().string();
    m.players = static_cast<int>(std::min<size_t>(st->items().size(), 8));
    if (const content::Value* v = doc->find("width")) m.width = static_cast<int>(v->as_int().value_or(0));
    if (const content::Value* v = doc->find("height")) m.height = static_cast<int>(v->as_int().value_or(0));
    if (const content::Value* v = doc->find("_leiras"); v && v->is_string()) m.desc = v->text();
    out.push_back(std::move(m));
  }
  std::sort(out.begin(), out.end(), [](const MapInfo& a, const MapInfo& b) { return a.name < b.name; });
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
    if (kind == SlotSetup::Kind::Closed) {
      c.setup.factions.emplace_back();
      continue;
    }
    std::string f = sl.faction;
    const bool known = std::any_of(factions.begin(), factions.end(), [&](const FactionInfo& fi) { return fi.id == f; });
    if (!known) f = factions[static_cast<size_t>(splitmix(rng) % factions.size())].id;   // véletlen nép
    c.setup.factions.push_back(f);
    ++participants;
    if (kind == SlotSetup::Kind::Ai) c.ai.push_back(static_cast<int>(i) + 1);
  }
  if (participants < 2) return fail("ui.skirmish.need_two");
  c.setup.fog = s.fog;
  c.setup.daytime = s.daytime;
  c.setup.weather = s.weather;
  c.setup.camps = s.camps;
  c.setup.upkeep = s.upkeep;
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

void Menu::go(MenuScreen s) {
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
  t.x = static_cast<float>(w_) / 2;
  t.y = k(14);
  t.w = static_cast<float>(w_) - k(80);
  t.text = s;
  t.font = MenuText::Huge;
  t.color = MenuText::Gold;
  t.center = true;
  t.title = true;
  texts_.push_back(std::move(t));
}

void Menu::back_button() {
  add(MenuWidget::Kind::Button, {k(36), static_cast<float>(h_) - k(76), k(190), k(50)}, tr("ui.back"), MenuAct::Back);
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
  logo_ = {};
  switch (screen_) {
    case MenuScreen::Main: layout_main(); break;
    case MenuScreen::Skirmish: layout_skirmish(); break;
    case MenuScreen::Settings: layout_settings(); break;
    case MenuScreen::Sound: layout_sound(); break;
    case MenuScreen::Keys: layout_keys(); break;
  }
}

void Menu::layout_main() {
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
    mw.selected = open_ == static_cast<int>(i);
    if (g.soon) mw.tag = tr("ui.main.soon");
    item_y.push_back(y);
    y += ih + gap;
  }
  // súgó és lábléc az oldalsáv alján (a régi `MainMenu.draw`)
  const float foot = H - k(70);
  text(sw / 2, foot + k(10), sw - k(30), tr("ui.main.creator"), MenuText::Tiny, MenuText::Dim, true);
  text(sw / 2, foot + k(34), sw - k(30), std::string("v") + core::version(), MenuText::Tiny, MenuText::Dim, true);
  if (open_ >= 0 && static_cast<size_t>(open_) < items.size()) {
    const auto& subs = items[static_cast<size_t>(open_)].subs;
    const float pw = k(300);
    const float ph = static_cast<float>(subs.size()) * (ih + gap) + k(18);
    const float px = sw + k(4);
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
  for (int i = 0; i < nmaps; ++i) {
    const MapInfo& m = maps_[static_cast<size_t>(i)];
    MenuWidget& mw = add(MenuWidget::Kind::MapRow, {lx + k(6), top + k(32) + static_cast<float>(i) * k(26), lw - k(12), k(24)},
                         "[" + std::to_string(m.players) + "]  " + m.name, MenuAct::PickMap, i);
    mw.selected = static_cast<size_t>(i) == sk_.map;
  }
  float ty = top + k(32) + static_cast<float>(nmaps) * k(26) + k(10);
  if (maps_.empty()) {
    text(lp.x + k(10), ty, lw - k(20), tr("ui.skirmish.no_map"), MenuText::Small, MenuText::Warn);
  } else {
    const MapInfo& md = maps_[sk_.map];
    text(lp.x + k(10), ty, lw - k(20), md.name, MenuText::Med, MenuText::Gold);
    ty += k(24);
    text(lp.x + k(10), ty, lw - k(20),
         loc_.tr("ui.skirmish.map_info", {{"n", std::to_string(md.players)}, {"w", std::to_string(md.width)},
                                           {"h", std::to_string(md.height)}}),
         MenuText::Tiny, MenuText::Dim);
    ty += k(22);
    const int lines = std::max(1, static_cast<int>((lp.y + lp.h - ty - k(6)) / k(18)));
    text(lp.x + k(10), ty, lw - k(20), md.desc, MenuText::Tiny, MenuText::Text, false, lines);
  }
  // ---- játékoshelyek: #, résztvevő, nép, szín (a régi oszlopok közül, ami a MatchSetup-ban megvan)
  const float weights[] = {24, 167, 156, 122};
  const float gap = k(4);
  float sum = 0;
  for (float wt : weights) sum += wt;
  const float sc = std::min((W - x0 - k(30) - gap * 3) / sum, 1.6f * u_);
  float cx[4], cw[4];
  float x = x0;
  for (int i = 0; i < 4; ++i) {
    cx[i] = x;
    cw[i] = std::floor(weights[i] * sc);
    x += cw[i] + gap;
  }
  const float row_y0 = top + k(36), row_h = k(34), dh = k(27);
  const MenuRect slots{x0 - k(10), top, W - x0 - k(14), row_y0 + 8 * row_h + k(8) - top};
  panels_.push_back({slots, 200, false});
  const char* heads[] = {"ui.skirmish.col.num", "ui.skirmish.col.participant", "ui.skirmish.col.faction",
                         "ui.skirmish.col.color"};
  for (int i = 0; i < 4; ++i) text(cx[i], slots.y + k(10), cw[i], tr(heads[i]), MenuText::Small, MenuText::Gold);
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
    MenuWidget& col = add(MenuWidget::Kind::Dropdown, {cx[3], y, cw[3], dh}, "", MenuAct::None, i);
    col.options = {loc_.tr((st_.colorblind ? "ui.color_safe." : "ui.color.") + std::to_string(i))};
    col.enabled = false;
    col.team = i;
  }
  // ---- játékbeállítások (saját panelen)
  const float oy = slots.y + slots.h + k(16);
  const float ow = std::min(k(300), std::floor((W - x0 - k(24)) / 3));
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
  };
  constexpr int kOpts = 5;
  panels_.push_back({{x0 - k(10), oy, 3 * ow + k(4), static_cast<float>((kOpts + 2) / 3) * k(62) + k(14)}, 200, false});
  for (int i = 0; i < kOpts; ++i) {
    const float ox = x0 + static_cast<float>(i % 3) * ow;
    const float oy2 = oy + k(10) + static_cast<float>(i / 3) * k(62);
    text(ox, oy2, ow - k(20), tr(opts[i].label), MenuText::Small, MenuText::Gold);
    MenuWidget& d = add(MenuWidget::Kind::Dropdown, {ox, oy2 + k(24), ow - k(20), dh}, "", MenuAct::Option, i);
    for (const char* o : opts[i].options) d.options.push_back(tr(o));
    d.current = opts[i].current;
  }
  add(MenuWidget::Kind::Button, {W - k(36 + 240), H - k(76), k(240), k(50)}, tr("ui.skirmish.start"), MenuAct::Start).enabled =
      !maps_.empty();
  back_button();
  if (!message_.empty()) text(W / 2, H - k(124), W - k(80), message_, MenuText::Med, MenuText::Warn, true);
}

void Menu::layout_settings() {
  const float W = static_cast<float>(w_), H = static_cast<float>(h_);
  title(tr("ui.settings.title"));
  const float top = k(96);
  const float pw = std::min(k(720), W - k(72));
  const float step = std::max(k(20), std::min(k(54), std::floor((H - top - k(150)) / kSettingRows)));
  const float bh = std::max(1.0f, step - k(2));
  const MenuRect pr{W / 2 - pw / 2, top, pw, kSettingRows * step + k(60)};
  panels_.push_back({pr, 215, false});
  const float bw = k(270);
  auto onoff = [&](bool v) { return tr(v ? "ui.on" : "ui.off"); };
  for (int i = 0; i < kSettingRows; ++i) {
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
      default: val = tr("ui.settings.keys_button"); act = MenuAct::OpenKeys; break;
    }
    const float y = pr.y + k(16) + static_cast<float>(i) * step;
    MenuWidget& b = add(MenuWidget::Kind::Button, {pr.x + pr.w - k(24) - bw, y, bw, bh}, val, act, i);
    b.small = true;
    text(pr.x + k(24), y + (bh - k(22)) / 2, pr.w - k(24 + 270 + 44), tr(kSettingLabels[i]), MenuText::Med, MenuText::Text);
  }
  text(pr.x + k(24), pr.y + pr.h - k(34), pr.w - k(48), tr("ui.settings.footer"),
       MenuText::Tiny, MenuText::Dim);
  back_button();
}

void Menu::layout_sound() {
  const float W = static_cast<float>(w_), H = static_cast<float>(h_);
  title(tr("ui.sound.title"));
  const float top = k(96);
  const float pw = std::min(k(770), W - k(72));
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
  const float W = static_cast<float>(w_), H = static_cast<float>(h_);
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
  add(MenuWidget::Kind::Button, {W - k(36 + 240), H - k(76), k(240), k(50)}, tr("ui.keys.default"), MenuAct::KeysDefault);
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
    case MenuAct::Toggle: open_ = open_ == w.arg ? -1 : w.arg; break;
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
    case MenuAct::Option:
      open_dd_ = w.act;
      open_dd_arg_ = w.arg;
      break;
    case MenuAct::Cycle: cycle(w.arg, button == 3 ? -1 : 1); break;
    case MenuAct::OpenSound: go(MenuScreen::Sound); break;
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
  } else if (act == MenuAct::Option) {
    switch (arg) {
      case 0: sk_.fog = static_cast<sim::MatchSetup::Fog>(std::clamp(index, 0, 2)); break;
      case 1: sk_.daytime = static_cast<sim::MatchSetup::Daytime>(std::clamp(index, 0, 2)); break;
      case 2: sk_.weather = static_cast<sim::MatchSetup::WeatherMode>(std::clamp(index, 0, 3)); break;
      case 3: sk_.camps = index == 0; break;
      case 4: sk_.upkeep = index == 0; break;
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
