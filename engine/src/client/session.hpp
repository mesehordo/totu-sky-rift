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

#include "content/vfs.hpp"
#include "map/pathfinder.hpp"
#include "script/host.hpp"
#include "sim/replay.hpp"
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
  bool hidden = false;                  // saját, rejtőzött egység (a talajjelhez)
  bool has_rally = false;               // épület: a gyülekezőpont (világ-képpont)
  float rally_x = 0, rally_y = 0;
  uint8_t queue_n = 0;                  // épület: a termelési sor (legfeljebb 5; egység: sablon, kutatás: sorszám)
  std::array<uint16_t, 5> queue_def{};
  uint8_t queue_research = 0;           // bitmaszk: az elem kutatás
  uint16_t queue_progress = 0;          // az első elem haladása ezrelékben
  uint8_t hero_level = 0, hero_points = 0;   // hős (D25): szint, el nem költött képességpont
  std::array<uint16_t, 6> items{};           // hős: a leltár (sim::kNoItem = üres)
  std::vector<uint16_t> learned;             // hős: a megtanult képességek (a szintváltozat)
  // a HUD karakterlapjához és parancsrácsához (M8; a régi `rts/hud.py` adatai)
  enum Order : uint8_t { kRest = 0, kMove, kAttackMove, kAttack, kGather, kGoBuild, kConstruct, kRepair, kHold, kCast };
  uint8_t order = kRest;
  int32_t energy = 0, energy_max = 0, shield = 0, shield_max = 0;
  int32_t armor = 0, damage = 0;             // a kutatásokkal és a hős bónuszaival
  float range = 0, speed = 0;                // csempe; csempe / s
  int32_t xp = 0;                            // hős: tapasztalat
  int64_t life_until = 0;                    // idézett egység: eddig a tickig él (0 = nem jár le)
  uint16_t packed = sim::kNoDef;             // vándorszekér: az összecsomagolt épület sablonja
  std::vector<std::pair<uint16_t, int64_t>> ready, buffs;   // képesség → használható ettől / erősítés eddig (tick)
  std::vector<uint16_t> toggles;             // bekapcsolt kapcsolók és énekek
  // átmeneti állapotok vége (tick), a régi EFFECT_INFO sorrendjében: ég, mérgezett, fagyott, lelassult, átázott, gúzsba
  // kötve, alszik, kábult, megrémült, elvakult, elnémítva, megjelölt, páncéltörés
  std::array<int64_t, 13> afflict{};
  // M9: a karakterlap további sorai (a régi `rts/hud.py` szerint)
  uint8_t rank = 0;                          // előléptetés fokozata (sim::Veteran; 0 újonc, 1 veterán, 2 elit)
  int32_t morale = -1, morale_max = 0;       // Ooni harci kedv (-1: nincs)
  bool broken = false;                       // megtört (a harci kedv elfogyott)
  int16_t prayers = -1;                      // fohászhely: a most fohászkodó munkások száma (-1: nem fohászhely)
};

// a játékos állapota a felületnek
struct SnapPlayer {
  bool active = false;
  std::string faction;                       // pl. `ursan` (a HUD fajkinézetéhez és a parancsrácshoz)
  std::array<int32_t, sim::kResources> res{};
  int32_t supply_cap = 0, supply_used = 0;
  bool defeated = false, ascended = false;
  std::string ally;
  std::string path;
  std::vector<std::string> researched;
  std::array<int32_t, sim::kResources> gathered{};   // az összesítőhöz (egész egység)
  sim::PlayerStats stats;                            // az idősor csak a meccs végén (a másolás miatt)
};

// látványesemény (a részecskéknek): találat (az életerő csökkent) vagy halál
struct FxEvent {
  float x = 0, y = 0;   // világ-képpont (a talpvonal)
  bool death = false;
};

