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
  // a régi szerint csak az egér alatti egység / épület csíkjai: fölül pajzs (kék), életerő (4 képpont; zöld > 60 % >
  // sárga > 30 % > piros), alul energia (lila, saját egységnél); `hover_*`: az egér világ-képpontban
  void draw_bars(render::SpriteBatch& batch, const render::Texture& white, float alpha, float hover_wx, float hover_wy) const;
  // helyőrség-jelzés (D39) a saját épületeken: jel, ha bent van valaki; a kurzor alatti épület fölött annyi fej, ahány
  // hely van (a foglaltak a játékos színével, a szabadok szürkén); az építő bentléte nem jelölendő
  void draw_garrison(render::SpriteBatch& batch, const render::Texture& white, float hover_wx, float hover_wy) const;
  // építés (ideiglenes kezelés a HUD-ig): a kísértetkép (sablon, bal felső csempe; kNoDef = nincs), és a parancs az
  // első kijelölt munkással
  void set_ghost(uint16_t def, int tx, int ty, bool ok = true) { ghost_def_ = def; ghost_tx_ = tx; ghost_ty_ = ty; ghost_ok_ = ok; }
  // a régi lerakási rács (a csapatszínben) és a lábnyom zöld/piros kerete – a kísértetkép alatt, a talajon
  void draw_placement_grid(render::SpriteBatch& batch, const render::Texture& white) const;
  // a tervezett épületek (a régi szerint): a lábnyom kék rácsa és az épület halvány képe
  void draw_planned(render::SpriteBatch& batch, const render::Texture& white, render::Atlas* atlas);
  bool order_build(uint16_t def, int tx, int ty);
  std::vector<uint16_t> buildable_defs(const std::string& faction) const;
  size_t selected() const { return selected_.size(); }
  // a HUD-nak (M6 9. lépés): a kijelölés, a kijelölt saját épület, a pillanatkép, parancs a futtató sorába
  const std::vector<sim::EntityId>& selection() const { return selected_; }
  // a kijelölés pecsétjei (a régi `sel_sigil`): saját (zöld), idegen (piros); a képek `sigil_base` gyűrűszélességgel
  // készültek, rajzoláskor a figura méretére nagyítva
  // a gyülekezőpont képei (a régi szerint): nyíl (16 irány rétegenként), kis pecsét (fehér), rúd, zászló és árnyék
  // (8 lengésfázis rétegenként); a zászló, a nyíl és a pecsét a csapatszínnel árnyalva
  struct RallyArt {
    const render::Texture *arrows = nullptr, *sigil = nullptr, *pole = nullptr, *cloth = nullptr, *shadow = nullptr;
    const render::Texture* move_sigil = nullptr;   // a mozgáscél: zöld, befelé mutató pecsét (38 képpont)
  };
  void set_rally_art(const RallyArt& a) { rally_ = a; }
  // a csapatjelző lobogó (D51): rúd és csúcsdísz (stílusonként egy réteg), vászon és szegély (stílus × 8 fázis réteg)
  struct BannerArt {
    const render::Texture *pole = nullptr, *knob = nullptr, *cloth = nullptr, *trim = nullptr;
  };
  void set_banner_art(BannerArt a) { banner_ = std::move(a); }
  // a munkások cipelt nyersanyaga (8. tétel): a zsák a figura jobb vállánál, a nyersanyag színében
  void set_sack(const render::Texture* t) { sack_ = t; }
  // a földön heverő tárgyak (11. tétel): ikonok azonosító szerint (az assets/items képeiből) és a fényudvar
  void set_item_art(std::map<std::string, const render::Texture*> icons, const render::Texture* glow) {
    item_icons_ = std::move(icons);
    item_glow_ = glow;
  }
  void draw_ground_items(render::SpriteBatch& batch, const render::Texture& white, double time) const;
  // légköri részecskék (7. tétel, a régi `Ambient` erdei készlete): szentjánosbogár, falevél, pollen – a világhoz
  // rögzítve, a látómező körül körbetekerve
  void set_ambient_art(const render::Texture* glow, const render::Texture* leaf, const render::Texture* dot) {
    amb_glow_ = glow;
    amb_leaf_ = leaf;
    amb_dot_ = dot;
  }
  void draw_ambient(render::SpriteBatch& batch, const Camera& cam, int sw, int sh, double time) const;
  // a terep apró díszletei (6. tétel, a régi `_scatter_doodads`): virág, fűcsomó, kavics, gomba, bokor, tuskó, farönk,
  // nád a parton, tavirózsa, csont – egyszer szórva (rögzített véletlen), a figurák alatt
  void draw_doodads(render::SpriteBatch& batch, render::Atlas* atlas, const Camera& cam, int sw, int sh);
  // a terep fedőrétege (földutak, talajfoltok – client/ground_overlay), az egész pályára
  void set_ground_overlay(const render::Texture* t) { ground_ = t; }
  // talajjelek és a hős jelei (9. tétel): hős talajjel (színezhető + rögzített), jelvény (keret + jelkép, stílusonként
  // egy réteg), ének (8 fázis), rejtőzés
  struct MarkArt {
    const render::Texture *hero_tint = nullptr, *hero_fixed = nullptr, *badge_frame = nullptr, *badge_symbol = nullptr, *chant = nullptr,
                          *hidden = nullptr;
  };
  void set_mark_art(const MarkArt& m) { marks_ = m; }
  // a küldetés célgyűrűje és jelzőfénye (10. tétel): a gyűrű 12 fázis (64-es sugárra rajzolva), a kristály
  void set_objective_art(const render::Texture* ring, const render::Texture* beacon) { obj_ring_ = ring; obj_beacon_ = beacon; }
  void draw_objective_rings(render::SpriteBatch& batch, double time) const;     // a talajon
  void draw_objective_beacons(render::SpriteBatch& batch, double time) const;   // a figurák fölött
  void draw_ground_marks(render::SpriteBatch& batch, float alpha, double time) const;   // a figurák alatt
  void draw_hero_badges(render::SpriteBatch& batch, float alpha, double time) const;    // a figurák fölött
  void draw_carry(render::SpriteBatch& batch, float alpha) const;
  void draw_banners(render::SpriteBatch& batch, double time) const;
  // a kijelölt saját épület gyülekezőpontja (jobb kattintás, ha csak épület van kijelölve – a régi szerint); igaz: kiadva
  bool order_rally(float wx, float wy);
  // a talajra (az egységek alá): a nyíl-ösvény és a pecsét; az egységek fölé: a zászló
  void draw_rally_ground(render::SpriteBatch& batch, double time) const;
  void draw_rally_flags(render::SpriteBatch& batch, double time) const;
  // a mozgáscél jelzése (a régi `draw_move_indicator`): a kijelölt csoporttól a legutóbbi mozgásparancs céljáig
  // menetelő zöld nyilak, a célnál befelé mutató pecsét – amíg a kijelöltek közül valaki még tart oda
  void draw_move_indicator(render::SpriteBatch& batch, float alpha, double time) const;
  void set_sigils(const render::Texture* own, const render::Texture* other, float sigil_base) {
    sigil_own_ = own;
    sigil_other_ = other;
    sigil_base_ = sigil_base;
  }
  sim::EntityId selected_building() const { return selected_building_; }
  // a HUD kijelölése (M8): az egységek, vagy egyetlen épület
  void set_selection(const std::vector<sim::EntityId>& ids);
  const Snapshot* snapshot() const { return cur_.get(); }
  std::shared_ptr<const Snapshot> snapshot_shared() const { return cur_; }   // a HUD megtartja a képkocka végéig
  // a parancs a futtató sorába; Shift (`queue_mode`) mellett az egységparancs sorba áll (D46)
  void post(sim::Command c);
  bool queue_mode = false;
  // D46 irányítás: járőr (a kijelöltek a mostani helyük és a pont között), vonalra mozgatás (a kijelöltek egyenletesen a
  // szakaszon, a vonal mentén rendezve), a kijelölés középpontja (a kontrollcsoport kétszeri lenyomásához)
  void order_patrol(float wx, float wy);
  void order_line(float wx0, float wy0, float wx1, float wy1);
  bool selection_center(float& wx, float& wy) const;
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
  const render::Texture* sigil_own_ = nullptr;
  const render::Texture* sigil_other_ = nullptr;
  float sigil_base_ = 48;
  RallyArt rally_;
  BannerArt banner_;
  const render::Texture* sack_ = nullptr;
  std::map<std::string, const render::Texture*> item_icons_;
  const render::Texture* item_glow_ = nullptr;
  const render::Texture *amb_glow_ = nullptr, *amb_leaf_ = nullptr, *amb_dot_ = nullptr;
  struct AmbPart {
    uint8_t kind;   // 0 szentjánosbogár, 1 falevél, 2 pollen
    float a, b, c, d;
  };
  mutable std::vector<AmbPart> amb_;
  struct Doodad {
    float x, y;
    uint8_t frame;
  };
  mutable std::vector<Doodad> doodads_;
  mutable bool doodads_done_ = false;
  const render::Texture* ground_ = nullptr;
  MarkArt marks_;
  const render::Texture *obj_ring_ = nullptr, *obj_beacon_ = nullptr;
  std::array<uint8_t, 3> faction_accent(const std::string& fid) const;
  bool last_move_ = false;   // a legutóbbi saját mozgásparancs célja (világ-képpont)
  float last_move_x_ = 0, last_move_y_ = 0;
  void note_move(float x, float y) { last_move_ = true; last_move_x_ = x; last_move_y_ = y; }
  std::array<uint8_t, 3> team_color(uint8_t owner) const;
  sim::EntityId selected_building_ = 0;
  std::map<uint8_t, std::vector<int>> owner_sheets_;   // tulajdonos → a népe egységlapjai
  bool sheets_ready_ = false;
  std::shared_ptr<const std::vector<uint8_t>> fog_seen_;
  std::vector<FxEvent> fx_;
  uint16_t ghost_def_ = sim::kNoDef;
  int ghost_tx_ = 0, ghost_ty_ = 0;
  bool ghost_ok_ = true;   // a szimuláció szerint építhető (különben piros – a régi `can_place` a kurzornál)
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
