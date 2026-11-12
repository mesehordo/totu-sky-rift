// SPDX-License-Identifier: GPL-3.0-or-later
#include "render/terrain_view.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "content/json.hpp"
#include "render/sprite_batch.hpp"

namespace totu::render {

namespace {

const char* kVertex = R"(#version 330 core
layout(location = 0) in vec4 a_rect;
layout(location = 1) in vec4 a_uv;
layout(location = 2) in vec4 a_muv;
layout(location = 3) in vec4 a_tint;
layout(location = 4) in vec4 a_rim;
uniform mat4 u_proj;
out vec3 v_uv;
out vec2 v_muv;
out float v_has_mask;
out vec3 v_tint;
out vec4 v_rim;
const vec2 corners[6] = vec2[6](vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 0), vec2(1, 1), vec2(0, 1));
void main() {
  vec2 c = corners[gl_VertexID % 6];
  gl_Position = u_proj * vec4(a_rect.xy + c * a_rect.zw, 0.0, 1.0);
  v_uv = vec3(mix(a_uv.xy, a_uv.zw, c), a_tint.a * 255.0);
  v_muv = mix(a_muv.xy, a_muv.zw, c);
  v_has_mask = a_muv.x < 0.0 ? 0.0 : 1.0;
  v_tint = a_tint.rgb;
  v_rim = a_rim;
}
)";

const char* kFragment = R"(#version 330 core
in vec3 v_uv;
in vec2 v_muv;
in float v_has_mask;
in vec3 v_tint;
in vec4 v_rim;
uniform sampler2DArray u_tiles;
uniform sampler2DArray u_masks;
out vec4 o_color;
void main() {
  vec4 c = texture(u_tiles, v_uv);
  float a = v_has_mask > 0.5 ? texture(u_masks, vec3(v_muv, 0.0)).r : 1.0;
  if (a <= 0.002) discard;
  // festett perem a folt szélén (a régi játék szerint: a maszk 0,5-ös szintje körüli sáv)
  float band = v_rim.a > 0.0 ? clamp(1.0 - abs(a - 0.5) / 0.274, 0.0, 1.0) * v_rim.a : 0.0;
  vec3 rgb = mix(c.rgb * v_tint, v_rim.rgb, band);
  float alpha = a * c.a;
  o_color = vec4(rgb * alpha, alpha);
}
)";

constexpr int kTileSheetPx = 64;   // a csempekészletekben (HD) egy csempe

struct SetInfo {
  std::string json, name;
};

}  // namespace

