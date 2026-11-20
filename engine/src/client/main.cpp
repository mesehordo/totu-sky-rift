// SPDX-License-Identifier: GPL-3.0-or-later
// TOTU-kliens (M5, 1. lépés): SDL3-ablak OpenGL 3.3 core környezettel, a saját grafikus réteggel (render/).
//   totu --version                 verzió és platform (ablak nélkül; a CI-ben is fut)
//   totu --frames N                N képkocka után kilép (füstteszt)
//   totu --demo N                  bemutató jelenet: N egység valódi sprite-okkal, csapatszínnel, animációval
//   totu --bench                   képkockaidő-mérés (függőleges szinkron nélkül, a GPU bevárásával): p50, p99
//   totu --screenshot kép.png      a futás végén képernyőkép (a képpróbákhoz)
//   totu --atlas mappa             a bemutató atlaszból rajzol (Y-rendezve, a népek keverve – mint a világnézet)
//   totu --world pálya.json        világnézet: terep, két sereg a szimulációból (--demo N: játékosonként N egység);
//                                  kamera: nyilak / WASD, görgő; bal gombos keret: kijelölés; jobb gomb: mozgatás
//                                  alakzatban (1–6: alakzat); --content mappa: a tartalom helye
//   totu --daytime T               rögzített napszak (0 = hajnal, 0,275 = dél, 0,75 = éjfél); különben 4 perces ciklus
//   totu --nofog                   köd nélkül
//   totu --weather NÉV[:ERŐSSÉG]   időjárás (derult, felhos, eso, zivatar, kod, ho, hovihar, homokvihar, hoseg,
//                                  hamueso; erősség 0..1, alapból 1); a V billentyű fokozatos átmenettel vált
//   (meccsben: jobb gomb ellenségre – támadás; A + bal gomb – támadó mozgás; Q – telepes képzése; B – építési mód, az ursan épületek léptetése; bal gomb: lerakás az első kijelölt munkással;
//    jobb gomb: kilépés az építési módból)
//   totu --battle-demo N           meccsben: két N fős vegyes ursan sereg a pálya közepén egymásnak megy (képpróba)
//   totu --build-demo              meccsben: az 1. játékos egy telepese tanyát kezd a főépület mellé (képpróba)
//   totu --look X,Y                a kamera középpontja (csempe)
//   totu --zoom Z                  a kamera nagyítása (1,0–2,0; alap 1,5)
//   totu --sync                    a szimuláció a rajzolás szálán, két képkockánként egy tick (képernyőképnél mindig így;
//                                  különben a szimuláció külön szálon fut, 30 Hz-en)
//   totu --nopost                  utófeldolgozás nélkül (a 4. lépés képe)
//   totu --check                   a képernyőképnek legalább 2%-a nem háttérszín (különben 1-es kilépési kód; CI)
//   totu --hidden                  rejtett ablak (CI);  --size SZxM  ablakméret;  --assets mappa  a grafika helye
//   totu [--menu]                  főmenü (M8): egyéni csata, beállítások; paraméter nélkül is ez indul
//                                  (--maps mappa: a pályák helye; a beállításfájl a felhasználói mappában;
//                                  --menu-screen skirmish|settings|sound|keys|mods|load: ezzel a képernyővel indul – képpróba)
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "core/platform_info.hpp"
#include "net/content_hash.hpp"
#include "net/relay_server.hpp"
#include "sim/chain.hpp"
#include "sim/mission.hpp"
#include "map/mapgen.hpp"
#include "client/audio.hpp"
#include "client/profiles.hpp"
#include "client/hud.hpp"
#include "client/atlas_cache.hpp"
#include "client/diagnostics.hpp"
#include "client/savegame.hpp"
#include "client/cursors.hpp"
#include "client/unit_shapes.hpp"
#include "client/ground_overlay.hpp"
#include "client/sigil.hpp"
#include "client/i18n.hpp"
#include "client/menu_view.hpp"
#include "client/net_play.hpp"
#include "client/editor_doc.hpp"
#include "client/editor_ui.hpp"
#include "client/editor_view.hpp"
#include "client/trigger_editor.hpp"
#include "client/settings.hpp"
#include "client/world.hpp"
#include "render/atlas.hpp"
#include "render/daylight.hpp"
#include "render/effects.hpp"
#include "render/particles.hpp"
#include "render/weather_look.hpp"
#include "render/lighting.hpp"
#include "render/terrain_view.hpp"
#include "render/rhi.hpp"
#include "render/sprite_batch.hpp"
#include "render/sprite_sheet.hpp"

namespace {

using namespace totu;

void print_version() {
  std::printf("TOTU: Sky Rift %s – %s/%s, %s, SDL %d.%d.%d\n", core::version(), core::os_name(), core::cpu_arch(),
              core::compiler().c_str(), SDL_VERSIONNUM_MAJOR(SDL_GetVersion()), SDL_VERSIONNUM_MINOR(SDL_GetVersion()),
              SDL_VERSIONNUM_MICRO(SDL_GetVersion()));
}

void* gl_loader(const char* name) { return reinterpret_cast<void*>(SDL_GL_GetProcAddress(name)); }

struct Options {
  uint64_t map_seed = 0;   // --map-seed: a pálya a leírásából ezzel a maggal újragenerálva
  bool build_atlas = false;
  std::vector<std::string> mods;   // --mods
  bool mods_given = false;
  long frames = -1;
  int demo = 0;
  bool bench = false, hidden = false, check = false;
  std::string screenshot, atlas, world;
  std::string content = TOTU_CONTENT_DIR;
  double daytime = -1;
  bool fog = true;
  bool post = true;
  render::Weather weather = render::Weather::Clear;
  float weather_power = 1.0f;
  float zoom = 0;   // 0: a régi játék alapja (1,5)
  bool sync = false;
  float look_x = -1, look_y = -1;
  std::string load;   // mentett játék (a régi `LoadMenu`)
  bool build_demo = false;
  bool garrison_demo = false;   // képpróba (D39): helyőrséges Hosszúház, a kurzor fölötte
  int battle_demo = 0;
  std::string assets = TOTU_ASSETS_DIR;
  int width = 1280, height = 720;
  bool size_given = false;
  bool menu = false;                 // M8: a főmenü
  client::MenuScreen menu_screen = client::MenuScreen::Main;
  std::string maps = TOTU_MAPS_DIR;
  // M10: többjátékos (lockstep relével): gazda (`--host <port>`, a relé a játékban fut) vagy csatlakozó
  // (`--join <cím:port>`); a lobbiban a `--faction` népet választja, és késznek jelentkezik
  int host_port = 0, seats = 2;
  std::string join, faction = "ursan", name = "jatekos";
  bool observe = false;
  int rejoin = 0;
  std::string mission;   // M11: küldetés indítása (a küldetés mappája)
  std::string editor;    // M11: a szerkesztő (a küldetés mappája)
  bool fog_blur = true;    // a köd stílusa: elmosott (D41 kiegészítés, 2026-11-19); `--fog-gray`: a régi szürke
  float fog_soft = 1.5f;   // a köd határa: mintatávolság csempében (a tulajdonos kérése: szélesebb, kb. 6 csempe)
  std::string chain;  // M11: küldetéssor (a küldetéssor mappája; a profil a felhasználói mappában)
  bool trigger_editor = false;   // képpróbához: a trigger-szerkesztő nyitva indul
  int data_editor = -1;          // képpróbához: az adatszerkesztő nyitva, ezzel a füllel
};

// a bemutató: népenként egy egységtípus (a sprite-lapok közül az első 8 csapatmaszkos), N egység rácsban
struct Demo {
  std::vector<render::SpriteSheet> sheets;
  int units = 0;

  bool load(render::Device& dev, const std::string& assets, std::string* error) {
    const std::filesystem::path dir = std::filesystem::path(assets) / "sprites" / "units";
    std::vector<std::string> files;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
      const std::string n = e.path().filename().string();
      if (e.path().extension() == ".json" && n.find("_old") == std::string::npos) files.push_back(e.path().string());
    }
    std::sort(files.begin(), files.end());
    std::string last_prefix;
    for (const auto& f : files) {
      const std::string prefix = std::filesystem::path(f).filename().string().substr(0, 3);   // nép-előtag
      if (prefix == last_prefix) continue;
      std::string err;
      auto s = render::load_sprite_sheet(dev, f, &err);
      if (!s || !s->mask.id || !s->anim("walk")) continue;
      sheets.push_back(std::move(*s));
      last_prefix = prefix;
      if (sheets.size() == 8) break;
    }
    if (sheets.empty()) {
      if (error) *error = "nincs betölthető egység-sprite: " + dir.string();
      return false;
    }
    return true;
  }

  void draw(render::SpriteBatch& batch, double t, int w, int h) const {
    static const uint8_t teams[8][3] = {{220, 40, 40},  {40, 90, 230},  {40, 180, 60},  {230, 200, 40},
                                        {160, 60, 200}, {240, 130, 30}, {40, 200, 200}, {235, 235, 235}};
    const int cols = std::max(1, static_cast<int>(std::sqrt(static_cast<double>(units) * w / h)));
    const float cell_w = static_cast<float>(w) / static_cast<float>(cols);
    const int rows = (units + cols - 1) / cols;
    const float cell_h = static_cast<float>(h) / static_cast<float>(std::max(1, rows));
    // népenként rajzolva (egy textúra = egy hívás); a világnézetben az atlasz és az Y-rendezés jön (2–3. lépés)
    for (size_t k = 0; k < sheets.size(); ++k) {
      const auto& s = sheets[k];
      const render::Animation* a = s.anim("walk");
      for (int i = static_cast<int>(k); i < units; i += static_cast<int>(sheets.size())) {
        const int dir = (i / 8 + static_cast<int>(t * 0.5)) % s.directions;
        const int frame = a->start + static_cast<int>(t * a->fps + i) % a->frames;
        render::SpriteInstance in;
        s.uv(dir, frame, in.u0, in.v0, in.u1, in.v1);
        const float scale = 0.5f / s.scale;   // a HD lap a játékban fele akkora; a bemutatóban még kisebb
        in.w = static_cast<float>(s.frame_w) * scale;
        in.h = static_cast<float>(s.frame_h) * scale;
        const float cx = (static_cast<float>(i % cols) + 0.5f) * cell_w, cy = (static_cast<float>(i / cols) + 0.8f) * cell_h;
        in.x = cx - s.anchor_x * scale;
        in.y = cy - s.anchor_y * scale;
        const auto* tc = teams[(i / 8) % 8];   // a csapat a típustól független
        in.team[0] = tc[0]; in.team[1] = tc[1]; in.team[2] = tc[2]; in.team[3] = 255;
        batch.draw(s.base, &s.mask, in);
      }
    }
  }
};

// Bemutató atlaszból: népenként egy egységtípus, véletlen (rögzített magú) helyen, Y szerint rendezve rajzolva.
struct AtlasDemo {
  render::Atlas atlas;
  std::vector<const render::AtlasSheet*> sheets;
  struct U {
    float x, y;
    int sheet, team, phase;
  };
  std::vector<U> units;

  bool load(render::Device& dev, const std::string& dir, int n, int w, int h, std::string* error) {
    if (!atlas.load(dev, dir, error)) return false;
    std::string last;
    for (const auto& s : atlas.data().sheets) {
      if (s.type != "unit" || !s.has_mask || !s.anims.count("walk")) continue;
      const std::string prefix = s.id.substr(0, 3);
      if (prefix == last) continue;
      sheets.push_back(&s);
      last = prefix;
      if (sheets.size() == 8) break;
    }
    if (sheets.empty()) {
      if (error) *error = "az atlaszban nincs egység";
      return false;
    }
    if (!atlas.prepare(sheets, error)) return false;   // a meccs elején: a résztvevő típusok lapjai egy tömbben
    uint32_t r = 12345;
    auto rnd = [&]() { r = r * 1664525u + 1013904223u; return r >> 8; };
    for (int i = 0; i < n; ++i) {
      units.push_back({static_cast<float>(rnd() % static_cast<uint32_t>(w)), static_cast<float>(rnd() % static_cast<uint32_t>(h)),
                       static_cast<int>(rnd() % sheets.size()), static_cast<int>(rnd() % 8), static_cast<int>(rnd() % 100)});
    }
    std::sort(units.begin(), units.end(), [](const U& a, const U& b) { return a.y < b.y; });
    return true;
  }

  void draw(render::SpriteBatch& batch, double t) {
    static const uint8_t teams[8][4] = {{220, 40, 40, 255},  {40, 90, 230, 255},  {40, 180, 60, 255}, {230, 200, 40, 255},
                                        {160, 60, 200, 255}, {240, 130, 30, 255}, {40, 200, 200, 255}, {235, 235, 235, 255}};
    for (const auto& u : units) {
      const auto& s = *sheets[static_cast<size_t>(u.sheet)];
      if (s.anims.empty()) continue;
      const auto wi = s.anims.find("walk");
      const auto& a = wi != s.anims.end() ? wi->second : s.anims.begin()->second;
      const int dir = (u.phase + static_cast<int>(t * 0.5)) % s.directions;
      const int col = a.start + static_cast<int>(t * a.fps + u.phase) % a.frames;
      atlas.draw(batch, s, dir, col, u.x, u.y, 0.5f / s.scale, teams[u.team]);
    }
  }
};

// egyszerű tartalék-textúrák: puha szélű kör (sprite nélküli egység, kijelölés) és fehér négyzet (keret)
render::Image circle_image(int n) {
  render::Image img;
  img.width = img.height = n;
  img.rgba.assign(static_cast<size_t>(n * n) * 4, 255);
  const float r = static_cast<float>(n) / 2.0f;
  for (int y = 0; y < n; ++y) {
    for (int x = 0; x < n; ++x) {
      const float d = std::sqrt((static_cast<float>(x) + 0.5f - r) * (static_cast<float>(x) + 0.5f - r) +
                                (static_cast<float>(y) + 0.5f - r) * (static_cast<float>(y) + 0.5f - r));
      const float a = std::clamp(r - d, 0.0f, 1.0f);
      img.at(x, y)[3] = static_cast<uint8_t>(a * 255.0f);
      if (d > r - 2.5f) { img.at(x, y)[0] = img.at(x, y)[1] = img.at(x, y)[2] = 40; }   // sötét perem
    }
  }
  return img;
}

double percentile(std::vector<double> v, double p) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  return v[std::min(v.size() - 1, static_cast<size_t>(p * static_cast<double>(v.size())))];
}

// a HUD bemenete (M8): billentyű és kattintás a HUD-modellbe (a parancsrács, a kijelölés-rács, a célzási módok);
// igaz, ha elfogyasztotta (a világ nem kapja meg)
bool hud_event(client::HudModel& m, const SDL_Event& ev, const client::Camera& cam, const client::World& world) {
  auto keycode = [](SDL_Keycode k) -> int {
    if (k >= SDLK_A && k <= SDLK_Z) return 'A' + static_cast<int>(k - SDLK_A);
    if (k >= SDLK_1 && k <= SDLK_9) return '1' + static_cast<int>(k - SDLK_1);
    if (k == SDLK_ESCAPE) return client::HudModel::kKeyEscape;
    if (k == SDLK_F8) return client::HudModel::kKeyF8;
    if (k == SDLK_F2) return client::HudModel::kKeyF2;
    if (k == SDLK_F3) return client::HudModel::kKeyF3;
    if (k == SDLK_F4) return client::HudModel::kKeyF4;
    if (k == SDLK_SPACE) return client::HudModel::kKeySpace;
    return -1;
  };
  if (ev.type == SDL_EVENT_KEY_DOWN && !m.text_focus) {   // a régi játék: + / − sebesség, F1 súgó
    const SDL_Keycode kk = ev.key.key;
    if (kk == SDLK_F1) {
      if (!ev.key.repeat) m.help_open = !m.help_open;
      return true;
    }
    if (kk == SDLK_KP_PLUS || kk == SDLK_EQUALS || kk == SDLK_PLUS || kk == SDLK_KP_MINUS || kk == SDLK_MINUS) {
      if (!ev.key.repeat && !m.speed_locked) m.speed_change = (kk == SDLK_KP_MINUS || kk == SDLK_MINUS) ? -1 : 1;
      return true;
    }
    if (kk == SDLK_ESCAPE && m.help_open) {
      m.help_open = false;
      return true;
    }
  }
  if (ev.type == SDL_EVENT_KEY_DOWN || ev.type == SDL_EVENT_KEY_UP) {
    const int k = keycode(ev.key.key);
    if (k < 0) return false;
    if (ev.type == SDL_EVENT_KEY_UP) {
      m.key_up(k);
      return false;
    }
    return m.key_down(k, ev.key.repeat, (ev.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0);
  }
  if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    const int b = ev.button.button == SDL_BUTTON_LEFT ? 1 : ev.button.button == SDL_BUTTON_RIGHT ? 3 : 2;
    if (m.click(ev.button.x, ev.button.y, b)) return true;
    const float wx = cam.x + ev.button.x / cam.zoom, wy = cam.y + ev.button.y / cam.zoom;
    return m.world_click(wx, wy, b, world.enemy_at(wx, wy));
  }
  return false;
}

}  // namespace

