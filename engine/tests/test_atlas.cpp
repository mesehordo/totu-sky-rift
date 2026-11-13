// SPDX-License-Identifier: GPL-3.0-or-later
// Az atlaszkészítő tesztjei (M5, 2. lépés), grafikus kártya nélkül: polcos elrendezés (átfedés nélkül, a lapon
// belül, determinisztikusan), vágás, több lapra bontás, minden képkocka és maszkja pixelre egyezik az eredetivel,
// a leírás oda-vissza alakítása, fél minőség, valódi sprite-lap.
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include <cstdio>
#include <cstdlib>

#include "check.hpp"
#include "core/rng.hpp"
#include "render/atlas_build.hpp"
#include "render/calm.hpp"

using namespace totu;

namespace {

std::filesystem::path temp_dir(const std::string& name) {
  auto p = std::filesystem::temp_directory_path() / ("totu_atlas_teszt_" + name);
  std::error_code ec;
  std::filesystem::remove_all(p, ec);
  std::filesystem::create_directories(p, ec);
  return p;
}

// mesterséges sprite-lap: 4 irány × 3 oszlop, képkockánként más méretű, átlátszó szélű téglalap (és maszk)
void make_sheet(const std::filesystem::path& dir, const std::string& id, int fw, int fh, uint32_t seed) {
  core::Pcg32 rng(seed, 1);
  render::Image img, mask;
  img.width = mask.width = fw * 3;
  img.height = mask.height = fh * 4;
  img.rgba.assign(static_cast<size_t>(img.width * img.height) * 4, 0);
  mask.rgba = img.rgba;
  for (int d = 0; d < 4; ++d) {
    for (int c = 0; c < 3; ++c) {
      if (d == 3 && c == 2) continue;   // egy teljesen üres képkocka
      const int w = rng.range(3, fw - 2), h = rng.range(3, fh - 2);
      const int x0 = rng.range(0, fw - w), y0 = rng.range(0, fh - h);
      for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
          uint8_t* p = img.at(c * fw + x0 + x, d * fh + y0 + y);
          p[0] = static_cast<uint8_t>(rng.below(256)); p[1] = static_cast<uint8_t>(d * 60); p[2] = static_cast<uint8_t>(c * 80);
          p[3] = static_cast<uint8_t>(1 + rng.below(255));
          if ((x + y) % 3 == 0) mask.at(c * fw + x0 + x, d * fh + y0 + y)[3] = 255;
        }
      }
    }
  }
  render::save_png((dir / (id + ".png")).string(), img);
  render::save_png((dir / (id + "_team.png")).string(), mask);
  std::ofstream js(dir / (id + ".json"));
  js << "{\"id\": \"" << id << "\", \"type\": \"unit\", \"image\": \"" << id << ".png\", \"team_mask\": \"" << id
     << "_team.png\", \"frame_size\": [" << fw << ", " << fh << "], \"anchor\": [" << fw / 2 << ", " << fh - 2
     << "], \"scale\": 2, \"directions\": [\"E\", \"S\", \"W\", \"N\"], "
     << "\"animations\": {\"idle\": {\"start\": 0, \"frames\": 2, \"fps\": 5}, \"walk\": {\"start\": 1, \"frames\": 2, \"fps\": 10, \"loop\": true}}}";
}

}  // namespace

