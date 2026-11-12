// SPDX-License-Identifier: GPL-3.0-or-later
// A szimuláció magja (M2): fix tick, parancssor, rendszerek, véletlenszám-folyamok, állapothash, mentés.
// Az M2-ben egy kicsi, de valódi szimulációval: pozíció, mozgás, életerő, támadás – ez igazolja a
// determinizmust mind az öt rendszeren (a kapu). A teljes szabályrendszer (útkeresés, gazdaság, képességek …)
// az M4–M7-ben erre épül.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/bytes.hpp"
#include "core/fixed.hpp"
#include "core/hash.hpp"
#include "core/rng.hpp"
#include "sim/world.hpp"

namespace totu::map {
class Pathfinder;
struct TileRect;
}

namespace totu::sim {

using core::Fixed;

// --- komponensek ---------------------------------------------------------------------------------------------------
struct Position {
  Fixed x, y;
  void hash(core::StateHash& h) const { h.add_i64(x.raw()); h.add_i64(y.raw()); }
  void save(core::ByteWriter& w) const { w.i32(x.raw()); w.i32(y.raw()); }
  static Position load(core::ByteReader& r) { Position p; p.x = Fixed::from_raw(r.i32()); p.y = Fixed::from_raw(r.i32()); return p; }
};

struct Movement {
  Fixed speed;            // csempe / tick
  Fixed tx, ty;           // cél
  bool moving = false;
  void hash(core::StateHash& h) const { h.add_i64(speed.raw()); h.add_i64(tx.raw()); h.add_i64(ty.raw()); h.add_u64(moving); }
  void save(core::ByteWriter& w) const { w.i32(speed.raw()); w.i32(tx.raw()); w.i32(ty.raw()); w.u8(moving ? 1 : 0); }
  static Movement load(core::ByteReader& r) {
    Movement m;
    m.speed = Fixed::from_raw(r.i32()); m.tx = Fixed::from_raw(r.i32()); m.ty = Fixed::from_raw(r.i32());
    m.moving = r.u8() != 0;
    return m;
  }
};

struct Health {
  int32_t hp = 0, max = 0;
  uint8_t owner = 0;
  void hash(core::StateHash& h) const { h.add_i64(hp); h.add_i64(max); h.add_u64(owner); }
  void save(core::ByteWriter& w) const { w.i32(hp); w.i32(max); w.u8(owner); }
  static Health load(core::ByteReader& r) { Health x; x.hp = r.i32(); x.max = r.i32(); x.owner = r.u8(); return x; }
};

struct Attack {
  int32_t damage = 0, spread = 0;   // sebzés = damage + véletlen [0, spread)
  Fixed range;
  int32_t cooldown = 30;            // tick
  int64_t ready_tick = 0;
  void hash(core::StateHash& h) const { h.add_i64(damage); h.add_i64(spread); h.add_i64(range.raw()); h.add_i64(cooldown); h.add_i64(ready_tick); }
  void save(core::ByteWriter& w) const { w.i32(damage); w.i32(spread); w.i32(range.raw()); w.i32(cooldown); w.i64(ready_tick); }
  static Attack load(core::ByteReader& r) {
    Attack a;
    a.damage = r.i32(); a.spread = r.i32(); a.range = Fixed::from_raw(r.i32()); a.cooldown = r.i32();
    a.ready_tick = r.i64();
    return a;
  }
};

// Navigációs komponensek (M4): csak csatolt pálya esetén kerülnek a világba (attach_navigation), így pálya nélkül a
// szimuláció állapota és hash-e az M2/M3-as marad.
struct Body {
  Fixed radius = Fixed::from_ratio(2, 5);
  int8_t grid = 0;          // navrács-index (NavGrid::grid_of); -1 = repülő
  uint8_t size = 0;         // méretosztály
  void hash(core::StateHash& h) const { h.add_i64(radius.raw()); h.add_i64(grid); h.add_u64(size); }
  void save(core::ByteWriter& w) const { w.i32(radius.raw()); w.u8(static_cast<uint8_t>(grid)); w.u8(size); }
  static Body load(core::ByteReader& r) {
    Body b;
    b.radius = Fixed::from_raw(r.i32());
    b.grid = static_cast<int8_t>(r.u8());
    b.size = r.u8();
    return b;
  }
};

struct Path {
  Fixed gx, gy;                                   // a kért cél
  std::vector<std::pair<Fixed, Fixed>> wps;       // útpontok
  uint32_t idx = 0;                               // a következő útpont
  uint8_t pending = 0;                            // útkérés vár a sorban
  uint8_t shoved = 0;                             // félretolt egység (megérkezve megáll, nem keres utat)
  int64_t request_tick = 0;
  int32_t stuck = 0;                              // tick óta nincs haladás
  int32_t repaths = 0;
  int32_t wait = 0;                               // tick óta vár egy mozgó egység mögött (sor a szűkületben)
  Fixed best;                                     // a következő útponttól mért eddigi legkisebb távolság
  void hash(core::StateHash& h) const {
    h.add_i64(gx.raw()); h.add_i64(gy.raw()); h.add_u64(wps.size());
    for (const auto& [x, y] : wps) { h.add_i64(x.raw()); h.add_i64(y.raw()); }
    h.add_u64(idx); h.add_u64(pending); h.add_u64(shoved); h.add_i64(request_tick); h.add_i64(stuck);
    h.add_i64(repaths); h.add_i64(wait); h.add_i64(best.raw());
  }
  void save(core::ByteWriter& w) const {
    w.i32(gx.raw()); w.i32(gy.raw()); w.u32(static_cast<uint32_t>(wps.size()));
    for (const auto& [x, y] : wps) { w.i32(x.raw()); w.i32(y.raw()); }
    w.u32(idx); w.u8(pending); w.u8(shoved); w.i64(request_tick); w.i32(stuck); w.i32(repaths); w.i32(wait); w.i32(best.raw());
  }
  static Path load(core::ByteReader& r) {
    Path p;
    p.gx = Fixed::from_raw(r.i32()); p.gy = Fixed::from_raw(r.i32());
    const uint32_t n = r.u32();
    for (uint32_t i = 0; i < n && i < 100000 && r.ok(); ++i) {
      const Fixed x = Fixed::from_raw(r.i32()), y = Fixed::from_raw(r.i32());
      p.wps.emplace_back(x, y);
    }
    p.idx = r.u32(); p.pending = r.u8(); p.shoved = r.u8(); p.request_tick = r.i64(); p.stuck = r.i32();
    p.repaths = r.i32(); p.wait = r.i32(); p.best = Fixed::from_raw(r.i32());
    return p;
  }
};

// A navigációs mozgás beállításai (ADR-006 5–6. pont).
struct NavParams {
  int path_budget = 32;                         // útkeresés tickenként (a többi a sorban vár)
  int stuck_ticks = 15;                         // ennyi tick haladás nélkül → kitérés / újratervezés
  int wait_ticks = 120;                         // ennyi várakozás egy mozgó egység mögött számít beragadásnak
  int max_repaths = 3;                          // utána megáll
  Fixed arrive_slack = Fixed::from_int(3);      // beragadva ennyin belül a célhoz: megérkezett (tömeg a célnál)
  Fixed hard_ratio = Fixed::from_ratio(7, 10);  // kemény sugár = sugár × arány (soha nem lapolódhat át)
};

struct NavStats {
  int paths_this_tick = 0;
  int queued = 0;
  int64_t paths_total = 0;
  int64_t direct_total = 0;    // látóvonalas közvetlen utak (keresés nélkül)
};

// --- parancsok -----------------------------------------------------------------------------------------------------
struct Command {
  enum class Kind : uint8_t { Spawn = 1, Move = 2 };
  Kind kind = Kind::Spawn;
  int64_t tick = 0;          // ebben a tickben hajtódik végre
  uint8_t owner = 0;         // Spawn
  Fixed x, y;                // Spawn: hely; Move: cél
  int32_t hp = 100;          // Spawn
  Fixed speed;               // Spawn
  int32_t damage = 10, spread = 4, cooldown = 30;   // Spawn
  Fixed range;               // Spawn
  std::vector<EntityId> units;   // Move
};

class Sim;

// Eseménycsatlakozó (M3): a szkriptréteg (script/host) ezen át kapcsolódik a szimulációhoz. A csatlakozó állapota
// a szimuláció állapotának része: benne van a hashben és a mentésben (a csatlakoztatás sorrendjében).
class SimHooks {
 public:
  virtual ~SimHooks() = default;
  virtual void on_tick(Sim& sim) = 0;                              // a tick parancsai után, a rendszerek előtt
  virtual void on_death(Sim& sim, EntityId id, uint8_t owner) = 0;   // a rendszerek után, a törlés előtt
  virtual uint64_t hash() const = 0;
  virtual void save(core::ByteWriter& w) const = 0;
  virtual bool load(core::ByteReader& r) = 0;
};

class Sim {
 public:
  explicit Sim(uint64_t seed);

