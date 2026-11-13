// SPDX-License-Identifier: GPL-3.0-or-later
#include "render/atlas.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace totu::render {

bool Atlas::load(Device& dev, const std::string& dir, std::string* error) {
  dev_ = &dev;
  dir_ = dir;
  std::ifstream in(std::filesystem::path(dir) / "atlas.json", std::ios::binary);
  if (!in) {
    if (error) *error = "nincs atlasz: " + (std::filesystem::path(dir) / "atlas.json").string();
    return false;
  }
  const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  auto d = AtlasData::from_json(text, error);
  if (!d) return false;
  data_ = std::move(*d);
  if (data_.page_size > dev.max_texture_size()) {
    if (error) *error = "az atlasz lapmérete nagyobb, mint amit a kártya elbír (fél minőségű atlasz kell)";
    return false;
  }
  layer_of_.assign(static_cast<size_t>(data_.pages), -1);
  return true;
}

void Atlas::release() {
  if (!dev_) return;
  dev_->destroy(base_);
  dev_->destroy(mask_);
  loaded_.clear();
  std::fill(layer_of_.begin(), layer_of_.end(), -1);
}

bool Atlas::prepare(const std::vector<const AtlasSheet*>& sheets, std::string* error) {
  std::vector<int> need = loaded_;
  for (const AtlasSheet* s : sheets) {
    for (int k = 0; k < s->directions * s->columns; ++k) {
      const AtlasFrame& f = data_.frames[s->first + static_cast<uint32_t>(k)];
      if (f.w) need.push_back(f.page);
    }
  }
  std::sort(need.begin(), need.end());
  need.erase(std::unique(need.begin(), need.end()), need.end());
  if (need == loaded_) return true;
  if (static_cast<int>(need.size()) > dev_->max_array_layers()) {
    if (error) *error = "túl sok atlaszlap egy tömbben";
    return false;
  }
  // a tömb újraépítése a régi és az új lapokkal (ritka: a meccs elején, vagy amikor új típus jelenik meg)
  dev_->destroy(base_);
  dev_->destroy(mask_);
  const int ps = data_.page_size, n = static_cast<int>(need.size());
  base_ = dev_->create_array(ps, ps, n, false);
  mask_ = dev_->create_array(ps, ps, n, true);
  std::fill(layer_of_.begin(), layer_of_.end(), -1);
  for (int i = 0; i < n; ++i) {
    const int p = need[static_cast<size_t>(i)];
    auto img = load_png((std::filesystem::path(dir_) / ("page_" + std::to_string(p) + ".png")).string(), error);
    auto msk = load_png((std::filesystem::path(dir_) / ("mask_" + std::to_string(p) + ".png")).string(), error);
    if (!img || !msk || img->width != ps || msk->width != ps) return false;
    // az épületek és a lelőhelyek képkockáinak oszloponkénti talpa (az árnyékhoz), még az előszorzás előtt
    if (col_base_.size() != data_.frames.size()) col_base_.assign(data_.frames.size(), {});
    if (solid_bottom_.size() != data_.frames.size()) solid_bottom_.assign(data_.frames.size(), -1);
    for (const AtlasSheet& s : data_.sheets) {
      if (s.type != "building" && s.type != "resource") continue;
      for (int k = 0; k < s.directions * s.columns; ++k) {
        const uint32_t fi = s.first + static_cast<uint32_t>(k);
        const AtlasFrame& f = data_.frames[fi];
        if (!f.w || f.page != p || !col_base_[fi].empty()) continue;
        if (s.type == "resource") {   // a fa törzsének töve: a festett, áttetsző talajfolt nem számít (régi: 160)
          for (int y = f.h - 1; y >= 0 && solid_bottom_[fi] < 0; --y) {
            for (int x = 0; x < f.w; ++x) {
              if (img->at(f.x + x, f.y + y)[3] >= 160) {
                solid_bottom_[fi] = static_cast<int16_t>(y);
                break;
              }
            }
          }
        }
        auto& cols = col_base_[fi];
        for (int x0 = 0; x0 < f.w; x0 += kStrip) {
          int base = -1;
          for (int x = x0; x < std::min(f.w, x0 + kStrip); ++x) {
            for (int y = f.h - 1; y > base; --y) {
              if (img->at(f.x + x, f.y + y)[3] > 60) {
                base = y;
                break;
              }
            }
          }
          cols.push_back(static_cast<int16_t>(base));
        }
      }
    }
    premultiply(*img);
    dev_->upload_layer(base_, i, *img);
    dev_->upload_layer(mask_, i, *msk);
    layer_of_[static_cast<size_t>(p)] = i;
  }
  dev_->finish_array(base_);
  dev_->finish_array(mask_);
  loaded_ = std::move(need);
  return true;
}

