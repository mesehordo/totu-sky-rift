// SPDX-License-Identifier: GPL-3.0-or-later
// TOTU-kliens, M1 platform-váz: SDL3-ablak OpenGL 3.3 core környezettel.
//   totu --version        verzió és platform kiírása (ablak nélkül; CI-ben is fut)
//   totu --frames N       N képkocka után kilép (füstteszt kijelzős gépen)
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/platform_info.hpp"

namespace {

// Csak a két szükséges GL-függvény, a meghajtótól lekérve (nincs szükség GL-fejlécre).
using ClearColorFn = void (*)(float, float, float, float);
using ClearFn = void (*)(unsigned int);
constexpr unsigned int GL_COLOR_BUFFER_BIT = 0x00004000;

void print_version() {
  std::printf("TOTU: Sky Rift %s – %s/%s, %s, SDL %d.%d.%d\n", totu::core::version(), totu::core::os_name(),
              totu::core::cpu_arch(), totu::core::compiler().c_str(), SDL_VERSIONNUM_MAJOR(SDL_GetVersion()),
              SDL_VERSIONNUM_MINOR(SDL_GetVersion()), SDL_VERSIONNUM_MICRO(SDL_GetVersion()));
}

}  // namespace

int main(int argc, char** argv) {
  long max_frames = -1;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--version") == 0) {
      print_version();
      return 0;
    }
    if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
      max_frames = std::strtol(argv[++i], nullptr, 10);
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
  SDL_Window* win = SDL_CreateWindow("TOTU: Sky Rift", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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
  SDL_GL_SetSwapInterval(1);
  auto clear_color = reinterpret_cast<ClearColorFn>(SDL_GL_GetProcAddress("glClearColor"));
  auto clear = reinterpret_cast<ClearFn>(SDL_GL_GetProcAddress("glClear"));
  if (!clear_color || !clear) {
    std::fprintf(stderr, "Hiányzó OpenGL-függvények\n");
    return 1;
  }

  bool running = true;
  long frame = 0;
  while (running && (max_frames < 0 || frame < max_frames)) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_EVENT_QUIT || (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE)) {
        running = false;
      }
    }
    clear_color(0.10f, 0.08f, 0.06f, 1.0f);
    clear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapWindow(win);
    ++frame;
  }
  SDL_GL_DestroyContext(ctx);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
