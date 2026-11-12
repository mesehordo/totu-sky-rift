// SPDX-License-Identifier: GPL-3.0-or-later
// A képkezelés tesztjei (M5, 1. lépés): PNG kódolás–visszaolvasás bitre, felezés előszorzott alfával, kivágás,
// képeltérés, és egy valódi sprite-lap betöltése (grafikus kártya nélkül).
#include <filesystem>

#include "check.hpp"
#include "render/image.hpp"

using namespace totu;

namespace {

render::Image checker(int w, int h) {
  render::Image img;
  img.width = w;
  img.height = h;
  img.rgba.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      uint8_t* p = img.at(x, y);
      p[0] = static_cast<uint8_t>(x * 7);
      p[1] = static_cast<uint8_t>(y * 11);
      p[2] = static_cast<uint8_t>((x + y) % 2 ? 255 : 0);
      p[3] = static_cast<uint8_t>((x * y) % 256);
    }
  }
  return img;
}

}  // namespace

TEST_CASE("kép: PNG kódolás és visszaolvasás bitre azonos") {
  const auto img = checker(37, 23);
  const auto bytes = render::encode_png(img);
  CHECK(bytes.size() > 8 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G');
  const auto back = render::decode_png(bytes);
  CHECK(back.has_value() && back->width == 37 && back->height == 23 && back->rgba == img.rgba);
  std::string err;
  CHECK(!render::decode_png({1, 2, 3, 4}, &err) && !err.empty());
  CHECK(render::diff_permille(img, *back, 0) == 0);
}

TEST_CASE("kép: felezés előszorzott alfával, kivágás, eltérés") {
  render::Image img;
  img.width = 2;
  img.height = 2;
  // egy piros, átlátszatlan képpont és három teljesen átlátszó fekete: a felezés piros marad (nincs sötét szegély)
  img.rgba = {255, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  const auto h = img.half();
  CHECK(h.width == 1 && h.height == 1);
  CHECK(h.rgba[0] == 255 && h.rgba[1] == 0 && h.rgba[3] == 64);
  const auto c = checker(10, 10).crop(8, 8, 4, 4);
  CHECK(c.width == 4 && c.at(0, 0)[0] == 56 && c.at(3, 3)[3] == 0);   // a lapon kívüli rész átlátszó
  auto a = checker(20, 20), b = a;
  b.at(5, 5)[0] = static_cast<uint8_t>(b.at(5, 5)[0] + 100);
  CHECK(render::diff_permille(a, b, 50) == 2);   // 1 képpont a 400-ból
  CHECK(render::diff_permille(a, b, 200) == 0);
}

TEST_CASE("kép: valódi sprite-lap és a csapatmaszkja betölthető") {
  const auto dir = std::filesystem::path(TOTU_ASSETS_DIR) / "sprites" / "units";
  if (!std::filesystem::exists(dir)) {
    std::printf("  (a grafika nincs meg: %s – kihagyva)\n", dir.string().c_str());
    return;
  }
  std::string err;
  const auto img = render::load_png((dir / "cro_agyfalo.png").string(), &err);
  const auto mask = render::load_png((dir / "cro_agyfalo_team.png").string(), &err);
  CHECK(img.has_value() && mask.has_value());
  CHECK(img->width == mask->width && img->height == mask->height);
  CHECK(img->width % 116 == 0 && img->height == 1024);   // 116 × 128-as képkockák, 8 irány
}
