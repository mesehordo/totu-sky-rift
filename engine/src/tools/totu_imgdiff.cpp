// SPDX-License-Identifier: GPL-3.0-or-later
// Képernyőkép-összevetés (M5, 6. lépés): a CI képpróbáinak képét a tárolt referenciaképpel veti össze.
//   totu_imgdiff <referencia.png> <új.png> [--threshold T] [--max P] [--out eltérés.png]
// Egy képpont eltér, ha bármely csatornája T-nél többel (alap: 12) különbözik; a próba elbukik, ha az eltérő
// képpontok aránya P ezreléknél (alap: 2) több, vagy a méret különbözik. Az --out kép a referencia halványított
// másolata, az eltérő képpontok pirosak. Kilépési kód: 0 egyezik, 1 eltér, 2 hibás hívás, 3 nincs referencia.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include "render/image.hpp"

using namespace totu;

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "használat: totu_imgdiff <referencia.png> <új.png> [--threshold T] [--max P] [--out kép.png]\n");
    return 2;
  }
  int threshold = 12, max_permille = 2;
  std::string out;
  for (int i = 3; i < argc; ++i) {
    if (std::strcmp(argv[i], "--threshold") == 0 && i + 1 < argc) threshold = std::atoi(argv[++i]);
    else if (std::strcmp(argv[i], "--max") == 0 && i + 1 < argc) max_permille = std::atoi(argv[++i]);
    else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) out = argv[++i];
    else {
      std::fprintf(stderr, "ismeretlen kapcsoló: %s\n", argv[i]);
      return 2;
    }
  }
  if (!std::filesystem::exists(argv[1])) {
    std::printf("%s: nincs referenciakép (az új kép: %s)\n", argv[1], argv[2]);
    return 3;
  }
  std::string err;
  const auto ref = render::load_png(argv[1], &err);
  if (!ref) {
    std::fprintf(stderr, "%s: %s\n", argv[1], err.c_str());
    return 2;
  }
  const auto img = render::load_png(argv[2], &err);
  if (!img) {
    std::fprintf(stderr, "%s: %s\n", argv[2], err.c_str());
    return 2;
  }
  if (ref->width != img->width || ref->height != img->height) {
    std::printf("%s: eltérő méret (%d × %d, a referencia %d × %d)\n", argv[2], img->width, img->height, ref->width,
                ref->height);
    return 1;
  }
  const int pm = render::diff_permille(*ref, *img, threshold);
  if (!out.empty()) {
    render::Image d = *ref;
    const size_t n = static_cast<size_t>(d.width) * static_cast<size_t>(d.height);
    for (size_t i = 0; i < n; ++i) {
      int m = 0;
      for (size_t c = 0; c < 3; ++c) m = std::max(m, std::abs(int{ref->rgba[i * 4 + c]} - int{img->rgba[i * 4 + c]}));
      uint8_t* p = &d.rgba[i * 4];
      if (m > threshold) {
        p[0] = 255;
        p[1] = 0;
        p[2] = 0;
      } else {
        for (int c = 0; c < 3; ++c) p[c] = static_cast<uint8_t>(p[c] / 4);
      }
      p[3] = 255;
    }
    render::save_png(out, d);
  }
  const bool ok = pm <= max_permille;
  std::printf("%s: %d‰ eltér (küszöb %d, megengedett %d‰) – %s\n", argv[2], pm, threshold, max_permille,
              ok ? "egyezik" : "ELTÉR");
  return ok ? 0 : 1;
}
