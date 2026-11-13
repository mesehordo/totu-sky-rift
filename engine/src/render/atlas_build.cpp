// SPDX-License-Identifier: GPL-3.0-or-later
#include "render/atlas_build.hpp"

#include "render/calm.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "content/json.hpp"

namespace totu::render {

namespace {

std::string num(long long v) { return std::to_string(v); }

std::string num(float v) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.3f", static_cast<double>(v));
  return buf;
}

float to_float(const content::Value* v, float def) {
  if (!v) return def;
  const auto f = v->as_fixed();
  return f ? static_cast<float>(f->raw()) / 65536.0f : def;
}

int to_int(const content::Value* v, int def) { return v ? static_cast<int>(v->as_int().value_or(def)) : def; }

}  // namespace

const AtlasSheet* AtlasData::sheet(const std::string& id) const {
  const auto it = std::lower_bound(sheets.begin(), sheets.end(), id,
                                   [](const AtlasSheet& s, const std::string& k) { return s.id < k; });
  return it != sheets.end() && it->id == id ? &*it : nullptr;
}

std::string AtlasData::to_json() const {
  using content::Value;
  Value root = Value::object();
  root.set("_leiras", Value::string("TOTU sprite-atlasz (M5): a lapok page_N.png és mask_N.png; képkocka = [lap, x, y, sz, m, eltolás x, eltolás y]"));
  root.set("page_size", Value::number(num(static_cast<long long>(page_size))));
  root.set("pages", Value::number(num(static_cast<long long>(pages))));
  root.set("half", Value::boolean(half));
  Value ss = Value::array();
  for (const auto& s : sheets) {
    Value o = Value::object();
    o.set("id", Value::string(s.id));
    o.set("type", Value::string(s.type));
    o.set("frame_w", Value::number(num(static_cast<long long>(s.frame_w))));
    o.set("frame_h", Value::number(num(static_cast<long long>(s.frame_h))));
    o.set("anchor_x", Value::number(num(s.anchor_x)));
    o.set("anchor_y", Value::number(num(s.anchor_y)));
    o.set("scale", Value::number(num(s.scale)));
    o.set("directions", Value::number(num(static_cast<long long>(s.directions))));
    o.set("columns", Value::number(num(static_cast<long long>(s.columns))));
    o.set("mask", Value::boolean(s.has_mask));
    o.set("first", Value::number(num(static_cast<long long>(s.first))));
    Value an = Value::object();
    for (const auto& [name, a] : s.anims) {
      Value x = Value::object();
      x.set("start", Value::number(num(static_cast<long long>(a.start))));
      x.set("frames", Value::number(num(static_cast<long long>(a.frames))));
      x.set("fps", Value::number(num(static_cast<long long>(a.fps))));
      x.set("loop", Value::boolean(a.loop));
      an.set(name, std::move(x));
    }
    o.set("anims", std::move(an));
    ss.push(std::move(o));
  }
  root.set("sheets", std::move(ss));
  Value fr = Value::array();
  for (const auto& f : frames) {
    Value a = Value::array();
    for (const long long v : {static_cast<long long>(f.page), static_cast<long long>(f.x), static_cast<long long>(f.y),
                              static_cast<long long>(f.w), static_cast<long long>(f.h), static_cast<long long>(f.ox),
                              static_cast<long long>(f.oy)}) {
      a.push(Value::number(num(v)));
    }
    fr.push(std::move(a));
  }
  root.set("frames", std::move(fr));
  return content::dump(root);
}