// a régi „minden indításkor új pálya”: a pályafájl leírásából új maggal (map/mapgen) a felhasználói mappába;
// hibánál a rögzített pálya marad
static std::string regenerate_map(const std::string& path, uint64_t seed) {
  std::ifstream in(path, std::ios::binary);
  const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  std::string gerr;
  const auto doc = totu::content::parse(text);
  const auto gen = doc ? totu::map::generate_map(*doc, seed, &gerr) : std::nullopt;
  if (!gen) {
    std::fprintf(stderr, "a pálya nem generálható (%s): a rögzített változat indul\n", gerr.c_str());
    return path;
  }
  const auto out_path = totu::client::saves_dir().parent_path() / "veletlen_palya.json";
  std::error_code ec;
  std::filesystem::create_directories(out_path.parent_path(), ec);
  std::ofstream of(out_path, std::ios::binary);
  of << totu::content::dump(*gen);
  return of ? out_path.string() : path;
}

int main(int argc, char** argv) {
  Options o;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
    if (a == "--version") { print_version(); return 0; }
    if (a == "--frames") o.frames = std::strtol(next(), nullptr, 10);
    else if (a == "--demo") o.demo = static_cast<int>(std::strtol(next(), nullptr, 10));
    else if (a == "--bench") o.bench = true;
    else if (a == "--hidden") o.hidden = true;
    else if (a == "--check") o.check = true;
    else if (a == "--screenshot") o.screenshot = next();
    else if (a == "--assets") o.assets = next();
    else if (a == "--atlas") o.atlas = next();
    else if (a == "--build-atlas") o.build_atlas = true;   // a játék atlaszának elkészítése a felhasználói mappába, majd kilépés
    else if (a == "--world") o.world = next();
    else if (a == "--map-seed") o.map_seed = std::strtoull(next(), nullptr, 10);   // a pálya újragenerálva ezzel a maggal
    else if (a == "--mods") {   // M13: az engedélyezett modok (vesszővel; felülírja a beállítást), üres: csak az alap
      o.mods_given = true;
      std::string list = next();
      for (size_t p = 0; p <= list.size();) {
        const size_t q = std::min(list.find(',', p), list.size());
        if (q > p) o.mods.push_back(list.substr(p, q - p));
        p = q + 1;
      }
    }
    else if (a == "--content") o.content = next();
    else if (a == "--daytime") o.daytime = std::strtod(next(), nullptr);
    else if (a == "--nofog") o.fog = false;
    else if (a == "--fog-blur") o.fog_blur = true;
    else if (a == "--fog-gray") o.fog_blur = false;   // összehasonlító képhez: a szürke köd
    else if (a == "--fog-soft") o.fog_soft = std::clamp(std::strtof(next(), nullptr), 0.5f, 3.0f);
    else if (a == "--nopost") o.post = false;
    else if (a == "--zoom") o.zoom = std::strtof(next(), nullptr);
    else if (a == "--sync") o.sync = true;
    else if (a == "--build-demo") o.build_demo = true;
    else if (a == "--garrison-demo") o.garrison_demo = true;
    else if (a == "--menu") o.menu = true;
    else if (a == "--menu-screen") {   // képpróbához: a menü ezzel a képernyővel indul
      const std::string v = next();
      o.menu = true;
      o.menu_screen = v == "skirmish" ? client::MenuScreen::Skirmish : v == "settings" ? client::MenuScreen::Settings
                      : v == "sound"  ? client::MenuScreen::Sound    : v == "keys"     ? client::MenuScreen::Keys
                      : v == "mods"   ? client::MenuScreen::Mods     : v == "load" ? client::MenuScreen::Load : v == "credits" ? client::MenuScreen::Credits : v == "lan" ? client::MenuScreen::Lan : v == "stories" ? client::MenuScreen::Stories : client::MenuScreen::Main;
    }
    else if (a == "--maps") o.maps = next();
    else if (a == "--mission") o.mission = next();
    else if (a == "--editor") o.editor = next();
    else if (a == "--chain") o.chain = next();
    else if (a == "--trigger-editor") o.trigger_editor = true;
    else if (a == "--data-editor") o.data_editor = static_cast<int>(std::strtol(next(), nullptr, 10));
    else if (a == "--host") o.host_port = static_cast<int>(std::strtol(next(), nullptr, 10));
    else if (a == "--join") o.join = next();
    else if (a == "--seats") o.seats = static_cast<int>(std::strtol(next(), nullptr, 10));
    else if (a == "--faction") o.faction = next();
    else if (a == "--name") o.name = next();
    else if (a == "--observe") o.observe = true;
    else if (a == "--rejoin") o.rejoin = static_cast<int>(std::strtol(next(), nullptr, 10));
    else if (a == "--battle-demo") o.battle_demo = static_cast<int>(std::strtol(next(), nullptr, 10));
    else if (a == "--load") o.world = o.load = next();   // mentett játék betöltése
    else if (a == "--look") {
      char* end = nullptr;
      const char* v = next();
      o.look_x = std::strtof(v, &end);
      if (end && *end == ',') o.look_y = std::strtof(end + 1, nullptr);
    }
    else if (a == "--weather") {
      const std::string v = next();
      const size_t colon = v.find(':');
      if (!render::weather_from_name(v.substr(0, colon), &o.weather)) {
        std::fprintf(stderr, "ismeretlen időjárás: %s\n", v.c_str());
        return 2;
      }
      if (colon != std::string::npos) o.weather_power = std::clamp(std::strtof(v.c_str() + colon + 1, nullptr), 0.0f, 1.0f);
    }
    else if (a == "--size") {   // SZxM
      char* end = nullptr;
      const char* v = next();
      const long w = std::strtol(v, &end, 10);
      if (end && *end == 'x') {
        const long h = std::strtol(end + 1, nullptr, 10);
        if (w > 0 && h > 0 && w <= 16384 && h <= 16384) {
          o.width = static_cast<int>(w);
          o.height = static_cast<int>(h);
          o.size_given = true;
        }
      }
    }
  }
  std::setvbuf(stdout, nullptr, _IONBF, 0);   // a kimenet azonnal (Windowson az SDL-indító kilépéskor nem üríti)
  print_version();
  if (o.build_atlas) {   // ablak nélkül: a gyorsítótár (a telepítőnek és a próbáknak is)
    char* pref = SDL_GetPrefPath("TOTU", "SkyRift");
    if (!pref) return 1;
    client::AtlasCache c;
    const auto t0 = std::chrono::steady_clock::now();
    const bool had = c.start(o.assets, pref, true);
    SDL_free(pref);
    c.wait();
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (!c.ready()) {
      std::fprintf(stderr, "atlasz: %s\n", c.error().empty() ? "nem készült el" : c.error().c_str());
      return 1;
    }
    std::printf("atlasz: %s (%s, %d lap, %.1f s)\n", c.dir().c_str(), had ? "már kész volt" : "elkészült", c.pages_done(), s);
    return 0;
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::fprintf(stderr, "SDL_Init hiba: %s\n", SDL_GetError());
    return 1;
  }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#if defined(__APPLE__)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
  // M8: a felhasználói beállítások (platformfüggetlen helyen) és a nyelvek (a locale mappák .po fájljai)
  if (o.map_seed != 0 && !o.world.empty()) o.world = regenerate_map(o.world, o.map_seed);
  if (!o.mission.empty() && o.world.empty()) o.world = o.mission;
  if (!o.editor.empty() && o.world.empty()) o.world = o.editor;
  if (!o.chain.empty() && o.world.empty()) o.world = o.chain;   // a világnézet ága (a pályát a küldetés adja)
  const bool menu_mode = o.menu || (o.world.empty() && o.atlas.empty() && o.demo == 0 && !o.bench);
  client::Settings settings;
  std::string settings_file;
  if (char* pref = SDL_GetPrefPath("TOTU", "SkyRift")) {
    settings_file = client::settings_path(pref);
    SDL_free(pref);
    std::string serr;
    if (!client::load_settings(settings_file, &settings, &serr)) std::fprintf(stderr, "%s\n", serr.c_str());
  }
  client::Locale locale;
  std::vector<content::ModInfo> mods_available;   // a modkezelő listája
  std::vector<content::ModInfo> mod_order;   // M13: az alap és az engedélyezett modok betöltési sorrendje
  {
    std::vector<std::filesystem::path> extra;
    std::filesystem::path zip_cache;
    if (char* pref = SDL_GetPrefPath("TOTU", "SkyRift")) {   // a felhasználói modmappa és a ZIP-modok kibontási helye
      extra.push_back(std::filesystem::path(pref) / "mods");
      zip_cache = std::filesystem::path(pref) / "mods_kibontva";
      SDL_free(pref);
    }
    std::vector<std::string> warns;
    const auto avail = content::discover_mods(o.content, extra, &warns, zip_cache);
    for (const auto& m : avail) {
      if (m.id != "totu") mods_available.push_back(m);
    }
    for (const auto& w : warns) std::fprintf(stderr, "mod: %s\n", w.c_str());
    const auto res = content::select_mods(avail, o.mods_given ? o.mods : settings.mods);
    if (!res.errors.empty()) {   // a hibás modkészlet nem töltődik be: az alapjáték indul, a hiba látszik
      for (const auto& e : res.errors) std::fprintf(stderr, "mod: %s\n", e.c_str());
      client::Diagnostics::get().count("LOAD_FAILED");
    } else {
      mod_order = res.order;
    }
    if (mod_order.size() > 1) {
      std::printf("modok:");
      for (const auto& m : mod_order) if (m.id != "totu") std::printf(" %s %s", m.id.c_str(), m.version.c_str());
      std::printf("\n");
    }
  }
  std::vector<std::string> mod_roots;
  for (const auto& m : mod_order) {
    if (m.id != "totu") mod_roots.push_back(m.root.string());
  }
  locale.load(o.content, mod_roots);
  locale.set_lang(settings.language);
  if (menu_mode && !o.size_given) {
    o.width = settings.width;
    o.height = settings.height;
  }
  client::MatchConfig match_cfg;
  bool have_cfg = false, menu_quit = false;
  SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
  if (o.hidden) flags |= SDL_WINDOW_HIDDEN;
  SDL_Window* win = SDL_CreateWindow("TOTU: Sky Rift", o.width, o.height, flags);
  if (!win) {
    std::fprintf(stderr, "Ablakhiba: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  client::Cursors cursors;   // a festett kurzorok (a régi `cursor_kind`)
  cursors.load(o.assets, 32 * std::clamp(settings.ui_scale, 100, 200) / 100);
  cursors.set("arrow");
  SDL_GLContext ctx = SDL_GL_CreateContext(win);
  if (!ctx) {
    std::fprintf(stderr, "OpenGL 3.3 core környezet nem jött létre: %s\n", SDL_GetError());
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 1;
  }
  SDL_GL_SetSwapInterval(o.bench ? 0 : 1);

  int rc = 0;
  bool game_recorded = false;   // a profil statisztikája egyszer rögzítve
  client::Audio audio;          // zene és hangok (a régi rts/audio.py)
  render::Image save_thumb;     // a mentés kis képe: a szünet első, még tiszta képkockájáról (320 × 180)
  bool want_thumb = false;
  std::vector<sim::EntityId> snd_sel;   // a legutóbbi kijelölés (a kijelölés hangjához)
  std::string restart_load;   // a szünetmenü „Betöltés” pontja: a játék újraindul ezzel a mentéssel
  {
    render::Device dev;
    std::string err;
    if (!dev.init(gl_loader, &err)) {
      std::fprintf(stderr, "%s\n", err.c_str());
      rc = 1;
    }
    render::SpriteBatch batch;
    Demo demo;
    demo.units = (o.atlas.empty() && o.world.empty()) ? o.demo : 0;
    AtlasDemo adem;
    // a világ atlasza (a sprite-ok): `--atlas` nélkül a felhasználói mappából; ha még nincs vagy elavult, a játék
    // háttérben elkészíti (csak interaktív indításkor – képpróbában, mérésben a kész atlaszt használja, ha van);
    // `--atlas -`: atlasz nélkül (tartalékalakok)
    client::AtlasCache atlas_cache;
    if (o.atlas.empty()) {
      if (char* pref = SDL_GetPrefPath("TOTU", "SkyRift")) {
        const bool interactive = o.screenshot.empty() && !o.bench && o.frames < 0 && !o.hidden;
        atlas_cache.start(o.assets, pref, interactive, settings.calm);
        SDL_free(pref);
      }
    } else if (o.atlas == "-") {
      o.atlas.clear();
    }
    client::Session session;
    session.set_mods(mod_order);
    net::RelayServer host_relay;   // M10: a gazda reléje (a SimRunner előtt jön létre, utána szűnik meg)
    client::NetPlay netplay;
    std::unique_ptr<sim::MatchInstance> mission_match;   // M11: a küldetés meccse (a munkamenet csak hivatkozik rá)
    std::string mission_cam;                            // az utolsó követett kameraparancs
    std::optional<sim::ChainDef> chain_def;           // M11: küldetéssor
    sim::ChainState chain_state;
    std::string chain_profile;
    bool chain_over = false;
    bool chain_pick = false;   // M12: a küldetésválasztó látszik (a játékos választ a megnyílt küldetések közül)
    // a térkép egy csomópontjának téglalapja (képernyő-koordináta)
    auto chain_node = [](const sim::ChainMission& m, int w, int h) {
      const float pw = std::min(static_cast<float>(w) - 80.0f, 900.0f), ph = std::min(static_cast<float>(h) - 140.0f, 560.0f);
      const float px = (static_cast<float>(w) - pw) / 2, py = (static_cast<float>(h) - ph) / 2;
      return std::array<float, 4>{px + m.mx * pw - 100.0f, py + m.my * ph - 24.0f, 200.0f, 48.0f};
    };
    // küldetés indulásakor a kamera a helyi játékos első egységére (az átvezető kamerája később felülírja)
    auto center_on_local = [&](client::Camera& c, int vw, int vh) {
      sim::Sim* sp = mission_match && mission_match->sim ? mission_match->sim.get()
                     : session.has_starts() && session.has_sim() ? &session.sim() : nullptr;
      if (!sp || session.local_player() == 0) return;   // a megfigyelő (0) marad középen
      sim::Sim& s = *sp;
      sim::EntityId best = sim::kNoEntity;
      s.world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building&) {   // a régi: a bázis (start_xy)
        const sim::Health* h = s.world().get<sim::Health>(e);
        if (best == sim::kNoEntity && h && h->owner == session.local_player()) best = e;
      });
      if (best == sim::kNoEntity) {
        s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit&) {
          const sim::Health* h = s.world().get<sim::Health>(e);
          if (best == sim::kNoEntity && h && h->owner == session.local_player()) best = e;
        });
      }
      if (const sim::Position* p = best != sim::kNoEntity ? s.world().get<sim::Position>(best) : nullptr) {
        c.x = static_cast<float>(p->x.to_double()) * client::World::kTilePx - static_cast<float>(vw) / 2 / c.zoom;
        c.y = static_cast<float>(p->y.to_double()) * client::World::kTilePx - static_cast<float>(vh) / 2 / c.zoom;
      }
    };
    client::EditorDoc edoc;                             // M11: a szerkesztő dokumentuma és felülete
    client::EditorUi eui;
    client::TriggerEditor tedit;   // F4
    client::DataEditor dedit;      // F5
    bool editor_on = false, editor_test = false;
    int editor_built = -1;
    std::vector<std::string> editor_problems;
    float pan_x0 = 0, pan_y0 = 0, pan_x1 = 0, pan_y1 = 0, pan_t = 1, pan_len = 0;
    client::SimRunner runner(session);
    client::World world(session, runner);
    {
      const auto& tc = client::team_colors(settings.colorblind);   // M8: színvak-barát csapatszínek
      std::array<std::array<uint8_t, 4>, 8> teams{};
      for (size_t i = 0; i < 8; ++i) teams[i] = tc[i];
      world.set_team_colors(teams);
    }
    render::TerrainView terrain;
    render::Atlas world_atlas;
    render::Texture banner_pole, banner_knob, banner_cloth, banner_trim, carry_sack, mark_hero_tint, mark_hero_fixed, mark_badge_frame,
        mark_badge_symbol, mark_chant, mark_hidden, obj_ring, obj_beacon, amb_glow, amb_leaf, fire_flame, fire_puff, found_tex;
    std::vector<render::Texture> unit_shapes;   // a kép nélküli egységek alakzatai (alakzatonként irány × réteg)
    std::map<std::string, render::Texture> item_tex;
    render::Texture ground_tex, module_tex;   // a terep fedőrétege; a népmechanikák fedőképe (fertőzött talaj, tavak)
    render::Texture fallback, white, sigil_own, sigil_other, sigil_ally, move_sigil, rally_arrows, rally_sigil, rally_pole, rally_cloth, rally_shadow;
    client::Hud hud;
    client::Camera cam;
    render::Lighting lighting;
    std::vector<render::Light> lights;
    render::Effects effects;
    render::ParticleSystem particles;
    uint32_t fx_rng = 0x9e3779b9u;   // a látvány saját véletlenje (eső cseppjei, parázs)
    auto fx_rand = [&fx_rng]() {
      fx_rng ^= fx_rng << 13;
      fx_rng ^= fx_rng >> 17;
      fx_rng ^= fx_rng << 5;
      return static_cast<float>(fx_rng >> 8) / 16777216.0f;
    };
    // időjárás: a jelenlegi és a cél, fokozatos átmenettel (V billentyű)
    render::Weather weather_from = o.weather, weather_to = o.weather;
    double weather_since = -100.0;
    constexpr double kWeatherBlend = 4.0;   // másodperc
    double prev_t = 0;
    bool have_world = false, world_atlas_ok = false;
    float demo_hover_x = -1, demo_hover_y = -1;   // képpróba: a kurzor helye (világ-képpont)
    sim::Formation formation = sim::Formation::Auto;
    float drag_x = 0, drag_y = 0;
    bool dragging = false;
    bool attack_move = false;
    bool patrol_mode = false;                                // D46: P + bal gomb: járőr
    std::array<std::vector<sim::EntityId>, 10> ctrl_groups;  // D46: kontrollcsoportok (Ctrl+szám)
    int last_group = -1;
    double last_group_t = 0;
    bool line_drag = false;                                  // D46: jobb gombos húzás: vonalra mozgatás
    float line_x0 = 0, line_y0 = 0;
    std::vector<double> sim_times, draw_times;   // a mérésben külön: szimulációs tick és rajzolás
    if (rc == 0 && o.screenshot.empty() && !o.bench && !o.hidden && o.frames < 0) {   // a képpróbák némák
      if (!audio.init(o.assets)) std::printf("hang: nincs hangeszköz (néma)\n");
    }
    if (rc == 0) {
      std::printf("grafika: %s (legnagyobb textúra %d)\n", dev.info().c_str(), dev.max_texture_size());
      if (!batch.init(dev, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        rc = 1;
      } else if (menu_mode && !(have_cfg = [&] {   // M8: a főmenü; a meccs a választott beállítással indul
                   if (settings.fullscreen && !o.size_given) client::apply_display(win, settings);
                   client::MenuRunOptions mo;
                   mo.assets = o.assets;
                   mo.content = o.content;
                   mo.maps_dir = o.maps;
                   mo.settings_file = settings_file;
                   mo.frames = o.frames;
                   mo.screenshot = o.screenshot;
                   mo.start = o.menu_screen;
                   mo.mods = mod_order;
                   mo.available_mods = mods_available;
                   {   // a modlock (a LAN-keresés így jelöli a más tartalmú játékot)
                     std::vector<std::string> hd = {(std::filesystem::path(o.content) / "schema").string()};
                     if (mod_order.empty()) hd.push_back((std::filesystem::path(o.content) / "base").string());
                     for (const auto& m : mod_order) hd.push_back(m.root.string());
                     mo.content_hash = net::content_hash(hd);
                   }
                   mo.status = [&]() -> std::string {
                     if (atlas_cache.building()) return locale.tr("ui.atlas.building", {{"n", std::to_string(atlas_cache.pages_done())}});
                     if (!atlas_cache.ready() && !atlas_cache.error().empty()) return locale.tr("ui.atlas.failed");
                     return {};
                   };
                   if (audio.ok()) audio.menu_music();   // a régi: a menükben a menüzene
                   mo.tick = [&] { audio.update(settings, static_cast<double>(SDL_GetTicks()) / 1000.0); };
                   if (!client::run_menu(win, dev, batch, settings, locale, mo, &match_cfg, &rc)) return false;
                   o.world = match_cfg.map_path;
                   if (match_cfg.map_seed != 0) o.world = regenerate_map(o.world, match_cfg.map_seed);
                   if (!match_cfg.load_path.empty()) o.world = o.load = match_cfg.load_path;   // mentett játék
                   if (match_cfg.net_host || !match_cfg.join_address.empty()) {   // többjátékos (a régi `LanMenu`)
                     o.name = match_cfg.net_name;
                     o.faction = match_cfg.net_faction;
                     if (match_cfg.net_host) {
                       o.host_port = 47800;
                       o.seats = match_cfg.net_seats;
                     } else {
                       o.join = match_cfg.join_address;
                       o.world = match_cfg.join_address;   // a hálózati ág feltétele (a pályát a relé küldi)
                     }
                   }
                   return true;
                 }())) {
        menu_quit = true;
      } else if (demo.units > 0 && !demo.load(dev, o.assets, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        rc = 1;
      } else if (!o.world.empty()) {
        const bool net_game = o.host_port > 0 || !o.join.empty();
        // M10: a hálózati meccs a lobbin át (konzolon; a menübeli lobbi később), utána a munkamenet átveszi
        auto start_net = [&]() -> bool {
          if (!session.load_content(o.content, &err)) return false;
          // modlock: a séma, az alap és az engedélyezett modok (a sorrendjükben) – az eltérő modkészlet nem csatlakozhat
          std::vector<std::string> hash_dirs = {(std::filesystem::path(o.content) / "schema").string()};
          if (mod_order.empty()) hash_dirs.push_back((std::filesystem::path(o.content) / "base").string());
          for (const auto& m : mod_order) hash_dirs.push_back(m.root.string());
          const uint64_t ch = net::content_hash(hash_dirs);
          std::string host = "127.0.0.1";
          int port = o.host_port;
          if (o.host_port > 0) {
            sim::Replay desc;
            desc.seed = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
            std::ifstream in(o.world, std::ios::binary);
            desc.map.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            desc.factions.assign(static_cast<size_t>(std::clamp(o.seats, 1, 8)), o.faction);
            if (!host_relay.start(desc, session.terrain_ptr(), session.content_rules(), ch, std::clamp(o.seats, 1, 8),
                                  static_cast<uint16_t>(o.host_port), &err)) {
              return false;
            }
            host_relay.announce(o.name);   // a helyi hálózati keresés válasza
            host_relay.spawn();
            std::printf("gazda: a relé a %d. porton, %d hely\n", o.host_port, o.seats);
          } else {
            const size_t c = o.join.rfind(':');
            host = c == std::string::npos ? o.join : o.join.substr(0, c);
            port = c == std::string::npos ? 47800 : static_cast<int>(std::strtol(o.join.c_str() + c + 1, nullptr, 10));
          }
          if (char* pref = SDL_GetPrefPath("TOTU", "SkyRift")) {   // szinkronhibánál ide kerül a mentés és a visszajátszás
            const std::string dump = (std::filesystem::path(pref) / "szinkronhiba").string();
            SDL_free(pref);
            host_relay.set_dump_dir(dump);
            netplay.set_dump_dir(dump);
          }
          if (!netplay.connect(host, static_cast<uint16_t>(port), session.terrain_ptr(), session.content_rules(), ch, o.name,
                               o.observe, static_cast<uint8_t>(std::clamp(o.rejoin, 0, 8)), &err)) {
            return false;
          }
          bool asked = false;
          std::vector<net::LobbySeat> shown;
          const auto t0 = std::chrono::steady_clock::now();
          const auto lobby_factions = client::scan_factions(o.content);   // a népnevek a lobbihoz
          std::string my_faction = o.faction, typing;
          bool chatting = false, my_ready = true;
          while (!netplay.client().ready()) {
            netplay.pump();
            net::Client& nc = netplay.client();
            if (nc.rejected()) { err = "a relé elutasította: " + nc.reject_reason(); return false; }
            if (!nc.error().empty()) { err = nc.error(); return false; }
            if (netplay.lost()) { err = "megszakadt a kapcsolat a relével"; return false; }
            if (nc.joined() && !asked && nc.slot() > 0) {
              netplay.pick(o.faction);
              netplay.set_ready(true);
              asked = true;
            }
            if (nc.lobby() != shown) {
              shown = nc.lobby();
              std::printf("lobbi:");
              for (size_t i = 0; i < shown.size(); ++i) {
                std::printf(" [%zu: %s %s%s]", i + 1, shown[i].taken ? shown[i].name.c_str() : "-", shown[i].faction.c_str(),
                            shown[i].ready ? " kész" : "");
              }
              std::printf("\n");
            }
            {   // a lobbi a képernyőn (a régi `SetupScreen` lobbija)
              std::vector<client::LobbyLine> lines;
              for (size_t i = 0; i < nc.lobby().size(); ++i) {
                const auto& ls = nc.lobby()[i];
                std::string fname = ls.faction;
                for (const auto& fi : lobby_factions) {
                  if (fi.id == ls.faction) fname = locale.name(fi.name_key);
                }
                lines.push_back({ls.name, fname, ls.taken, ls.ready, nc.slot() == i + 1});
              }
              std::vector<std::string> chat;
              for (size_t i = nc.chats().size() > 5 ? nc.chats().size() - 5 : 0; i < nc.chats().size(); ++i) {
                const auto& c = nc.chats()[i];
                const std::string who = c.from >= 1 && c.from <= nc.lobby().size() ? nc.lobby()[c.from - 1u].name : std::string("?");
                chat.push_back(who + ": " + c.text);
              }
              const double lt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
              client::lobby_frame(win, dev, batch, locale, o.assets, static_cast<float>(std::clamp(settings.ui_scale, 100, 200)) / 100.0f, lines, chat,
                                  locale.tr(nc.joined() ? "ui.lobby.waiting_players" : "ui.lobby.connecting"), lt, chatting ? &typing : nullptr);
            }
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
              if (e.type == SDL_EVENT_QUIT) { err = "kilépés a lobbiból"; return false; }
              if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
                if (chatting) {   // az üzenetírás megszakítása
                  chatting = false;
                  typing.clear();
                  SDL_StopTextInput(win);
                  continue;
                }
                err = "kilépés a lobbiból";
                return false;
              }
              // a régi lobbi: ← → a nép, Szóköz kész / nem kész, Enter üzenet (írás, küldés)
              if (e.type == SDL_EVENT_TEXT_INPUT && chatting && typing.size() < 120) typing += e.text.text;
              if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat || !nc.joined() || nc.slot() == 0) continue;
              if (chatting) {
                if (e.key.key == SDLK_BACKSPACE && !typing.empty()) {
                  while (!typing.empty() && (static_cast<unsigned char>(typing.back()) & 0xC0) == 0x80) typing.pop_back();
                  if (!typing.empty()) typing.pop_back();
                } else if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) {
                  if (!typing.empty()) netplay.chat(typing);
                  typing.clear();
                  chatting = false;
                  SDL_StopTextInput(win);
                }
                continue;
              }
              if ((e.key.key == SDLK_LEFT || e.key.key == SDLK_RIGHT) && !lobby_factions.empty()) {
                size_t fi = 0;
                for (size_t k2 = 0; k2 < lobby_factions.size(); ++k2) {
                  if (lobby_factions[k2].id == my_faction) fi = k2;
                }
                fi = (fi + (e.key.key == SDLK_RIGHT ? 1 : lobby_factions.size() - 1)) % lobby_factions.size();
                my_faction = lobby_factions[fi].id;
                netplay.pick(my_faction);
                if (my_ready) {
                  my_ready = false;
                  netplay.set_ready(false);
                }
              } else if (e.key.key == SDLK_SPACE) {
                my_ready = !my_ready;
                netplay.set_ready(my_ready);
              } else if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) {
                chatting = true;
                SDL_StartTextInput(win);
              }
            }
            if (std::chrono::steady_clock::now() - t0 > std::chrono::minutes(10)) { err = "a meccs nem indult el"; return false; }
            SDL_Delay(10);
          }
          session.adopt(*netplay.client().match());
          session.set_local_player(netplay.client().slot() > 0 ? netplay.client().slot() : 1);
          runner.set_source(&netplay);
          std::printf("a meccs indul: %d. hely\n", netplay.client().slot());
          return true;
        };
        auto start_mission = [&]() -> bool {   // M11: a küldetéscsomagból (mission.json + pálya + szkript)
          if (!session.load_content(o.content, &err)) return false;
          std::vector<std::string> mod_maps;   // M13: a modok pályamappái is
          for (const auto& m : mod_order) {
            if (m.id != "totu") mod_maps.push_back((m.root / "maps").string());
          }
          auto desc = sim::load_mission_package(o.mission, o.maps, 20261116, &err, mod_maps);
          if (!desc) return false;
          mission_match = sim::start_match(*desc, session.terrain_ptr(), session.content_rules(), err);
          if (!mission_match) return false;
          session.adopt(*mission_match);
          session.set_match_desc(*desc);   // a küldetés is menthető
          return true;
        };
        auto start_load = [&]() -> bool {   // mentett játék: a leírásból a kezdés nélkül, majd az állapot
          if (!session.load_content(o.content, &err)) return false;
          auto f = client::read_save(o.load);
          if (!f) {
            err = "a mentés nem olvasható: " + o.load;
            return false;
          }
          mission_match = sim::start_match(f->desc, session.terrain_ptr(), session.content_rules(), err, true);
          if (!mission_match) return false;
          if (!mission_match->sim->load(f->state)) {
            err = "a mentés nem tölthető be (eltérő tartalom vagy sérült fájl): " + o.load;
            return false;
          }
          if (mission_match->map) mission_match->sim->restore_obstacles(*mission_match->map);   // fák, épületek a rácson
          session.adopt(*mission_match);
          session.set_match_desc(f->desc);
          if (f->desc.fog == 2) session.set_reveal_all(true);
          if (const auto mv = content::parse(f->meta)) {   // a mentett színek
            if (const auto* cv = mv->find("colors"); cv && cv->is_array() && cv->items().size() == 8) {
              std::array<uint8_t, 9> slots{0, 0, 1, 2, 3, 4, 5, 6, 7};
              for (size_t i = 0; i < 8; ++i) slots[i + 1] = static_cast<uint8_t>(cv->items()[i].as_int().value_or(static_cast<long long>(i)) & 7);
              world.set_color_slots(slots);
              hud.model().color_slot = slots;
            }
          }
          return true;
        };
        auto start_editor = [&]() -> bool {   // M11: a szerkesztő (WC3 World Editor mintájára)
          if (!session.load_content(o.content, &err) || !edoc.open(o.editor, o.maps, &err)) return false;
          eui.init(*session.terrain_ptr(), *session.rules());
          tedit.init(session.rules());
          dedit.init(session.rules());
          if (o.data_editor >= 0) {
            dedit.open = true;
            dedit.tab = std::clamp(o.data_editor, 0, 4);
            dedit.sel1 = 0;
            dedit.sel2 = dedit.tab >= 2 ? 1 : -1;
          }
          tedit.open = o.trigger_editor;
          if (tedit.open) tedit.trigger = 0;
          mission_match = sim::start_match(edoc.replay(1), session.terrain_ptr(), session.content_rules(), err);
          if (!mission_match) return false;
          session.adopt(*mission_match);
          editor_on = true;
          editor_built = 0;
          editor_problems = edoc.validate(session.content_rules(), session.terrain_ptr());
          return true;
        };
        auto start_chain = [&]() -> bool {   // M11: a küldetéssor (a profil folytatása, ha van)
          if (!session.load_content(o.content, &err)) return false;
          std::ifstream cin_(std::filesystem::path(o.chain) / "chain.json", std::ios::binary);
          const std::string ctext{std::istreambuf_iterator<char>(cin_), std::istreambuf_iterator<char>()};
          std::vector<std::string> cerrs;
          chain_def = sim::ChainDef::parse(ctext, cerrs);
          if (!chain_def) {
            err = cerrs.empty() ? "hibás küldetéssor" : cerrs.front();
            return false;
          }
          chain_state = sim::ChainState::fresh(*chain_def);
          if (char* pref = SDL_GetPrefPath("TOTU", "SkyRift")) {
            chain_profile = (std::filesystem::path(pref) / ("kuldetessor_" + chain_def->id + ".json")).string();
            SDL_free(pref);
            std::ifstream pin(chain_profile, std::ios::binary);
            const std::string ptext{std::istreambuf_iterator<char>(pin), std::istreambuf_iterator<char>()};
            if (const auto pv = content::parse(ptext)) {
              if (auto ps = sim::ChainState::from_json(*pv); ps && ps->chain == chain_def->id) {
                const auto av = sim::available_missions(*chain_def, *ps);
                if (!ps->current.empty()) {
                  chain_state = *ps;
                } else if (!av.empty()) {   // választás közben mentett profil: a térkép, mögötte az első megnyílt küldetés
                  chain_state = *ps;
                  chain_state.current = av.front();
                  chain_pick = true;
                }
              }
            }
          }
          auto desc = sim::prepare_mission(*chain_def, chain_state, o.chain, o.maps, 20261116, &err);
          if (!desc) return false;
          mission_match = sim::start_match(*desc, session.terrain_ptr(), session.content_rules(), err);
          if (!mission_match) return false;
          session.adopt(*mission_match);
          return true;
        };
        const bool loaded = net_game ? start_net() : !o.load.empty() ? start_load() : !o.chain.empty() ? start_chain() : !o.editor.empty() ? start_editor()
                            : !o.mission.empty() ? start_mission() : session.load(o.content, o.world, 20261112, &err);
        if (!loaded || !terrain.init(dev, o.assets, &err)) {
          std::fprintf(stderr, "%s\n", err.c_str());
          rc = 1;
        } else {
          {   // a régi „Táj”: a téli / erdei csempekészlet (a Csata beállításából vagy a mentett meccsből)
            const std::string th = have_cfg ? match_cfg.setup.climate : session.match_desc() ? session.match_desc()->climate : std::string();
            terrain.set_theme(th == "winter" ? "winter" : th == "forest" ? "forest" : std::string());
          }
          terrain.set_map(world.map(), world.terrain());
          if (!lighting.init(dev, &err)) std::fprintf(stderr, "%s\n", err.c_str());
          if (!effects.init(dev, &err)) std::fprintf(stderr, "%s\n", err.c_str());
          // meccs (M6), ha a pályán vannak kezdőhelyek; különben a bemutató seregei
          if (editor_on) {
            session.set_reveal_all(true);   // a szerkesztőben a köd nem takar
          } else if (net_game) {
            if (netplay.client().slot() == 0) session.set_reveal_all(true);   // a megfigyelő mindent lát
          } else if (mission_match) {
            // a küldetés már elindult (a start_match)
          } else if (session.has_starts()) {
            const bool started = have_cfg ? session.start_match(match_cfg.setup, match_cfg.ai, "ai_nep.lua", &err)
                                          : session.start_match({o.faction.empty() ? std::string("ursan") : o.faction, "ursan"},
                                                                true, &err);
            if (started && have_cfg && !match_cfg.colors.empty()) {   // a választott színek (a Csata „Szín” oszlopa)
              std::array<uint8_t, 9> slots{0, 0, 1, 2, 3, 4, 5, 6, 7};
              for (size_t i = 0; i < match_cfg.colors.size() && i < 8; ++i) slots[i + 1] = static_cast<uint8_t>(match_cfg.colors[i] & 7);
              world.set_color_slots(slots);
              hud.model().color_slot = slots;
            }
            if (!started) {   // N-04: a sikertelen indítás nem fut tovább részben felépített meccsként
              std::fprintf(stderr, "meccs: %s\n", err.c_str());
              rc = 1;
            }
          } else {
            session.spawn_demo(2, o.demo > 0 ? o.demo : 60);
          }
          if (!editor_on && (!net_game || netplay.client().slot() > 0)) session.set_reveal_all(!o.fog);
          if (o.battle_demo > 0 && session.has_starts()) session.battle_demo(o.battle_demo);
          if (o.atlas.empty() && atlas_cache.ready()) o.atlas = atlas_cache.dir();   // a játék saját atlasza
          if (o.atlas.empty() && atlas_cache.building()) std::printf("az atlasz még készül: ez a meccs sprite-ok nélkül indul\n");
          if (!o.atlas.empty()) world_atlas_ok = world_atlas.load(dev, o.atlas, &err) && world.prepare_atlas(world_atlas, &err);
          if (!o.atlas.empty() && !world_atlas_ok) std::fprintf(stderr, "atlasz nélkül: %s\n", err.c_str());
          fallback = dev.create_array_from(circle_image(32), false);
          {   // a kijelölés pecsétjei (a régi színekkel: saját zöld, idegen piros)
            auto own = client::sigil_image({90, 255, 110}, 48.0f, false);
            auto other = client::sigil_image({255, 90, 80}, 48.0f, false);
            render::premultiply(own);
            render::premultiply(other);
            sigil_own = dev.create_array_from(own, false);
            sigil_other = dev.create_array_from(other, false);
            auto ally = client::sigil_image({120, 200, 255}, 48.0f, false);   // csapattárs: kék (a régi szerint)
            render::premultiply(ally);
            sigil_ally = dev.create_array_from(ally, false);
            world.set_sigils(&sigil_own, &sigil_other, 48.0f, &sigil_ally);
            // a gyülekezőpont képei (a régi szerint): 16 irányú nyíl, kis pecsét, rúd, 8 fázisú zászló és árnyék
            auto layers = [&](int n, int lw, int lh, auto make) {
              render::Texture t = dev.create_array(lw, lh, n, false);
              for (int i = 0; i < n; ++i) {
                render::Image im = make(i);
                render::premultiply(im);
                dev.upload_layer(t, i, im);
              }
              dev.finish_array(t);
              return t;
            };
            rally_arrows = layers(16, 16, 16, [](int i) { return client::rally_arrow_image(static_cast<float>(i) * 6.2831853f / 16.0f); });
            rally_cloth = layers(8, 22, 32, [](int i) { return client::rally_cloth_image(i); });
            rally_shadow = layers(8, 40, 20, [](int i) { return client::rally_shadow_image(i); });
            {
              auto pole = client::rally_pole_image();
              render::premultiply(pole);
              rally_pole = dev.create_array_from(pole, false);
              auto sg = client::sigil_image({255, 255, 255}, 26.0f, false);
              render::premultiply(sg);
              rally_sigil = dev.create_array_from(sg, false);
              auto mv = client::sigil_image({60, 205, 92}, 38.0f, true);
              render::premultiply(mv);
              move_sigil = dev.create_array_from(mv, false);
            }
            world.set_rally_art({&rally_arrows, &rally_sigil, &rally_pole, &rally_cloth, &rally_shadow, &move_sigil});
            {   // D51: a csapatjelző lobogó, népenként saját stílusban (címer nélkül)
              constexpr int kS = client::kBannerStyles;
              banner_pole = layers(kS, 20, 56, [](int i) { return client::banner_pole_image(i); });
              banner_knob = layers(kS, 20, 56, [](int i) { return client::banner_knob_image(i); });
              banner_cloth = layers(kS * 8, 20, 56, [](int i) { return client::banner_cloth_image(i / 8, i % 8); });
              banner_trim = layers(kS * 8, 20, 56, [](int i) { return client::banner_trim_image(i / 8, i % 8); });
              client::World::BannerArt ba;
              ba.pole = &banner_pole;
              ba.knob = &banner_knob;
              ba.cloth = &banner_cloth;
              ba.trim = &banner_trim;
              world.set_banner_art(ba);
              // D49: a lángoló épület és a kéményfüst
              fire_flame = layers(client::kFlameFrames, 16, 24, [](int i) { return client::flame_image(i); });
              {
                auto puff = client::smoke_puff_image();
                render::premultiply(puff);
                fire_puff = dev.create_array_from(puff, false);
              }
              auto sack = client::carry_sack_image();
              render::premultiply(sack);
              carry_sack = dev.create_array_from(sack, false);
              world.set_sack(&carry_sack);
              // talajjelek és a hős jelei (a régi szerint)
              auto one = [&](render::Image im) {
                render::premultiply(im);
                return dev.create_array_from(im, false);
              };
              mark_hero_tint = one(client::hero_ground_tinted(48.0f));
              mark_hero_fixed = one(client::hero_ground_fixed(48.0f));
              mark_badge_frame = one(client::hero_badge_frame());
              mark_badge_symbol = one(client::hero_badge_symbol());
              mark_chant = layers(8, 54, 38, [](int i) { return client::chant_mark_image(40.0f, i); });
              mark_hidden = one(client::hidden_mark_image(40.0f));
              obj_ring = layers(12, 148, 96, [](int i) { return client::objective_ring_image(64.0f, i); });
              obj_beacon = one(client::objective_beacon_image());
              world.set_objective_art(&obj_ring, &obj_beacon);
              amb_glow = dev.create_array_from(client::ambient_glow_image(), false);   // additív: nincs előszorzás
              amb_leaf = layers(8, 8, 8, [](int i) { return client::ambient_leaf_image(i); });
              world.set_ambient_art(&amb_glow, &amb_leaf, &fallback);
              world.set_fire_art({&fire_flame, &fire_puff, &amb_glow});
              // a kép nélküli egységek alakzatai (a régi `draw_shape`): alakzat × irány × réteg
              for (int sh = 0; sh < client::kShapeKinds; ++sh) {
                unit_shapes.push_back(layers(client::kShapeDirs * client::kShapeLayers, client::kShapeSize, client::kShapeSize, [sh](int i) {
                  return client::unit_shape_layer(sh, i / client::kShapeLayers, i % client::kShapeLayers);
                }));
              }
              world.set_shape_art(&unit_shapes);
              if (session.has_sim() && session.rules()) {   // a terep fedőrétege (a régi földutak és talajfoltok)
                const auto g0 = std::chrono::steady_clock::now();
                render::Image gi = client::ground_overlay(world.map(), session.terrain(), session.seed());
                if (gi.width <= dev.max_texture_size() && gi.height <= dev.max_texture_size()) {
                  render::premultiply(gi);
                  ground_tex = dev.create_array_from(gi, false);
                  world.set_ground_overlay(&ground_tex);
                }
                std::printf("terep-fedőréteg: %d × %d, %.0f ms\n", gi.width, gi.height,
                            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - g0).count());
              }
              if (auto fp = render::load_png((std::filesystem::path(o.assets) / "ui" / "foundation.png").string())) {   // a lapkő
                found_tex = one(*fp);
                world.set_foundation_art(&found_tex);
              }
              {   // a földön heverő tárgyak ikonjai (assets/items, 32 × 32-re kicsinyítve) és a kör alakú fényudvar
                std::map<std::string, const render::Texture*> icons;
                std::error_code iec;
                for (const auto& e : std::filesystem::directory_iterator(std::filesystem::path(o.assets) / "items", iec)) {
                  const std::string nm = e.path().filename().string();
                  if (e.path().extension() != ".png" || nm.find("_old") != std::string::npos) continue;
                  auto png = render::load_png(e.path().string());
                  if (!png) continue;
                  render::Image small;
                  small.width = small.height = 32;
                  small.rgba.assign(32 * 32 * 4, 0);
                  for (int y = 0; y < 32; ++y) {
                    for (int x = 0; x < 32; ++x) {
                      const uint8_t* p = png->at(std::min(png->width - 1, x * png->width / 32 + png->width / 64),
                                                 std::min(png->height - 1, y * png->height / 32 + png->height / 64));
                      std::copy(p, p + 4, small.at(x, y));
                    }
                  }
                  item_tex[e.path().stem().string()] = one(small);
                }
                for (auto& [k, t] : item_tex) icons[k] = &t;
                world.set_item_art(std::move(icons), &fallback);
              }
              world.set_mark_art({&mark_hero_tint, &mark_hero_fixed, &mark_badge_frame, &mark_badge_symbol, &mark_chant, &mark_hidden});
            }
          }
          render::Image px;
          px.width = px.height = 1;
          px.rgba = {255, 255, 255, 255};
          white = dev.create_array_from(px, false);
          if ((session.has_starts() || editor_on) && !hud.init(dev, o.assets, &err)) std::fprintf(stderr, "HUD: %s\n", err.c_str());
          hud.model().player = session.local_player();
          const float mw = static_cast<float>(world.map().width()) * client::World::kTilePx;
          const float mh = static_cast<float>(world.map().height()) * client::World::kTilePx;
          cam.zoom = std::clamp(static_cast<float>(settings.view_zoom) / 100.0f, 1.0f, 2.0f);   // a régi „Kamera-távolság”
          if (o.zoom > 0) cam.zoom = std::clamp(o.zoom, 1.0f, 2.0f);
          cam.x = std::max(0.0f, mw / 2 - static_cast<float>(o.width) / 2 / cam.zoom);
          cam.y = std::max(0.0f, mh / 2 - static_cast<float>(o.height) / 2 / cam.zoom);
          if (!editor_on) center_on_local(cam, o.width, o.height);   // AS-02: csatában is a saját bázis látszik
          if (o.look_x >= 0 && o.look_y >= 0) {
            cam.x = o.look_x * client::World::kTilePx - static_cast<float>(o.width) / 2 / cam.zoom;
            cam.y = o.look_y * client::World::kTilePx - static_cast<float>(o.height) / 2 / cam.zoom;
          }
          if ((o.bench || !o.screenshot.empty()) && !session.has_starts()) session.order_demo_attack();
          have_world = true;
          if (audio.ok() && !editor_on) {   // a csata zenéje (küldetésnél a saját zenéi előbb)
            audio.game_music(o.mission.empty() ? std::string() : std::filesystem::path(o.mission).stem().string());
          }
          if (o.build_demo && session.has_starts() && session.rules()) {
            world.update();
            const auto [sx, sy] = session.map().objects().starts.front();
            world.select_box(static_cast<float>(sx - 6) * client::World::kTilePx, static_cast<float>(sy - 6) * client::World::kTilePx,
                             static_cast<float>(sx + 6) * client::World::kTilePx, static_cast<float>(sy + 6) * client::World::kTilePx);
            world.order_build(session.rules()->def("urs_tanya"), sx + 4, sy - 5);
          }
          if (o.garrison_demo && session.has_starts() && session.rules()) {   // D39: 2 a 4 helyből foglalt
            const auto [sx, sy] = session.map().objects().starts.front();
            sim::Sim& gs = session.sim();
            const sim::Rules& gr = *session.rules();
            const sim::EntityId house = gs.place_building(gr.def("urs_hosszuhaz"), 1, sx + 5, sy, true);
            std::vector<sim::EntityId> gu;
            for (int k = 0; k < 2; ++k) {
              gu.push_back(gs.spawn_unit(gr.def("urs_pajzshordozo"), 1, core::Fixed::from_int(sx + 4),
                                         core::Fixed::from_ratio((sy + k) * 10 + 5, 10)));
            }
            gs.order_garrison(gu, house);
            demo_hover_x = static_cast<float>(sx + 6) * client::World::kTilePx;
            demo_hover_y = static_cast<float>(sy + 1) * client::World::kTilePx;
          }
          // képernyőképnél a szimuláció szinkron (a kép képkockára megismételhető); különben külön szálon
          if (!net_game) runner.set_speed_pct(have_cfg && match_cfg.speed_pct > 0 ? match_cfg.speed_pct : settings.game_speed);   // a régi „Sebesség”
          if (!o.sync && o.screenshot.empty() && !editor_on) runner.start();   // a szerkesztő előnézete áll
        }
      } else if (!o.atlas.empty() && o.demo > 0 && !adem.load(dev, o.atlas, o.demo, o.width, o.height, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        rc = 1;
      }
    }
    std::vector<double> times;
    bool running = rc == 0 && !menu_quit;
    bool game_paused = false;   // a játékon belüli szünetmenü (Esc, fogaskerék)
    size_t diag_desyncs = 0;    // a már beszámolt szinkronhibák
    // a diagnosztikai jelentés (DIAGNOSTICS_DESIGN; csak helyi fájl): a meccs adatai a pillanatképből
    auto save_diag = [&](client::HudModel& hm) {
      auto& dg = client::Diagnostics::get();
      if (o.host_port > 0 || !o.join.empty()) {
        for (size_t n = netplay.client().desyncs().size(); diag_desyncs < n; ++diag_desyncs) dg.count("NET_DESYNC");
      }
      client::DiagSession ds;
      ds.mode = (o.host_port > 0 || !o.join.empty()) ? "net" : !o.chain.empty() ? "chain" : !o.mission.empty() ? "mission" : "skirmish";
      ds.map_id = std::filesystem::path(o.world).stem().string();
      if (session.has_sim()) ds.seed = static_cast<int64_t>(session.seed() & 0x7fffffffffffffffULL);
      if (const client::Snapshot* sn = hm.snap()) {
        static const char* kVict[] = {"conquest", "capital", "hill", "timed", "script"};
        static const char* kTempo[] = {"classic", "calm", "rush", "survival"};
        ds.players = 0;
        for (int i = 1; i <= sim::Sim::kMaxPlayers; ++i) {
          const auto& p = sn->players[static_cast<size_t>(i)];
          if (p.faction.empty()) continue;
          ++ds.players;
          ds.factions.push_back(p.faction);
        }
        ds.victory = kVict[std::min<int>(sn->mode.victory, 4)];
        ds.tempo = kTempo[std::min<int>(sn->mode.tempo, 3)];
        ds.duration_ticks = sn->tick;
        if (hm.match_over()) {
          const bool won = sn->mission.active ? sn->mission.result == 1 : sn->won(hm.player);
          ds.result = won ? "won" : "lost";
        }
      }
      std::string dir, file, derr;
      if (char* pref = SDL_GetPrefPath("TOTU", "SkyRift")) {
        dir = pref;
        SDL_free(pref);
      }
      if (client::save_diagnostics(dir, dg.to_json(ds, "opengl33"), &file, &derr)) {
        hm.post_notice(hm.tr("diag.saved", {file}), {150, 220, 150});
      } else {
        dg.count("SAVE_FAILED");
        hm.post_notice(hm.tr("diag.failed", {derr}));
      }
    };
    double pause_t = 0;           // a megnyitás ideje (a becsúszáshoz és az elmosáshoz)
    bool pause_was = false;
    float pause_slide = 0;
    std::vector<client::Rect> pause_items;   // a menüpontok (folytatás, kilépés) a kattintáshoz
    auto frame_prev = std::chrono::steady_clock::now();   // AS-01: képkockaidő, FPS-kijelzés, képkocka-korlát
    float frame_dt = 0, fps_acc = 0;
    int fps_frames = 0, fps_shown = -1;
    bool mid_drag = false;   // a középső gombos kamerahúzás (a régi `mid_drag`)
    float mid_mx = 0, mid_my = 0, mid_cx = 0, mid_cy = 0;
    long frame = 0;
    const auto start = std::chrono::steady_clock::now();
    while (running && (o.frames < 0 || frame < o.frames)) {
      SDL_Event ev;
      // M11: a szerkesztő előnézete a dokumentum változása után újraépül (húzás közben nem)
      if (editor_on && !editor_test && !eui.dragging && eui.revision != editor_built) {
        std::string perr;
        auto pm = sim::start_match(edoc.replay(1), session.terrain_ptr(), session.content_rules(), perr);
        editor_problems = edoc.validate(session.content_rules(), session.terrain_ptr());
        if (pm) {
          session.adopt(*pm);
          mission_match = std::move(pm);
          runner.reset_snapshots();
          terrain.set_map(world.map(), world.terrain());
        } else if (editor_problems.empty()) {
          editor_problems.push_back(perr);
        }
        editor_built = eui.revision;
      }
      const bool hud_on = have_world && session.has_starts() && session.rules() && (!editor_on || editor_test);
      if (hud_on) {   // a HUD (M8): a pillanatkép és a kijelölés átvétele a bemenet előtt
        int hw = 0, hh = 0;
        SDL_GetWindowSizeInPixels(win, &hw, &hh);
        hud.sync(world, session, hw, hh);
        hud.model().formation = static_cast<uint8_t>(formation);
        hud.model().shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
        const client::HudMode& hm = hud.model().mode;   // építés / letelepítés: a kísértetkép a kurzornál
        if (hm.kind == client::HudMode::Place || hm.kind == client::HudMode::Unpack) {
          float gx = 0, gy = 0;
          SDL_GetMouseState(&gx, &gy);
          int tx = 0, ty = 0;
          hud.model().place_tile(cam.x + gx / cam.zoom, cam.y + gy / cam.zoom, hm.def, &tx, &ty);
          if (hm.kind == client::HudMode::Place) session.set_place_probe(hm.def, tx, ty);
          world.set_ghost(hm.def, tx, ty, hm.kind != client::HudMode::Place || !hud.model().place_blocked(hm.def, tx, ty, nullptr));
        } else {
          world.set_ghost(sim::kNoDef, 0, 0);
          session.set_place_probe(sim::kNoDef, 0, 0);
        }
      }
      while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT) running = false;
        if (game_paused) {   // szünetmenü: Esc / Enter = folytatás, Q = kilépés; kattintás a menüpontra, a panelen kívül folytatás
          if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat) {
            if (ev.key.key == SDLK_Q) running = false;
            else if (ev.key.key == SDLK_ESCAPE || ev.key.key == SDLK_RETURN) game_paused = false;
          } else if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.button == SDL_BUTTON_LEFT) {
            int hit = -1;
            for (size_t i = 0; i < pause_items.size(); ++i) {
              if (pause_items[i].contains(ev.button.x, ev.button.y)) hit = static_cast<int>(i);
            }
            if (hit == 6) running = false;
            else if (hit == 5) {
              save_diag(hud.model());
              game_paused = false;
            } else if (hit == 2 || hit == 3 || hit == 4) {   // betöltés, beállítások, zene (a régi szünetmenü): a menü képernyője a játék fölött
              client::MenuRunOptions pm;
              pm.assets = o.assets;
              pm.content = o.content;
              pm.maps_dir = o.maps;
              pm.settings_file = settings_file;
              pm.start = hit == 2 ? client::MenuScreen::Load : hit == 3 ? client::MenuScreen::Settings : client::MenuScreen::Music;
              pm.tick = [&] { audio.update(settings, static_cast<double>(SDL_GetTicks()) / 1000.0); };
              pm.tracks = audio.tracks();
              pm.current_track = audio.current();
              pm.play_track = [&](const std::string& rel) { audio.choose(rel); };
              pm.single = true;
              pm.mods = mod_order;
              pm.available_mods = mods_available;
              client::MatchConfig pick;
              int prc = 0;
              runner.set_paused(true);
              if (client::run_menu(win, dev, batch, settings, locale, pm, &pick, &prc) && !pick.load_path.empty()) {
                restart_load = pick.load_path;   // a mentés új folyamatban töltődik be (tiszta állapotból)
                running = false;
              }
              if (prc == client::kMenuRestart) {   // a modkészlet változott
                rc = prc;
                running = false;
              }
            } else if (hit == 1) {   // mentés (a régi `SaveMenu`): a felhasználói adatmappa `mentesek` alá, időbélyeggel
              client::HudModel& hm = hud.model();
              if (!session.match_desc()) {
                hm.post_notice(hm.tr("save_unavailable"));
              } else {
                const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                char stamp[32] = {}, date[32] = {};
                std::strftime(stamp, sizeof stamp, "%Y%m%d_%H%M%S", std::localtime(&now));
                std::strftime(date, sizeof date, "%Y-%m-%d %H:%M", std::localtime(&now));
                const auto path = client::saves_dir() / (std::string(stamp) + ".totumentes");
                std::string mapname = "?";
                if (const auto mv = content::parse(session.match_desc()->map)) {
                  if (const auto* nm = mv->find("name")) {   // szöveg vagy nyelvenként ({"hu": …, "en": …})
                    if (nm->is_string()) mapname = locale.name(nm->text());
                    else if (const auto* ln = nm->is_object() ? nm->find(settings.language) : nullptr; ln && ln->is_string()) mapname = ln->text();
                    else if (const auto* hu = nm->is_object() ? nm->find("hu") : nullptr; hu && hu->is_string()) mapname = hu->text();
                  }
                }
                const int64_t secs = hm.snap() ? hm.snap()->tick / 30 : 0;
                char clock[16] = {};
                std::snprintf(clock, sizeof clock, "%02lld:%02lld", static_cast<long long>(secs / 60), static_cast<long long>(secs % 60));
                content::Value meta = content::Value::object();
                meta.set("title", content::Value::string(mapname + "  " + clock));
                meta.set("date", content::Value::string(date));
                content::Value cols = content::Value::array();   // a választott színek (a betöltéshez)
                for (int pi = 1; pi <= 8; ++pi) cols.push(content::Value::number(std::to_string(world.color_slot(static_cast<uint8_t>(pi)))));
                meta.set("colors", std::move(cols));
                bool ok = false;
                std::string serr;
                runner.with_sim([&] { ok = session.save_game(path.string(), content::dump(meta), &serr); });
                if (ok && save_thumb.width > 0) render::save_png(std::filesystem::path(path).replace_extension(".png").string(), save_thumb);
                if (ok) hm.post_notice(hm.tr("saved", {path.filename().string()}), {150, 220, 150});
                else hm.post_notice(hm.tr("save_failed", {serr}));
              }
              game_paused = false;
            } else if (hit == 0 || pause_items.empty() || ev.button.x > static_cast<float>(pause_items[0].right()) + 40.0f) game_paused = false;
          }
          continue;
        }
        if (editor_on && have_world) {   // M11: a szerkesztő bemenete
          const bool ctrl = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
          if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat && ev.key.key == SDLK_F9) {   // kipróbálás innen / vissza
            editor_test = !editor_test;
            if (editor_test) {
              std::string perr;
              auto pm = sim::start_match(edoc.replay(20261116), session.terrain_ptr(), session.content_rules(), perr);
              if (pm) {
                session.adopt(*pm);
                mission_match = std::move(pm);
                runner.reset_snapshots();
                session.set_reveal_all(false);
                runner.start();
              } else {
                editor_test = false;
                eui.status = perr;
              }
            } else {
              runner.stop();
              session.set_reveal_all(true);
              editor_built = -1;   // a következő képkockában újraépül
            }
            continue;
          }
          if (!editor_test && ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat && (ev.key.key == SDLK_F4 || ev.key.key == SDLK_F5)) {
            const bool f4 = ev.key.key == SDLK_F4;   // F4: triggerek (WC3), F5: célok, változók, párbeszédek, átvezetők
            (f4 ? tedit.open : dedit.open) = !(f4 ? tedit.open : dedit.open);
            (f4 ? dedit.open : tedit.open) = false;
            if (tedit.open || dedit.open) SDL_StartTextInput(win);
            else SDL_StopTextInput(win);
            continue;
          }
          if (!editor_test && dedit.open) {   // az adatszerkesztő elfogyasztja a bemenetet
            int tw = 0, th = 0;
            SDL_GetWindowSizeInPixels(win, &tw, &th);
            dedit.layout(tw, th, edoc);
            if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) dedit.mouse_down(edoc, ev.button.x, ev.button.y, ev.button.button == SDL_BUTTON_RIGHT);
            else if (ev.type == SDL_EVENT_MOUSE_WHEEL) dedit.wheel(ev.wheel.mouse_x, ev.wheel.mouse_y, ev.wheel.y > 0 ? 1 : -1);
            else if (ev.type == SDL_EVENT_TEXT_INPUT) dedit.text_input(ev.text.text);
            else if (ev.type == SDL_EVENT_KEY_DOWN) {
              const bool ctrl2 = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
              if (ctrl2 && ev.key.key == SDLK_Z) edoc.undo();
              else if (ctrl2 && ev.key.key == SDLK_Y) edoc.redo();
              else if (ctrl2 && ev.key.key == SDLK_S) eui.status = edoc.save(&err) ? "editor.saved" : err;
              else {
                const int k = ev.key.key == SDLK_RETURN || ev.key.key == SDLK_KP_ENTER ? client::TriggerEditor::kEnter
                              : ev.key.key == SDLK_ESCAPE    ? client::TriggerEditor::kEscape
                              : ev.key.key == SDLK_BACKSPACE ? client::TriggerEditor::kBackspace : 0;
                if (k) dedit.key(edoc, k);
              }
              if (!dedit.open) SDL_StopTextInput(win);
            }
            eui.revision++;
            if (ev.type != SDL_EVENT_QUIT) continue;
          }
          if (!editor_test && tedit.open) {   // a nyitott ablak elfogyasztja a bemenetet
            int tw = 0, th = 0;
            SDL_GetWindowSizeInPixels(win, &tw, &th);
            tedit.layout(tw, th, edoc);
            if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) tedit.mouse_down(edoc, ev.button.x, ev.button.y, ev.button.button == SDL_BUTTON_RIGHT);
            else if (ev.type == SDL_EVENT_MOUSE_WHEEL) tedit.wheel(ev.wheel.mouse_x, ev.wheel.mouse_y, ev.wheel.y > 0 ? 1 : -1);
            else if (ev.type == SDL_EVENT_TEXT_INPUT) tedit.text_input(ev.text.text);
            else if (ev.type == SDL_EVENT_KEY_DOWN) {
              const bool ctrl2 = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
              if (ctrl2 && ev.key.key == SDLK_Z) edoc.undo();
              else if (ctrl2 && ev.key.key == SDLK_Y) edoc.redo();
              else if (ctrl2 && ev.key.key == SDLK_S) eui.status = edoc.save(&err) ? "editor.saved" : err;
              else {
                const int k = ev.key.key == SDLK_RETURN || ev.key.key == SDLK_KP_ENTER ? client::TriggerEditor::kEnter
                              : ev.key.key == SDLK_ESCAPE    ? client::TriggerEditor::kEscape
                              : ev.key.key == SDLK_BACKSPACE ? client::TriggerEditor::kBackspace
                              : ev.key.key == SDLK_DELETE    ? client::TriggerEditor::kDelete
                              : ev.key.key == SDLK_UP        ? client::TriggerEditor::kUp
                              : ev.key.key == SDLK_DOWN      ? client::TriggerEditor::kDown : 0;
                if (k) tedit.key(edoc, k);
              }
              if (!tedit.open) SDL_StopTextInput(win);
            }
            eui.revision++;   // az előnézet és az ellenőrzés frissül
            if (ev.type != SDL_EVENT_QUIT) continue;
          }
          if (!editor_test) {
            float emx = 0, emy = 0;
            SDL_GetMouseState(&emx, &emy);
            const double ewx = (cam.x + emx / cam.zoom) / client::World::kTilePx, ewy = (cam.y + emy / cam.zoom) / client::World::kTilePx;
            if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat && ctrl && ev.key.key == SDLK_S) {
              eui.status = edoc.save(&err) ? "editor.saved" : err;
              continue;
            }
            if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat) {
              int k = 0;
              if (ev.key.key >= SDLK_A && ev.key.key <= SDLK_Z) k = 'a' + static_cast<int>(ev.key.key - SDLK_A);
              else if (ev.key.key >= SDLK_0 && ev.key.key <= SDLK_9) k = '0' + static_cast<int>(ev.key.key - SDLK_0);
              else if (ev.key.key == SDLK_DELETE) k = 127;
              else if (ev.key.key == SDLK_KP_PLUS || ev.key.key == SDLK_EQUALS) k = '+';
              else if (ev.key.key == SDLK_KP_MINUS || ev.key.key == SDLK_MINUS) k = '-';
              if (k && eui.key(edoc, k, ctrl)) continue;
              if (ev.key.key == SDLK_ESCAPE) {
                running = false;
                continue;
              }
            }
            if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && (ev.button.button == SDL_BUTTON_LEFT || ev.button.button == SDL_BUTTON_RIGHT)) {
              eui.mouse_down(edoc, ev.button.x, ev.button.y, ewx, ewy, ev.button.button == SDL_BUTTON_RIGHT);
              continue;
            }
            if (ev.type == SDL_EVENT_MOUSE_MOTION) eui.mouse_move(edoc, ev.motion.x, ev.motion.y, ewx, ewy);
            if (ev.type == SDL_EVENT_MOUSE_BUTTON_UP) {
              eui.mouse_up(edoc, ewx, ewy);
              continue;
            }
            if (ev.type != SDL_EVENT_MOUSE_WHEEL && ev.type != SDL_EVENT_QUIT) continue;   // a többi a kamerának (görgő)
          }
        }
        if (chain_def && chain_pick && have_world) {   // M12: a küldetésválasztó elnyeli a bemenetet
          if (ev.type != SDL_EVENT_MOUSE_BUTTON_DOWN || ev.button.button != SDL_BUTTON_LEFT) {
            if (ev.type == SDL_EVENT_QUIT) running = false;
            continue;
          }
          int cw = 0, ch = 0;
          SDL_GetWindowSizeInPixels(win, &cw, &ch);
          std::string picked;
          for (const auto& id : sim::available_missions(*chain_def, chain_state)) {
            const auto r = chain_node(*chain_def->mission(id), cw, ch);
            if (ev.button.x >= r[0] && ev.button.x < r[0] + r[2] && ev.button.y >= r[1] && ev.button.y < r[1] + r[3]) picked = id;
          }
          if (picked.empty() || !sim::choose_mission(*chain_def, chain_state, picked)) continue;
          runner.stop();
          if (!chain_profile.empty()) {
            std::ofstream pout(chain_profile, std::ios::binary | std::ios::trunc);
            pout << content::dump_pretty(chain_state.to_json());
          }
          chain_pick = false;
          if (auto desc = sim::prepare_mission(*chain_def, chain_state, o.chain, o.maps, 20261116, &err)) {
            if (auto nm = sim::start_match(*desc, session.terrain_ptr(), session.content_rules(), err)) {
              session.adopt(*nm);
              mission_match = std::move(nm);
              runner.reset_snapshots();
              terrain.set_map(world.map(), world.terrain());
              mission_cam.clear();
              center_on_local(cam, cw, ch);
            }
          }
          runner.start();
          continue;
        }
        if (chain_def && have_world && ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat &&
            (ev.key.key == SDLK_RETURN || ev.key.key == SDLK_KP_ENTER) && mission_match && mission_match->mission &&
            mission_match->mission->result() == sim::MissionHost::Result::Won && !chain_over) {
          // M11: a küldetéssor következő küldetése (a szimulációs szál áll, amíg a meccs cserélődik)
          runner.stop();
          sim::finish_mission(*chain_def, chain_state, *mission_match);
          if (!chain_profile.empty()) {
            std::ofstream pout(chain_profile, std::ios::binary | std::ios::trunc);
            pout << content::dump_pretty(chain_state.to_json());
          }
          if (chain_state.current.empty()) {
            chain_pick = !sim::available_missions(*chain_def, chain_state).empty();   // M12: választás a térképen
            chain_over = !chain_pick;
            if (chain_pick) chain_state.current = sim::available_missions(*chain_def, chain_state).front();   // csak a háttér
            if (chain_pick) {
              runner.start();
              continue;
            }
          }
          if (chain_over) {
          } else if (auto desc = sim::prepare_mission(*chain_def, chain_state, o.chain, o.maps, 20261116, &err)) {
            if (auto nm = sim::start_match(*desc, session.terrain_ptr(), session.content_rules(), err)) {
              session.adopt(*nm);
              mission_match = std::move(nm);
              runner.reset_snapshots();
              terrain.set_map(world.map(), world.terrain());
              mission_cam.clear();
              int cw = 0, ch = 0;
              SDL_GetWindowSizeInPixels(win, &cw, &ch);
              center_on_local(cam, cw, ch);
            } else {
              std::fprintf(stderr, "küldetéssor: %s\n", err.c_str());
              client::Diagnostics::get().count("LOAD_FAILED");
            }
          }
          runner.start();
          continue;
        }
        if (have_world && session.rules()) {   // M11: küldetés – átvezető (kihagyás), párbeszéd-válasz
          const client::Snapshot* ms = world.snapshot();
          if (ms && ms->mission.active) {
            if (ms->mission.letterbox || ms->mission.cutscene) {
              if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE && !ev.key.repeat) {
                sim::Command c;
                c.kind = sim::Command::Kind::Mission;
                c.owner = session.local_player();
                c.def = "__skip";
                runner.post(c);
              }
              if (ms->mission.letterbox && ev.type != SDL_EVENT_QUIT) continue;   // a bemenet tiltva
            }
            int pick = -1;
            if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.button == SDL_BUTTON_LEFT) {
              const auto& rs = hud.model().mission_choices;
              for (size_t i = 0; i < rs.size(); ++i) {
                if (rs[i].contains(ev.button.x, ev.button.y)) pick = static_cast<int>(i);
              }
            }
            if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key >= SDLK_1 && ev.key.key <= SDLK_9 &&
                static_cast<size_t>(ev.key.key - SDLK_1) < ms->mission.choices.size()) {
              pick = static_cast<int>(ev.key.key - SDLK_1);
            }
            if (pick >= 0 && !ms->mission.choices.empty()) {
              sim::Command c;
              c.kind = sim::Command::Kind::Mission;
              c.owner = session.local_player();
              c.def = ms->mission.dialogue;
              c.rx = pick;
              runner.post(c);
              continue;
            }
          }
        }
        if (hud_on && hud_event(hud.model(), ev, cam, world)) continue;   // a HUD elfogyasztotta
        if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE && !ev.key.repeat) {
          if (hud_on) game_paused = true;   // a régi szerint: Esc (ha a HUD nem fogyasztotta el) a szünetmenü
          else running = false;
        }
        if (!have_world) continue;
        // a régi játék: a középső gomb húzása mozgatja a kamerát
        if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.button == SDL_BUTTON_MIDDLE) {
          mid_drag = true;
          mid_mx = ev.button.x;
          mid_my = ev.button.y;
          mid_cx = cam.x;
          mid_cy = cam.y;
          continue;
        }
        if (ev.type == SDL_EVENT_MOUSE_BUTTON_UP && ev.button.button == SDL_BUTTON_MIDDLE) {
          mid_drag = false;
          continue;
        }
        if (ev.type == SDL_EVENT_MOUSE_MOTION && mid_drag) {
          cam.x = mid_cx - (ev.motion.x - mid_mx) / cam.zoom;
          cam.y = mid_cy - (ev.motion.y - mid_my) / cam.zoom;
        }
        if (ev.type == SDL_EVENT_MOUSE_WHEEL) {
          const float old = cam.zoom;
          cam.zoom = std::clamp(cam.zoom * (ev.wheel.y > 0 ? 1.12f : 1.0f / 1.12f), 1.0f, 2.0f);
          float mx = 0, my = 0;
          SDL_GetMouseState(&mx, &my);
          cam.x += mx / old - mx / cam.zoom;   // a kurzor alatti pont marad
          cam.y += my / old - my / cam.zoom;
        }
        if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key >= SDLK_A && ev.key.key <= SDLK_Z && session.rules()) {
          float kmx = 0, kmy = 0;   // a kijelöltek képessége a betűjével (a régi játék gyorsbillentyűi)
          SDL_GetMouseState(&kmx, &kmy);
          const char key = static_cast<char>('A' + (ev.key.key - SDLK_A));
          if (world.ability_hotkey(key, cam.x + kmx / cam.zoom, cam.y + kmy / cam.zoom)) continue;
        }
        if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_Q && session.rules()) {   // telepes képzése (ideiglenes)
          sim::Command c;
          c.kind = sim::Command::Kind::Train;
          c.owner = session.local_player();
          c.def = "urs_telepes";
          runner.post(c);
        }
        world.queue_mode = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;   // D46: Shift + parancs: sorba áll
        if (patrol_mode && ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.button == SDL_BUTTON_LEFT) {
          world.order_patrol(cam.x + ev.button.x / cam.zoom, cam.y + ev.button.y / cam.zoom);
          patrol_mode = false;
          continue;
        }
        if (attack_move && ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.button == SDL_BUTTON_LEFT) {
          world.order_attack_move(cam.x + ev.button.x / cam.zoom, cam.y + ev.button.y / cam.zoom);
          attack_move = false;
          continue;
        }
        if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.button == SDL_BUTTON_LEFT) {
          dragging = true;
          drag_x = cam.x + ev.button.x / cam.zoom;
          drag_y = cam.y + ev.button.y / cam.zoom;
        }
        if (ev.type == SDL_EVENT_MOUSE_BUTTON_UP && ev.button.button == SDL_BUTTON_LEFT && dragging) {
          dragging = false;
          world.select_box(drag_x, drag_y, cam.x + ev.button.x / cam.zoom, cam.y + ev.button.y / cam.zoom);
        }
        if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.button == SDL_BUTTON_RIGHT) {
          const float wx = cam.x + ev.button.x / cam.zoom, wy = cam.y + ev.button.y / cam.zoom;
          if (world.order_rally(wx, wy)) {   // csak épület kijelölve: a gyülekezőpont (a régi szerint)
          } else if (const sim::EntityId enemy = world.enemy_at(wx, wy)) world.order_attack(enemy);   // ellenségre: támadás
          else if (const sim::EntityId gb = world.garrison_at(wx, wy)) world.order_garrison(gb);   // saját helyőrségbe
          else if (const sim::EntityId fu = world.friendly_unit_at(wx, wy)) world.order_follow(fu);   // saját egységre: követés (WC3)
          else {   // mozgás: a felengedéskor (húzva vonalra, különben alakzatban)
            line_drag = true;
            line_x0 = wx;
            line_y0 = wy;
          }
        }
        if (line_drag && ev.type == SDL_EVENT_MOUSE_BUTTON_UP && ev.button.button == SDL_BUTTON_RIGHT) {
          line_drag = false;
          const float wx = cam.x + ev.button.x / cam.zoom, wy = cam.y + ev.button.y / cam.zoom;
          if (std::hypot(wx - line_x0, wy - line_y0) * cam.zoom > 24 && world.selected() > 1) world.order_line(line_x0, line_y0, wx, wy);
          else world.order_move(line_x0, line_y0, formation);
        }
        if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_P && !ev.key.repeat) patrol_mode = true;   // P + bal gomb: járőr
        if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_A) attack_move = true;   // A + bal gomb: támadó mozgás
        if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_U && have_world) {   // U: a kurzor alatti épületből ki
          float umx = 0, umy = 0;
          SDL_GetMouseState(&umx, &umy);
          world.order_unload(cam.x + umx / cam.zoom, cam.y + umy / cam.zoom);
        }
        if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_V && have_world) {   // következő időjárás
          weather_from = weather_to;
          weather_to = static_cast<render::Weather>((static_cast<int>(weather_to) + 1) % static_cast<int>(render::Weather::Count));
          weather_since = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
          std::printf("időjárás: %s\n", render::weather_name(weather_to));
        }
        if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat && ev.key.key >= SDLK_0 && ev.key.key <= SDLK_9) {
          const SDL_Keymod mod = SDL_GetModState();
          const int g = static_cast<int>(ev.key.key - SDLK_0);
          if (mod & SDL_KMOD_ALT) {   // Alt+1–6: alakzat (1 nincs, 2 WC3-szerű, 3 sor, 4 doboz …)
            if (g >= 1 && g <= 6) formation = static_cast<sim::Formation>(g - 1);
          } else if (mod & SDL_KMOD_CTRL) {   // Ctrl+szám: a kijelölés a csoportba
            ctrl_groups[static_cast<size_t>(g)] = world.selection();
          } else if (mod & SDL_KMOD_SHIFT) {   // Shift+szám: a csoport hozzáadása a kijelöléshez
            std::vector<sim::EntityId> sel = world.selection();
            for (const auto id : ctrl_groups[static_cast<size_t>(g)]) {
              if (std::find(sel.begin(), sel.end(), id) == sel.end()) sel.push_back(id);
            }
            world.set_selection(sel);
          } else if (!ctrl_groups[static_cast<size_t>(g)].empty()) {   // szám: a csoport kijelölése; kétszer: oda a kamera
            world.set_selection(ctrl_groups[static_cast<size_t>(g)]);
            ctrl_groups[static_cast<size_t>(g)] = world.selection();   // az elesettek kihullanak
            const double now = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            float cx = 0, cy = 0;
            if (last_group == g && now - last_group_t < 0.4 && world.selection_center(cx, cy)) {
              cam.x = cx - static_cast<float>(o.width) / 2 / cam.zoom;
              cam.y = cy - static_cast<float>(o.height) / 2 / cam.zoom;
            }
            last_group = g;
            last_group_t = now;
          }
        }
      }
      {   // AS-01: a képkockák közti idő (a görgetéshez és az FPS-kijelzéshez)
        const auto now = std::chrono::steady_clock::now();
        frame_dt = static_cast<float>(std::chrono::duration<double>(now - frame_prev).count());
        frame_prev = now;
        fps_acc += frame_dt;
        ++fps_frames;
        if (fps_acc >= 0.5f) {
          fps_shown = static_cast<int>(static_cast<float>(fps_frames) / fps_acc + 0.5f);
          fps_acc = 0;
          fps_frames = 0;
        }
      }
      if (have_world && !game_paused && !chain_pick) {   // kamera: billentyű és (a beállítás szerint) képernyőszél
        const bool* keys = SDL_GetKeyboardState(nullptr);
        client::ScrollInput si;
        si.kx = (keys[SDL_SCANCODE_RIGHT] ? 1 : 0) - (keys[SDL_SCANCODE_LEFT] ? 1 : 0);
        si.ky = (keys[SDL_SCANCODE_DOWN] ? 1 : 0) - (keys[SDL_SCANCODE_UP] ? 1 : 0);
        // képpróbában és mérésnél nincs szélgörgetés (a rejtett ablak egere a sarokban áll – H-003b)
        si.mouse_in = !o.hidden && o.screenshot.empty() && !o.bench &&
                      (SDL_GetWindowFlags(win) & SDL_WINDOW_MOUSE_FOCUS) != 0;
        SDL_GetMouseState(&si.mx, &si.my);
        SDL_GetWindowSize(win, &si.w, &si.h);
        float dx = 0, dy = 0;
        client::scroll_delta(settings, si, frame_dt, cam.zoom, &dx, &dy);
        cam.x += dx;
        cam.y += dy;
      }
      if (hud_on) {   // a régi játék sebességvezérlése: −/+ gomb és billentyű (egyjátékos módban)
        auto& hm = hud.model();
        hm.speed_locked = o.host_port > 0 || !o.join.empty();
        if (hm.speed_change != 0 && !hm.speed_locked) {   // a régi: „Játéksebesség: x1.5”
          const int before = runner.speed_pct();
          runner.set_speed_pct(client::HudModel::next_speed(runner.speed_pct(), hm.speed_change));
          if (runner.speed_pct() != before) {
            char sp[16];
            std::snprintf(sp, sizeof sp, "%g", static_cast<double>(runner.speed_pct()) / 100.0);
            hm.post_notice(hm.tr("speed", {sp}), {230, 230, 200});
          }
        }
        hm.speed_change = 0;
        hm.speed_pct = runner.speed_pct();
      }
      if (hud_on && hud.model().jump) {   // a HUD kameraugrása (minimap, tétlen munkás)
        int jw = 0, jh = 0;
        SDL_GetWindowSize(win, &jw, &jh);
        cam.x = hud.model().jump_x - static_cast<float>(jw) / 2 / cam.zoom;
        cam.y = hud.model().jump_y - static_cast<float>(jh) / 2 / cam.zoom;
        hud.model().jump = false;
      }
      if (hud_on && hud.model().menu_requested) game_paused = true;   // a fogaskerék: a szünetmenü
      if (hud_on && hud.model().quit_requested) running = false;      // az összesítő „Főmenü” gombja
      if (hud_on && hud.model().diag_requested) {                       // az összesítő „Diagnosztikai jelentés”
        hud.model().diag_requested = false;
        save_diag(hud.model());
      }
      if (hud_on) {
        hud.model().menu_requested = false;
        hud.model().paused = game_paused;   // szünetben nincs parancs (a régi őr)
        std::vector<sim::SimEvent> notes;   // a szimuláció felületi eseményei (újratestesülés, osztódás, …)
        runner.take_notes(notes);
        if (!notes.empty()) hud.model().sim_events(notes);
        // a profil statisztikája (a régi `record_game`): a csata vége egyszer, küldetésen és visszajátszáson kívül
        if (!game_recorded && hud.model().match_over() && have_cfg && !settings.profile.empty()) {
          game_recorded = true;
          if (const client::Snapshot* sn = hud.model().snap(); sn && !sn->mission.active) {
            const uint8_t me = hud.model().player;
            const auto& mp = sn->players[std::min<size_t>(me, sim::Sim::kMaxPlayers)];
            client::ProfileStore(client::saves_dir().parent_path() / "profilok")
                .record_game(settings.profile, mp.faction, sn->won(me), sn->tick / 30, mp.stats.units_killed);
          }
        }
      }
      runner.set_paused(game_paused);
      if (audio.ok()) {   // zene és hangok (a régi `play_sounds`): ami a játékosnak látható vagy fontos
        audio.update(settings, static_cast<double>(SDL_GetTicks()) / 1000.0);
        const sim::Rules* sr = session.rules();
        const client::Snapshot* sn = hud_on ? hud.model().snap() : nullptr;
        auto who = [&](uint16_t def, uint8_t owner, std::string& id, std::string& fac, std::string& ut) {
          if (sr && def < sr->defs.size()) {
            id = sr->defs[def].id;
            ut = sr->defs[def].utype;
            fac = sr->defs[def].faction;
          }
          if (sn && owner >= 1 && owner <= sim::Sim::kMaxPlayers && !sn->players[owner].faction.empty()) fac = sn->players[owner].faction;
        };
        if (have_world) {
          const auto& sel = world.selection();   // új kijelölés: a kijelölt saját egység megszólal
          if (sel != snd_sel) {
            for (const sim::EntityId e : sel) {
              if (std::find(snd_sel.begin(), snd_sel.end(), e) != snd_sel.end() || !sn) continue;
              const client::SnapUnit* u = sn->find(e);
              if (!u || u->owner != session.local_player()) break;
              std::string id, fac, ut;
              who(u->def, u->owner, id, fac, ut);
              audio.play("select", id, fac, ut, 1.0f, 300, "select");
              break;
            }
            snd_sel = sel;
          }
          std::vector<client::World::SoundEv> evs;
          world.take_sounds(evs);
          int sww = 0, swh = 0;
          SDL_GetWindowSizeInPixels(win, &sww, &swh);
          const float vw = static_cast<float>(sww) / cam.zoom, vh = static_cast<float>(swh) / cam.zoom;
          for (const auto& e : evs) {
            std::string id, fac, ut;
            who(e.def, e.owner, id, fac, ut);
            const bool on = e.x >= cam.x - 80 && e.x <= cam.x + vw + 80 && e.y >= cam.y - 80 && e.y <= cam.y + vh + 80;
            if (e.ev == "move" || e.ev == "attack_order") audio.play(e.ev, id, fac, ut, 1.0f, 900, "ack");
            else if (e.ev == "trained" || e.ev == "building_done") audio.play(e.ev, id, fac, ut, 1.0f, 400, e.ev + id, e.building);
            else if (!on) continue;
            else if (e.ev == "attack") audio.play("attack", id, fac, ut, 0.55f, 120, "atk" + id);
            else if (e.ev == "death") audio.play("death", id, fac, ut, 0.8f, 150, "death" + id, e.building);
            else if (e.ev.rfind("ability_", 0) == 0) audio.play(e.ev, id, fac, ut, 0.85f, 180, e.ev);
            else if (e.ev.rfind("impact_", 0) == 0) audio.play(e.ev, "", "", "", 0.45f, 95, e.ev);
          }
          if (!game_paused) audio.play("environment", "", "", "", 0.6f, 25000, "env");   // környezeti hang (ha van fájl)
          if (hud_on && hud.model().alarm) {   // a régi riasztás („Támadás ér!”, 12 mp-enként legfeljebb egy)
            hud.model().alarm = false;
            std::string fac = sn && session.local_player() <= sim::Sim::kMaxPlayers ? sn->players[session.local_player()].faction : std::string();
            audio.play("under_attack", "", fac, "", 1.0f, 12000, "alarm");
          }
        }
      }
      {   // a szünetpanel becsúszása (0,22 mp; képpróbában azonnal)
        const double now_s = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        if (game_paused && !pause_was) {
          pause_t = now_s;
          want_thumb = true;   // ezen a képkockán a panel még kint van, az elmosás 0
        }
        pause_was = game_paused;
        pause_slide = !game_paused ? 0.0f
                      : !o.screenshot.empty() ? 1.0f
                                               : static_cast<float>(std::clamp((now_s - pause_t) / 0.22, 0.0, 1.0));
      }
      const auto f0 = std::chrono::steady_clock::now();
      int w = 0, h = 0;
      SDL_GetWindowSizeInPixels(win, &w, &h);
      dev.bind_target(nullptr, w, h);
      dev.clear(0.12f, 0.16f, 0.10f, 1.0f);
      if (have_world) {
        // a szimuláció: külön szálon 30 Hz-en (a kép a két legutóbbi pillanatkép között simít), vagy szinkron módban
        // két képkockánként egy tick a rajzolás szálán (képernyőkép: determinisztikus kép)
        const bool fixed = o.bench || !o.screenshot.empty();
        float alpha = 0;
        if (!runner.threaded()) {
          if (frame % 2 == 1) {
            const auto s0 = std::chrono::steady_clock::now();
            runner.step_sync();
            sim_times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - s0).count());
          }
          world.update();
          alpha = frame % 2 == 1 ? 0.0f : 0.5f;
        } else {
          world.update();
          alpha = world.live_alpha();
        }
        if (const client::Snapshot* ms = world.snapshot(); ms && ms->mission.active && ms->mission.camera != mission_cam) {
          // M11: az átvezető kameraparancsa – a kamera a pontra úszik (0 tick: azonnal)
          mission_cam = ms->mission.camera;
          if (!mission_cam.empty()) {
            if (ms->mission.cam_zoom > 0) cam.zoom = std::clamp(ms->mission.cam_zoom, 1.0f, 2.0f);
            pan_x0 = cam.x;
            pan_y0 = cam.y;
            pan_x1 = ms->mission.cam_x * client::World::kTilePx - static_cast<float>(w) / 2 / cam.zoom;
            pan_y1 = ms->mission.cam_y * client::World::kTilePx - static_cast<float>(h) / 2 / cam.zoom;
            pan_len = static_cast<float>(ms->mission.cam_ticks) / 30.0f;
            pan_t = 0;
          }
        }
        if (pan_t < 1) {
          pan_t = pan_len <= 0 ? 1.0f : std::min(1.0f, pan_t + 1.0f / 60.0f / pan_len);
          const float e = pan_t * pan_t * (3 - 2 * pan_t);   // simított
          cam.x = pan_x0 + (pan_x1 - pan_x0) * e;
          cam.y = pan_y0 + (pan_y1 - pan_y0) * e;
        }
        const double t = fixed ? static_cast<double>(frame) / 60.0
                               : std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        const float vx1 = cam.x + static_cast<float>(w) / cam.zoom, vy1 = cam.y + static_cast<float>(h) / cam.zoom;
        const render::Mat4 proj = render::ortho(cam.x, vx1, vy1, cam.y);
        // napszak (D35): rögzített, vagy 4 perces ciklus a hajnaltól
        const double day_t = o.daytime >= 0 ? o.daytime : std::fmod(t / 240.0 + 0.10, 1.0);
        render::DayLight day = render::daylight(day_t);
        // időjárás (D35): átmenet a kettő között; a napszak fényét és árnyékát módosítja
        const float wk = static_cast<float>(std::clamp((t - weather_since) / kWeatherBlend, 0.0, 1.0));
        const render::WeatherLook wl = render::mix(render::weather_look(weather_from, o.weather_power),
                                                   render::weather_look(weather_to, o.weather_power), wk);
        for (int c = 0; c < 3; ++c) day.ambient[c] = std::min(1.2f, day.ambient[c] * wl.ambient[c]);
        day.shadow_alpha *= wl.shadow;
        day.lights = std::max(day.lights, wl.lights);
        const float flash = render::lightning_flash(t, wl.lightning);
        // részecskék: a szimuláció eseményeiből (találat, halál), a fáklyák parazsa, az eső cseppjei a talajon
        const float dt = static_cast<float>(std::clamp(t - prev_t, 0.0, 0.1));
        prev_t = t;
        for (const auto& fe : world.fx_events()) {   // találat: szikra; a halál képét a világ játssza le (blood, explosion)
          if (!fe.death) particles.emit(render::Effect::Hit, fe.x, fe.y);
        }
        world.fx_events().clear();
        if (wl.splashes > 0 && wl.precip == render::Precip::Rain) {
          const float area = (vx1 - cam.x) * (vy1 - cam.y) * cam.zoom * cam.zoom / (1280.0f * 720.0f);
          const float want = wl.splashes * wl.density / 0.55f * area * dt;
          for (int i = 0, n = static_cast<int>(want + fx_rand()); i < n; ++i) {
            particles.emit(render::Effect::Splash, cam.x + fx_rand() * (vx1 - cam.x), cam.y + fx_rand() * (vy1 - cam.y));
          }
        }
        particles.update(dt, wl.wind);
        if (auto mv = world.take_view_change()) {   // a népmechanikák fedőképe (csak ha a nézet változott)
          render::Image mi = client::module_overlay(mv->w, mv->h, mv->creep, mv->pond);
          if (module_tex.id) dev.destroy(module_tex);
          module_tex = {};
          if (mi.width > 0 && mi.width <= dev.max_texture_size() && mi.height <= dev.max_texture_size()) {
            render::premultiply(mi);
            module_tex = dev.create_array_from(mi, false);
          }
          world.set_module_overlay(module_tex.id ? &module_tex : nullptr);
        }
        lighting.resize(w, h);
        lighting.begin_scene(0.0f, 0.0f, 0.0f);
        terrain.draw(proj, cam.x, cam.y, vx1, vy1, settings.water_anim ? t : 0.0);   // a régi „Mozgó víz”
        render::Atlas* atl = world_atlas_ok ? &world_atlas : nullptr;
        lighting.begin_shadow_mask();   // az árnyékok külön maszkba (maximum-keveréssel), majd egyszer a terepre
        batch.begin_shadows(proj, day.shadow_x, day.shadow_y, day.shadow_alpha);   // a napszakkal forgó árnyék
        if (settings.shadows) {   // a régi „Árnyékok” beállítás
          world.draw(batch, atl, fallback, cam, w, h, alpha, t, true);
          world.draw_banners(batch, t);       // D52: a zászlók is árnyékot vetnek, a nappal együtt
          world.draw_rally_flags(batch, t);
        }
        batch.end();
        lighting.apply_shadows();
        batch.begin(proj);
        world.draw_doodads(batch, atl, cam, w, h);   // a terep apró díszletei (a régi szerint)
        world.draw_rally_ground(batch, t);   // a gyülekezőpont ösvénye, pecsétje és árnyéka (a talajon)
        world.draw_move_indicator(batch, alpha, t);   // a mozgáscél (a talajon)
        world.draw_ground_marks(batch, alpha, t);     // talajjelek: hős, ének, rejtőzés
        world.draw_objective_rings(batch, t);         // a küldetés célgyűrűi
        world.draw_foundations(batch, cam, w, h);     // az Aurethi alapozás lapkövei
        world.draw_planned(batch, white, atl);       // a tervezett épületek (rács és halvány kép)
        world.draw_placement_grid(batch, white);     // a régi lerakási rács és keret
        world.draw_ground_items(batch, white, t);     // a földön heverő tárgyak
        world.draw_links(batch, white, t);            // a micélia spórafonalai
        world.draw_areas(batch, atl, alpha);          // a területi mezők (a régi `draw_area`)
        world.draw(batch, atl, fallback, cam, w, h, alpha, t);
        world.draw_vfx(batch, atl, alpha);            // varázslás, robbanó varázslat, becsapódás, szintlépés
        world.draw_larva(batch, white);               // a Kaptárak lárvái
        if (settings.status_fx) world.draw_rot_bubbles(batch, white, t);   // a rothadó fák buborékai
        world.draw_meal_pips(batch, white, alpha);    // micélia: ölések az osztódásig
        if (settings.status_fx) world.draw_status_fx(batch, white, alpha, t); // állapotok a figurán (a régi `draw_status_fx`)
        world.draw_carry(batch, alpha);      // a munkások cipelt nyersanyaga
        world.draw_hero_badges(batch, alpha, t);   // a hős jelvénye a feje fölött
        world.draw_objective_beacons(batch, t);    // a célok jelzőfénye
        world.draw_banners(batch, t);        // D51: a csapatjelző lobogók
        world.draw_building_fx(batch, atl, t);   // D49: az épületek animációs rétege (füst, atlaszlapos elemek)
        world.draw_rally_flags(batch, t);    // a zászló az egységek fölött
        {
          float hmx = 0, hmy = 0;
          SDL_GetMouseState(&hmx, &hmy);
          world.draw_bars(batch, white, alpha, cam.x + hmx / cam.zoom, cam.y + hmy / cam.zoom);   // a régi: csak az egér alattié
          if (demo_hover_x >= 0) world.draw_garrison(batch, white, demo_hover_x, demo_hover_y);
          else world.draw_garrison(batch, white, cam.x + hmx / cam.zoom, cam.y + hmy / cam.zoom);
        }
        batch.end();
        effects.draw_scene(batch, proj, particles);
        batch.begin(proj);
        if (dragging) {   // kijelölő keret
          float mx = 0, my = 0;
          SDL_GetMouseState(&mx, &my);
          const float x0 = std::min(drag_x, cam.x + mx / cam.zoom), y0 = std::min(drag_y, cam.y + my / cam.zoom);
          const float x1 = std::max(drag_x, cam.x + mx / cam.zoom), y1 = std::max(drag_y, cam.y + my / cam.zoom);
          const float t1 = 1.5f / cam.zoom;
          for (const auto& r : {std::array<float, 4>{x0, y0, x1 - x0, t1}, std::array<float, 4>{x0, y1 - t1, x1 - x0, t1},
                                std::array<float, 4>{x0, y0, t1, y1 - y0}, std::array<float, 4>{x1 - t1, y0, t1, y1 - y0}}) {
            render::SpriteInstance in;
            in.x = r[0]; in.y = r[1]; in.w = r[2]; in.h = r[3];
            in.tint[0] = 90; in.tint[1] = 255; in.tint[2] = 90;
            batch.draw(white, nullptr, in);
          }
        }
        batch.end();
        // fények (a napszak környezeti színe + fáklyák), köd, összetétel az ablakba
        lights.clear();
        world.lights(lights, alpha);
        if (day.lights > 0.05f) {   // a fáklyák parazsa (éjjel)
          for (const auto& l : lights) {
            if (fx_rand() < 2.5f * dt * day.lights) particles.emit(render::Effect::Ember, l.x, l.y + 20.0f);
          }
        }
        lighting.draw_lights(batch, proj, day.ambient, lights, day.lights,
                             [&] { effects.draw_glow(batch, proj, particles, 0.4f + 0.6f * day.lights); });
        bool fog_changed = false;
        const auto& fogv = world.fog(&fog_changed);
        if (!o.fog) lighting.set_fog(0, 0, {});
        else if (fog_changed) lighting.set_fog(world.map().width(), world.map().height(), fogv);
        lighting.set_fog_style(o.fog_blur, o.fog_soft, 6.0f * cam.zoom);
        lighting.set_blur(game_paused ? pause_slide : 0.0f);
        lighting.composite(w, h, cam.x, cam.y, cam.zoom, client::World::kTilePx, wl, t, flash, o.post);
        batch.begin(proj);   // a légköri részecskék a fényezés után (a szentjánosbogár éjjel is világít – a régi szerint)
        world.draw_building_flames(batch, t);   // D49: a lángok (éjjel is világítanak)
        if (settings.ambient) world.draw_ambient(batch, cam, w, h, t);   // a régi „Környezeti részecskék”
        batch.end();
        if (editor_on && !editor_test) {   // M11: a szerkesztő rétegei és palettája
          int ww = 0, wh = 0;
          SDL_GetWindowSizeInPixels(win, &ww, &wh);
          eui.sync(edoc);
          eui.layout(ww, wh);
          batch.begin(render::ortho(0, static_cast<float>(w), static_cast<float>(h), 0));
          hud.begin_ui(batch, white);
          client::draw_editor(hud, eui, edoc, cam, w, h, [&](const std::string& k) { return locale.tr(k); }, editor_problems);
          if (tedit.open) {
            float tmx = 0, tmy = 0;
            SDL_GetMouseState(&tmx, &tmy);
            tedit.layout(ww, wh, edoc);
            client::draw_trigger_editor(hud, tedit, edoc, tmx, tmy, [&](const std::string& k) { return locale.tr(k); });
          }
          if (dedit.open) {
            float dmx = 0, dmy = 0;
            SDL_GetMouseState(&dmx, &dmy);
            dedit.layout(ww, wh, edoc);
            client::draw_data_editor(hud, dedit, dmx, dmy, [&](const std::string& k) { return locale.tr(k); });
          }
          batch.end();
        } else if (session.has_starts() && session.rules()) {   // a HUD (M8; a képernyő vetítésében)
          client::HudFrame hf;
          hf.w = w;
          hf.h = h;
          SDL_GetMouseState(&hf.mx, &hf.my);
          hf.t = t;
          hf.cam_x = cam.x;
          hf.cam_y = cam.y;
          hf.cam_w = static_cast<float>(w) / cam.zoom;
          hf.cam_h = static_cast<float>(h) / cam.zoom;
          hf.fps = settings.show_fps ? fps_shown : -1;
          hf.show_clock = settings.show_clock;
          hf.help_hint = settings.help_hint;
          hf.speed_buttons = settings.speed_buttons;
          world.set_hero_mark(settings.hero_mark);
          world.set_status_pulse(settings.status_pulse);
          {   // a kurzor a helyzet szerint (a régi `cursor_kind`)
            client::HudModel& chm = hud.model();
            const auto* cs = world.snapshot();
            const sim::Rules* cr = session.rules();
            std::string kind = "arrow";
            const int ex = hf.mx <= 2 ? -1 : hf.mx >= static_cast<float>(w) - 3 ? 1 : 0;
            const int ey = hf.my <= 2 ? -1 : hf.my >= static_cast<float>(h) - 3 ? 1 : 0;
            static const char* kScroll[3][3] = {{"scroll_nw", "scroll_n", "scroll_ne"}, {"scroll_w", "", "scroll_e"}, {"scroll_sw", "scroll_s", "scroll_se"}};
            if (settings.edge_scroll && chm.mode.kind == client::HudMode::None && (ex || ey)) {
              kind = kScroll[ey + 1][ex + 1];
            } else if (chm.over_hud(hf.mx, hf.my)) {
              kind = "arrow";
            } else if (chm.mode.kind != client::HudMode::None) {
              switch (chm.mode.kind) {
                case client::HudMode::Move: kind = "move"; break;
                case client::HudMode::Attack: kind = "attack"; break;
                case client::HudMode::Place:
                  kind = cs && cs->place.def == chm.mode.def && !cs->place.ok ? "no" : "build";
                  break;
                case client::HudMode::Unpack: kind = "build"; break;
                default: kind = "cast"; break;
              }
            } else if (cs && cr) {
              const float wx = cam.x + hf.mx / cam.zoom, wy = cam.y + hf.my / cam.zoom;
              bool any_worker = false, any_attacker = false;
              for (const sim::EntityId id : world.selection()) {
                const auto* su = cs->find(id);
                if (!su || su->owner != session.local_player() || su->def >= cr->defs.size()) continue;
                any_worker = any_worker || cr->defs[su->def].worker;
                any_attacker = any_attacker || su->damage > 0;
              }
              const client::SnapUnit* res = nullptr;   // a lelőhely a lábnyoma szerint
              for (const auto& su : cs->units) {
                if (su.kind == client::SnapUnit::kResource && wx >= su.tx * 32.0f && wx < (su.tx + su.w) * 32.0f && wy >= su.ty * 32.0f &&
                    wy < (su.ty + su.h) * 32.0f) {
                  res = &su;
                }
              }
              const int hi = world.hovered(alpha, wx, wy);
              if (hi >= 0) {
                const client::SnapUnit& e = cs->units[static_cast<size_t>(hi)];
                const uint8_t me = session.local_player();
                const bool hostile = e.owner != me && !cs->allied(me, e.owner);
                if (hostile) kind = any_attacker ? "attack" : "select_enemy";
                else if (e.owner == me && e.hp < e.max_hp && any_worker && e.kind == client::SnapUnit::kBuilding) kind = "repair";
                else kind = "select_friend";
              } else if (res) {
                static const char* kGather[4] = {"gather_wood", "gather_stone", "gather_meteor", "gather_stone"};
                kind = any_worker ? kGather[std::min<int>(res->res, 3)] : "select_neutral";
              }
            }
            cursors.set(kind);
          }
          batch.begin(render::ortho(0, static_cast<float>(w), static_cast<float>(h), 0));
          hud.draw_world_labels(batch, white, world.labels(alpha, t, cam.x + hf.mx / cam.zoom, cam.y + hf.my / cam.zoom), cam.x, cam.y,
                                cam.zoom);   // a világ feliratai (a HUD alatt)
          hud.draw(batch, white, hf);
          if (chain_def && chain_pick) {   // M12: küldetésválasztó – vonalak a feltételekhez, csomópontok, cím
            auto box = [&](float x, float y, float bw, float bh, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
              render::SpriteInstance q;
              q.x = x;
              q.y = y;
              q.w = bw;
              q.h = bh;
              q.tint[0] = r;
              q.tint[1] = g;
              q.tint[2] = b;
              q.tint[3] = a;
              batch.draw(white, nullptr, q);
            };
            box(0, 0, static_cast<float>(w), static_cast<float>(h), 12, 14, 20, 255);
            const auto av = sim::available_missions(*chain_def, chain_state);
            auto has = [](const std::vector<std::string>& v, const std::string& x) { return std::find(v.begin(), v.end(), x) != v.end(); };
            for (const auto& m : chain_def->missions) {
              const auto b = chain_node(m, w, h);
              for (const auto& req : m.needs) {
                const auto a = chain_node(*chain_def->mission(req), w, h);
                const float x0 = a[0] + a[2] / 2, y0 = a[1] + a[3] / 2, x1 = b[0] + b[2] / 2, y1 = b[1] + b[3] / 2;
                const int n = static_cast<int>(std::hypot(x1 - x0, y1 - y0) / 6.0f);
                for (int i = 0; i <= n; i += 2) {   // szaggatott vonal
                  const float f = n > 0 ? static_cast<float>(i) / static_cast<float>(n) : 0.0f;
                  box(x0 + (x1 - x0) * f - 2, y0 + (y1 - y0) * f - 2, 4, 4, 170, 150, 110, 220);
                }
              }
            }
            for (const auto& m : chain_def->missions) {
              const auto b = chain_node(m, w, h);
              const bool done = has(chain_state.done, m.id), open = has(av, m.id);
              const bool hover = open && hf.mx >= b[0] && hf.mx < b[0] + b[2] && hf.my >= b[1] && hf.my < b[1] + b[3];
              box(b[0] - 2, b[1] - 2, b[2] + 4, b[3] + 4, hover ? 250 : open ? 210 : 70, hover ? 220 : open ? 170 : 70,
                  hover ? 120 : open ? 80 : 70, 255);
              box(b[0], b[1], b[2], b[3], done ? 40 : 30, done ? 60 : 32, done ? 40 : 44, 255);
              const uint8_t c = open || done ? 235 : 120;
              hud.text(batch, 16, b[0] + 10, b[1] + 6, locale.tr(m.name.empty() ? m.id : m.name), c, c, c - 20);
              if (done) hud.text(batch, 12, b[0] + 10, b[1] + 28, locale.tr("chain.pick.done"), 150, 200, 150);
            }
            hud.text(batch, 32, static_cast<float>(w) / 2 - 140, 24, locale.tr("chain.pick.title"), 240, 220, 160);
            hud.text(batch, 16, static_cast<float>(w) / 2 - 170, static_cast<float>(h) - 40, locale.tr("chain.pick.hint"), 200, 200, 200);
          } else if (chain_def && mission_match && mission_match->mission && mission_match->mission->result() == sim::MissionHost::Result::Won) {
            hud.text(batch, 18, static_cast<float>(w) / 2 - 170, static_cast<float>(h) / 2 + 48,
                     locale.tr(chain_over ? "chain.end" : "chain.next"), 240, 230, 200);
          }
          if (game_paused) {   // szünetmenü: bal oldali panel, balról becsúszva; a játék háttere elmosódik (a fényezőben)
            render::SpriteInstance dim;
            dim.w = static_cast<float>(w);
            dim.h = static_cast<float>(h);
            dim.tint[0] = dim.tint[1] = dim.tint[2] = 0;
            dim.tint[3] = static_cast<uint8_t>(110 * pause_slide);
            batch.draw(white, nullptr, dim);
            pause_items = hud.draw_pause(batch, white, hf, pause_slide, locale.tr("ui.pause.title"),
                                         {locale.tr("ui.pause.resume"), locale.tr("ui.pause.save"), locale.tr("ui.pause.load"),
                                          locale.tr("ui.pause.settings"), locale.tr("ui.pause.music"), locale.tr("ui.pause.diag"),
                                          locale.tr("ui.pause.quit")});
          } else {
            pause_items.clear();
          }
          batch.end();
        }
      } else if (demo.units > 0 || !adem.units.empty()) {
        // a bemutató determinisztikus időt használ, ha képernyőkép vagy mérés készül (azonos kép minden gépen)
        const double t = (o.bench || !o.screenshot.empty())
                             ? static_cast<double>(frame) / 60.0
                             : std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        batch.begin(render::ortho(0, static_cast<float>(w), static_cast<float>(h), 0));
        if (demo.units > 0) demo.draw(batch, t, w, h);
        else adem.draw(batch, t);
        batch.end();
      }
      if (want_thumb && have_world) {   // a mentés kis képe (a régi `SaveMenu`)
        want_thumb = false;
        const render::Image full = dev.read_pixels(w, h);
        if (full.width > 0 && full.height > 0) {
          save_thumb.width = 320;
          save_thumb.height = 180;
          save_thumb.rgba.assign(320 * 180 * 4, 255);
          for (int y = 0; y < 180; ++y) {
            for (int x = 0; x < 320; ++x) {
              const uint8_t* p = full.at(std::min(full.width - 1, x * full.width / 320), std::min(full.height - 1, y * full.height / 180));
              std::copy(p, p + 3, save_thumb.at(x, y));
            }
          }
        }
      }
      const bool last = o.frames >= 0 && frame + 1 >= o.frames;
      if (last && !o.screenshot.empty()) {
        const render::Image shot = dev.read_pixels(w, h);
        if (!render::save_png(o.screenshot, shot)) {
          std::fprintf(stderr, "nem írható: %s\n", o.screenshot.c_str());
          rc = 1;
        } else {
          std::printf("képernyőkép: %s (%d × %d)\n", o.screenshot.c_str(), w, h);
        }
        if (o.check) {   // a háttér (31, 41, 26) ± 6-tól eltérő képpontok aránya
          size_t drawn = 0;
          const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
          for (size_t i = 0; i < n; ++i) {
            const uint8_t* px = &shot.rgba[i * 4];
            if (std::abs(px[0] - 31) > 6 || std::abs(px[1] - 41) > 6 || std::abs(px[2] - 26) > 6) ++drawn;
          }
          const double pct = n ? 100.0 * static_cast<double>(drawn) / static_cast<double>(n) : 0.0;
          std::printf("képpróba: a képpontok %.1f%%-a rajzolt (legalább 2%% kell)\n", pct);
          if (pct < 2.0) rc = 1;
        }
      }
      if (o.bench) dev.api().Finish();
      SDL_GL_SwapWindow(win);
      if (settings.fps_limit > 0 && !o.bench) {   // AS-01: képkocka-korlát (a függőleges szinkron mellett)
        const double target = 1.0 / static_cast<double>(settings.fps_limit);
        const double spent = std::chrono::duration<double>(std::chrono::steady_clock::now() - frame_prev).count();
        if (spent < target) SDL_DelayPrecise(static_cast<Uint64>((target - spent) * 1e9));
      }
      times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - f0).count());
      if (have_world) {
        client::Diagnostics::get().frame_ms(times.back());
        if (frame % 30 == 0 && hud.model().snap()) {   // a csúcs-egységszám (csak egységek, fél másodpercenként)
          int n = 0;
          for (const auto& u : hud.model().snap()->units) n += u.kind == client::SnapUnit::kUnit ? 1 : 0;
          client::Diagnostics::get().note_units(n);
        }
      }
      if (have_world && o.bench) {
        double st = 0;
        if (!runner.threaded() && frame % 2 == 1 && !sim_times.empty()) st = sim_times.back();
        draw_times.push_back(times.back() - st);
      }
      if (!dev.check("képkocka", &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        rc = 1;
        running = false;
      }
      ++frame;
    }
    runner.stop();
    if ((o.host_port > 0 || !o.join.empty()) && have_world) {   // M10: a hálózati meccs összegzése (a próbákhoz és a hibakereséshez)
      std::printf("hálózat: %lld forduló, tick %lld, hash 0x%016llx, szinkronhiba: %zu\n",
                  static_cast<long long>(netplay.client().turns()),
                  static_cast<long long>(session.sim().tick()), static_cast<unsigned long long>(session.sim().hash()),
                  netplay.client().desyncs().size());
    }
    runner.take_tick_ms(sim_times);
    if (o.bench && times.size() > 10) {
      times.erase(times.begin(), times.begin() + 10);   // bemelegedés
      std::printf("mérés: %d egység, %ld képkocka, %d rajzoló hívás, %d sprite – p50 %.2f ms, p99 %.2f ms, max %.2f ms\n",
                  o.demo, frame, batch.draw_calls(), batch.sprites(), percentile(times, 0.5), percentile(times, 0.99),
                  percentile(times, 1.0));
    }
    if (o.bench && !adem.units.empty()) std::printf("atlasz: %d lap betöltve\n", adem.atlas.loaded_pages());
    if (o.bench && have_world && draw_times.size() > 10) {
      draw_times.erase(draw_times.begin(), draw_times.begin() + 10);
      std::printf("  rajzolás (szimuláció nélkül): p50 %.2f ms, p99 %.2f ms; szimulációs tick: p50 %.2f ms, p99 %.2f ms"
                  " (%s, %zu tick)\n",
                  percentile(draw_times, 0.5), percentile(draw_times, 0.99), percentile(sim_times, 0.5),
                  percentile(sim_times, 0.99), o.sync || !o.screenshot.empty() ? "szinkron" : "külön szálon",
                  sim_times.size());
    }
    if (o.bench && have_world) {
      std::printf("világ: %d egység látszott, %d terep-négyzet, %lld tick, élő egység %zu\n", world.drawn_units(),
                  terrain.last_quads(), static_cast<long long>(world.tick()), world.unit_count());
      std::printf("részecskék: %zu él, %zu kibocsátva, %zu elmaradt (betelt készlet)\n", particles.particles().size(),
                  particles.emitted(), particles.dropped());
    }
    world_atlas.release();
    lighting.shutdown();
    effects.shutdown();
    terrain.shutdown();
    dev.destroy(fallback);
    dev.destroy(sigil_own);
    dev.destroy(sigil_ally);
    dev.destroy(sigil_other);
    for (render::Texture* t : {&rally_arrows, &rally_sigil, &rally_pole, &rally_cloth, &rally_shadow, &move_sigil, &banner_pole, &banner_knob, &banner_cloth, &banner_trim, &carry_sack, &mark_hero_tint, &mark_hero_fixed, &mark_badge_frame, &mark_badge_symbol, &mark_chant, &mark_hidden, &obj_ring, &obj_beacon, &amb_glow, &amb_leaf, &fire_flame, &fire_puff, &found_tex, &ground_tex, &module_tex}) dev.destroy(*t);
    for (render::Texture& t : unit_shapes) dev.destroy(t);
    for (auto& [k, t] : item_tex) dev.destroy(t);
    dev.destroy(white);
    adem.atlas.release();
    for (auto& s : demo.sheets) {
      dev.destroy(s.base);
      dev.destroy(s.mask);
    }
    batch.shutdown();
  }
  audio.shutdown();   // a hangfolyamok az SDL leállítása előtt
  SDL_GL_DestroyContext(ctx);
  SDL_DestroyWindow(win);
  SDL_Quit();
  if (!restart_load.empty()) {   // a szünetmenüből választott mentés: új folyamat a mentéssel (a pálya- és menükapcsolók nélkül)
    std::vector<std::string> keep;
    static const char* kDrop1[] = {"--world", "--load", "--mission", "--chain", "--editor", "--menu-screen", "--host", "--join"};
    for (int i = 1; i < argc; ++i) {
      const std::string a = argv[i];
      if (a == "--menu") continue;
      if (std::find(std::begin(kDrop1), std::end(kDrop1), a) != std::end(kDrop1)) {
        ++i;
        continue;
      }
      keep.push_back(a);
    }
    keep.push_back("--load");
    keep.push_back(restart_load);
    std::vector<const char*> args = {argv[0]};
    for (const auto& k : keep) args.push_back(k.c_str());
    args.push_back(nullptr);
    if (SDL_Process* pr = SDL_CreateProcess(args.data(), false)) {
      SDL_DestroyProcess(pr);
      return 0;
    }
    std::fprintf(stderr, "a betöltés újraindítása nem sikerült\n");
    return 0;
  }
  if (rc == client::kMenuRestart) {   // M13: a modkészlet változott – a játék újraindul ugyanazokkal a kapcsolókkal
    std::vector<const char*> args(argv, argv + argc);
    args.push_back(nullptr);
    if (SDL_Process* pr = SDL_CreateProcess(args.data(), false)) {
      SDL_DestroyProcess(pr);
      return 0;
    }
    std::fprintf(stderr, "az újraindítás nem sikerült: indítsd el újra a játékot\n");
    return 0;
  }
  return rc;
}
