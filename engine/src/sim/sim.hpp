// SPDX-License-Identifier: GPL-3.0-or-later
// A szimuláció magja (M2): fix tick, parancssor, rendszerek, véletlenszám-folyamok, állapothash, mentés.
// Az M2-ben egy kicsi, de valódi szimulációval: pozíció, mozgás, életerő, támadás – ez igazolja a
// determinizmust mind az öt rendszeren (a kapu). A teljes szabályrendszer (útkeresés, gazdaság, képességek …)
// az M4–M7-ben erre épül.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/bytes.hpp"
#include "core/fixed.hpp"
#include "core/hash.hpp"
#include "core/rng.hpp"
#include "sim/formation.hpp"
#include "sim/rules.hpp"
#include "sim/world.hpp"

namespace totu::map {
class Map;
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
  uint8_t group = 0;                              // alakzat tagja (a saját helyére megy)
  int64_t request_tick = 0;
  int64_t moved_tick = -1000;                     // az utolsó tick, amikor ténylegesen elmozdult
  int32_t stuck = 0;                              // tick óta nincs haladás
  int32_t repaths = 0;
  int32_t retries = 0;                            // dugó miatti újrakezdések (utána végleg megáll)
  int32_t wait = 0;                               // tick óta vár egy mozgó egység mögött (sor a szűkületben)
  Fixed cap;                                      // csoportsebesség (0 = nincs): a leglassabb tag sebessége
  Fixed best;                                     // a következő útponttól mért eddigi legkisebb távolság
  void hash(core::StateHash& h) const {
    h.add_i64(gx.raw()); h.add_i64(gy.raw()); h.add_u64(wps.size());
    for (const auto& [x, y] : wps) { h.add_i64(x.raw()); h.add_i64(y.raw()); }
    h.add_u64(idx); h.add_u64(pending); h.add_u64(shoved); h.add_u64(group); h.add_i64(request_tick); h.add_i64(moved_tick); h.add_i64(stuck);
    h.add_i64(repaths); h.add_i64(retries); h.add_i64(wait); h.add_i64(cap.raw()); h.add_i64(best.raw());
  }
  void save(core::ByteWriter& w) const {
    w.i32(gx.raw()); w.i32(gy.raw()); w.u32(static_cast<uint32_t>(wps.size()));
    for (const auto& [x, y] : wps) { w.i32(x.raw()); w.i32(y.raw()); }
    w.u32(idx); w.u8(pending); w.u8(shoved); w.u8(group); w.i64(request_tick); w.i64(moved_tick); w.i32(stuck); w.i32(repaths); w.i32(retries); w.i32(wait); w.i32(cap.raw()); w.i32(best.raw());
  }
  static Path load(core::ByteReader& r) {
    Path p;
    p.gx = Fixed::from_raw(r.i32()); p.gy = Fixed::from_raw(r.i32());
    const uint32_t n = r.u32();
    for (uint32_t i = 0; i < n && i < 100000 && r.ok(); ++i) {
      const Fixed x = Fixed::from_raw(r.i32()), y = Fixed::from_raw(r.i32());
      p.wps.emplace_back(x, y);
    }
    p.idx = r.u32(); p.pending = r.u8(); p.shoved = r.u8(); p.group = r.u8(); p.request_tick = r.i64(); p.moved_tick = r.i64(); p.stuck = r.i32();
    p.repaths = r.i32(); p.retries = r.i32(); p.wait = r.i32(); p.cap = Fixed::from_raw(r.i32()); p.best = Fixed::from_raw(r.i32());
    return p;
  }
};

// --- M6: meccs (ADR-007) – csak `attach_rules` után regisztrálva (a régi visszajátszások hashe így változatlan) ---
struct Unit {   // sablonból létrehozott egység
  uint16_t def = kNoDef;
  void hash(core::StateHash& h) const { h.add_u64(def); }
  void save(core::ByteWriter& w) const { w.u32(def); }
  static Unit load(core::ByteReader& r) { Unit u; u.def = static_cast<uint16_t>(r.u32()); return u; }
};