bool TerrainView::init(Device& dev, const std::string& assets_dir, std::string* error) {
  dev_ = &dev;
  prog_ = dev.create_program(kVertex, kFragment, error);
  if (!prog_.id) return false;
  const gl::Api& gl = dev.api();
  loc_proj_ = gl.GetUniformLocation(prog_.id, "u_proj");
  loc_tiles_ = gl.GetUniformLocation(prog_.id, "u_tiles");
  loc_masks_ = gl.GetUniformLocation(prog_.id, "u_masks");
  gl.GenVertexArrays(1, &vao_);
  gl.GenBuffers(1, &vbo_);
  gl.BindVertexArray(vao_);
  gl.BindBuffer(gl::ARRAY_BUFFER, vbo_);
  const auto stride = static_cast<gl::GLsizei>(sizeof(Quad));
  auto attr = [&](gl::GLuint i, gl::GLint n, gl::GLenum type, bool norm, size_t off) {
    gl.EnableVertexAttribArray(i);
    gl.VertexAttribPointer(i, n, type, norm ? 1 : 0, stride, reinterpret_cast<const void*>(off));
    gl.VertexAttribDivisor(i, 1);
  };
  attr(0, 4, gl::FLOAT, false, offsetof(Quad, x));
  attr(1, 4, gl::FLOAT, false, offsetof(Quad, u0));
  attr(2, 4, gl::FLOAT, false, offsetof(Quad, m0));
  attr(3, 4, gl::UNSIGNED_BYTE, true, offsetof(Quad, tint));
  attr(4, 4, gl::UNSIGNED_BYTE, true, offsetof(Quad, rim));
  gl.BindVertexArray(0);

  // tájak: a csempekészletek (azonos méretű lapok) egy tömbben
  const auto dir = std::filesystem::path(assets_dir) / "sprites" / "tiles";
  const std::vector<SetInfo> sets = {{"tileset.json", "tileset"}, {"tileset_winter.json", "winter"},
                                     {"tileset_forest.json", "forest"}};
  std::vector<Image> images;
  for (const auto& si : sets) {
    std::ifstream in(dir / si.json, std::ios::binary);
    if (!in) continue;
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    const auto doc = content::parse(text);
    const content::Value* image = doc ? doc->find("image") : nullptr;
    const content::Value* rows = doc ? doc->find("rows") : nullptr;
    if (!image || !image->is_string() || !rows || !rows->is_object()) continue;
    auto img = load_png((dir / image->text()).string());
    if (!img || (!images.empty() && (img->width != images[0].width || img->height != images[0].height))) continue;
    std::vector<std::pair<std::string, int>> r;
    for (const auto& [name, v] : rows->members()) {
      const content::Value* row = v.find("row");
      const content::Value* var = v.find("variants");
      const int ri = row ? static_cast<int>(row->as_int().value_or(0)) : 0;
      const int vn = var ? static_cast<int>(var->as_int().value_or(1)) : 1;
      if (static_cast<int>(r.size()) <= ri) r.resize(static_cast<size_t>(ri) + 1, {"", 1});
      r[static_cast<size_t>(ri)] = {name, std::max(1, vn)};
    }
    sets_.push_back(si.name);
    rows_.push_back(std::move(r));
    images.push_back(std::move(*img));
  }
  auto masks = load_png((dir / "masks.png").string());
  if (images.empty() || !masks) {
    if (error) *error = "hiányzó csempekészlet vagy átmenet-maszk: " + dir.string();
    return false;
  }
  tiles_ = dev.create_array(images[0].width, images[0].height, static_cast<int>(images.size()), false);
  for (size_t i = 0; i < images.size(); ++i) {
    premultiply(images[i]);
    dev.upload_layer(tiles_, static_cast<int>(i), images[i]);
  }
  dev.finish_array(tiles_);
  masks_ = dev.create_array_from(*masks, true);
  return dev.check("terep", error);
}

void TerrainView::shutdown() {
  if (!dev_) return;
  const gl::Api& gl = dev_->api();
  if (vbo_) gl.DeleteBuffers(1, &vbo_);
  if (vao_) gl.DeleteVertexArrays(1, &vao_);
  dev_->destroy(prog_);
  dev_->destroy(tiles_);
  dev_->destroy(masks_);
  vbo_ = vao_ = 0;
}

int TerrainView::kind_of(const std::string& set, const std::string& row, const uint8_t tint[3], int prio) {
  int layer = 0;
  for (size_t i = 0; i < sets_.size(); ++i) {
    if (sets_[i] == set) layer = static_cast<int>(i);
  }
  int r = 0, variants = 1;
  const auto& rows = rows_[static_cast<size_t>(layer)];
  for (size_t i = 0; i < rows.size(); ++i) {
    if (rows[i].first == row) { r = static_cast<int>(i); variants = rows[i].second; }
  }
  for (size_t i = 0; i < kinds_.size(); ++i) {
    const Kind& k = kinds_[i];
    if (k.layer == layer && k.row == r && k.prio == prio && std::equal(k.tint, k.tint + 3, tint)) return static_cast<int>(i);
  }
  Kind k;
  k.layer = layer;
  k.row = r;
  k.variants = variants;
  k.prio = prio;
  std::copy(tint, tint + 3, k.tint);
  k.water = row == "water";
  k.rimmed = row == "dirt" || row == "sand";
  kinds_.push_back(k);
  return static_cast<int>(kinds_.size()) - 1;
}

