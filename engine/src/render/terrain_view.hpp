// SPDX-License-Identifier: GPL-3.0-or-later
// Terep-kirajzoló (M5, 3. lépés), a meglévő grafika és a régi játék módszere szerint (WC2-szerű átmenetek): minden
// csempe alapcsempét kap (a helyéből számolt változattal); a magasabb rangú szomszéd terep zajos szélű maszkon át
// ráfolyik (oldalanként és belső sarkonként), a föld és a homok szélén festett, sötét peremsáv; a víz előtt homokos
// part; a víz animált. Képenként csak a látható csempék rajzolódnak (példányos rajzolás, saját shader).
// A szint (D33) egyelőre árnyalással látszik (magaslat világosabb, mélység sötétebb), a szirt sötét sziklasávval –
// a festett szirtgrafika a tulajdonosé, később jön.
#pragma once

#include <string>
#include <vector>

#include "map/map.hpp"
#include "render/rhi.hpp"

namespace totu::render {

class TerrainView {
 public:
  // A csempekészletek (tileset, tileset_winter, tileset_forest) és az átmenet-maszkok betöltése.
  bool init(Device& dev, const std::string& assets_dir, std::string* error);
  void shutdown();
  // Pályaváltáskor (vagy terepváltozáskor) a csempék rajzolási adatainak újraszámolása.
  void set_map(const map::Map& m, const map::TerrainTable& table);
  // A látható rész rajzolása világ-képpontban (1 csempe = 32 képpont); `time` másodpercben (víz animáció).
  void draw(const Mat4& projection, float view_x0, float view_y0, float view_x1, float view_y1, double time);
  int last_quads() const { return last_quads_; }

  static constexpr float kTilePx = 32.0f;   // világ-képpont csempénként

 private:
  struct Quad {
    float x, y, w, h;
    float u0, v0, u1, v1;       // csempe (a tájtömbben)
    float m0, n0, m1, n1;       // maszk (m0 < 0: nincs maszk, teljes fedés)
    uint8_t tint[4];            // a negyedik bájt: a táj rétege
    uint8_t rim[4];             // peremszín és -erősség
  };
  struct Kind {                 // egy rajzolási fajta: (táj, sor, árnyalat)
    int layer = 0, row = 0, variants = 1;
    uint8_t tint[3] = {255, 255, 255};
    int prio = 0;
    int shore = -1;             // a part fajtája (ha van)
    bool water = false, rimmed = false;
  };
  int kind_of(const std::string& set, const std::string& row, const uint8_t tint[3], int prio);
  void emit(const Kind& k, int tx, int ty, int variant, int mask_index, int mask_row, double time,
            const uint8_t shade[3]);

  Device* dev_ = nullptr;
  Program prog_;
  gl::GLuint vao_ = 0, vbo_ = 0;
  gl::GLint loc_proj_ = -1, loc_tiles_ = -1, loc_masks_ = -1;
  Texture tiles_, masks_;
  std::vector<std::string> sets_;                   // a tájtömb rétegei
  std::vector<std::vector<std::pair<std::string, int>>> rows_;   // rétegenként: sor neve → változatok száma
  std::vector<Kind> kinds_;
  // pálya
  int w_ = 0, h_ = 0;
  std::vector<int> kind_;                           // csempénként a fajta
  std::vector<int8_t> level_;
  std::vector<uint8_t> cliff_;
  std::vector<Quad> quads_;
  int last_quads_ = 0;
};

}  // namespace totu::render
