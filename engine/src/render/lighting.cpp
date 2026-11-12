// SPDX-License-Identifier: GPL-3.0-or-later
#include "render/lighting.hpp"

#include <algorithm>
#include <cmath>

namespace totu::render {

namespace {

const char* kVertex = R"(#version 330 core
out vec2 v_uv;
void main() {
  // egyetlen, a képernyőt lefedő háromszög
  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
  v_uv = p;
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

const char* kFragment = R"(#version 330 core
in vec2 v_uv;
uniform sampler2D u_scene;
uniform sampler2D u_light;
uniform sampler2D u_fog;
uniform int u_use_fog;
uniform vec4 u_cam;      // x, y, nagyítás, csempeméret (világ-képpont)
uniform vec2 u_screen;
uniform vec2 u_fogsize;  // a köd mérete csempében
out vec4 o_color;
void main() {
  vec3 scene = texture(u_scene, v_uv).rgb;
  vec3 light = texture(u_light, v_uv).rgb;
  vec3 c = scene * light;
  if (u_use_fog != 0) {
    // a képpont világkoordinátája (a képernyő teteje y = 0)
    vec2 px = vec2(v_uv.x, 1.0 - v_uv.y) * u_screen;
    vec2 world = u_cam.xy + px / u_cam.z;
    float f = texture(u_fog, world / (u_cam.w * u_fogsize)).r;
    float lum = dot(c, vec3(0.299, 0.587, 0.114));
    vec3 explored = mix(vec3(lum), c, 0.45) * 0.5;               // felderített: fakó és sötétebb
    vec3 unknown = vec3(0.02, 0.025, 0.035);
    c = f < 0.5 ? mix(unknown, explored, smoothstep(0.0, 0.5, f)) : mix(explored, c, smoothstep(0.5, 1.0, f));
  }
  o_color = vec4(c, 1.0);
}
)";

Image glow_image(int n) {
  Image img;
  img.width = img.height = n;
  img.rgba.assign(static_cast<size_t>(n) * static_cast<size_t>(n) * 4, 0);
  const float r = static_cast<float>(n) / 2.0f;
  for (int y = 0; y < n; ++y) {
    for (int x = 0; x < n; ++x) {
      const float dx = (static_cast<float>(x) + 0.5f - r) / r, dy = (static_cast<float>(y) + 0.5f - r) / r;
      const float d = std::sqrt(dx * dx + dy * dy);
      const float a = std::clamp(1.0f - d, 0.0f, 1.0f);
      const auto v = static_cast<uint8_t>(a * a * 255.0f);   // négyzetes lecsengés
      uint8_t* p = img.at(x, y);
      p[0] = p[1] = p[2] = v;
      p[3] = v;
    }
  }
  return img;
}

}  // namespace

bool Lighting::init(Device& dev, std::string* error) {
  dev_ = &dev;
  prog_ = dev.create_program(kVertex, kFragment, error);
  if (!prog_.id) return false;
  const gl::Api& gl = dev.api();
  loc_scene_ = gl.GetUniformLocation(prog_.id, "u_scene");
  loc_light_ = gl.GetUniformLocation(prog_.id, "u_light");
  loc_fog_ = gl.GetUniformLocation(prog_.id, "u_fog");
  loc_use_fog_ = gl.GetUniformLocation(prog_.id, "u_use_fog");
  loc_cam_ = gl.GetUniformLocation(prog_.id, "u_cam");
  loc_screen_ = gl.GetUniformLocation(prog_.id, "u_screen");
  loc_fogsize_ = gl.GetUniformLocation(prog_.id, "u_fogsize");
  gl.GenVertexArrays(1, &vao_);
  glow_ = dev.create_array_from(glow_image(128), false);
  return dev.check("fényréteg", error);
}

void Lighting::shutdown() {
  if (!dev_) return;
  dev_->destroy(scene_);
  dev_->destroy(light_);
  dev_->destroy(glow_);
  dev_->destroy(fog_);
  dev_->destroy(prog_);
  if (vao_) dev_->api().DeleteVertexArrays(1, &vao_);
  vao_ = 0;
}

void Lighting::resize(int w, int h) {
  if (w == w_ && h == h_) return;
  dev_->destroy(scene_);
  dev_->destroy(light_);
  w_ = w;
  h_ = h;
  scene_ = dev_->create_target(w, h);
  light_ = dev_->create_target(std::max(1, w / 2), std::max(1, h / 2));
  dev_->set_linear(light_.color, true);
}

void Lighting::begin_scene(float r, float g, float b) {
  dev_->bind_target(&scene_, w_, h_);
  dev_->clear(r, g, b, 1.0f);
}

void Lighting::draw_lights(SpriteBatch& batch, const Mat4& projection, const float ambient[3],
                           const std::vector<Light>& lights, float strength) {
  dev_->bind_target(&light_, std::max(1, w_ / 2), std::max(1, h_ / 2));
  dev_->clear(ambient[0], ambient[1], ambient[2], 1.0f);
  if (lights.empty() || strength <= 0.01f) return;
  const gl::Api& gl = dev_->api();
  gl.BlendFuncSeparate(gl::ONE, gl::ONE, gl::ONE, gl::ONE);   // a fények összeadódnak
  batch.begin(projection);
  for (const Light& l : lights) {
    SpriteInstance in;
    in.x = l.x - l.radius;
    in.y = l.y - l.radius;
    in.w = in.h = l.radius * 2;
    const float k = std::clamp(l.intensity * strength, 0.0f, 1.0f);
    in.tint[0] = static_cast<uint8_t>(static_cast<float>(l.r) * k);
    in.tint[1] = static_cast<uint8_t>(static_cast<float>(l.g) * k);
    in.tint[2] = static_cast<uint8_t>(static_cast<float>(l.b) * k);
    in.tint[3] = 255;
    batch.draw(glow_, nullptr, in);
  }
  batch.end();
  gl.BlendFuncSeparate(gl::ONE, gl::ONE_MINUS_SRC_ALPHA, gl::ONE, gl::ONE_MINUS_SRC_ALPHA);
}

void Lighting::set_fog(int map_w, int map_h, const std::vector<uint8_t>& fog) {
  use_fog_ = !fog.empty();
  if (!use_fog_) return;
  if (map_w != fog_w_ || map_h != fog_h_) {
    dev_->destroy(fog_);
    fog_ = dev_->create_r8(map_w, map_h);
    fog_w_ = map_w;
    fog_h_ = map_h;
  }
  dev_->update_r8(fog_, fog.data());
}

void Lighting::composite(int sw, int sh, float cam_x, float cam_y, float zoom, float tile_px) {
  dev_->bind_target(nullptr, sw, sh);
  const gl::Api& gl = dev_->api();
  gl.Disable(gl::BLEND);
  gl.UseProgram(prog_.id);
  gl.Uniform1i(loc_scene_, 0);
  gl.Uniform1i(loc_light_, 1);
  gl.Uniform1i(loc_fog_, 2);
  gl.Uniform1i(loc_use_fog_, use_fog_ ? 1 : 0);
  gl.Uniform4f(loc_cam_, cam_x, cam_y, zoom, tile_px);
  gl.Uniform2f(loc_screen_, static_cast<float>(sw), static_cast<float>(sh));
  gl.Uniform2f(loc_fogsize_, static_cast<float>(std::max(1, fog_w_)), static_cast<float>(std::max(1, fog_h_)));
  gl.ActiveTexture(gl::TEXTURE0);
  gl.BindTexture(gl::TEXTURE_2D, scene_.color.id);
  gl.ActiveTexture(gl::TEXTURE0 + 1);
  gl.BindTexture(gl::TEXTURE_2D, light_.color.id);
  gl.ActiveTexture(gl::TEXTURE0 + 2);
  gl.BindTexture(gl::TEXTURE_2D, fog_.id);
  gl.BindVertexArray(vao_);
  gl.DrawArrays(gl::TRIANGLES, 0, 3);
  gl.Enable(gl::BLEND);
}

}  // namespace totu::render
