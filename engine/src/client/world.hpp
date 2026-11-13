// SPDX-License-Identifier: GPL-3.0-or-later
// A kliens világnézete (M5, 3. lépés; M6 1. lépés óta a pillanatképekből): a szimuláció két legutóbbi pillanatképe
// között simít, az egységek irányát a megtett útból, az animációt a mozgásból számolja; Y-rendezés, takarás, tartalék
// alakzat a sprite nélküli egységeknek; kijelölés és csoportos mozgatás alakzatban (a parancs a futtató sorába megy).
// A szimulációt közvetlenül nem olvassa – így az külön szálon futhat.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "client/session.hpp"
#include "render/atlas.hpp"
#include "render/lighting.hpp"
#include "render/terrain_view.hpp"

namespace totu::client {

struct Camera {
  float x = 0, y = 0;   // a bal felső sarok világ-képpontban
  float zoom = 1.5f;    // képernyő-képpont / világ-képpont (a régi játék alapja: 2 × 0,75; tartomány 1,0–2,0)
};

class World {
 public:
  World(Session& session, SimRunner& runner) : session_(session), runner_(runner) {}

  // A résztvevő népek egységlapjainak előkészítése egyszerre (a meccs töltésekor; menet közben nincs akadás).
  bool prepare_atlas(render::Atlas& atlas, std::string* error);

  // Képkockánként egyszer: a két legutóbbi pillanatkép átvétele (új tick esetén az irányok és az animációk frissülnek).
  void update();
  // a simítás aránya a két pillanatkép között a futtató órája szerint (szálas módban)
  float live_alpha() const;
  int64_t tick() const { return cur_ ? cur_->tick : 0; }

  const map::Map& map() const { return session_.map(); }
  const map::TerrainTable& terrain() const { return session_.terrain(); }

  // kijelölés és parancs (az 1. játékos egységei)
  void select_box(float wx0, float wy0, float wx1, float wy1);
  void order_move(float wx, float wy, sim::Formation f);
  size_t selected() const { return selected_.size(); }

  // rajzolás: `alpha` a két pillanatkép közötti arány (0..1), `time` másodperc
  void draw(render::SpriteBatch& batch, render::Atlas* atlas, const render::Texture& fallback, const Camera& cam,
            int screen_w, int screen_h, float alpha, double time, bool shadow_pass = false);
  // fényforrások: a fáklyavivők (minden hatodik egység) – a napszak fényrétege éjjel mutatja
  void lights(std::vector<render::Light>& out, float alpha) const;
  // köd (a pillanatképből); `changed` igaz, ha a legutóbbi lekérés óta változott
  const std::vector<uint8_t>& fog(bool* changed);
  int drawn_units() const { return drawn_; }
  size_t unit_count() const { return cur_ ? cur_->units.size() : 0; }

  // látványesemények a legutóbbi lekérés óta (a részecskéknek); a hívó üríti
  std::vector<FxEvent>& fx_events() { return fx_; }

  static constexpr float kTilePx = render::TerrainView::kTilePx;

 private:
  struct Visual {   // a jelenlegi pillanatkép egységeivel párhuzamos (azonos sorrend)
    sim::EntityId id = 0;
    float px = 0, py = 0;   // az előző pillanatkép helye (új egységnél a mostani)
    int dir = 2;            // utolsó irány (0 = K, 2 = D …)
    bool moving = false;
    int sheet = -1;         // az atlaszlap sorszáma (-1 = tartalék alakzat, -2 = még nincs kiválasztva)
    const render::AtlasAnim* walk = nullptr;   // a lap animációi (egyszer feloldva)
    const render::AtlasAnim* idle = nullptr;
  };
  struct Item {
    float x, y;
    size_t i;   // index a pillanatképben (és a vis_-ben)
  };
  int sheet_for(render::Atlas* atlas, sim::EntityId e, uint8_t owner);
  void collect(const Camera& cam, int sw, int sh, float alpha);

  Session& session_;
  SimRunner& runner_;
  std::shared_ptr<const Snapshot> prev_, cur_;
  std::vector<Visual> vis_;
  std::vector<sim::EntityId> selected_;
  std::map<uint8_t, std::vector<int>> owner_sheets_;   // tulajdonos → a népe egységlapjai
  bool sheets_ready_ = false;
  std::shared_ptr<const std::vector<uint8_t>> fog_seen_;
  std::vector<FxEvent> fx_;
  int drawn_ = 0;
  std::vector<Item> items_;
  struct ItemsKey {
    int64_t tick = -1;
    float alpha = -1, x = 0, y = 0, zoom = 0;
    int sw = 0, sh = 0;
    bool operator==(const ItemsKey&) const = default;
  } items_key_;
};

}  // namespace totu::client