  // A csatlakozót a hívó birtokolja, és túl kell élnie a szimulációt. Betöltés előtt ugyanazokat a csatlakozókat
  // kell ugyanabban a sorrendben felvenni, mint mentéskor.
  void add_hooks(SimHooks* h) { hooks_.push_back(h); }

  // Közvetlen műveletek a csatlakozóknak (a szkripteknek): azonnal hatnak, ezért csak a tick rögzített pontjain
  // (on_tick / on_death) hívhatók – így a sorrend minden gépen ugyanaz.
  EntityId spawn(const Command& c);                       // Spawn parancs azonnal; az új egység azonosítója
  bool order_move(EntityId e, Fixed x, Fixed y);
  bool kill(EntityId e);                                  // életerő 0, törlés a tick végén, halál-esemény

  // Navigáció (M4): a pálya útkeresőjének csatolása (az egységek létrehozása előtt). Utána a mozgás útkereséssel,
  // ütközéssel és kitéréssel megy; az útkereső a hívóé. Mentés betöltése előtt ugyanígy csatolni kell.
  void attach_navigation(map::Pathfinder* pf, NavParams params = {});
  bool has_navigation() const { return nav_ != nullptr; }
  // A test (sugár, mozgásosztály a TerrainTable sorszámával) beállítása; az alapérték 0,4 sugarú szárazföldi.
  bool set_body(EntityId e, Fixed radius, uint8_t movement_class);
  // Akadály- vagy terepváltozás után: minden mozgó egység újratervez (a következő útkeresési körben).
  void navigation_changed();
  const NavStats& nav_stats() const { return nav_stats_; }

