// SPDX-License-Identifier: GPL-3.0-or-later
// Atlaszkészítő (M5, 2. lépés) – grafikus kártya nélkül. A sprite-lapok képkockáit a tényleges tartalmukra vágja
// (az átlátszó szélt eltávolítja, az eltolást megjegyzi), és polcos elrendezéssel, determinisztikusan rakja a
// megadott méretű lapokra; a csapatmaszk ugyanazzal az elrendezéssel kerül a párhuzamos maszklapokra. Egy
// sprite-lap képkockái egymás után kerülnek a lapokra (egy egységtípushoz csak a saját lapjai kellenek), és egy lap
// csak addig van a memóriában, amíg tele nem lesz. Minőségi lépcső: felezés (gyengébb kártyára).
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "render/image.hpp"

namespace totu::render {

struct AtlasFrame {
  uint16_t page = 0;
  int x = 0, y = 0, w = 0, h = 0;   // a lapon; w = 0: üres képkocka
  int ox = 0, oy = 0;               // a vágott rész bal felső sarka az eredeti képkockában
};

struct AtlasAnim {
  int start = 0, frames = 1, fps = 8;
  bool loop = true;
};

struct AtlasSheet {
  std::string id, type;             // type: unit, building, decor, effect …
  int frame_w = 0, frame_h = 0;     // az eredeti képkocka (a lépcső szerint felezve)
  float anchor_x = 0, anchor_y = 0;
  float scale = 1;                  // a lap felbontása a játékbeli mérethez képest
  int directions = 1, columns = 1;
  bool has_mask = false;
  uint32_t first = 0;               // az első képkocka indexe a közös listában (irány × oszlop sorrendben)
  std::map<std::string, AtlasAnim> anims;
};

struct AtlasData {
  int page_size = 4096;
  int pages = 0;
  bool half = false;
  std::vector<AtlasSheet> sheets;   // azonosító szerint rendezve
  std::vector<AtlasFrame> frames;

  const AtlasSheet* sheet(const std::string& id) const;
  const AtlasFrame& frame(const AtlasSheet& s, int direction, int column) const {
    return frames[s.first + static_cast<uint32_t>(direction * s.columns + column)];
  }
  std::string to_json() const;
  static std::optional<AtlasData> from_json(const std::string& text, std::string* error = nullptr);
};

// A képkocka nem átlátszó részének befoglaló téglalapja (x, y, w, h); teljesen átlátszónál w = h = 0.
void trim_bounds(const Image& img, int fx, int fy, int fw, int fh, int& x, int& y, int& w, int& h);

// Polcos elrendező egyetlen lapra (determinisztikus): a következő téglalap helye, vagy false, ha nem fér el.
class ShelfPacker {
 public:
  ShelfPacker(int size, int padding) : size_(size), pad_(padding) {}
  bool place(int w, int h, int& x, int& y);
  void reset() { shelves_.clear(); next_y_ = 0; }

 private:
  struct Shelf {
    int y, h, x;
  };
  int size_, pad_;
  std::vector<Shelf> shelves_;
  int next_y_ = 0;
};

struct AtlasOptions {
  int page_size = 4096;
  int padding = 2;
  bool half = false;
};

// Az atlasz építése a megadott sprite-leírásokból. A kész lapokat (alap és maszk) a `write_page` kapja meg, amint
// betelnek (lapszám, alapkép, maszkkép). Hibás leírás kimarad (a hiba a `warnings`-ba).
AtlasData build_atlas(const std::vector<std::string>& sheet_jsons, const AtlasOptions& opt,
                      const std::function<void(int, const Image&, const Image&)>& write_page,
                      std::vector<std::string>* warnings = nullptr);

// A grafika mappájából a használható sprite-leírások (a `_old` változatok nélkül), rendezve.
std::vector<std::string> list_sprite_sheets(const std::string& assets_dir);

}  // namespace totu::render
