// SPDX-License-Identifier: GPL-3.0-or-later
// Munkamenet és szimulációs szál (M6, 1. lépés; ADR-007 1. pont).
//
// A munkamenet (`Session`) a szimuláció oldala: pálya, navigáció, szimuláció, és tickenként egy megváltoztathatatlan
// pillanatkép (`Snapshot`) a rajzoláshoz. A `SimRunner` futtatja: külön szálon a fix 30 Hz-es ütem szerint (a rajzolás
// sosem várja a szimulációt), vagy szinkron módban a hívó szálán (képpróba, visszajátszás-nézés – a kép így
// képkockára megismételhető). A kliens parancsait szálbiztos sor viszi át; a parancs a következő tickben érvényesül,
// mint a hálózati játékban. A determinizmust a szál nem érinti: a szimuláció bemenete csak a parancssor.
#pragma once

#include <array>
#include <atomic>
#include <map>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "map/pathfinder.hpp"
#include "sim/sim.hpp"

namespace totu::client {

// egy entitás a pillanatképben (világ-képpontban; 1 csempe = 32 képpont)
struct SnapUnit {
  enum Kind : uint8_t { kUnit = 0, kBuilding = 1, kResource = 2, kShot = 3 };
  sim::EntityId id = 0;
  float x = 0, y = 0;
  int32_t hp = 0, max_hp = 0;
  uint8_t owner = 0;
  uint8_t kind = kUnit;
  uint16_t def = sim::kNoDef;           // a sablon (M6; a bemutató egységeinél nincs)
  int16_t tx = 0, ty = 0, w = 0, h = 0;   // épület és lelőhely: a lábnyom (csempe)
  uint8_t res = 0;                      // lelőhely: a nyersanyag
  int32_t amount = 0;                   // lelőhely: a maradék; munkás: a rakomány
  bool complete = true;
  uint16_t progress = 1000;             // épület: az építés haladása ezrelékben; lövedék: az út aránya
  float sx = 0, sy = 0;                 // lövedék: a kiindulás (az ívhez és az irányhoz)
  float lift = 0;                       // lövedék: az ív magassága most (képpont)
  uint8_t garrison = 0, slots = 0;      // épület: a helyőrség (bent lévők, helyek; D39)
};

// a játékos állapota a felületnek
struct SnapPlayer {
  bool active = false;
  std::array<int32_t, sim::kResources> res{};
  int32_t supply_cap = 0, supply_used = 0;
};

// látványesemény (a részecskéknek): találat (az életerő csökkent) vagy halál
struct FxEvent {
  float x = 0, y = 0;   // világ-képpont (a talpvonal)
  bool death = false;
};

struct Snapshot {
  int64_t tick = 0;
  double published = 0;           // a közzététel ideje (a futtató órája szerint, másodperc) – a simításhoz
  std::vector<SnapUnit> units;    // azonosító szerint rendezve
  // köd (ideiglenes, az M6 6. lépéséig: az 1. játékos egységei körül 8 csempe): 0 ismeretlen, 128 felderített,
  // 255 látható; csak változáskor új (a régit a korábbi pillanatképek megosztva tartják)
  std::shared_ptr<const std::vector<uint8_t>> fog;
  std::array<SnapPlayer, sim::Sim::kMaxPlayers + 1> players{};
  const SnapUnit* find(sim::EntityId id) const;   // bináris keresés
};

class Session {
 public:
  bool load(const std::string& content_dir, const std::string& map_path, uint64_t seed, std::string* error);
  // bemutató seregek: játékosonként `per_player` egység a pálya két oldalán
  void spawn_demo(int players, int per_player);
  void order_demo_attack();   // mindkét sereg a másik felé indul (méréshez, képpróbához)
  // meccs (M6): ha a pályán vannak kezdőhelyek; játékosonként a nép (pl. {"ursan", "ursan"})
  bool has_starts() const { return map_ && !map_->objects().starts.empty(); }
  bool start_match(const std::vector<std::string>& factions, bool upkeep, std::string* error);
  const sim::Rules* rules() const { return rules_ ? &*rules_ : nullptr; }
  // köd nélkül (--nofog): minden látszik, a pillanatkép nem szűr (D41)
  void set_reveal_all(bool on) { reveal_all_ = on; }
  // képpróba (M6 5. lépés): két vegyes ursan sereg a pálya közepe két oldalán, támadó mozgással egymásnak
  void battle_demo(int per_side);

  // egy tick: a parancsok, a szimuláció lépése, majd az új pillanatkép és a tick látványeseményei
  std::shared_ptr<const Snapshot> step(const std::vector<sim::Command>& commands, std::vector<FxEvent>& fx);
  std::shared_ptr<const Snapshot> snapshot();   // a jelenlegi állapot pillanatképe (lépés nélkül)

  // a betöltés után nem változik (a rajzolás szálából is olvasható)
  const map::Map& map() const { return *map_; }
  const map::TerrainTable& terrain() const { return terrain_; }
  sim::Sim& sim() { return *sim_; }

 private:
  void update_fog();
  map::TerrainTable terrain_;
  std::optional<sim::Rules> rules_;   // a szimuláció előtt jön létre, utána szűnik meg
  std::optional<map::Map> map_;
  std::unique_ptr<map::NavGrid> nav_;
  std::unique_ptr<map::Pathfinder> pf_;
  std::unique_ptr<sim::Sim> sim_;
  std::shared_ptr<const Snapshot> last_;
  std::shared_ptr<const std::vector<uint8_t>> fog_;
  // D41: a ködben az 1. játékos a nem saját épületeket és a lelőhelyeket abban az állapotban látja, ahogy utoljára
  // látta (a lerombolt is ott marad, amíg újra oda nem lát)
  std::map<sim::EntityId, SnapUnit> remembered_;
  bool reveal_all_ = false;
  bool tile_visible(int x, int y) const;
  bool rect_visible(int x, int y, int w, int h) const;
};

class SimRunner {
 public:
  explicit SimRunner(Session& s) : s_(s) {}
  ~SimRunner() { stop(); }
  SimRunner(const SimRunner&) = delete;
  SimRunner& operator=(const SimRunner&) = delete;

  void start();                      // külön szál, 30 Hz
  void stop();
  bool threaded() const { return thread_.joinable(); }
  void step_sync();                  // szinkron mód: egy tick a hívó szálán
  void post(sim::Command c);         // a következő tickben hajtódik végre (c.tick-et a futtató állítja)

  // a két legutóbbi pillanatkép (előző, jelenlegi) – a rajzolás a kettő között simít
  void latest(std::shared_ptr<const Snapshot>* prev, std::shared_ptr<const Snapshot>* cur);
  void take_fx(std::vector<FxEvent>& out);       // a legutóbbi lekérés óta keletkezett látványesemények
  void take_tick_ms(std::vector<double>& out);   // a tickek ideje (méréshez)
  double now() const;                            // a futtató órája (másodperc; ehhez mért a `published`)
  static constexpr double kTickSeconds = 1.0 / 30.0;

 private:
  void run_one();
  void loop();
  Session& s_;
  std::thread thread_;
  std::atomic<bool> stop_{false};
  std::mutex mu_;
  std::vector<sim::Command> pending_;
  std::shared_ptr<const Snapshot> prev_, cur_;
  std::vector<FxEvent> fx_;
  std::vector<double> tick_ms_;
  std::vector<FxEvent> fx_scratch_;
};

}  // namespace totu::client