struct Building {
  uint16_t def = kNoDef;
  int16_t tx = 0, ty = 0, w = 1, h = 1;   // lábnyom (csempe)
  uint8_t complete = 1;
  int32_t progress = 0;        // építés: eltelt (súlyozott) tick
  EntityId builder_in = 0;     // MECH-BUILD `enter`: a bent építő munkás
  uint8_t builders = 0;        // MECH-BUILD `stay`: az e tickben mellette dolgozók (nem állapot: tickenként újra)
  void hash(core::StateHash& hh) const {
    hh.add_u64(def); hh.add_i64(tx); hh.add_i64(ty); hh.add_i64(w); hh.add_i64(h); hh.add_u64(complete);
    hh.add_i64(progress); hh.add_u64(builder_in);
  }
  void save(core::ByteWriter& wr) const {
    wr.u32(def); wr.i32(tx); wr.i32(ty); wr.i32(w); wr.i32(h); wr.u8(complete); wr.i32(progress); wr.u32(builder_in);
  }
  static Building load(core::ByteReader& r) {
    Building b;
    b.def = static_cast<uint16_t>(r.u32());
    b.tx = static_cast<int16_t>(r.i32()); b.ty = static_cast<int16_t>(r.i32());
    b.w = static_cast<int16_t>(r.i32()); b.h = static_cast<int16_t>(r.i32());
    b.complete = r.u8();
    b.progress = r.i32();
    b.builder_in = r.u32();
    return b;
  }
};

// termelési sor (M6 4. lépés; a régi játék szerint legfeljebb 5 elem, mindig az első halad) és gyülekezőpont
struct QueueItem {
  uint8_t research = 0;      // 0: egység (`def` a sablon), 1: kutatás (`def` a kutatás sorszáma)
  uint16_t def = kNoDef;
  int32_t time = 0;          // tick
  int32_t progress = 0;      // tick
  std::array<int32_t, kResources> cost{};
  int32_t supply = 0;
};

struct Producer {
  std::vector<QueueItem> queue;
  uint8_t has_rally = 0;
  Fixed rx, ry;
  EntityId rally_target = 0;   // lelőhely: az új munkás gyűjteni megy
  void hash(core::StateHash& h) const {
    h.add_u64(queue.size());
    for (const QueueItem& q : queue) {
      h.add_u64(q.research); h.add_u64(q.def); h.add_i64(q.time); h.add_i64(q.progress); h.add_i64(q.supply);
      for (const int32_t c : q.cost) h.add_i64(c);
    }
    h.add_u64(has_rally); h.add_i64(rx.raw()); h.add_i64(ry.raw()); h.add_u64(rally_target);
  }
  void save(core::ByteWriter& w) const {
    w.u32(static_cast<uint32_t>(queue.size()));
    for (const QueueItem& q : queue) {
      w.u8(q.research); w.u32(q.def); w.i32(q.time); w.i32(q.progress); w.i32(q.supply);
      for (const int32_t c : q.cost) w.i32(c);
    }
    w.u8(has_rally); w.i32(rx.raw()); w.i32(ry.raw()); w.u32(rally_target);
  }
  static Producer load(core::ByteReader& r) {
    Producer p;
    const uint32_t n = r.u32();
    for (uint32_t i = 0; i < n && i < 64 && r.ok(); ++i) {
      QueueItem q;
      q.research = r.u8(); q.def = static_cast<uint16_t>(r.u32()); q.time = r.i32(); q.progress = r.i32(); q.supply = r.i32();
      for (int32_t& c : q.cost) c = r.i32();
      p.queue.push_back(q);
    }
    p.has_rally = r.u8(); p.rx = Fixed::from_raw(r.i32()); p.ry = Fixed::from_raw(r.i32()); p.rally_target = r.u32();
    return p;
  }
};