TEST_CASE("atlasz: polcos elrendezés – a lapon belül, átfedés nélkül, determinisztikusan") {
  core::Pcg32 rng(5, 2);
  std::vector<std::pair<int, int>> sizes;
  for (int i = 0; i < 200; ++i) sizes.push_back({rng.range(4, 60), rng.range(4, 60)});
  auto run = [&]() {
    render::ShelfPacker pk(256, 2);
    std::vector<std::array<int, 4>> placed;
    for (const auto& [w, h] : sizes) {
      int x = 0, y = 0;
      if (!pk.place(w, h, x, y)) break;
      placed.push_back({x, y, w, h});
    }
    return placed;
  };
  const auto a = run(), b = run();
  CHECK(a == b && a.size() > 10 && a.size() < 200);   // determinisztikus, és betelik
  bool ok = true;
  for (size_t i = 0; i < a.size(); ++i) {
    ok &= a[i][0] >= 0 && a[i][1] >= 0 && a[i][0] + a[i][2] <= 256 && a[i][1] + a[i][3] <= 256;
    for (size_t j = i + 1; j < a.size(); ++j) {
      const bool apart = a[i][0] + a[i][2] + 2 <= a[j][0] || a[j][0] + a[j][2] + 2 <= a[i][0] ||
                         a[i][1] + a[i][3] + 2 <= a[j][1] || a[j][1] + a[j][3] + 2 <= a[i][1];
      ok &= apart;   // a 2 képpontos térközzel együtt sincs átfedés
    }
  }
  CHECK(ok);
  render::ShelfPacker big(64, 0);
  int x = 0, y = 0;
  CHECK(!big.place(65, 10, x, y));   // a lapnál nagyobb nem fér el
}

