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
uniform int u_post;
uniform vec4 u_cam;      // x, y, nagyítás, csempeméret (világ-képpont)
uniform vec2 u_screen;
uniform vec2 u_fogsize;  // a köd mérete csempében
uniform vec4 u_grade;    // telítettség, kontraszt, vignetta, villám
uniform vec4 u_haze;     // pára színe, fedése
uniform vec4 u_fx;       // felhőárnyék, hőremegés, idő (s), szél
uniform vec4 u_precip;   // fajta (0 nincs, 1 eső, 2 hó, 3 homok, 4 hamu), sűrűség, sebesség, csíkhossz
uniform vec4 u_pcolor;   // a csapadék színe, fedése
uniform vec4 u_pdir;     // esési irány (x, y), méret
out vec4 o_color;

float hash12(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}
float vnoise(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash12(i), hash12(i + vec2(1.0, 0.0)), u.x), mix(hash12(i + vec2(0.0, 1.0)), hash12(i + vec2(1.0, 1.0)), u.x), u.y);
}
float fbm(vec2 p) { return vnoise(p) * 0.55 + vnoise(p * 2.03 + 7.1) * 0.3 + vnoise(p * 4.1 + 3.7) * 0.15; }

// csapadék: három réteg, a kamerához rögzítve (a réteg mélysége szerint kissé eltérő elmozdulással)
float precip(vec2 px, float time) {
  vec2 dir = u_pdir.xy;
  vec2 side = vec2(-dir.y, dir.x);
  bool streak = u_precip.w > 0.5;
  float acc = 0.0;
  for (int i = 0; i < 3; ++i) {
    float fi = float(i);
    float depth = 0.6 + 0.3 * fi;
    vec2 s = px + u_cam.xy * u_cam.z * (0.8 + 0.2 * fi);
    float along = dot(s, dir) - time * u_precip.z * depth;
    float across = dot(s, side);
    vec2 cell = (streak ? vec2(14.0, 90.0) : vec2(34.0, 34.0)) * (0.8 + 0.25 * fi);
    vec2 q = vec2(across, along) / cell + vec2(fi * 13.7, fi * 7.3);
    vec2 id = floor(q);
    vec2 f = fract(q);
    float h = hash12(id + fi * 31.0);
    if (h > u_precip.y) continue;
    float h2 = hash12(id + 17.0 + fi), h3 = hash12(id + 53.0 + fi);
    if (streak) {
      float x0 = 0.2 + 0.6 * h2, y0 = 0.05 + 0.45 * h3;
      float dx = abs(f.x - x0) * cell.x;
      float t = (f.y - y0) * cell.y / (u_precip.w * depth);
      float w = u_pdir.z * depth;
      acc += (1.0 - smoothstep(w * 0.5, w * 0.5 + 0.9, dx)) * step(0.0, t) * step(t, 1.0) * t * (0.5 + 0.5 * depth);
    } else {
      vec2 d = (f - vec2(0.2 + 0.6 * h2, 0.2 + 0.6 * h3)) * cell;
      d.x += sin(time * (1.1 + h3) + h * 6.28) * 3.5;
      float r = u_pdir.z * (0.7 + 0.6 * h2) * depth;
      acc += 1.0 - smoothstep(r * 0.6, r + 0.8, length(d));
    }
  }
  return clamp(acc, 0.0, 1.0);
}

void main() {
  vec2 px = vec2(v_uv.x, 1.0 - v_uv.y) * u_screen;   // a képernyő teteje y = 0
  vec2 world = u_cam.xy + px / u_cam.z;
  float time = u_fx.z;
  vec2 uv = v_uv;
  if (u_post != 0 && u_fx.y > 0.0) {   // hőremegés
    uv += vec2(sin(world.y * 0.09 + time * 2.7), cos(world.x * 0.07 + time * 2.1)) * vec2(0.0012, 0.0008) * u_fx.y;
  }
  vec3 scene = texture(u_scene, uv).rgb;
  vec3 light = texture(u_light, uv).rgb;
  vec3 c = scene * light;
  if (u_post != 0) {
    if (u_fx.x > 0.0) {   // vándorló felhőárnyék
      float n = fbm(world * 0.0022 + vec2(time * 0.0016 * u_fx.w, time * 0.004));
      c *= 1.0 - u_fx.x * smoothstep(0.45, 0.7, n);
    }
    if (u_haze.a > 0.0) {   // foltos pára: a világban vándorol, a fény színezi
      float n = fbm(world * 0.004 + vec2(time * (0.02 + u_fx.w * 0.002), time * 0.006));
      c = mix(c, u_haze.rgb * light, clamp(u_haze.a * (0.65 + 0.7 * n), 0.0, 1.0));
    }
  }
  float f = 1.0;
  if (u_use_fog != 0) {
    f = texture(u_fog, world / (u_cam.w * u_fogsize)).r;
    float lum = dot(c, vec3(0.299, 0.587, 0.114));
    // felderített, de most nem látott (D41): szürkeárnyalatos és világos (jól látszik a táj); csak a felfedezetlen
    // teljesen fekete. A fényesség amúgy is megvan: plusz költség nincs.
    vec3 explored = vec3(lum * 0.8);
    vec3 unknown = vec3(0.02, 0.025, 0.035);
    c = f < 0.5 ? mix(unknown, explored, smoothstep(0.0, 0.5, f)) : mix(explored, c, smoothstep(0.5, 1.0, f));
  }
  if (u_post != 0) {
    if (u_precip.x > 0.5 && u_precip.y > 0.0) {
      float p = precip(px, time) * u_pcolor.a * (0.35 + 0.65 * f);
      float lit = clamp(dot(light, vec3(0.333)) * 1.1 + 0.15, 0.0, 1.2);
      c = mix(c, u_pcolor.rgb * lit, p);
    }
    c += vec3(0.8, 0.86, 1.0) * u_grade.w * 0.55;   // villám
    float lum = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(vec3(lum), c, u_grade.x);
    c = (c - 0.5) * u_grade.y + 0.5;
    vec2 d = v_uv - 0.5;
    c *= 1.0 - u_grade.z * dot(d, d) * 2.0;
  }
  o_color = vec4(clamp(c, 0.0, 1.0), 1.0);
}
)";