struct ResourceNode {   // fa (1 × 1) vagy lelőhely (pl. 2 × 2); a középpontja a Position
  uint8_t kind = 0;       // Res
  int16_t tx = 0, ty = 0, size = 1;
  int32_t amount = 0;
  void hash(core::StateHash& h) const { h.add_u64(kind); h.add_i64(tx); h.add_i64(ty); h.add_i64(size); h.add_i64(amount); }
  void save(core::ByteWriter& w) const { w.u8(kind); w.i32(tx); w.i32(ty); w.i32(size); w.i32(amount); }
  static ResourceNode load(core::ByteReader& r) {
    ResourceNode n;
    n.kind = r.u8();
    n.tx = static_cast<int16_t>(r.i32()); n.ty = static_cast<int16_t>(r.i32()); n.size = static_cast<int16_t>(r.i32());
    n.amount = r.i32();
    return n;
  }
};

struct Worker {   // a gyűjtés állapota (a régi játék `_o_gather`-je szerint)
  // Build: az építési helyre megy (a költség már levonva); Construct: az épülő épületen dolgozik (vagy bent van)
  enum State : uint8_t { Idle = 0, Go = 1, Work = 2, Return = 3, Build = 4, Construct = 5 };
  uint8_t state = Idle;
  EntityId res = 0;            // a célzott lelőhely (0 = nincs)
  EntityId move_to = 0;        // amelyik célhoz a legutóbbi mozgásparancs ment (0 = nincs)
  uint8_t last_kind = 0;       // a legutóbb gyűjtött nyersanyag (a kimerült lelőhely utáni kereséshez)
  Fixed lx, ly;                // a legutóbbi lelőhely helye
  uint8_t carry_kind = 0;
  int32_t carry = 0;
  int32_t timer = 0;           // tick
  uint8_t fails = 0;
  EntityId bad_drop = 0;       // elérhetetlen leadóhely (ideiglenesen kihagyva)
  int64_t retry_tick = 0;
  // építés
  uint16_t build_def = kNoDef;
  int16_t btx = 0, bty = 0;
  EntityId site = 0;           // Construct: az épülő épület
  EntityId inside = 0;         // MECH-BUILD `enter`: ebben az épületben van (nem látszik, nem akadály, nem célpont)
  EntityId resume = 0;         // az építés után ide megy vissza gyűjteni (0 = nem)
  void hash(core::StateHash& h) const {
    h.add_u64(state); h.add_u64(res); h.add_u64(move_to); h.add_u64(last_kind); h.add_i64(lx.raw()); h.add_i64(ly.raw());
    h.add_u64(carry_kind); h.add_i64(carry); h.add_i64(timer); h.add_u64(fails); h.add_u64(bad_drop); h.add_i64(retry_tick);
    h.add_u64(build_def); h.add_i64(btx); h.add_i64(bty); h.add_u64(site); h.add_u64(inside); h.add_u64(resume);
  }
  void save(core::ByteWriter& w) const {
    w.u8(state); w.u32(res); w.u32(move_to); w.u8(last_kind); w.i32(lx.raw()); w.i32(ly.raw());
    w.u8(carry_kind); w.i32(carry); w.i32(timer); w.u8(fails); w.u32(bad_drop); w.i64(retry_tick);
    w.u32(build_def); w.i32(btx); w.i32(bty); w.u32(site); w.u32(inside); w.u32(resume);
  }
  static Worker load(core::ByteReader& r) {
    Worker k;
    k.state = r.u8(); k.res = r.u32(); k.move_to = r.u32(); k.last_kind = r.u8();
    k.lx = Fixed::from_raw(r.i32()); k.ly = Fixed::from_raw(r.i32());
    k.carry_kind = r.u8(); k.carry = r.i32(); k.timer = r.i32(); k.fails = r.u8(); k.bad_drop = r.u32(); k.retry_tick = r.i64();
    k.build_def = static_cast<uint16_t>(r.u32()); k.btx = static_cast<int16_t>(r.i32()); k.bty = static_cast<int16_t>(r.i32());
    k.site = r.u32(); k.inside = r.u32(); k.resume = r.u32();
    return k;
  }
};