// M11: a küldetés a felületnek (célok, párbeszéd, átvezető, üzenetek, eredmény); a szövegek fordítási kulcsok
struct SnapMission {
  bool active = false;
  struct Objective {
    std::string text;
    uint8_t kind = 0, state = 1;   // kind: 0 elsődleges, 1 másodlagos; state: 1 aktív, 2 kész, 3 elbukott
    bool placed = false;           // van helye a pályán (a régi célgyűrű és jelzőfény)
    float x = 0, y = 0, r = 0;     // világ-képpont; a gyűrű sugara
    std::string marker;            // a jelzőfény felirata
  };
  std::vector<Objective> objectives;   // csak a látható
  std::string dialogue, speaker, portrait, text;   // a futó párbeszéd (üres: nincs)
  std::vector<std::string> choices;
  bool cutscene = false, letterbox = false;
  std::string camera;                  // az utolsó kameraparancs célpontja (azonosító)
  float cam_x = 0, cam_y = 0, cam_zoom = 0;   // csempe; 0 nagyítás: változatlan
  int32_t cam_ticks = 0;
  std::string subtitle, sub_speaker;
  std::vector<std::string> messages;
  uint8_t result = 0;                  // 0 tart, 1 győzelem, 2 bukás
};

struct Snapshot {
  int64_t tick = 0;
  double published = 0;           // a közzététel ideje (a futtató órája szerint, másodperc) – a simításhoz
  std::vector<SnapUnit> units;    // azonosító szerint rendezve
  // köd (ideiglenes, az M6 6. lépéséig: az 1. játékos egységei körül 8 csempe): 0 ismeretlen, 128 felderített,
  // 255 látható; csak változáskor új (a régit a korábbi pillanatképek megosztva tartják)
  std::shared_ptr<const std::vector<uint8_t>> fog;
  std::array<SnapPlayer, sim::Sim::kMaxPlayers + 1> players{};
  int winner = 0;                 // 0: a meccs tart
  int32_t night = 0;              // 0 nappal .. 1000 éjjel
  std::string weather;            // "" = derült
  // a régi `draw_corpses`, `draw_ground_items`: a látott holttestek (a halálanimáció a végén megáll) és a felderített
  // helyen heverő tárgyak
  struct Corpse {
    uint16_t def = sim::kNoDef;
    uint8_t owner = 0;
    float x = 0, y = 0, age = 0;   // világ-képpont; másodperc a halál óta
  };
  std::vector<Corpse> corpses;
  struct Item {
    float x = 0, y = 0;
    uint32_t id = 0;
    uint8_t rarity = 0;            // 0 közönséges, 1 ritka, 2 ereklye
    std::string item;              // a tárgy azonosítója (az ikonhoz)
  };
  std::vector<Item> items;
  SnapMission mission;            // M11
  sim::MatchMode mode;            // M12: győzelmi feltétel, tempó (béke, domb, hullámok)
  // az építési hely próbája (a kísértetkép és a „Nem lehet építeni” üzenet): a kért sablon és csempe, és hogy a
  // szimuláció szerint oda építhető-e (ha nem, az ok a szimuláció szövegével)
  struct PlaceProbe {
    uint16_t def = sim::kNoDef;
    int tx = 0, ty = 0;
    bool ok = true;
    std::string why;
  } place;
  const SnapUnit* find(sim::EntityId id) const;   // bináris keresés
};

