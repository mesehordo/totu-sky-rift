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
//   totu --look X,Y                a kamera középpontja (csempe)
//   totu --zoom Z                  a kamera nagyítása (1,0–2,0; alap 1,5)
//   totu --sync                    a szimuláció a rajzolás szálán, két képkockánként egy tick (képernyőképnél mindig így;
//                                  különben a szimuláció külön szálon fut, 30 Hz-en)
//   totu --nopost                  utófeldolgozás nélkül (a 4. lépés képe)
//   totu --check                   a képernyőképnek legalább 2%-a nem háttérszín (különben 1-es kilépési kód; CI)
//   totu --hidden                  rejtett ablak (CI);  --size SZxM  ablakméret;  --assets mappa  a grafika helye
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
#include <string>
#include <vector>

#include "core/platform_info.hpp"
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
  std::string assets = TOTU_ASSETS_DIR;
  int width = 1280, height = 720;
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
      const auto& a = s.anims.at("walk");
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

}  // namespace

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
    else if (a == "--world") o.world = next();
    else if (a == "--content") o.content = next();
    else if (a == "--daytime") o.daytime = std::strtod(next(), nullptr);
    else if (a == "--nofog") o.fog = false;
    else if (a == "--nopost") o.post = false;
    else if (a == "--zoom") o.zoom = std::strtof(next(), nullptr);
    else if (a == "--sync") o.sync = true;
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
        if (w > 0 && h > 0 && w <= 16384 && h <= 16384) { o.width = static_cast<int>(w); o.height = static_cast<int>(h); }
      }
    }
  }
  print_version();
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
  SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
  if (o.hidden) flags |= SDL_WINDOW_HIDDEN;
  SDL_Window* win = SDL_CreateWindow("TOTU: Sky Rift", o.width, o.height, flags);
  if (!win) {
    std::fprintf(stderr, "Ablakhiba: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SDL_GLContext ctx = SDL_GL_CreateContext(win);
  if (!ctx) {
    std::fprintf(stderr, "OpenGL 3.3 core környezet nem jött létre: %s\n", SDL_GetError());
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 1;
  }
  SDL_GL_SetSwapInterval(o.bench ? 0 : 1);

  int rc = 0;
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
    client::Session session;
    client::SimRunner runner(session);
    client::World world(session, runner);
    render::TerrainView terrain;
    render::Atlas world_atlas;
    render::Texture fallback, white;
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
    sim::Formation formation = sim::Formation::Auto;
    float drag_x = 0, drag_y = 0;
    bool dragging = false;
    std::vector<double> sim_times, draw_times;   // a mérésben külön: szimulációs tick és rajzolás
    if (rc == 0) {
      std::printf("grafika: %s (legnagyobb textúra %d)\n", dev.info().c_str(), dev.max_texture_size());
      if (!batch.init(dev, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        rc = 1;
      } else if (demo.units > 0 && !demo.load(dev, o.assets, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        rc = 1;
      } else if (!o.world.empty()) {
        if (!session.load(o.content, o.world, 20261112, &err) || !terrain.init(dev, o.assets, &err)) {
          std::fprintf(stderr, "%s\n", err.c_str());
          rc = 1;
        } else {
          terrain.set_map(world.map(), world.terrain());
          if (!lighting.init(dev, &err)) std::fprintf(stderr, "%s\n", err.c_str());
          if (!effects.init(dev, &err)) std::fprintf(stderr, "%s\n", err.c_str());
          // meccs (M6), ha a pályán vannak kezdőhelyek; különben a bemutató seregei
          if (session.has_starts()) {
            if (!session.start_match({"ursan", "ursan"}, true, &err)) std::fprintf(stderr, "meccs: %s\n", err.c_str());
          } else {
            session.spawn_demo(2, o.demo > 0 ? o.demo : 60);
          }
          if (!o.atlas.empty()) world_atlas_ok = world_atlas.load(dev, o.atlas, &err) && world.prepare_atlas(world_atlas, &err);
          if (!o.atlas.empty() && !world_atlas_ok) std::fprintf(stderr, "atlasz nélkül: %s\n", err.c_str());
          fallback = dev.create_array_from(circle_image(32), false);
          render::Image px;
          px.width = px.height = 1;
          px.rgba = {255, 255, 255, 255};
          white = dev.create_array_from(px, false);
          const float mw = static_cast<float>(world.map().width()) * client::World::kTilePx;
          const float mh = static_cast<float>(world.map().height()) * client::World::kTilePx;
          if (o.zoom > 0) cam.zoom = std::clamp(o.zoom, 1.0f, 2.0f);
          cam.x = std::max(0.0f, mw / 2 - static_cast<float>(o.width) / 2 / cam.zoom);
          cam.y = std::max(0.0f, mh / 2 - static_cast<float>(o.height) / 2 / cam.zoom);
          if (o.look_x >= 0 && o.look_y >= 0) {
            cam.x = o.look_x * client::World::kTilePx - static_cast<float>(o.width) / 2 / cam.zoom;
            cam.y = o.look_y * client::World::kTilePx - static_cast<float>(o.height) / 2 / cam.zoom;
          }
          if ((o.bench || !o.screenshot.empty()) && !session.has_starts()) session.order_demo_attack();
          have_world = true;
          // képernyőképnél a szimuláció szinkron (a kép képkockára megismételhető); különben külön szálon
          if (!o.sync && o.screenshot.empty()) runner.start();
        }
      } else if (!o.atlas.empty() && o.demo > 0 && !adem.load(dev, o.atlas, o.demo, o.width, o.height, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        rc = 1;
      }
    }
    std::vector<double> times;
    bool running = rc == 0;
    long frame = 0;
    const auto start = std::chrono::steady_clock::now();
    while (running && (o.frames < 0 || frame < o.frames)) {
      SDL_Event ev;
      while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT || (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE)) running = false;
        if (!have_world) continue;
        if (ev.type == SDL_EVENT_MOUSE_WHEEL) {
          const float old = cam.zoom;
          cam.zoom = std::clamp(cam.zoom * (ev.wheel.y > 0 ? 1.12f : 1.0f / 1.12f), 1.0f, 2.0f);
          float mx = 0, my = 0;
          SDL_GetMouseState(&mx, &my);
          cam.x += mx / old - mx / cam.zoom;   // a kurzor alatti pont marad
          cam.y += my / old - my / cam.zoom;
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
          world.order_move(cam.x + ev.button.x / cam.zoom, cam.y + ev.button.y / cam.zoom, formation);
        }
        if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_V && have_world) {   // következő időjárás
          weather_from = weather_to;
          weather_to = static_cast<render::Weather>((static_cast<int>(weather_to) + 1) % static_cast<int>(render::Weather::Count));
          weather_since = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
          std::printf("időjárás: %s\n", render::weather_name(weather_to));
        }
        if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key >= SDLK_1 && ev.key.key <= SDLK_6) {
          formation = static_cast<sim::Formation>(ev.key.key - SDLK_1);   // 1 nincs, 2 WC3-szerű, 3 sor, 4 doboz …
        }
      }
      if (have_world) {   // kamera billentyűkkel
        const bool* keys = SDL_GetKeyboardState(nullptr);
        const float step = 12.0f / cam.zoom;
        if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) cam.x -= step;
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) cam.x += step;
        if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) cam.y -= step;
        if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) cam.y += step;
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
        for (const auto& fe : world.fx_events()) particles.emit(fe.death ? render::Effect::Death : render::Effect::Hit, fe.x, fe.y);
        world.fx_events().clear();
        if (wl.splashes > 0 && wl.precip == render::Precip::Rain) {
          const float area = (vx1 - cam.x) * (vy1 - cam.y) * cam.zoom * cam.zoom / (1280.0f * 720.0f);
          const float want = wl.splashes * wl.density / 0.55f * area * dt;
          for (int i = 0, n = static_cast<int>(want + fx_rand()); i < n; ++i) {
            particles.emit(render::Effect::Splash, cam.x + fx_rand() * (vx1 - cam.x), cam.y + fx_rand() * (vy1 - cam.y));
          }
        }
        particles.update(dt, wl.wind);
        lighting.resize(w, h);
        lighting.begin_scene(0.0f, 0.0f, 0.0f);
        terrain.draw(proj, cam.x, cam.y, vx1, vy1, t);
        render::Atlas* atl = world_atlas_ok ? &world_atlas : nullptr;
        lighting.begin_shadow_mask();   // az árnyékok külön maszkba (maximum-keveréssel), majd egyszer a terepre
        batch.begin_shadows(proj, day.shadow_x, day.shadow_y, day.shadow_alpha);   // a napszakkal forgó árnyék
        world.draw(batch, atl, fallback, cam, w, h, alpha, t, true);
        batch.end();
        lighting.apply_shadows();
        batch.begin(proj);
        world.draw(batch, atl, fallback, cam, w, h, alpha, t);
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
        lighting.composite(w, h, cam.x, cam.y, cam.zoom, client::World::kTilePx, wl, t, flash, o.post);
        // ideiglenes kijelzés (a HUD az M6 9. lépésében): az 1. játékos nyersanyagai és létszáma a címsorban
        if (frame % 15 == 0 && session.has_starts()) {
          if (const client::SnapPlayer* pl = world.player(1); pl && pl->active) {
            char title[160];
            std::snprintf(title, sizeof title, "TOTU: Sky Rift – fa %d · arany %d · meteorit %d · létszám %d/%d",
                          pl->res[0], pl->res[1], pl->res[2], pl->supply_used, pl->supply_cap);
            SDL_SetWindowTitle(win, title);
          }
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
      times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - f0).count());
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
    dev.destroy(white);
    adem.atlas.release();
    for (auto& s : demo.sheets) {
      dev.destroy(s.base);
      dev.destroy(s.mask);
    }
    batch.shutdown();
  }
  SDL_GL_DestroyContext(ctx);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return rc;
}
