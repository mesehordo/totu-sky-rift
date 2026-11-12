// SPDX-License-Identifier: GPL-3.0-or-later
#include "render/sprite_sheet.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>

#include "content/json.hpp"
#include "render/sprite_batch.hpp"

namespace totu::render {

void SpriteSheet::uv(int direction, int column, float& u0, float& v0, float& u1, float& v1) const {
  const float tw = static_cast<float>(base.width), th = static_cast<float>(base.height);
  const float x = static_cast<float>(column * frame_w), y = static_cast<float>(direction * frame_h);
  u0 = x / tw;
  v0 = y / th;
  u1 = (x + static_cast<float>(frame_w)) / tw;
  v1 = (y + static_cast<float>(frame_h)) / th;
}

const Animation* SpriteSheet::anim(const std::string& name) const {
  const auto it = anims.find(name);
  return it == anims.end() ? nullptr : &it->second;
}

namespace {

long long int_of(const content::Value* v, long long def) {
  return v ? v->as_int().value_or(def) : def;
}

}  // namespace

std::optional<SpriteSheet> load_sprite_sheet(Device& dev, const std::string& json_path, std::string* error) {
  std::ifstream in(json_path, std::ios::binary);
  if (!in) {
    if (error) *error = "nem olvasható: " + json_path;
    return std::nullopt;
  }
  const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  const auto doc = content::parse(text);
  if (!doc) {
    if (error) *error = "hibás JSON: " + json_path;
    return std::nullopt;
  }
  const std::filesystem::path dir = std::filesystem::path(json_path).parent_path();
  const content::Value* image = doc->find("image");
  const content::Value* fs = doc->find("frame_size");
  if (!image || !image->is_string() || !fs || !fs->is_array() || fs->items().size() != 2) {
    if (error) *error = "hiányzó image vagy frame_size: " + json_path;
    return std::nullopt;
  }
  SpriteSheet s;
  s.id = doc->find("id") && doc->find("id")->is_string() ? doc->find("id")->text() : json_path;
  s.frame_w = static_cast<int>(fs->items()[0].as_int().value_or(0));
  s.frame_h = static_cast<int>(fs->items()[1].as_int().value_or(0));
  if (const content::Value* a = doc->find("anchor"); a && a->is_array() && a->items().size() == 2) {
    s.anchor_x = static_cast<float>(a->items()[0].as_int().value_or(0));
    s.anchor_y = static_cast<float>(a->items()[1].as_int().value_or(0));
  } else {
    s.anchor_x = static_cast<float>(s.frame_w) / 2.0f;
    s.anchor_y = static_cast<float>(s.frame_h);
  }
  if (const content::Value* sc = doc->find("scale")) {
    const auto f = sc->as_fixed();
    if (f && f->raw() > 0) s.scale = static_cast<float>(f->raw()) / 65536.0f;
  }
  if (const content::Value* d = doc->find("directions"); d && d->is_array()) {
    s.directions = static_cast<int>(d->items().size());
  }
  if (const content::Value* an = doc->find("animations"); an && an->is_object()) {
    for (const auto& [name, v] : an->members()) {
      Animation a;
      a.start = static_cast<int>(int_of(v.find("start"), 0));
      a.frames = static_cast<int>(int_of(v.find("frames"), 1));
      a.fps = static_cast<int>(int_of(v.find("fps"), 8));
      const content::Value* lp = v.find("loop");
      a.loop = !lp || !lp->is_bool() || lp->as_bool();
      s.anims[name] = a;
    }
  }
  std::string err;
  auto img = load_png((dir / image->text()).string(), &err);
  if (!img) {
    if (error) *error = err;
    return std::nullopt;
  }
  std::optional<Image> mask;
  if (const content::Value* tm = doc->find("team_mask"); tm && tm->is_string()) {
    mask = load_png((dir / tm->text()).string(), nullptr);
    if (mask && (mask->width != img->width || mask->height != img->height)) mask.reset();
  }
  // a kártya korlátja szerint felezés (a képkockák mérete is feleződik)
  while (img->width > dev.max_texture_size() || img->height > dev.max_texture_size()) {
    *img = img->half();
    if (mask) *mask = mask->half();
    s.frame_w /= 2;
    s.frame_h /= 2;
    s.anchor_x /= 2.0f;
    s.anchor_y /= 2.0f;
    s.scale /= 2.0f;
  }
  premultiply(*img);
  s.base = dev.create_array_from(*img, false);
  if (mask) s.mask = dev.create_array_from(*mask, true);
  return s;
}

}  // namespace totu::render