// játékosállapot (1–8. játékos; a 0. a semleges)
struct PlayerState {
  std::string faction;                       // pl. `ursan`
  std::array<int64_t, kResources> res{};     // ezredegységben (a Hadtáp tört bevétele miatt)
  std::array<int64_t, kResources> gathered{};
  int32_t supply_cap = 0, supply_used = 0;
  bool active = false;
  std::vector<std::string> researched;       // a kész kutatások (rendezve; az 5. lépésben töltődik)
  std::string path;                          // a választott fejlődési irány (MECH-ASC; 8. lépés)
  bool ascended = false;
  int32_t resource(int r) const { return static_cast<int32_t>(res[static_cast<size_t>(r)] / 1000); }
};

struct MatchSetup {
  std::vector<std::string> factions;   // játékosonként (1. játékos = [0]); üres szöveg = nem játszik
  bool upkeep = true;                  // Hadtáp (a régi játék alapja: be)
};

// A navigációs mozgás beállításai (ADR-006 5–6. pont).
struct NavParams {
  int path_budget = 32;                         // útkeresés tickenként (a többi a sorban vár)
  int64_t node_budget = 40000;                  // kibontott navcella tickenként (a keresések idejének kerete)
  int stuck_ticks = 15;                         // ennyi tick haladás nélkül → kitérés / újratervezés
  int wait_ticks = 120;                         // ennyi várakozás egy mozgó egység mögött számít beragadásnak
  int max_repaths = 3;                          // utána megáll (dugóban előbb szünet és újrakezdés)
  int max_retries = 3;                          // dugó miatti újrakezdések
  int retry_pause = 90;                         // tick: ennyivel tovább próbálkozik egy újrakezdés
  Fixed arrive_slack = Fixed::from_int(3);      // beragadva ennyin belül a célhoz: megérkezett (tömeg a célnál)
  Fixed hard_ratio = Fixed::from_ratio(45, 100);  // kemény sugár = sugár × arány (soha nem lapolódhat át; a
                                                  // puha sugár – a teljes sugár – tömegben megsérthető, ADR-006)
};

struct NavStats {
  int paths_this_tick = 0;
  int64_t expanded_this_tick = 0;
  int queued = 0;
  int64_t paths_total = 0;
  int64_t direct_total = 0;    // látóvonalas közvetlen utak (keresés nélkül)
  int64_t shared_total = 0;    // csoporttagok, akik a csoport közös útját kapták
  int64_t expanded_total = 0;  // a finom keresések kibontott navcellái
  double path_ms = 0, move_ms = 0;   // mérés (nem állapot): az útkeresés és a mozgás ideje összesen
};