TEST_CASE("atlasz: vágás, több lapra bontás, pixelre egyező képkockák és maszkok, leírás oda-vissza") {
  const auto dir = temp_dir("szint");
  std::filesystem::create_directories(dir / "sprites" / "units");
  make_sheet(dir / "sprites" / "units", "aaa_egy", 40, 36, 11);
  make_sheet(dir / "sprites" / "units", "bbb_ketto", 52, 44, 12);
  const auto sheets = render::list_sprite_sheets(dir.string());
  CHECK(sheets.size() == 2);
  std::map<int, std::pair<render::Image, render::Image>> pages;
  render::AtlasOptions opt;
  opt.page_size = 128;
  opt.calm = false;   // a pixelre egyezés a szűrő nélkül (a szűrőnek saját próbája van)
  std::vector<std::string> warnings;
  const auto atlas = render::build_atlas(
      sheets, opt, [&](int p, const render::Image& b, const render::Image& m) { pages[p] = {b, m}; }, &warnings);
  CHECK(warnings.empty());
  CHECK(atlas.sheets.size() == 2 && atlas.frames.size() == 24);
  CHECK(atlas.pages >= 2 && static_cast<int>(pages.size()) == atlas.pages);   // több lapra kellett bontani
  bool exact = true;
  int empty = 0;
  for (const auto& s : atlas.sheets) {
    const auto src = render::load_png((dir / "sprites" / "units" / (s.id + ".png")).string());
    const auto msk = render::load_png((dir / "sprites" / "units" / (s.id + "_team.png")).string());
    CHECK(s.directions == 4 && s.columns == 3 && s.has_mask && s.anims.at("walk").fps == 10);
    for (int d = 0; d < 4; ++d) {
      for (int c = 0; c < 3; ++c) {
        const auto& f = atlas.frame(s, d, c);
        if (!f.w) { ++empty; continue; }
        const auto& [pb, pm] = pages.at(f.page);
        // a teljes eredeti képkocka: a vágott részen kívül átlátszó, a vágott rész pixelre egyezik
        for (int y = 0; y < s.frame_h; ++y) {
          for (int x = 0; x < s.frame_w; ++x) {
            const uint8_t* o = src->at(c * s.frame_w + x, d * s.frame_h + y);
            const uint8_t* om = msk->at(c * s.frame_w + x, d * s.frame_h + y);
            const bool inside = x >= f.ox && y >= f.oy && x < f.ox + f.w && y < f.oy + f.h;
            if (!inside) { exact &= o[3] == 0; continue; }
            const uint8_t* a = pb.at(f.x + x - f.ox, f.y + y - f.oy);
            const uint8_t* am = pm.at(f.x + x - f.ox, f.y + y - f.oy);
            exact &= std::equal(o, o + 4, a) && std::equal(om, om + 4, am);
          }
        }
      }
    }
  }
  CHECK(exact);
  CHECK(empty == 2);   // a két üres képkocka nem foglal helyet
  const auto back = render::AtlasData::from_json(atlas.to_json());
  CHECK(back.has_value() && back->to_json() == atlas.to_json());
  CHECK(back->sheet("bbb_ketto") && back->sheet("bbb_ketto")->anchor_y == 42.0f && !back->sheet("nincs"));
  // fél minőség: a képkockák és a horgony fele akkora, a méretarány 1
  render::AtlasOptions half = opt;
  half.half = true;
  const auto h = render::build_atlas(sheets, half, [](int, const render::Image&, const render::Image&) {});
  CHECK(h.half && h.sheet("aaa_egy")->frame_w == 20 && h.sheet("aaa_egy")->scale == 1.0f);
  CHECK(h.pages <= atlas.pages);
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

TEST_CASE("atlasz: valódi sprite-lap (ha a grafika megvan)") {
  const auto json = std::filesystem::path(TOTU_ASSETS_DIR) / "sprites" / "units" / "cro_agyfalo.json";
  if (!std::filesystem::exists(json)) {
    std::printf("  (a grafika nincs meg – kihagyva)\n");
    return;
  }
  int pages = 0;
  const auto atlas = render::build_atlas({json.string()}, {}, [&](int, const render::Image&, const render::Image&) { ++pages; });
  const auto* s = atlas.sheet("cro_agyfalo");
  CHECK(s && s->directions == 8 && s->columns == 28 && s->has_mask && s->scale == 2.0f);
  CHECK(pages == 1 && atlas.frames.size() == 8u * 28u);
  long long used = 0;
  for (const auto& f : atlas.frames) used += static_cast<long long>(f.w) * f.h;
  std::printf("  cro_agyfalo: %zu képkocka, a vágás után az eredeti terület %.0f%%-a\n", atlas.frames.size(),
              100.0 * static_cast<double>(used) / (8.0 * 28.0 * 116.0 * 128.0));
}

TEST_CASE("nyugodt + belső átlag: körvonal a sziluett köré, a belső zaj simul, a felület és a háttér marad") {
  CHECK(render::calm_applies("unit") && render::calm_applies("building") && render::calm_applies("resource"));
  CHECK(render::calm_applies("prop") && !render::calm_applies("decor") && !render::calm_applies("tileset"));
  render::Image img;
  img.width = img.height = 32;
  img.rgba.assign(32 * 32 * 4, 0);
  for (int y = 10; y < 22; ++y) {
    for (int x = 10; x < 22; ++x) {
      uint8_t* p = img.at(x, y);
      p[0] = 200; p[1] = 100; p[2] = 50; p[3] = 255;
    }
  }
  img.at(16, 16)[0] = 206;   // halk zaj belül (élnek nem számít, a medián elsimítja)
  img.at(20, 11)[0] = img.at(20, 11)[1] = img.at(20, 11)[2] = 255;   // erős folt: él, megmarad (élvédelem)
  render::calm_frame(img, 0, 0, 32, 32);
  // 1 képpontos körvonal a sziluett körül, a belső szín 0,40-szerese + (8, 6, 8)
  const uint8_t* ring = img.at(9, 16);
  std::printf("  körvonal: %d %d %d %d; zaj: %d; folt: %d\n", ring[0], ring[1], ring[2], ring[3], img.at(16, 16)[0],
              img.at(20, 11)[2]);
  CHECK(ring[3] == 255);
  CHECK(std::abs(ring[0] - 88) <= 4 && std::abs(ring[1] - 46) <= 4 && std::abs(ring[2] - 28) <= 4);
  CHECK(img.at(8, 16)[3] == 0 && img.at(0, 0)[3] == 0);   // a körvonalon túl átlátszó marad
  // a belső zaj a medián felé simul
  CHECK(img.at(16, 16)[0] <= 202);   // a halk zaj a medián felé simul
  CHECK(img.at(20, 11)[2] > 150);    // az erős folt (él) megmarad
  // a sima felület (a tinta csak a helyi maximumnál sötétebb pontokat érinti) lényegében változatlan
  CHECK(std::abs(img.at(12, 20)[0] - 200) <= 6 && img.at(12, 20)[3] == 255);
}
