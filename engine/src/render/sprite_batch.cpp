// SPDX-License-Identifier: GPL-3.0-or-later
#include "render/sprite_batch.hpp"

#include <cstddef>

namespace totu::render {

namespace {

const char* kVertex = R"(#version 330 core
layout(location = 0) in vec4 a_rect;   // x, y, w, h
layout(location = 1) in vec4 a_uv;     // u0, v0, u1, v1
layout(location = 2) in vec4 a_tint;
layout(location = 3) in vec4 a_team;
layout(location = 4) in vec4 a_layer;
layout(location = 5) in float a_foot;
uniform mat4 u_proj;
out vec3 v_uv;
out vec4 v_tint;
out vec4 v_team;
const vec2 corners[6] = vec2[6](vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 0), vec2(1, 1), vec2(0, 1));
void main() {
  vec2 c = corners[gl_VertexID % 6];
  gl_Position = u_proj * vec4(a_rect.xy + c * a_rect.zw, 0.0, 1.0);
  v_uv = vec3(mix(a_uv.xy, a_uv.zw, c), a_layer.x);
  v_tint = a_tint;
  v_team = a_team;
}
)";

const char* kFragment = R"(#version 330 core
in vec3 v_uv;
in vec4 v_tint;
in vec4 v_team;
uniform sampler2DArray u_base;
uniform sampler2DArray u_mask;
uniform int u_use_mask;
out vec4 o_color;
void main() {
  vec4 c = texture(u_base, v_uv);                 // előszorzott alfa
  if (c.a <= 0.002) discard;
  if (u_use_mask != 0 && v_team.a > 0.0) {
    float m = texture(u_mask, v_uv).r * v_team.a;   // R8 maszk
    vec3 straight = c.rgb / c.a;
    // „shade” mód (a meglévő grafika szerint): a maszk alatti, szürkére festett részt szorozza a csapatszínnel
    c.rgb = mix(straight, straight * v_team.rgb, m) * c.a;
  }
  o_color = c * vec4(v_tint.rgb * v_tint.a, v_tint.a);
}
)";

// árnyék: a csúcsokat a talpvonalhoz képest vetítjük (a talp feletti magasság × napirány)
const char* kShadowVertex = R"(#version 330 core
layout(location = 0) in vec4 a_rect;
layout(location = 1) in vec4 a_uv;
layout(location = 4) in vec4 a_layer;
layout(location = 5) in float a_foot;
uniform mat4 u_proj;
uniform vec4 u_sun;   // x, y: vetület magasság-képpontonként; z: erősség
out vec3 v_uv;
out float v_fade;
const vec2 corners[6] = vec2[6](vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 0), vec2(1, 1), vec2(0, 1));
void main() {
  vec2 c = corners[gl_VertexID % 6];
  vec2 p = a_rect.xy + c * a_rect.zw;
  float foot = a_foot > 0.0 ? a_foot : a_rect.y + a_rect.w;
  float rel = max(foot - p.y, 0.0);                       // a talp feletti magasság
  float height = max(foot - a_rect.y, 1.0);
  gl_Position = u_proj * vec4(p.x + rel * u_sun.x, foot + rel * u_sun.y, 0.0, 1.0);
  v_uv = vec3(mix(a_uv.xy, a_uv.zw, c), a_layer.x);
  v_fade = rel / height;
}
)";

const char* kShadowFragment = R"(#version 330 core
in vec3 v_uv;
in float v_fade;
uniform sampler2DArray u_base;
uniform vec4 u_sun;
out vec4 o_color;
void main() {
  float a = texture(u_base, v_uv, 1.5).a;                  // elmosott mipmap-szint: puha szél
  a = smoothstep(0.15, 0.6, a) * u_sun.z * mix(1.0, 0.55, v_fade);   // a végén halványabb
  if (a <= 0.003) discard;
  o_color = vec4(vec3(8.0, 10.0, 14.0) / 255.0 * a, a);
}
)";

}  // namespace

void premultiply(Image& img) {
  const size_t n = static_cast<size_t>(img.width) * static_cast<size_t>(img.height);
  for (size_t i = 0; i < n; ++i) {
    uint8_t* p = &img.rgba[i * 4];
    const uint32_t a = p[3];
    p[0] = static_cast<uint8_t>((p[0] * a + 127) / 255);
    p[1] = static_cast<uint8_t>((p[1] * a + 127) / 255);
    p[2] = static_cast<uint8_t>((p[2] * a + 127) / 255);
  }
}