void TerrainView::set_map(const map::Map& m, const map::TerrainTable& table) {
  kinds_.clear();
  w_ = m.width();
  h_ = m.height();
  std::vector<int> per_terrain(table.terrains().size(), 0);
  for (size_t i = 0; i < table.terrains().size(); ++i) {
    const auto& art = table.terrains()[i].art;
    per_terrain[i] = kind_of(art.set, art.row, art.tint, art.prio);
    if (!art.shore.empty()) {
      static const uint8_t white[3] = {255, 255, 255};
      const int shore = kind_of(art.set, art.shore, white, art.prio - 1);
      kinds_[static_cast<size_t>(per_terrain[i])].shore = shore;
    }
  }
  kind_.assign(static_cast<size_t>(w_) * static_cast<size_t>(h_), 0);
  level_.assign(kind_.size(), 0);
  cliff_.assign(kind_.size(), 0);
  for (int y = 0; y < h_; ++y) {
    for (int x = 0; x < w_; ++x) {
      const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w_) + static_cast<size_t>(x);
      kind_[i] = per_terrain[m.terrain(x, y)];
      level_[i] = static_cast<int8_t>(m.level(x, y));
      cliff_[i] = m.cliff(x, y) ? 1 : 0;
    }
  }
}

void TerrainView::emit(const Kind& k, int tx, int ty, int variant, int mask_index, int mask_row, double time,
                       const uint8_t shade[3]) {
  const float ts = static_cast<float>(tiles_.width);
  int v = variant % k.variants;
  if (k.water) v = (v + static_cast<int>(time * 3.0)) % k.variants;   // a víz változatai képkockaként
  Quad q{};
  q.x = static_cast<float>(tx) * kTilePx;
  q.y = static_cast<float>(ty) * kTilePx;
  q.w = q.h = kTilePx;
  const float inset = 0.5f / ts;   // fél texel befelé: a szomszéd csempe nem szivárog be
  q.u0 = static_cast<float>(v * kTileSheetPx) / ts + inset;
  q.v0 = static_cast<float>(k.row * kTileSheetPx) / ts + inset;
  q.u1 = static_cast<float>((v + 1) * kTileSheetPx) / ts - inset;
  q.v1 = static_cast<float>((k.row + 1) * kTileSheetPx) / ts - inset;
  if (mask_index < 0) {
    q.m0 = -1;
  } else {
    const float mw = static_cast<float>(masks_.width), mh = static_cast<float>(masks_.height);
    q.m0 = static_cast<float>(mask_index * kTileSheetPx) / mw;
    q.n0 = static_cast<float>(mask_row * kTileSheetPx) / mh;
    q.m1 = static_cast<float>((mask_index + 1) * kTileSheetPx) / mw;
    q.n1 = static_cast<float>((mask_row + 1) * kTileSheetPx) / mh;
  }
  for (int c = 0; c < 3; ++c) q.tint[c] = static_cast<uint8_t>(k.tint[c] * shade[c] / 255);
  q.tint[3] = static_cast<uint8_t>(k.layer);
  if (mask_index >= 0 && k.rimmed) {
    const bool dirt = rows_[static_cast<size_t>(k.layer)][static_cast<size_t>(k.row)].first == "dirt";
    q.rim[0] = dirt ? 52 : 96;
    q.rim[1] = dirt ? 36 : 80;
    q.rim[2] = dirt ? 22 : 50;
    q.rim[3] = 150;
  }
  quads_.push_back(q);
}