// --- parancsok -----------------------------------------------------------------------------------------------------
struct Command {
  // M4: GroupMove (alakzatban), Block (építés/bontás: csempés téglalap akadály), Gate (kapu csapatmaszkkal)
  // M6 (a 4-es formátumtól): Gather (munkások egy lelőhelyre), Stop (a parancs és a munka abbahagyása),
  // Build (az első munkás a `def` épületet a (rx, ry) csempére építi), CancelBuild (a `target` félkész épület
  // megszakítása visszatérítéssel)
  // Demolish (a `target` kész épület lebontása, ha a népe lebonthat – D38); Train (`def` egység, `owner` játékos,
  // `target` épület vagy 0 = a legrövidebb sorú), Research (`def` kutatás, `owner`), CancelQueue (`target` épület,
  // `rx` a sor indexe), Rally (`units` épületek, `x`, `y`, `target` lelőhely vagy 0)
  enum class Kind : uint8_t { Spawn = 1, Move = 2, GroupMove = 3, Block = 4, Gate = 5, Gather = 6, Stop = 7, Build = 8,
                              CancelBuild = 9, Demolish = 10, Train = 11, Research = 12, CancelQueue = 13, Rally = 14 };
  Kind kind = Kind::Spawn;
  int64_t tick = 0;          // ebben a tickben hajtódik végre
  uint8_t owner = 0;         // Spawn
  Fixed x, y;                // Spawn: hely; Move, GroupMove: cél
  int32_t hp = 100;          // Spawn
  Fixed speed;               // Spawn
  int32_t damage = 10, spread = 4, cooldown = 30;   // Spawn
  Fixed range;               // Spawn
  std::vector<EntityId> units;   // Move, GroupMove
  // M4 (a 3-as parancsformátumtól)
  Fixed radius;              // Spawn: test sugara (0 = alapérték)
  uint8_t move_class = 0xFF; // Spawn: mozgásosztály (TerrainTable sorszám; 0xFF = szárazföldi)
  uint8_t formation = 1;     // GroupMove: Formation
  int32_t rx = 0, ry = 0, rw = 0, rh = 0;   // Block, Gate: csempés téglalap
  uint8_t flag = 0;          // Block: 1 = lerakás, 0 = bontás; Gate: csapatmaszk
  // M6 (a 4-es formátumtól)
  EntityId target = 0;       // Gather: a lelőhely
  std::string def;           // a sablon azonosítója (a későbbi Build, Train parancsokhoz)
  // a parancs csak a 3-as formátumban írható le (M4-mezők vagy -fajták)
  bool needs_v3() const;
  bool needs_v4() const;
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
  // Építés / bontás (csempés téglalap akadály) és kapu (csapatmaszk, 0 = eltávolítás): a navrács és a hierarchia
  // frissül, a mozgó egységek újraterveznek.
  void nav_block(int x, int y, int w, int h, bool add);
  void nav_gate(int x, int y, int w, int h, uint8_t mask);
  // Csoportos mozgás alakzatban (ADR-006 7. pont): minden tag a saját helyére megy (a csoport célpontja köré, a
  // haladási irány szerint forgatva); a WC3-szerű alakzatnál elöl a közelharcosok; együtt haladva a leglassabb tag
  // sebességével. Az azonos méretű és mozgásosztályú tagok egy közös utat kapnak (egy útkeresés a csoportnak).
  void order_group_move(std::vector<EntityId> units, Fixed x, Fixed y, Formation f = Formation::Auto);
  void set_formations(const FormationSet& f) { formations_ = f; }
  const NavStats& nav_stats() const { return nav_stats_; }