void Atlas::draw(SpriteBatch& batch, const AtlasSheet& s, int direction, int column, float ax, float ay, float scale,
                 const uint8_t team[4], const uint8_t tint[4], float foot, uint8_t fx) {
  if (direction < 0 || direction >= s.directions || column < 0 || column >= s.columns) return;
  const AtlasFrame& f = data_.frame(s, direction, column);
  if (!f.w) return;
  if (layer_of_[f.page] < 0 && !prepare({&s})) return;
  const float ps = static_cast<float>(data_.page_size);
  SpriteInstance in;
  in.x = ax + (static_cast<float>(f.ox) - s.anchor_x) * scale;
  in.y = ay + (static_cast<float>(f.oy) - s.anchor_y) * scale;
  in.w = static_cast<float>(f.w) * scale;
  in.h = static_cast<float>(f.h) * scale;
  in.u0 = static_cast<float>(f.x) / ps;
  in.v0 = static_cast<float>(f.y) / ps;
  in.u1 = static_cast<float>(f.x + f.w) / ps;
  in.v1 = static_cast<float>(f.y + f.h) / ps;
  in.layer[0] = static_cast<uint8_t>(layer_of_[f.page]);
  in.foot = foot >= 0 ? foot : ay;   // a horgony a talpvonal (az épületnél a lábnyom alja; 0: a kép alja)
  if (foot == 0.0f) {   // a kép alja helyett a törzs töve, ha ismert
    const uint32_t fi = s.first + static_cast<uint32_t>(direction * s.columns + column);
    if (fi < solid_bottom_.size() && solid_bottom_[fi] >= 0) in.foot = in.y + static_cast<float>(solid_bottom_[fi] + 1) * scale;
  }
  in.layer[1] = fx;
  for (int k = 0; k < 4; ++k) {
    in.team[k] = team ? team[k] : 0;
    if (tint) in.tint[k] = tint[k];
  }
  batch.draw(base_, &mask_, in);
}

void Atlas::draw_shadow_columns(SpriteBatch& batch, const AtlasSheet& s, int direction, int column, float ax, float ay,
                                float scale, uint8_t length) {
  if (direction < 0 || direction >= s.directions || column < 0 || column >= s.columns) return;
  const uint32_t fi = s.first + static_cast<uint32_t>(direction * s.columns + column);
  const AtlasFrame& f = data_.frames[fi];
  if (!f.w) return;
  if (layer_of_[f.page] < 0 && !prepare({&s})) return;
  if (fi >= col_base_.size() || col_base_[fi].empty()) {   // nincs oszlopadat: egyben vetül
    const uint8_t no_team[4] = {0, 0, 0, 0};
    draw(batch, s, direction, column, ax, ay, scale, no_team, nullptr, 0.0f);
    return;
  }
  const float ps = static_cast<float>(data_.page_size);
  const float left = ax + (static_cast<float>(f.ox) - s.anchor_x) * scale;
  const float top = ay + (static_cast<float>(f.oy) - s.anchor_y) * scale;
  const auto& cols = col_base_[fi];
  for (size_t k = 0; k < cols.size(); ++k) {
    const int base = cols[k];
    if (base < 0) continue;
    const int x0 = static_cast<int>(k) * kStrip, w = std::min(kStrip, f.w - x0);
    SpriteInstance in;
    in.x = left + static_cast<float>(x0) * scale;
    in.y = top;
    in.w = static_cast<float>(w) * scale;
    in.h = static_cast<float>(base + 1) * scale;   // a csík talpa alatti rész nem vet árnyékot
    in.u0 = static_cast<float>(f.x + x0) / ps;
    in.v0 = static_cast<float>(f.y) / ps;
    in.u1 = static_cast<float>(f.x + x0 + w) / ps;
    in.v1 = static_cast<float>(f.y + base + 1) / ps;
    in.layer[0] = static_cast<uint8_t>(layer_of_[f.page]);
    in.layer[2] = length;
    in.foot = top + static_cast<float>(base + 1) * scale;
    batch.draw(base_, &mask_, in);
  }
}

}  // namespace totu::render