constexpr float kVignette = 0.22f;   // enyhe sötétedés a sarkokban

// az árnyékmaszk a terepre: előszorzott sötét szín a maszk erősségével
const char* kShadowApply = R"(#version 330 core
in vec2 v_uv;
uniform sampler2D u_shadow;
out vec4 o_color;
void main() {
  float a = texture(u_shadow, v_uv).a;
  o_color = vec4(vec3(8.0, 10.0, 14.0) / 255.0 * a, a);
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
  loc_post_ = gl.GetUniformLocation(prog_.id, "u_post");
  loc_grade_ = gl.GetUniformLocation(prog_.id, "u_grade");
  loc_haze_ = gl.GetUniformLocation(prog_.id, "u_haze");
  loc_fx_ = gl.GetUniformLocation(prog_.id, "u_fx");
  loc_precip_ = gl.GetUniformLocation(prog_.id, "u_precip");
  loc_pcolor_ = gl.GetUniformLocation(prog_.id, "u_pcolor");
  loc_pdir_ = gl.GetUniformLocation(prog_.id, "u_pdir");
  shadow_prog_ = dev.create_program(kVertex, kShadowApply, error);
  if (!shadow_prog_.id) return false;
  loc_shadow_ = gl.GetUniformLocation(shadow_prog_.id, "u_shadow");
  gl.GenVertexArrays(1, &vao_);
  glow_ = dev.create_array_from(glow_image(128), false);
  return dev.check("fényréteg", error);
}

void Lighting::shutdown() {
  if (!dev_) return;
  dev_->destroy(scene_);
  dev_->destroy(light_);
  dev_->destroy(shadow_);
  dev_->destroy(shadow_prog_);
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
  dev_->destroy(shadow_);
  w_ = w;
  h_ = h;
  scene_ = dev_->create_target(w, h);
  light_ = dev_->create_target(std::max(1, w / 2), std::max(1, h / 2));
  dev_->set_linear(light_.color, true);
  shadow_ = dev_->create_target(std::max(1, w / 2), std::max(1, h / 2));
  dev_->set_linear(shadow_.color, true);
}

void Lighting::begin_scene(float r, float g, float b) {
  dev_->bind_target(&scene_, w_, h_);
  dev_->clear(r, g, b, 1.0f);
}

void Lighting::begin_shadow_mask() {
  dev_->bind_target(&shadow_, std::max(1, w_ / 2), std::max(1, h_ / 2));
  dev_->clear(0.0f, 0.0f, 0.0f, 0.0f);
  const gl::Api& gl = dev_->api();
  gl.BlendEquation(gl::MAX);
  gl.BlendFuncSeparate(gl::ONE, gl::ONE, gl::ONE, gl::ONE);
}

void Lighting::apply_shadows() {
  const gl::Api& gl = dev_->api();
  gl.BlendEquation(gl::FUNC_ADD);
  gl.BlendFuncSeparate(gl::ONE, gl::ONE_MINUS_SRC_ALPHA, gl::ONE, gl::ONE_MINUS_SRC_ALPHA);
  dev_->bind_target(&scene_, w_, h_);
  gl.UseProgram(shadow_prog_.id);
  gl.Uniform1i(loc_shadow_, 0);
  gl.ActiveTexture(gl::TEXTURE0);
  gl.BindTexture(gl::TEXTURE_2D, shadow_.color.id);
  gl.BindVertexArray(vao_);
  gl.DrawArrays(gl::TRIANGLES, 0, 3);
}

void Lighting::draw_lights(SpriteBatch& batch, const Mat4& projection, const float ambient[3],
                           const std::vector<Light>& lights, float strength, const std::function<void()>& extra) {
  dev_->bind_target(&light_, std::max(1, w_ / 2), std::max(1, h_ / 2));
  dev_->clear(ambient[0], ambient[1], ambient[2], 1.0f);
  const bool own = !lights.empty() && strength > 0.01f;
  if (!own && !extra) return;
  const gl::Api& gl = dev_->api();
  gl.BlendFuncSeparate(gl::ONE, gl::ONE, gl::ONE, gl::ONE);   // a fények összeadódnak
  if (extra) extra();
  if (own) {
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
  }
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

void Lighting::composite(int sw, int sh, float cam_x, float cam_y, float zoom, float tile_px, const WeatherLook& wl,
                         double time, float flash, bool post) {
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
  gl.Uniform1i(loc_post_, post ? 1 : 0);
  // az idő a shaderben float: a pontosság miatt egy órás körre vágva
  const auto tf = static_cast<float>(std::fmod(time, 3600.0));
  gl.Uniform4f(loc_grade_, wl.saturation, wl.contrast, kVignette, flash);
  gl.Uniform4f(loc_haze_, wl.haze[0], wl.haze[1], wl.haze[2], wl.haze_amount);
  gl.Uniform4f(loc_fx_, wl.clouds, wl.heat, tf, wl.wind);
  gl.Uniform4f(loc_precip_, static_cast<float>(wl.precip), wl.density, wl.speed, wl.length);
  gl.Uniform4f(loc_pcolor_, wl.color[0], wl.color[1], wl.color[2], wl.color[3]);
  gl.Uniform4f(loc_pdir_, wl.fall_x, wl.fall_y, wl.size, 0.0f);
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
