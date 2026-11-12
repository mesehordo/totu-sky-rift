// SPDX-License-Identifier: GPL-3.0-or-later
// A kliens világnézete (M5, 3. lépés): munkamenet (pálya, navigáció, szimuláció fix tickkel) és a megjelenítés
// (kamera, terep, egységek a szimulációból – irány a mozgásból, járó/álló animáció, simítás két tick között,
// Y-rendezés, tartalék alakzat a sprite nélküli egységeknek), kijelölés és csoportos mozgatás alakzatban.
#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "map/pathfinder.hpp"
#include "render/atlas.hpp"
#include "render/lighting.hpp"
#include "render/terrain_view.hpp"
#include "sim/sim.hpp"

namespace totu::client {

struct Camera {
  float x = 0, y = 0;   // a bal felső sarok világ-képpontban
  float zoom = 1.5f;    // képernyő-képpont / világ-képpont (a régi játék alapja: 2 × 0,75; tartomány 1,0–2,0)
};

class World {
 public:
  bool load(const std::string& content_dir, const std::string& map_path, uint64_t seed, std::string* error);
  // Bemutató seregek: játékosonként `per_player` egység a pálya két oldalán.
  void spawn_demo(int players, int per_player);
  void order_demo_attack();   // mindkét sereg a másik felé indul (méréshez, képpróbához)
  // A résztvevő népek egységlapjainak előkészítése egyszerre (a meccs töltésekor; menet közben nincs akadás).
  bool prepare_atlas(render::Atlas& atlas, std::string* error);

  void step();   // egy szimulációs tick (a simításhoz eltárolja az előző helyeket)
  sim::Sim& sim() { return *sim_; }
  const map::Map& map() const { return *map_; }
  const map::TerrainTable& terrain() const { return terrain_; }

  // kijelölés és parancs (az 1. játékos egységei)
  void select_box(float wx0, float wy0, float wx1, float wy1);
  void order_move(float wx, float wy, sim::Formation f);
  size_t selected() const { return selected_.size(); }

  // rajzolás: `alpha` a két tick közötti arány (0..1), `time` másodperc
  void draw(render::SpriteBatch& batch, render::Atlas* atlas, const render::Texture& fallback, const Camera& cam,
            int screen_w, int screen_h, float alpha, double time, bool shadow_pass = false);
  // fényforrások: a fáklyavivők (minden hatodik egység) – a napszak fényrétege éjjel mutatja
  void lights(std::vector<render::Light>& out, float alpha) const;
  // köd (ideiglenes, a kliensben: az 1. játékos egységei körül 8 csempe látható; a valódi látótér az M6-ban):
  // csempénként 0 ismeretlen, 128 felderített, 255 látható; `changed` igaz, ha a legutóbbi lekérés óta változott
  const std::vector<uint8_t>& fog(bool* changed);
  int drawn_units() const { return drawn_; }

  static constexpr float kTilePx = render::TerrainView::kTilePx;

 private:
  struct Visual {
    float px = 0, py = 0;   // az előző tick helye (világ-képpont)
    int dir = 2;            // utolsó irány (0 = K, 2 = D …)
    bool moving = false;
    int sheet = -1;         // az atlaszlap sorszáma (-1 = tartalék alakzat)
  };
  int sheet_for(render::Atlas* atlas, sim::EntityId e, uint8_t owner);

  map::TerrainTable terrain_;
  std::optional<map::Map> map_;
  std::unique_ptr<map::NavGrid> nav_;
  std::unique_ptr<map::Pathfinder> pf_;
  std::unique_ptr<sim::Sim> sim_;
  std::map<sim::EntityId, Visual> vis_;
  std::vector<sim::EntityId> selected_;
  std::map<uint8_t, std::vector<int>> owner_sheets_;   // tulajdonos → a népe egységlapjai
  bool sheets_ready_ = false;
  std::vector<uint8_t> fog_;
  bool fog_dirty_ = true;
  void update_fog();
  int drawn_ = 0;
};

}  // namespace totu::client