std::optional<AtlasData> AtlasData::from_json(const std::string& text, std::string* error) {
  content::ParseError pe;
  const auto doc = content::parse(text, &pe);
  if (!doc) {
    if (error) *error = "hibás atlaszleírás: " + pe.message;
    return std::nullopt;
  }
  AtlasData a;
  a.page_size = to_int(doc->find("page_size"), 4096);
  a.pages = to_int(doc->find("pages"), 0);
  const content::Value* h = doc->find("half");
  a.half = h && h->is_bool() && h->as_bool();
  const content::Value* ss = doc->find("sheets");
  const content::Value* fr = doc->find("frames");
  if (!ss || !ss->is_array() || !fr || !fr->is_array()) {
    if (error) *error = "hiányzó sheets vagy frames";
    return std::nullopt;
  }
  for (const auto& v : ss->items()) {
    AtlasSheet s;
    const content::Value* id = v.find("id");
    const content::Value* ty = v.find("type");
    s.id = id && id->is_string() ? id->text() : "";
    s.type = ty && ty->is_string() ? ty->text() : "";
    s.frame_w = to_int(v.find("frame_w"), 0);
    s.frame_h = to_int(v.find("frame_h"), 0);
    s.anchor_x = to_float(v.find("anchor_x"), 0);
    s.anchor_y = to_float(v.find("anchor_y"), 0);
    s.scale = to_float(v.find("scale"), 1);
    s.directions = to_int(v.find("directions"), 1);
    s.columns = to_int(v.find("columns"), 1);
    const content::Value* m = v.find("mask");
    s.has_mask = m && m->is_bool() && m->as_bool();
    s.first = static_cast<uint32_t>(to_int(v.find("first"), 0));
    if (const content::Value* an = v.find("anims"); an && an->is_object()) {
      for (const auto& [name, x] : an->members()) {
        AtlasAnim aa;
        aa.start = to_int(x.find("start"), 0);
        aa.frames = to_int(x.find("frames"), 1);
        aa.fps = to_int(x.find("fps"), 8);
        const content::Value* lp = x.find("loop");
        aa.loop = !lp || !lp->is_bool() || lp->as_bool();
        s.anims[name] = aa;
      }
    }
    a.sheets.push_back(std::move(s));
  }
  for (const auto& v : fr->items()) {
    if (!v.is_array() || v.items().size() != 7) {
      if (error) *error = "hibás képkocka";
      return std::nullopt;
    }
    const auto& it = v.items();
    AtlasFrame f;
    f.page = static_cast<uint16_t>(to_int(&it[0], 0));
    f.x = to_int(&it[1], 0);
    f.y = to_int(&it[2], 0);
    f.w = to_int(&it[3], 0);
    f.h = to_int(&it[4], 0);
    f.ox = to_int(&it[5], 0);
    f.oy = to_int(&it[6], 0);
    a.frames.push_back(f);
  }
  for (const auto& s : a.sheets) {
    if (s.first + static_cast<uint32_t>(s.directions * s.columns) > a.frames.size()) {
      if (error) *error = "a képkockák száma kevés: " + s.id;
      return std::nullopt;
    }
  }
  return a;
}

void trim_bounds(const Image& img, int fx, int fy, int fw, int fh, int& x, int& y, int& w, int& h) {
  int x0 = fw, y0 = fh, x1 = -1, y1 = -1;
  for (int yy = 0; yy < fh; ++yy) {
    const int sy = fy + yy;
    if (sy < 0 || sy >= img.height) continue;
    for (int xx = 0; xx < fw; ++xx) {
      const int sx = fx + xx;
      if (sx < 0 || sx >= img.width) continue;
      if (img.at(sx, sy)[3] == 0) continue;
      x0 = std::min(x0, xx);
      y0 = std::min(y0, yy);
      x1 = std::max(x1, xx);
      y1 = std::max(y1, yy);
    }
  }
  if (x1 < 0) {
    x = y = w = h = 0;
    return;
  }
  x = x0;
  y = y0;
  w = x1 - x0 + 1;
  h = y1 - y0 + 1;
}

bool ShelfPacker::place(int w, int h, int& x, int& y) {
  const int pw = w + pad_, ph = h + pad_;
  if (pw > size_ || ph > size_) return false;
  // az első polc, amelyen elfér (magasságban és a sor végéig); különben új polc
  for (auto& s : shelves_) {
    if (ph <= s.h && s.x + pw <= size_) {
      x = s.x;
      y = s.y;
      s.x += pw;
      return true;
    }
  }
  if (next_y_ + ph > size_) return false;
  shelves_.push_back({next_y_, ph, pw});
  x = 0;
  y = next_y_;
  next_y_ += ph;
  return true;
}