  void queue(Command c);                 // a parancs a saját tickjében hajtódik végre
  void step();                           // egy tick (1/30 s)
  int64_t tick() const { return tick_; }
  uint64_t hash() const;                 // a teljes állapot (világ + véletlenszám-folyamok + tick)
  // komponensenként, majd csatlakozónként („Szkript1”, …) – az eltéréskereső ebből mondja meg, mi tért el
  std::vector<std::pair<std::string, uint64_t>> component_hashes() const;
  World& world() { return world_; }

  std::vector<uint8_t> save() const;     // teljes mentés (a még végre nem hajtott parancsokkal)
  bool load(const std::vector<uint8_t>& data);

 private:
  void apply(const Command& c);
  void report_deaths();
  void nav_request(EntityId e, Fixed x, Fixed y);
  void nav_paths();
  void nav_movement();
  void nav_rebuild_buckets();
  bool nav_free(EntityId e, const Body& b, uint8_t owner, Fixed x, Fixed y, Fixed cx, Fixed cy, EntityId* blocker);
  void nav_bucket_move(EntityId e, Fixed ox, Fixed oy, Fixed nx, Fixed ny);
  bool nav_shove(EntityId blocker, Fixed vx, Fixed vy, Fixed px, Fixed py);
  void nav_arrive(EntityId e, Movement& m, Path& p, const Position& pos);
  void movement_system();
  void combat_system();
  void register_components();

  World world_;
  core::Pcg32 combat_rng_;
  int64_t tick_ = 0;
  std::vector<Command> pending_;         // a jövőbeli parancsok (tick szerint, beérkezési sorrendben)
  std::vector<std::pair<EntityId, uint8_t>> died_;   // a tick halottai (a halál sorrendjében)
  std::vector<SimHooks*> hooks_;
  map::Pathfinder* nav_ = nullptr;
  NavParams nav_params_;
  NavStats nav_stats_;
  int nav_bw_ = 0, nav_bh_ = 0;                  // a térbeli rács (2 × 2 csempés vödrök) mérete
  std::vector<std::vector<EntityId>> buckets_;  // vödrönként az egységek (nem állapot: tickenként újraépül)
};

void save_command(core::ByteWriter& w, const Command& c);
std::optional<Command> load_command(core::ByteReader& r);

}  // namespace totu::sim
