// SPDX-License-Identifier: GPL-3.0-or-later
// Vékony grafikus réteg (RHI, ADR-003): textúra, shader-program, render-célpont – csak ami a 2D megjelenítőhöz
// kell. Első háttér: OpenGL 3.3 core; a második (SDL3 GPU) ugyanezt a felületet valósítja meg (M5 után).
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "render/gl.hpp"
#include "render/image.hpp"

namespace totu::render {

struct Texture {
  gl::GLuint id = 0;
  int width = 0, height = 0;
  int layers = 0;              // > 0: tömbtextúra (a sprite-köteg ezt használja)
  bool single_channel = false; // R8 (csapatmaszk: csak az alfa)
};

struct Program {
  gl::GLuint id = 0;
};

struct RenderTarget {
  gl::GLuint fbo = 0;
  Texture color;
};

// Oszlopfolytonos 4 × 4-es mátrix (OpenGL).
using Mat4 = std::array<float, 16>;
Mat4 ortho(float left, float right, float bottom, float top);

class Device {
 public:
  // A GL-környezetnek már élnie kell (a hívó hozza létre, pl. SDL-lel).
  bool init(gl::Loader loader, std::string* error);
  const gl::Api& api() const { return gl_; }
  std::string info() const;     // gyártó, eszköz, GL-változat
  int max_texture_size() const { return max_texture_; }

  Texture create_texture(const Image& img, bool smooth = true);   // egyszerű 2D (render-célpont)
  // Tömbtextúra `layers` réteggel (RGBA8, vagy R8 a maszkhoz – abba a kép alfája kerül); a rétegeket utána egyenként
  // kell feltölteni, végül a finish_array készíti a mipmapeket.
  Texture create_array(int w, int h, int layers, bool single_channel);
  void upload_layer(const Texture& t, int layer, const Image& img);
  void finish_array(const Texture& t);
  Texture create_array_from(const Image& img, bool single_channel);   // egyrétegű tömb egy képből
  int max_array_layers() const { return max_layers_; }
  void destroy(Texture& t);
  Program create_program(const char* vertex_src, const char* fragment_src, std::string* error);
  void destroy(Program& p);
  RenderTarget create_target(int w, int h);
  void destroy(RenderTarget& rt);

  void bind_target(const RenderTarget* rt, int w, int h);   // nullptr: az ablak
  void clear(float r, float g, float b, float a);
  Image read_pixels(int w, int h);                           // a kötött célpontból, fentről lefelé
  bool check(const char* where, std::string* error);         // glGetError

 private:
  gl::Api gl_;
  int max_texture_ = 0, max_layers_ = 0;
};

}  // namespace totu::render