class Session {
 public:
  bool load(const std::string& content_dir, const std::string& map_path, uint64_t seed, std::string* error);
  // csak a tartalom (terep, szabályok, nevek) – a hálózati meccs előtt (M10)
  bool load_content(const std::string& content_dir, std::string* error);
  // kívül (a hálózati kliensben) felépített meccs átvétele: a pálya és a szimuláció ott él, a munkamenet csak
  // hivatkozik rájuk (a `match` a munkamenetnél tovább él)
  void adopt(sim::MatchInstance& match);
  const map::TerrainTable* terrain_ptr() const { return &terrain_; }
  // bemutató seregek: játékosonként `per_player` egység a pálya két oldalán
  void spawn_demo(int players, int per_player);
  void order_demo_attack();   // mindkét sereg a másik felé indul (méréshez, képpróbához)
  // meccs (M6): ha a pályán vannak kezdőhelyek; játékosonként a nép (pl. {"ursan", "ursan"})
  bool has_starts() const { return map_ && !map_->objects().starts.empty(); }
  // `ai`: a szkriptelt ellenfél ezekhez a játékosokhoz (M6 9. lépés; `content/base/scripts/ai_ursan.lua`)
  bool start_match(const std::vector<std::string>& factions, bool upkeep, std::string* error, std::vector<int> ai = {2});
  // meccs a teljes beállítással (M8, az egyéni csata képernyője): a gépi játékosok a `scripts/<ai_script>` szkripttel
  // (az adatvezérelt `ai_nep.lua` bármely főnépnek); köd nélkül (Fog::None) a pillanatkép sem szűr
  bool start_match(const sim::MatchSetup& setup, const std::vector<int>& ai, const std::string& ai_script, std::string* error);
  // a küldetés saját szabályai (objektumszerkesztő), ha az átvett meccsnek van; különben a tartalomé
  const sim::Rules* rules() const { return match_rules_ ? match_rules_ : rules_ ? &*rules_ : nullptr; }
  const sim::Rules* content_rules() const { return rules_ ? &*rules_ : nullptr; }   // a meccsindításhoz (felülírás nélkül)
  // a megjelenített név a magyar fordításból (`locale/hu.po`: msgctxt → msgid); ha nincs, maga a kulcs
  std::string display_name(const std::string& key) const;
  // köd nélkül (--nofog): minden látszik, a pillanatkép nem szűr (D41)
  void set_reveal_all(bool on) { reveal_all_ = on; }
  // a helyi játékos (M10: hálózaton a kapott hely): az ő szemével látszik a köd, az ő egységei választhatók
  void set_local_player(uint8_t p) { local_ = p; }
  // az építési hely próbája (a rajzoló szálról; a következő pillanatkép `place` mezője válaszol); kNoDef: nincs
  void set_place_probe(uint16_t def, int tx, int ty) {
    probe_.store((uint64_t{def} << 32) | (uint64_t{static_cast<uint16_t>(tx)} << 16) | static_cast<uint16_t>(ty));
  }
  uint8_t local_player() const { return local_; }
  // képpróba (M6 5. lépés): két vegyes ursan sereg a pálya közepe két oldalán, támadó mozgással egymásnak
  void battle_demo(int per_side);

  // egy tick: a parancsok, a szimuláció lépése, majd az új pillanatkép és a tick látványeseményei
  std::shared_ptr<const Snapshot> step(const std::vector<sim::Command>& commands, std::vector<FxEvent>& fx);
  std::shared_ptr<const Snapshot> snapshot();   // a jelenlegi állapot pillanatképe (lépés nélkül)

  // a betöltés után nem változik (a rajzolás szálából is olvasható)
  const map::Map& map() const { return *map_; }
  const map::TerrainTable& terrain() const { return terrain_; }
  sim::Sim& sim() { return *sim_; }
  bool has_sim() const { return sim_ != nullptr; }
  uint64_t seed() const { return seed_; }
  // M13: az engedélyezett modok betöltési sorrendje (az alappal; `content::select_mods`); a következő betöltéstől hat
  void set_mods(std::vector<content::ModInfo> order) { mods_ = std::move(order); }
  const std::vector<content::ModInfo>& mods() const { return mods_; }

