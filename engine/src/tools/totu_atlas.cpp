// SPDX-License-Identifier: GPL-3.0-or-later
// Atlaszkészítő eszköz (M5): a grafika sprite-lapjaiból 4096²-es atlaszlapok és leírás.
//   totu_atlas <grafika-mappa> <kimeneti-mappa> [--half] [--page N] [--only előtag1,előtag2]
// A kimenet (page_N.png, mask_N.png, atlas.json) gyorsítótár: a játék a grafikából építi, a repóba nem kerül.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "render/atlas_build.hpp"

using namespace totu;

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "használat: totu_atlas <grafika-mappa> <kimeneti-mappa> [--half] [--page N] [--only e1,e2]\n");
    return 2;
  }
  render::AtlasOptions opt;
  std::vector<std::string> only;
  for (int i = 3; i < argc; ++i) {
    if (std::strcmp(argv[i], "--half") == 0) opt.half = true;
    else if (std::strcmp(argv[i], "--page") == 0 && i + 1 < argc) opt.page_size = std::atoi(argv[++i]);
    else if (std::strcmp(argv[i], "--only") == 0 && i + 1 < argc) {
      std::string s = argv[++i];
      size_t p = 0;
      while (p <= s.size()) {
        const size_t q = s.find(',', p);
        only.push_back(s.substr(p, q == std::string::npos ? std::string::npos : q - p));
        if (q == std::string::npos) break;
        p = q + 1;
      }
    }
  }
  if (opt.page_size < 256 || opt.page_size > 16384) {
    std::fprintf(stderr, "a lapméret 256 és 16384 között legyen\n");
    return 2;
  }
  auto sheets = render::list_sprite_sheets(argv[1]);
  if (!only.empty()) {
    std::vector<std::string> keep;
    for (const auto& s : sheets) {
      const std::string name = std::filesystem::path(s).filename().string();
      for (const auto& o : only) {
        if (!o.empty() && name.rfind(o, 0) == 0) { keep.push_back(s); break; }
      }
    }
    sheets = keep;
  }
  const std::filesystem::path out = argv[2];
  std::error_code ec;
  std::filesystem::create_directories(out, ec);
  const auto t0 = std::chrono::steady_clock::now();
  std::vector<std::string> warnings;
  bool write_ok = true;
  const auto atlas = render::build_atlas(
      sheets, opt,
      [&](int page, const render::Image& base, const render::Image& mask) {
        write_ok &= render::save_png((out / ("page_" + std::to_string(page) + ".png")).string(), base);
        write_ok &= render::save_png((out / ("mask_" + std::to_string(page) + ".png")).string(), mask);
        std::printf("  %d. lap kész\n", page);
      },
      &warnings);
  std::ofstream js(out / "atlas.json", std::ios::binary);
  js << atlas.to_json();
  write_ok &= static_cast<bool>(js);
  for (const auto& w : warnings) std::printf("figyelmeztetés: %s\n", w.c_str());
  long long used = 0;
  for (const auto& f : atlas.frames) used += static_cast<long long>(f.w) * f.h;
  const double fill = atlas.pages ? 100.0 * static_cast<double>(used) /
                                        (static_cast<double>(atlas.pages) * opt.page_size * opt.page_size)
                                  : 0.0;
  const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  std::printf("atlasz: %zu sprite-lap, %zu képkocka, %d lap (%d²%s), kitöltés %.0f%%, %.1f s\n", atlas.sheets.size(),
              atlas.frames.size(), atlas.pages, opt.page_size, opt.half ? ", fél minőség" : "", fill, s);
  return write_ok ? 0 : 1;
}