  // --- M6: meccs (ADR-007) ---
  // A szabályadatok csatolása (a navigáció után, a meccs előtt): regisztrálja a meccs komponenseit. A szabályok a
  // hívóé, és túl kell élniük a szimulációt. Mentés betöltése előtt ugyanígy csatolni kell.
  void attach_rules(const Rules* rules);
  const Rules* rules() const { return rules_; }
  // A meccs indítása a pályából: lelőhelyek és fák (akadályok a navrácsban), játékosonként a főépület a kezdőhelyen és
  // a kezdő egységek, a munkások a régi játék szerint azonnal gyűjteni indulnak. Hibánál false (az ok az `error`-ban).
  bool setup_match(const map::Map& m, const MatchSetup& setup, std::string* error);
  EntityId spawn_unit(uint16_t def, uint8_t owner, Fixed x, Fixed y);   // sablonból, azonnal
  EntityId place_building(uint16_t def, uint8_t owner, int tx, int ty, bool complete);
  bool order_gather(EntityId worker, EntityId resource);
  // Építés (ADR-007 3. lépés): hibánál a hiba oka (magyarul) az `error`-ba; a költség a parancskor levonódik.
  bool order_build(EntityId worker, uint16_t def, int tx, int ty, std::string* error = nullptr);
  // Lerakható-e: a pályán belül, építhető terepen, szirt nélkül, egy szinten, szabad helyen (épület, lelőhely
  // nélkül), az előfeltételekkel; `owner` 0: az előfeltételek nélkül.
  bool can_place(uint8_t owner, uint16_t def, int tx, int ty, std::string* error = nullptr);
  bool cancel_build(EntityId building);   // félkész épület: megszakítás, a költség `cancel_refund` része vissza
  bool demolish(EntityId building);       // kész épület lebontása (D38): csak a `demolish` népnél, 70% vissza
  // Termelés (ADR-007 4. lépés): hibánál az ok az `error`-ba. A költség és a létszám a parancskor foglalódik.
  bool train(uint8_t owner, uint16_t def, EntityId building = 0, std::string* error = nullptr);
  bool research(uint8_t owner, const std::string& id, std::string* error = nullptr);
  bool cancel_queue_item(EntityId building, int index);   // a teljes költség vissza
  void set_rally(EntityId building, Fixed x, Fixed y, EntityId target);
  // Nyersanyag jóváírása vagy levonása (küldetés, szkript, teszt; a Hadtáp nélkül); a készlet nem mehet 0 alá.
  void add_resources(uint8_t owner, int kind, int32_t amount);
  bool missing_requirements(uint8_t owner, const EntityDef& d, std::string* what = nullptr);
  // Mentés betöltése után: az épületek és a lelőhelyek akadályai a (meccsindítás nélküli) navrácsba.
  void restore_obstacles(const map::Map& m);
  void stop(EntityId e);                 // a mozgás és a munka abbahagyása
  const PlayerState& player(int i) const { return players_[static_cast<size_t>(i)]; }
  static constexpr int kMaxPlayers = 8;

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
  bool nav_nudge(EntityId blocker, Fixed vx, Fixed vy, Fixed px, Fixed py);
  void nav_arrive(EntityId e, Movement& m, Path& p, const Position& pos);
  void movement_system();
  void combat_system();
  void economy_system();
  void construction_system();
  void production_system();
  bool finish_item(EntityId building, const QueueItem& item);
  void supply_system();
  void enter_construction(EntityId worker, EntityId building);
  void leave_construction(EntityId building, bool hurt);
  void place_at_free(EntityId unit, Fixed x, Fixed y);   // a legközelebbi szabad helyre (épület alól, kilépéskor)
  void build_order_step(EntityId e, Worker& w);
  // Új parancs előtt: a tervezett (még le nem rakott) építés költsége vissza; false, ha a munkás bent épít (a bent
  // lévő nem kap parancsot – a régi játék szerint)
  bool interrupt(EntityId e);
  bool pay(uint8_t owner, const std::array<int32_t, kResources>& cost);
  void refund(uint8_t owner, const std::array<int32_t, kResources>& cost, int32_t permille);
  const map::Map* map_ = nullptr;
  void income(uint8_t owner, int kind, int32_t amount);
  EntityId nearest_resource(int kind, Fixed x, Fixed y, Fixed max_dist, EntityId exclude);
  EntityId nearest_dropoff(uint8_t owner, int kind, Fixed x, Fixed y, EntityId exclude);
  bool in_reach(EntityId unit, EntityId target);
  void remove_resource(EntityId e);
  int32_t carry_cap(const EntityDef& d, int kind) const;
  void register_components();

  World world_;
  core::Pcg32 combat_rng_;
  int64_t tick_ = 0;
  std::vector<Command> pending_;         // a jövőbeli parancsok (tick szerint, beérkezési sorrendben)
  std::vector<std::pair<EntityId, uint8_t>> died_;   // a tick halottai (a halál sorrendjében)
  std::vector<SimHooks*> hooks_;
  map::Pathfinder* nav_ = nullptr;
  FormationSet formations_;
  NavParams nav_params_;
  NavStats nav_stats_;
  int nav_bw_ = 0, nav_bh_ = 0;                  // a térbeli rács (2 × 2 csempés vödrök) mérete
  std::vector<std::vector<EntityId>> buckets_;  // vödrönként az egységek (nem állapot: tickenként újraépül)
  // M6
  const Rules* rules_ = nullptr;
  std::array<PlayerState, kMaxPlayers + 1> players_{};
  bool upkeep_ = true;
};

// format: 1 = M2-es parancs (Spawn, Move), 3 = M4-es (a további mezőkkel és fajtákkal), 4 = M6-os (cél, sablon)
void save_command(core::ByteWriter& w, const Command& c, int format = 3);
std::optional<Command> load_command(core::ByteReader& r, int format = 3);

}  // namespace totu::sim
