// SPDX-License-Identifier: GPL-3.0-or-later
#include "render/rhi.hpp"

#include <cstring>

namespace totu::render {

Mat4 ortho(float l, float r, float b, float t) {
  Mat4 m{};
  m[0] = 2.0f / (r - l);
  m[5] = 2.0f / (t - b);
  m[10] = -1.0f;
  m[12] = -(r + l) / (r - l);
  m[13] = -(t + b) / (t - b);
  m[15] = 1.0f;
  return m;
}

bool Device::init(gl::Loader loader, std::string* error) {
  std::string missing;
  if (!gl::load(gl_, loader, &missing)) {
    if (error) *error = "hiányzó OpenGL-függvény: " + missing;
    return false;
  }
  gl_.GetIntegerv(gl::MAX_TEXTURE_SIZE, &max_texture_);
  gl_.PixelStorei(gl::UNPACK_ALIGNMENT, 1);
  gl_.PixelStorei(gl::PACK_ALIGNMENT, 1);
  gl_.Disable(gl::DEPTH_TEST);
  gl_.Disable(gl::CULL_FACE);
  gl_.Enable(gl::BLEND);
  // előszorzott alfa: a sprite-ok a shaderben szoroznak, így a szélek simák és a keverés sorrendfüggetlenebb
  gl_.BlendFuncSeparate(gl::ONE, gl::ONE_MINUS_SRC_ALPHA, gl::ONE, gl::ONE_MINUS_SRC_ALPHA);
  return check("init", error);
}

std::string Device::info() const {
  auto s = [&](gl::GLenum e) {
    const auto* p = gl_.GetString(e);
    return p ? std::string(reinterpret_cast<const char*>(p)) : std::string("?");
  };
  return s(gl::VENDOR) + " / " + s(gl::RENDERER) + " / OpenGL " + s(gl::VERSION);
}

Texture Device::create_texture(const Image& img, bool smooth) {
  Texture t;
  t.width = img.width;
  t.height = img.height;
  gl_.GenTextures(1, &t.id);
  gl_.BindTexture(gl::TEXTURE_2D, t.id);
  gl_.TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MIN_FILTER, smooth ? gl::LINEAR_MIPMAP_LINEAR : gl::NEAREST);
  gl_.TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MAG_FILTER, smooth ? gl::LINEAR : gl::NEAREST);
  gl_.TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_S, gl::CLAMP_TO_EDGE);
  gl_.TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_T, gl::CLAMP_TO_EDGE);
  gl_.TexImage2D(gl::TEXTURE_2D, 0, static_cast<gl::GLint>(gl::RGBA8), img.width, img.height, 0, gl::RGBA,
                 gl::UNSIGNED_BYTE, img.rgba.data());
  if (smooth) gl_.GenerateMipmap(gl::TEXTURE_2D);
  return t;
}

void Device::destroy(Texture& t) {
  if (t.id) gl_.DeleteTextures(1, &t.id);
  t = {};
}

namespace {

gl::GLuint compile(const gl::Api& gl, gl::GLenum type, const char* src, std::string* error) {
  const gl::GLuint s = gl.CreateShader(type);
  gl.ShaderSource(s, 1, &src, nullptr);
  gl.CompileShader(s);
  gl::GLint ok = 0;
  gl.GetShaderiv(s, gl::COMPILE_STATUS, &ok);
  if (!ok) {
    gl::GLint len = 0;
    gl.GetShaderiv(s, gl::INFO_LOG_LENGTH, &len);
    std::string log(static_cast<size_t>(len > 0 ? len : 1), '\0');
    gl.GetShaderInfoLog(s, len, nullptr, log.data());
    if (error) *error = std::string(type == gl::VERTEX_SHADER ? "csúcsárnyaló" : "képpontárnyaló") + ": " + log;
    gl.DeleteShader(s);
    return 0;
  }
  return s;
}

}  // namespace

