// SPDX-License-Identifier: GPL-3.0-or-later
// TOTU-kliens (M5, 1. lépés): SDL3-ablak OpenGL 3.3 core környezettel, a saját grafikus réteggel (render/).
//   totu --version                 verzió és platform (ablak nélkül; a CI-ben is fut)
//   totu --frames N                N képkocka után kilép (füstteszt)
//   totu --demo N                  bemutató jelenet: N egység valódi sprite-okkal, csapatszínnel, animációval
//   totu --bench                   képkockaidő-mérés (függőleges szinkron nélkül, a GPU bevárásával): p50, p99
//   totu --screenshot kép.png      a futás végén képernyőkép (a képpróbákhoz)
//   totu --check                   a képernyőképnek legalább 2%-a nem háttérszín (különben 1-es kilépési kód; CI)
//   totu --hidden                  rejtett ablak (CI);  --size SZxM  ablakméret;  --assets mappa  a grafika helye
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "core/platform_info.hpp"
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
  std::string screenshot;
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
    else if (a == "--size") std::sscanf(next(), "%dx%d", &o.width, &o.height);
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
    demo.units = o.demo;
    if (rc == 0) {
      std::printf("grafika: %s (legnagyobb textúra %d)\n", dev.info().c_str(), dev.max_texture_size());
      if (!batch.init(dev, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        rc = 1;
      } else if (o.demo > 0 && !demo.load(dev, o.assets, &err)) {
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
      }
      const auto f0 = std::chrono::steady_clock::now();
      int w = 0, h = 0;
      SDL_GetWindowSizeInPixels(win, &w, &h);
      dev.bind_target(nullptr, w, h);
      dev.clear(0.12f, 0.16f, 0.10f, 1.0f);
      if (demo.units > 0) {
        // a bemutató determinisztikus időt használ, ha képernyőkép vagy mérés készül (azonos kép minden gépen)
        const double t = (o.bench || !o.screenshot.empty())
                             ? static_cast<double>(frame) / 60.0
                             : std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        batch.begin(render::ortho(0, static_cast<float>(w), static_cast<float>(h), 0));
        demo.draw(batch, t, w, h);
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
      if (!dev.check("képkocka", &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        rc = 1;
        running = false;
      }
      ++frame;
    }
    if (o.bench && times.size() > 10) {
      times.erase(times.begin(), times.begin() + 10);   // bemelegedés
      std::printf("mérés: %d egység, %ld képkocka, %d rajzoló hívás, %d sprite – p50 %.2f ms, p99 %.2f ms, max %.2f ms\n",
                  o.demo, frame, batch.draw_calls(), batch.sprites(), percentile(times, 0.5), percentile(times, 0.99),
                  percentile(times, 1.0));
    }
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