void TerrainView::draw(const Mat4& projection, float vx0, float vy0, float vx1, float vy1, double time) {
  quads_.clear();
  const int tx0 = std::max(0, static_cast<int>(vx0 / kTilePx) - 1), ty0 = std::max(0, static_cast<int>(vy0 / kTilePx) - 1);
  const int tx1 = std::min(w_ - 1, static_cast<int>(vx1 / kTilePx) + 1), ty1 = std::min(h_ - 1, static_cast<int>(vy1 / kTilePx) + 1);
  const int sides[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};                        // N, E, S, W → maszk 0..3
  const int corners[4][4] = {{1, -1, 0, 1}, {1, 1, 2, 1}, {-1, 1, 2, 3}, {-1, -1, 0, 3}};  // NE, SE, SW, NW → 4..7
  auto kind_at = [&](int x, int y) { return kind_[static_cast<size_t>(y) * static_cast<size_t>(w_) + static_cast<size_t>(x)]; };
  for (int y = ty0; y <= ty1; ++y) {
    for (int x = tx0; x <= tx1; ++x) {
      const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w_) + static_cast<size_t>(x);
      const Kind& base = kinds_[static_cast<size_t>(kind_[i])];
      // a szint árnyalata (D33): magaslat világosabb, mélység sötétebb; a szirtperem sötét
      int sh = 255 + level_[i] * 22;
      if (cliff_[i]) sh = 150;
      const uint8_t shade[3] = {static_cast<uint8_t>(std::clamp(sh, 0, 255)), static_cast<uint8_t>(std::clamp(sh, 0, 255)),
                                static_cast<uint8_t>(std::clamp(sh - (level_[i] > 0 ? 8 : 0), 0, 255))};
      const int variant = x * 7 + y * 3;
      emit(base, x, y, variant, -1, 0, time, shade);
      // a magasabb rangú szomszéd fajták, rang szerint növekvő sorrendben
      int higher[8];
      int nh = 0;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          const int nx = x + dx, ny = y + dy;
          if ((!dx && !dy) || nx < 0 || ny < 0 || nx >= w_ || ny >= h_) continue;
          const int k = kind_at(nx, ny);
          if (kinds_[static_cast<size_t>(k)].prio <= base.prio) continue;
          if (std::find(higher, higher + nh, k) == higher + nh) higher[nh++] = k;
        }
      }
      // legfeljebb 8 elem: beszúrásos rendezés (rang, majd fajtaszám szerint)
      auto before = [&](int a, int b) {
        const int pa = kinds_[static_cast<size_t>(a)].prio, pb = kinds_[static_cast<size_t>(b)].prio;
        return pa != pb ? pa < pb : a < b;
      };
      for (int i2 = 1; i2 < nh; ++i2) {
        const int v = higher[i2];
        int j = i2 - 1;
        while (j >= 0 && before(v, higher[j])) {
          higher[j + 1] = higher[j];
          --j;
        }
        higher[j + 1] = v;
      }
      for (int h = 0; h < nh; ++h) {
        const int K = higher[h];
        auto is_k = [&](int dx, int dy) {
          const int nx = x + dx, ny = y + dy;
          return nx >= 0 && ny >= 0 && nx < w_ && ny < h_ && kind_at(nx, ny) == K;
        };
        bool on[8] = {};
        for (int s = 0; s < 4; ++s) on[s] = is_k(sides[s][0], sides[s][1]);
        for (int c = 0; c < 4; ++c) on[4 + c] = is_k(corners[c][0], corners[c][1]) && !on[corners[c][2]] && !on[corners[c][3]];
        const Kind& kk = kinds_[static_cast<size_t>(K)];
        for (int mi = 0; mi < 8; ++mi) {
          if (!on[mi]) continue;
          if (kk.shore >= 0) emit(kinds_[static_cast<size_t>(kk.shore)], x, y, variant, mi, 1, time, shade);   // széles part
          emit(kk, x, y, variant, mi, 0, time, shade);
        }
      }
    }
  }
  last_quads_ = static_cast<int>(quads_.size());
  if (quads_.empty()) return;
  const gl::Api& gl = dev_->api();
  gl.UseProgram(prog_.id);
  gl.UniformMatrix4fv(loc_proj_, 1, 0, projection.data());
  gl.Uniform1i(loc_tiles_, 0);
  gl.Uniform1i(loc_masks_, 1);
  gl.ActiveTexture(gl::TEXTURE0);
  gl.BindTexture(gl::TEXTURE_2D_ARRAY, tiles_.id);
  gl.ActiveTexture(gl::TEXTURE0 + 1);
  gl.BindTexture(gl::TEXTURE_2D_ARRAY, masks_.id);
  gl.BindVertexArray(vao_);
  gl.BindBuffer(gl::ARRAY_BUFFER, vbo_);
  gl.BufferData(gl::ARRAY_BUFFER, static_cast<gl::GLsizeiptr>(quads_.size() * sizeof(Quad)), quads_.data(), gl::STREAM_DRAW);
  gl.DrawArraysInstanced(gl::TRIANGLES, 0, 6, static_cast<gl::GLsizei>(quads_.size()));
}

}  // namespace totu::render