Program Device::create_program(const char* vs, const char* fs, std::string* error) {
  Program p;
  const gl::GLuint v = compile(gl_, gl::VERTEX_SHADER, vs, error);
  if (!v) return p;
  const gl::GLuint f = compile(gl_, gl::FRAGMENT_SHADER, fs, error);
  if (!f) {
    gl_.DeleteShader(v);
    return p;
  }
  p.id = gl_.CreateProgram();
  gl_.AttachShader(p.id, v);
  gl_.AttachShader(p.id, f);
  gl_.LinkProgram(p.id);
  gl_.DeleteShader(v);
  gl_.DeleteShader(f);
  gl::GLint ok = 0;
  gl_.GetProgramiv(p.id, gl::LINK_STATUS, &ok);
  if (!ok) {
    gl::GLint len = 0;
    gl_.GetProgramiv(p.id, gl::INFO_LOG_LENGTH, &len);
    std::string log(static_cast<size_t>(len > 0 ? len : 1), '\0');
    gl_.GetProgramInfoLog(p.id, len, nullptr, log.data());
    if (error) *error = "összeszerkesztés: " + log;
    gl_.DeleteProgram(p.id);
    p.id = 0;
  }
  return p;
}

void Device::destroy(Program& p) {
  if (p.id) gl_.DeleteProgram(p.id);
  p = {};
}

RenderTarget Device::create_target(int w, int h) {
  RenderTarget rt;
  Image blank;
  blank.width = w;
  blank.height = h;
  blank.rgba.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 0);
  rt.color = create_texture(blank, false);
  gl_.GenFramebuffers(1, &rt.fbo);
  gl_.BindFramebuffer(gl::FRAMEBUFFER, rt.fbo);
  gl_.FramebufferTexture2D(gl::FRAMEBUFFER, gl::COLOR_ATTACHMENT0, gl::TEXTURE_2D, rt.color.id, 0);
  if (gl_.CheckFramebufferStatus(gl::FRAMEBUFFER) != gl::FRAMEBUFFER_COMPLETE) {
    gl_.BindFramebuffer(gl::FRAMEBUFFER, 0);
    destroy(rt);
    return rt;
  }
  gl_.BindFramebuffer(gl::FRAMEBUFFER, 0);
  return rt;
}

void Device::destroy(RenderTarget& rt) {
  if (rt.fbo) gl_.DeleteFramebuffers(1, &rt.fbo);
  destroy(rt.color);
  rt = {};
}

void Device::bind_target(const RenderTarget* rt, int w, int h) {
  gl_.BindFramebuffer(gl::FRAMEBUFFER, rt ? rt->fbo : 0);
  gl_.Viewport(0, 0, w, h);
}

void Device::clear(float r, float g, float b, float a) {
  gl_.ClearColor(r, g, b, a);
  gl_.Clear(gl::COLOR_BUFFER_BIT);
}

Image Device::read_pixels(int w, int h) {
  Image img;
  img.width = w;
  img.height = h;
  img.rgba.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 0);
  gl_.ReadPixels(0, 0, w, h, gl::RGBA, gl::UNSIGNED_BYTE, img.rgba.data());
  // a GL alulról felfelé ad: megfordítjuk
  const size_t row = static_cast<size_t>(w) * 4;
  std::vector<uint8_t> tmp(row);
  for (int y = 0; y < h / 2; ++y) {
    uint8_t* a = img.rgba.data() + static_cast<size_t>(y) * row;
    uint8_t* b = img.rgba.data() + static_cast<size_t>(h - 1 - y) * row;
    std::memcpy(tmp.data(), a, row);
    std::memcpy(a, b, row);
    std::memcpy(b, tmp.data(), row);
  }
  return img;
}

bool Device::check(const char* where, std::string* error) {
  const gl::GLenum e = gl_.GetError();
  if (e == gl::NO_ERROR) return true;
  if (error) *error = std::string("OpenGL-hiba (") + where + "): " + std::to_string(e);
  return false;
}

}  // namespace totu::render