bool SpriteBatch::init(Device& dev, std::string* error) {
  dev_ = &dev;
  prog_ = dev.create_program(kVertex, kFragment, error);
  if (!prog_.id) return false;
  const gl::Api& gl = dev.api();
  loc_proj_ = gl.GetUniformLocation(prog_.id, "u_proj");
  loc_base_ = gl.GetUniformLocation(prog_.id, "u_base");
  loc_mask_ = gl.GetUniformLocation(prog_.id, "u_mask");
  loc_use_mask_ = gl.GetUniformLocation(prog_.id, "u_use_mask");
  shadow_prog_ = dev.create_program(kShadowVertex, kShadowFragment, error);
  if (!shadow_prog_.id) return false;
  s_proj_ = gl.GetUniformLocation(shadow_prog_.id, "u_proj");
  s_base_ = gl.GetUniformLocation(shadow_prog_.id, "u_base");
  s_sun_ = gl.GetUniformLocation(shadow_prog_.id, "u_sun");
  gl.GenVertexArrays(1, &vao_);
  gl.GenBuffers(1, &vbo_);
  gl.BindVertexArray(vao_);
  gl.BindBuffer(gl::ARRAY_BUFFER, vbo_);
  gl.BufferData(gl::ARRAY_BUFFER, static_cast<gl::GLsizeiptr>(kCapacity * sizeof(SpriteInstance)), nullptr, gl::STREAM_DRAW);
  const auto stride = static_cast<gl::GLsizei>(sizeof(SpriteInstance));
  auto attr = [&](gl::GLuint i, gl::GLint n, gl::GLenum type, bool norm, size_t off) {
    gl.EnableVertexAttribArray(i);
    gl.VertexAttribPointer(i, n, type, norm ? 1 : 0, stride, reinterpret_cast<const void*>(off));
    gl.VertexAttribDivisor(i, 1);
  };
  attr(0, 4, gl::FLOAT, false, offsetof(SpriteInstance, x));
  attr(1, 4, gl::FLOAT, false, offsetof(SpriteInstance, u0));
  attr(2, 4, gl::UNSIGNED_BYTE, true, offsetof(SpriteInstance, tint));
  attr(3, 4, gl::UNSIGNED_BYTE, true, offsetof(SpriteInstance, team));
  attr(4, 4, gl::UNSIGNED_BYTE, false, offsetof(SpriteInstance, layer));
  attr(5, 1, gl::FLOAT, false, offsetof(SpriteInstance, foot));
  gl.BindVertexArray(0);
  pending_.reserve(kCapacity);
  return dev.check("sprite-köteg", error);
}

void SpriteBatch::shutdown() {
  if (!dev_) return;
  const gl::Api& gl = dev_->api();
  if (vbo_) gl.DeleteBuffers(1, &vbo_);
  if (vao_) gl.DeleteVertexArrays(1, &vao_);
  dev_->destroy(prog_);
  dev_->destroy(shadow_prog_);
  vbo_ = vao_ = 0;
}

void SpriteBatch::begin_shadows(const Mat4& projection, float sun_x, float sun_y, float alpha) {
  begin(projection);
  shadow_ = true;
  sun_x_ = sun_x;
  sun_y_ = sun_y;
  shadow_alpha_ = alpha;
}

void SpriteBatch::begin(const Mat4& projection) {
  shadow_ = false;
  proj_ = projection;
  draw_calls_ = sprites_ = 0;
  pending_.clear();
  base_ = mask_ = 0;
}

void SpriteBatch::draw(const Texture& base, const Texture* mask, const SpriteInstance& s) {
  const gl::GLuint m = mask ? mask->id : 0;
  if ((base.id != base_ || m != mask_ || pending_.size() >= kCapacity) && !pending_.empty()) flush();
  base_ = base.id;
  mask_ = m;
  pending_.push_back(s);
}

void SpriteBatch::end() {
  if (!pending_.empty()) flush();
}

void SpriteBatch::flush() {
  const gl::Api& gl = dev_->api();
  if (shadow_) {
    gl.UseProgram(shadow_prog_.id);
    gl.UniformMatrix4fv(s_proj_, 1, 0, proj_.data());
    gl.Uniform1i(s_base_, 0);
    gl.Uniform4f(s_sun_, sun_x_, sun_y_, shadow_alpha_, 0.0f);
  } else {
    gl.UseProgram(prog_.id);
    gl.UniformMatrix4fv(loc_proj_, 1, 0, proj_.data());
    gl.Uniform1i(loc_base_, 0);
    gl.Uniform1i(loc_mask_, 1);
    gl.Uniform1i(loc_use_mask_, mask_ ? 1 : 0);
  }
  gl.ActiveTexture(gl::TEXTURE0);
  gl.BindTexture(gl::TEXTURE_2D_ARRAY, base_);
  gl.ActiveTexture(gl::TEXTURE0 + 1);
  gl.BindTexture(gl::TEXTURE_2D_ARRAY, mask_ ? mask_ : base_);
  gl.BindVertexArray(vao_);
  gl.BindBuffer(gl::ARRAY_BUFFER, vbo_);
  // a puffer „árvává tétele” (orphaning): a meghajtó nem vár a korábbi rajzolásra
  gl.BufferData(gl::ARRAY_BUFFER, static_cast<gl::GLsizeiptr>(kCapacity * sizeof(SpriteInstance)), nullptr, gl::STREAM_DRAW);
  gl.BufferSubData(gl::ARRAY_BUFFER, 0, static_cast<gl::GLsizeiptr>(pending_.size() * sizeof(SpriteInstance)), pending_.data());
  gl.DrawArraysInstanced(gl::TRIANGLES, 0, 6, static_cast<gl::GLsizei>(pending_.size()));
  ++draw_calls_;
  sprites_ += static_cast<int>(pending_.size());
  pending_.clear();
}

}  // namespace totu::render
