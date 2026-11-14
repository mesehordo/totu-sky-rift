// SPDX-License-Identifier: GPL-3.0-or-later
// A kliens világnézete (M5, 3. lépés; M6 1. lépés óta a pillanatképekből): a szimuláció két legutóbbi pillanatképe
// között simít, az egységek irányát a megtett útból, az animációt a mozgásból számolja; Y-rendezés, takarás, tartalék
// alakzat a sprite nélküli egységeknek; kijelölés és csoportos mozgatás alakzatban (a parancs a futtató sorába megy).
// A szimulációt közvetlenül nem olvassa – így az külön szálon futhat.
#pragma once

#include <array>
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
  // a csapatszínek (M8: a beállítások színvak-barát palettája; játékosonként, az 1. játékos az első)
  void set_team_colors(const std::array<std::array<uint8_t, 4>, 8>& c) { teams_ = c; }

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
  // harc (M6 5. lépés): az ellenség a pont alatt (0 = nincs), támadás, támadó mozgás
  sim::EntityId enemy_at(float wx, float wy) const;
  void order_attack(sim::EntityId target);
  void order_attack_move(float wx, float wy);
  // helyőrség (D39): a pont alatti saját, kész helyőrség-épület (0 = nincs); a kijelöltek beszállnak; U: kiszállás
  sim::EntityId garrison_at(float wx, float wy) const;
  void order_garrison(sim::EntityId building);
  void order_unload(float wx, float wy);
  // életerőcsíkok (a sérült és a kijelölt egységek és épületek fölött); a fő menet után
  void draw_bars(render::SpriteBatch& batch, const render::Texture& white, float alpha) const;
  // helyőrség-jelzés (D39) a saját épületeken: jel, ha bent van valaki; a kurzor alatti épület fölött annyi fej, ahány
  // hely van (a foglaltak a játékos színével, a szabadok szürkén); az építő bentléte nem jelölendő
  void draw_garrison(render::SpriteBatch& batch, const render::Texture& white, float hover_wx, float hover_wy) const;
  // építés (ideiglenes kezelés a HUD-ig): a kísértetkép (sablon, bal felső csempe; kNoDef = nincs), és a parancs az
  // első kijelölt munkással
  void set_ghost(uint16_t def, int tx, int ty) { ghost_def_ = def; ghost_tx_ = tx; ghost_ty_ = ty; }
  bool order_build(uint16_t def, int tx, int ty);
  std::vector<uint16_t> buildable_defs(const std::string& faction) const;
  size_t selected() const { return selected_.size(); }
  // a HUD-nak (M6 9. lépés): a kijelölés, a kijelölt saját épület, a pillanatkép, parancs a futtató sorába
  const std::vector<sim::EntityId>& selection() const { return selected_; }
  sim::EntityId selected_building() const { return selected_building_; }
  // a HUD kijelölése (M8): az egységek, vagy egyetlen épület
  void set_selection(const std::vector<sim::EntityId>& ids);
  const Snapshot* snapshot() const { return cur_.get(); }
  void post(sim::Command c) { runner_.post(std::move(c)); }
  // a kijelöltek képessége a gyorsbillentyűvel (a régi játék szerint): a pont és az alatta álló ellenség a célpont
  bool ability_hotkey(char key, float wx, float wy);

  // rajzolás: `alpha` a két pillanatkép közötti arány (0..1), `time` másodperc
  void draw(render::SpriteBatch& batch, render::Atlas* atlas, const render::Texture& fallback, const Camera& cam,
            int screen_w, int screen_h, float alpha, double time, bool shadow_pass = false);
  // fényforrások: a fáklyavivők (minden hatodik egység) – a napszak fényrétege éjjel mutatja
  void lights(std::vector<render::Light>& out, float alpha) const;
  // köd (a pillanatképből); `changed` igaz, ha a legutóbbi lekérés óta változott
  const std::vector<uint8_t>& fog(bool* changed);
  int drawn_units() const { return drawn_; }
  size_t unit_count() const { return cur_ ? cur_->units.size() : 0; }
  const SnapPlayer* player(int i) const { return cur_ && i >= 0 && i <= sim::Sim::kMaxPlayers ? &cur_->players[static_cast<size_t>(i)] : nullptr; }

  // látványesemények a legutóbbi lekérés óta (a részecskéknek); a hívó üríti
  std::vector<FxEvent>& fx_events() { return fx_; }

  static constexpr float kTilePx = render::TerrainView::kTilePx;

 private:
  std::array<std::array<uint8_t, 4>, 8> teams_ = {{{220, 40, 40, 255},  {40, 90, 230, 255},  {40, 180, 60, 255},
                                                  {230, 200, 40, 255}, {160, 60, 200, 255}, {240, 130, 30, 255},
                                                  {40, 200, 200, 255}, {235, 235, 235, 255}}};
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
    float x, y;   // a rajzolás helye (egységnél a talp, épületnél és lelőhelynél a lábnyom bal felső sarka)
    float key;    // Y-rendezés kulcsa (a talpvonal)
    size_t i;     // index a pillanatképben (és a vis_-ben)
  };
  int sheet_for(render::Atlas* atlas, sim::EntityId e, uint8_t owner);
  int sheet_by_id(render::Atlas* atlas, const std::string& id);   // -1, ha nincs ilyen lap
  int sheet_of(render::Atlas* atlas, const SnapUnit& u);          // a sablon vagy a nyersanyag lapja
  std::map<std::string, int> sheet_ids_;
  void collect(const Camera& cam, int sw, int sh, float alpha);

  Session& session_;
  SimRunner& runner_;
  std::shared_ptr<const Snapshot> prev_, cur_;
  std::vector<Visual> vis_;
  std::vector<sim::EntityId> selected_;
  sim::EntityId selected_building_ = 0;
  std::map<uint8_t, std::vector<int>> owner_sheets_;   // tulajdonos → a népe egységlapjai
  bool sheets_ready_ = false;
  std::shared_ptr<const std::vector<uint8_t>> fog_seen_;
  std::vector<FxEvent> fx_;
  uint16_t ghost_def_ = sim::kNoDef;
  int ghost_tx_ = 0, ghost_ty_ = 0;
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