std::vector<std::string> list_sprite_sheets(const std::string& assets_dir) {
  std::vector<std::string> out;
  const auto root = std::filesystem::path(assets_dir) / "sprites";
  for (const char* kind : {"units", "buildings", "decor", "effects", "props", "resources", "projectiles"}) {
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(root / kind, ec)) {
      const std::string name = e.path().filename().string();
      if (e.path().extension() == ".json" && name.find("_old") == std::string::npos) out.push_back(e.path().string());
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

AtlasData build_atlas(const std::vector<std::string>& sheet_jsons, const AtlasOptions& opt,
                      const std::function<void(int, const Image&, const Image&)>& write_page,
                      std::vector<std::string>* warnings) {
  AtlasData atlas;
  atlas.page_size = opt.page_size;
  atlas.half = opt.half;
  auto blank = [&]() {
    Image p;
    p.width = p.height = opt.page_size;
    p.rgba.assign(static_cast<size_t>(opt.page_size) * static_cast<size_t>(opt.page_size) * 4, 0);
    return p;
  };
  Image page = blank(), mask_page = blank();
  ShelfPacker packer(opt.page_size, opt.padding);
  bool page_used = false;
  auto flush = [&]() {
    if (!page_used) return;
    write_page(atlas.pages, page, mask_page);
    ++atlas.pages;
    std::fill(page.rgba.begin(), page.rgba.end(), 0);
    std::fill(mask_page.rgba.begin(), mask_page.rgba.end(), 0);
    packer.reset();
    page_used = false;
  };
  auto warn = [&](const std::string& w) {
    if (warnings) warnings->push_back(w);
  };

  for (const auto& path : sheet_jsons) {
    std::ifstream in(path, std::ios::binary);
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    const auto doc = content::parse(text);
    if (!doc) { warn("hibás JSON: " + path); continue; }
    const content::Value* image = doc->find("image");
    const content::Value* fs = doc->find("frame_size");
    if (!image || !image->is_string() || !fs || !fs->is_array() || fs->items().size() != 2) {
      warn("hiányzó image vagy frame_size: " + path);
      continue;
    }
    const auto dir = std::filesystem::path(path).parent_path();
    std::string err;
    auto img = load_png((dir / image->text()).string(), &err);
    if (!img) { warn(err); continue; }
    std::optional<Image> mask;
    if (const content::Value* tm = doc->find("team_mask"); tm && tm->is_string()) {
      mask = load_png((dir / tm->text()).string(), nullptr);
      if (mask && (mask->width != img->width || mask->height != img->height)) mask.reset();
    }
    AtlasSheet s;
    const content::Value* id = doc->find("id");
    s.id = id && id->is_string() ? id->text() : std::filesystem::path(path).stem().string();
    const content::Value* ty = doc->find("type");
    s.type = ty && ty->is_string() ? ty->text() : "";
    s.frame_w = to_int(&fs->items()[0], 0);
    s.frame_h = to_int(&fs->items()[1], 0);
    if (const content::Value* a = doc->find("anchor"); a && a->is_array() && a->items().size() == 2) {
      s.anchor_x = to_float(&a->items()[0], 0);
      s.anchor_y = to_float(&a->items()[1], 0);
    } else if (const content::Value* fo = doc->find("footprint_offset"); fo && fo->is_array() && fo->items().size() == 2) {
      // épület és lelőhely (a régi játék szerint): a horgony a lábnyom bal felső sarka a képen
      s.anchor_x = to_float(&fo->items()[0], 0);
      s.anchor_y = to_float(&fo->items()[1], 0);
    } else {
      s.anchor_x = static_cast<float>(s.frame_w) / 2.0f;
      s.anchor_y = static_cast<float>(s.frame_h);
    }
    s.scale = to_float(doc->find("scale"), 1);
    if (s.scale <= 0) s.scale = 1;
    if (const content::Value* an = doc->find("animations"); an && an->is_object()) {
      for (const auto& [name, v] : an->members()) {
        AtlasAnim a;
        a.start = to_int(v.find("start"), 0);
        a.frames = to_int(v.find("frames"), 1);
        a.fps = to_int(v.find("fps"), 8);
        const content::Value* lp = v.find("loop");
        a.loop = !lp || !lp->is_bool() || lp->as_bool();
        s.anims[name] = a;
      }
    }
    if (opt.half) {
      *img = img->half();
      if (mask) *mask = mask->half();
      s.frame_w /= 2;
      s.frame_h /= 2;
      s.anchor_x /= 2.0f;
      s.anchor_y /= 2.0f;
      s.scale /= 2.0f;
    }
    if (s.frame_w <= 0 || s.frame_h <= 0) { warn("érvénytelen képkockaméret: " + path); continue; }
    s.columns = std::max(1, img->width / s.frame_w);
    s.directions = std::max(1, img->height / s.frame_h);
    s.has_mask = mask.has_value();
    s.first = static_cast<uint32_t>(atlas.frames.size());
    if (opt.calm && calm_applies(s.type)) {   // a képkockák szűrése a vágás és a csomagolás előtt
      for (int d = 0; d < s.directions; ++d) {
        for (int c = 0; c < s.columns; ++c) calm_frame(*img, c * s.frame_w, d * s.frame_h, s.frame_w, s.frame_h);
      }
    }
    for (int d = 0; d < s.directions; ++d) {
      for (int c = 0; c < s.columns; ++c) {
        const int fx = c * s.frame_w, fy = d * s.frame_h;
        AtlasFrame f;
        int tx = 0, ty2 = 0, tw = 0, th = 0;
        trim_bounds(*img, fx, fy, s.frame_w, s.frame_h, tx, ty2, tw, th);
        if (tw > 0) {
          int px = 0, py = 0;
          if (!packer.place(tw, th, px, py)) {
            flush();
            if (!packer.place(tw, th, px, py)) {
              warn("a képkocka nagyobb a lapnál: " + s.id);
              tw = th = 0;
            }
          }
          if (tw > 0) {
            page_used = true;
            f.page = static_cast<uint16_t>(atlas.pages);
            f.x = px;
            f.y = py;
            f.w = tw;
            f.h = th;
            f.ox = tx;
            f.oy = ty2;
            for (int yy = 0; yy < th; ++yy) {
              std::memcpy(page.at(px, py + yy), img->at(fx + tx, fy + ty2 + yy), static_cast<size_t>(tw) * 4);
              if (mask) std::memcpy(mask_page.at(px, py + yy), mask->at(fx + tx, fy + ty2 + yy), static_cast<size_t>(tw) * 4);
            }
          }
        }
        atlas.frames.push_back(f);
      }
    }
    atlas.sheets.push_back(std::move(s));
  }
  flush();
  std::sort(atlas.sheets.begin(), atlas.sheets.end(), [](const AtlasSheet& a, const AtlasSheet& b) { return a.id < b.id; });
  return atlas;
}

}  // namespace totu::render