 private:
  void update_fog();
  void hud_fields(sim::EntityId e, SnapUnit& u);   // a HUD adatai (M8): energia, pajzs, páncél, parancs, állapotok
  map::TerrainTable terrain_;
  std::optional<sim::Rules> rules_;   // a szimuláció előtt jön létre, utána szűnik meg
  std::optional<map::Map> own_map_;
  map::Map* map_ = nullptr;   // a saját vagy az átvett (hálózati) meccsé
  std::unique_ptr<map::NavGrid> nav_;
  std::unique_ptr<map::Pathfinder> pf_;
  std::unique_ptr<sim::Sim> own_sim_;
  sim::Sim* sim_ = nullptr;
  std::unique_ptr<script::ScriptHost> ai_;
  std::vector<std::unique_ptr<script::ScriptHost>> ais_;   // M9: az új gépi ellenfél játékosonként
  std::string content_dir_;
  std::vector<content::ModInfo> mods_;   // üres: csak az alap
  std::map<std::string, std::string> names_;
  uint64_t seed_ = 0;
  std::shared_ptr<const Snapshot> last_;
  std::shared_ptr<const std::vector<uint8_t>> fog_;
  // D41: a ködben az 1. játékos a nem saját épületeket és a lelőhelyeket abban az állapotban látja, ahogy utoljára
  // látta (a lerombolt is ott marad, amíg újra oda nem lát)
  std::map<sim::EntityId, SnapUnit> remembered_;
  bool reveal_all_ = false;
  uint8_t local_ = 1;
  std::atomic<uint64_t> probe_{uint64_t{sim::kNoDef} << 32};
  sim::MissionHost* mission_ = nullptr;   // M11: az átvett meccs küldetése
  const sim::Rules* match_rules_ = nullptr;
  map::NavConfig nav_config_;   // a game.json `navigation` része (H-90)
  bool tile_visible(int x, int y) const;
  bool rect_visible(int x, int y, int w, int h) const;
};

// A szimuláció tickforrása (M10): hálózati játékban a lockstep-kliens dönti el, léphet-e a következő tick, és milyen
// parancsokkal; a lépés után megkapja az állapothash-t. Helyi játékban nincs forrás.
class TickSource {
 public:
  virtual ~TickSource() = default;
  virtual bool begin(std::vector<sim::Command>& cmds) = 0;   // false: a következő tick még nincs megerősítve
  virtual void end(uint64_t hash) = 0;
  virtual void submit(sim::Command c) = 0;                    // a helyi játékos parancsa
  virtual int backlog() const = 0;                            // a megérkezett, még le nem futott fordulók
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
  void set_source(TickSource* src) { source_ = src; }   // hálózati játék (a start előtt)
  void reset_snapshots() {   // M11: a munkamenet új meccset vett át (szerkesztő-előnézet)
    std::lock_guard<std::mutex> lock(mu_);
    prev_.reset();
    cur_.reset();
  }
  void set_paused(bool p) { paused_.store(p); }   // szünet: a szimuláció nem lép (a kép tovább rajzolódik)
  // játéksebesség százalékban (a régi `SPEEDS`: 50 … 300); hálózati játékban mindig 100 (a lockstep üteme)
  void set_speed_pct(int p) { speed_pct_.store(source_ ? 100 : std::clamp(p, 50, 300)); }
  int speed_pct() const { return speed_pct_.load(); }
  double tick_seconds() const { return kTickSeconds * 100.0 / static_cast<double>(speed_pct_.load()); }
  bool paused() const { return paused_.load(); }

  // a két legutóbbi pillanatkép (előző, jelenlegi) – a rajzolás a kettő között simít
  void latest(std::shared_ptr<const Snapshot>* prev, std::shared_ptr<const Snapshot>* cur);
  void take_fx(std::vector<FxEvent>& out);       // a legutóbbi lekérés óta keletkezett látványesemények
  void take_tick_ms(std::vector<double>& out);   // a tickek ideje (méréshez)
  double now() const;                            // a futtató órája (másodperc; ehhez mért a `published`)
  static constexpr double kTickSeconds = 1.0 / 30.0;

 private:
  bool run_one();
  void loop();
  TickSource* source_ = nullptr;
  Session& s_;
  std::thread thread_;
  std::atomic<bool> stop_{false};
  std::atomic<bool> paused_{false};
  std::atomic<int> speed_pct_{100};
  std::mutex mu_;
  std::vector<sim::Command> pending_;
  std::shared_ptr<const Snapshot> prev_, cur_;
  std::vector<FxEvent> fx_;
  std::vector<double> tick_ms_;
  std::vector<FxEvent> fx_scratch_;
};

}  // namespace totu::client
