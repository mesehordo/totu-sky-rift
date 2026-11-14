// SPDX-License-Identifier: GPL-3.0-or-later
// Helyőrség, tornyok, felemelkedés és fejlődési irány, főépület-képességek, szabálymódosítók (M6, ADR-007 8. lépés).
// A régi játék szerint (G 656–734, 1661–1730, 2129–2172, 2961–2988; E 1178–1192, 1706–1757):
//   - beszállás (`beszallas` képességgel): odamegy, és ha van hely, bemegy; bent nem látszik, nem célpont, csak
//     gyógyul; kiszálláskor és az épület pusztulásakor az épület köré kerül;
//   - a helyőrséges épület fejenként erősebben (és gyorsabban) lő; ha nincs saját fegyvere, a bent lévők íjat adnak
//     (5 sebzés, 6 csempe, 1,3 s); fejenként + `garrison_dmg` (4, a Gyors helyőrséggel 7), újratöltés × (1 − 0,06 n);
//   - felemelkedés: 3 különböző fajta kész épület (a főépület, a fal, a csomópont és a leadóhely nem számít);
//     utána egyszer irány választható – az irány hatásai kutatásként, a szabályai a kutatásokéi mellé.
#include <algorithm>

#include "core/angle.hpp"
#include "map/map.hpp"
#include "map/pathfinder.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

const Fixed kReach = Fixed::from_ratio(22, 32);
const Fixed kRangeSlack = Fixed::from_ratio(3, 32);
constexpr int kRepath = 24;
constexpr int kScanTicks = 12;

Fixed rect_gap(Fixed px, Fixed py, const Building& b) {
  const Fixed x0 = Fixed::from_int(b.tx), y0 = Fixed::from_int(b.ty);
  const Fixed x1 = Fixed::from_int(b.tx + b.w), y1 = Fixed::from_int(b.ty + b.h);
  const Fixed dx = std::max({x0 - px, Fixed{}, px - x1});
  const Fixed dy = std::max({y0 - py, Fixed{}, py - y1});
  return core::length(dx, dy);
}

int64_t milli_of(Fixed f) { return (int64_t{f.raw()} * 1000) >> 16; }

}  // namespace

bool Sim::sheltered(EntityId e) {
  if (world_.get<Sheltered>(e)) return true;
  const Worker* w = world_.get<Worker>(e);
  return w && w->inside;
}

int Sim::garrison_count(EntityId b) {
  const Garrison* g = world_.get<Garrison>(b);
  return g ? static_cast<int>(g->units.size()) : 0;
}

void Sim::rebuild_rules(uint8_t owner) {
  if (!rules_ || owner > kMaxPlayers) return;
  RuleSet rs;
  const PlayerState& p = players_[owner];
  for (const std::string& id : p.researched) {   // a kész kutatások (azonosító szerint), majd az irány
    if (const ResearchDef* rd = rules_->research(id)) rs.merge(rd->rules);
  }
  if (!p.path.empty()) {
    if (const FactionDef* f = rules_->faction(p.faction)) {
      if (const PathDef* pd = f->path(p.path)) rs.merge(pd->rules);
    }
  }
  prules_[owner] = rs;
}

void Sim::grant_modifiers(uint8_t owner, const std::vector<Modifier>& mods) {
  PlayerState& p = players_[owner];
  // az életerő és a pajzs maximuma a már élő egységekre és épületekre is (a régi `apply_research`)
  world_.pool<Health>().each([&](EntityId e, Health& h) {
    if (h.owner != owner || h.hp <= 0) return;
    const Unit* u = world_.get<Unit>(e);
    const Building* eb = u ? nullptr : world_.get<Building>(e);
    if (!u && !eb) return;
    const EntityDef& d = rules_->defs[u ? u->def : eb->def];
    const Fixed old_hp = apply_modifiers({&p.research_mods}, "Health.max", Fixed::from_int(d.hp), d);
    const Fixed new_hp = apply_modifiers({&p.research_mods, &mods}, "Health.max", Fixed::from_int(d.hp), d);
    const int32_t dhp = new_hp.floor_int() - old_hp.floor_int();
    if (dhp) {
      h.max += dhp;
      h.hp = std::min(h.max, h.hp + dhp);
    }
    if (Vital* v = world_.get<Vital>(e)) {
      const Fixed old_sh = apply_modifiers({&p.research_mods}, "Health.shield", Fixed::from_int(d.shield), d);
      const Fixed new_sh = apply_modifiers({&p.research_mods, &mods}, "Health.shield", Fixed::from_int(d.shield), d);
      const int32_t dsh = new_sh.floor_int() - old_sh.floor_int();
      if (dsh) v->shield = std::max(0, v->shield + dsh * 1000);
    }
  });
  p.research_mods.insert(p.research_mods.end(), mods.begin(), mods.end());
}

// --- helyőrség -------------------------------------------------------------------------------------------------------
bool Sim::order_garrison(const std::vector<EntityId>& units, EntityId target) {
  if (!rules_) return false;
  const int beszallas = rules_->ability_index("beszallas");
  std::vector<EntityId> us;
  for (const EntityId e : units) {
    if (!world_.alive(e) || !world_.get<Unit>(e) || sheltered(e)) continue;
    if (beszallas < 0 || !has_ability(e, static_cast<uint16_t>(beszallas))) continue;
    us.push_back(e);
  }
  if (us.empty()) return false;
  const uint8_t owner = world_.get<Health>(us.front())->owner;
  std::vector<EntityId> cands;
  auto usable = [&](EntityId b) {
    const Building* bd = world_.get<Building>(b);
    const Health* bh = world_.get<Health>(b);
    return bd && bh && bh->owner == owner && bh->hp > 0 && bd->complete && world_.get<Garrison>(b);
  };
  if (target) {
    if (usable(target)) cands.push_back(target);
  } else {
    world_.pool<Garrison>().each([&](EntityId b, const Garrison&) {
      if (usable(b)) cands.push_back(b);
    });
  }
  bool issued = false;
  for (const EntityId e : us) {
    if (world_.get<Health>(e)->owner != owner) continue;
    // a szabad hely: a bent lévők és a már odaküldöttek nélkül
    EntityId best = kNoEntity;
    int64_t best_d = 0;
    const Position& p = *world_.get<Position>(e);
    for (const EntityId b : cands) {
      int pending = 0;
      world_.pool<Enter>().each([&](EntityId, const Enter& en) { pending += en.building == b ? 1 : 0; });
      const int slots = rules_->defs[world_.get<Building>(b)->def].garrison;
      if (garrison_count(b) + pending >= slots) continue;
      const Position& bp = *world_.get<Position>(b);
      const int64_t d = core::length_sq_raw(bp.x - p.x, bp.y - p.y);
      if (best == kNoEntity || d < best_d) {
        best = b;
        best_d = d;
      }
    }
    if (best == kNoEntity) break;   // nincs szabad hely
    interrupt(e);
    world_.add(e, Enter{best, 0});
    issued = true;
  }
  return issued;
}

void Sim::enter_garrison(EntityId e, EntityId b) {
  Garrison* g = world_.get<Garrison>(b);
  Position* p = world_.get<Position>(e);
  const Position* bp = world_.get<Position>(b);
  if (!g || !p || !bp) return;
  world_.pool<Enter>().remove(e);
  stop(e);
  if (Combat* c = world_.get<Combat>(e)) {
    c->order = Combat::None;
    c->target = 0;
  }
  world_.pool<Body>().remove(e);   // bent nem akadály: a test és az útvonal kikerül a navigációból
  world_.pool<Path>().remove(e);
  p->x = bp->x;
  p->y = bp->y;
  world_.add(e, Sheltered{b});
  g->units.push_back(e);
}

bool Sim::unload(EntityId b) {
  Garrison* g = world_.get<Garrison>(b);
  const Building* bd = world_.get<Building>(b);
  if (!g || !bd || g->units.empty()) return false;
  const std::vector<EntityId> out = g->units;
  g->units.clear();
  release_units(*bd, out, false);
  return true;
}

void Sim::garrison_system() {
  std::vector<EntityId> ids;
  world_.pool<Enter>().each([&](EntityId e, const Enter&) { ids.push_back(e); });
  for (const EntityId e : ids) {
    Enter* en = world_.get<Enter>(e);
    const Health* h = world_.get<Health>(e);
    if (!en || !h || h->hp <= 0) continue;
    const EntityId b = en->building;
    const Building* bd = world_.get<Building>(b);
    const Health* bh = world_.get<Health>(b);
    if (!bd || !bh || bh->hp <= 0 || !bd->complete || bh->owner != h->owner ||
        garrison_count(b) >= rules_->defs[bd->def].garrison) {
      world_.pool<Enter>().remove(e);
      halt(e);
      continue;
    }
    const Position& p = *world_.get<Position>(e);
    const Fixed r = rules_->defs[world_.get<Unit>(e)->def].radius;
    if (rect_gap(p.x, p.y, *bd) <= r + kReach) {
      enter_garrison(e, b);
      continue;
    }
    if (incapacitated(e)) continue;
    const Movement* mv = world_.get<Movement>(e);
    if (--en->repath <= 0 || !(mv && mv->moving)) {
      const Position& bp = *world_.get<Position>(b);
      order_move(e, bp.x, bp.y);
      en->repath = kRepath;
    }
  }
}

// --- tornyok és helyőrséges épületek ---------------------------------------------------------------------------------
void Sim::tower_system() {
  std::vector<EntityId> ids;
  world_.pool<Building>().each([&](EntityId e, const Building& b) {
    if (b.complete && world_.get<Combat>(e)) ids.push_back(e);
  });
  for (const EntityId e : ids) {
    const Building& b = *world_.get<Building>(e);
    const Health* h = world_.get<Health>(e);
    Combat& c = *world_.get<Combat>(e);
    if (!h || h->hp <= 0) continue;
    if (c.cooldown > 0) --c.cooldown;
    const EntityDef& d = rules_->defs[b.def];
    const int n = garrison_count(e);
    // a fegyver: a sajátja, vagy a bent lévők íja (a régi alapja)
    AttackDef a = d.attack;
    if (!a.has) {
      if (n == 0) continue;
      a.has = true;
      a.damage = Fixed::from_int(5);
      a.range = Fixed::from_int(6);
      a.cooldown = 39;
      a.ground = a.air = true;
      const int arrow = rules_->projectile_index("arrow");
      a.projectile = static_cast<uint8_t>(arrow >= 0 ? arrow + 1 : 0);
    }
    if (!a.ranged()) continue;
    const int32_t gd = rules_of(h->owner).garrison_dmg >= 0 ? rules_of(h->owner).garrison_dmg : rules_->garrison_dmg;
    const Fixed dmg = research_stat(h->owner, "Attack.damage", a.damage, d) + Fixed::from_int(gd * n);
    const int32_t cooldown = std::max(15, a.cooldown * (1000 - 60 * n) / 1000);
    Fixed range = research_stat(h->owner, "Attack.range", a.range, d);
    auto in_reach = [&](EntityId t) -> bool {
      const Health* th = world_.get<Health>(t);
      if (!th || th->hp <= 0 || !hostile(h->owner, th->owner) || sheltered(t) || !is_visible(h->owner, t)) return false;
      const Position& tp = *world_.get<Position>(t);
      Fixed g;
      if (const Unit* tu = world_.get<Unit>(t)) {
        if (rules_->defs[tu->def].flying ? !a.air : !a.ground) return false;
        g = rect_gap(tp.x, tp.y, b) - rules_->defs[tu->def].radius;
      } else if (const Building* tb = world_.get<Building>(t)) {
        if (!a.ground) return false;
        g = std::max(Fixed{}, rect_gap(tp.x, tp.y, *tb) - Fixed::from_ratio(std::max(tb->w, tb->h), 2));
      } else {
        return false;
      }
      const int delta = level_at(e) - level_at(t);   // magaslatról messzebb (D33)
      const Fixed r = delta > 0 ? range + rules_->elevation.building_range_per_level * Fixed::from_int(delta) : range;
      return g <= r + kRangeSlack;
    };
    if (c.target && !in_reach(c.target)) c.target = 0;
    if (!c.target && --c.scan <= 0) {   // a régi: 0,4 s-onként a legközelebbi egység, ha nincs, épület
      c.scan = kScanTicks;
      EntityId best = kNoEntity;
      int64_t best_d = 0;
      for (const bool units : {true, false}) {
        auto consider = [&](EntityId o) {
          if (!in_reach(o)) return;
          const Position& op = *world_.get<Position>(o);
          const int64_t dd = rect_gap(op.x, op.y, b).raw();
          if (best == kNoEntity || dd < best_d || (dd == best_d && o < best)) {
            best = o;
            best_d = dd;
          }
        };
        if (units) world_.pool<Unit>().each([&](EntityId o, const Unit&) { consider(o); });
        else world_.pool<Building>().each([&](EntityId o, const Building&) { consider(o); });
        if (best != kNoEntity) break;
      }
      c.target = best;
    }
    if (!c.target || c.cooldown > 0) continue;
    c.cooldown = cooldown;
    int64_t milli = milli_of(dmg);
    const int delta = level_at(e) - level_at(c.target);
    milli = milli * std::max<int64_t>(0, 1000 + int64_t{delta} * rules_->elevation.damage_per_level) / 1000;
    launch_shot(e, h->owner, b.def, c.target, milli, a.projectile, a.splash, range);
  }
}

// --- felemelkedés és fejlődési irány ---------------------------------------------------------------------------------
void Sim::ascension_system() {
  for (int i = 1; i <= kMaxPlayers; ++i) {
    PlayerState& p = players_[static_cast<size_t>(i)];
    if (!p.active || p.ascended) continue;
    const FactionDef* f = rules_->faction(p.faction);
    if (!f || f->paths.empty()) continue;
    std::vector<uint16_t> kinds;
    world_.pool<Building>().each([&](EntityId e, const Building& b) {
      const Health* h = world_.get<Health>(e);
      if (!b.complete || !h || h->owner != i || h->hp <= 0 || b.def == f->hq) return;
      const std::string& sh = rules_->defs[b.def].shape;
      if (sh == "wall" || sh == "node" || sh == "dropoff") return;
      if (std::find(kinds.begin(), kinds.end(), b.def) == kinds.end()) kinds.push_back(b.def);
    });
    if (static_cast<int>(kinds.size()) >= f->ascension_buildings) p.ascended = true;
  }
}

bool Sim::choose_path(uint8_t owner, const std::string& id) {
  if (!rules_ || owner < 1 || owner > kMaxPlayers) return false;
  PlayerState& p = players_[owner];
  const FactionDef* f = rules_->faction(p.faction);
  const PathDef* pd = f ? f->path(id) : nullptr;
  if (!p.active || !p.ascended || !p.path.empty() || !pd) return false;
  p.path = id;
  grant_modifiers(owner, pd->effects);
  rebuild_rules(owner);
  return true;
}

bool Sim::choose_ally(uint8_t owner, const std::string& people, std::string* error) {
  auto fail = [&](const char* msg) {
    if (error) *error = msg;
    return false;
  };
  if (!rules_ || owner < 1 || owner > kMaxPlayers) return fail("érvénytelen játékos");
  PlayerState& p = players_[owner];
  const FactionDef* ally = rules_->faction(people);
  const FactionDef* mine = rules_->faction(p.faction);
  if (!ally || !ally->ally || !mine) return fail("nem szövetséges kisnép");
  if (!p.ally.empty()) return fail("már van szövetséged");
  bool hall = false;
  world_.pool<Building>().each([&](EntityId e, const Building& b) {
    const Health* h = world_.get<Health>(e);
    hall = hall || (b.complete && h && h->owner == owner && h->hp > 0 && rules_->defs[b.def].diplomacy);
  });
  if (!hall) return fail("ehhez kész Diplomácia csarnoka kell");
  if (!pay(owner, mine->ally_cost)) return fail("nincs elég nyersanyag");
  p.ally = people;
  return true;
}

std::string Sim::ai_choose_ally(uint8_t owner, const std::string& forced) {
  if (!rules_ || owner < 1 || owner > kMaxPlayers || !players_[owner].ally.empty()) return {};
  // a véletlen csak akkor fogy, ha a szövetség meg is köthető (kész csarnok, elég nyersanyag)
  const FactionDef* own = rules_->faction(players_[owner].faction);
  if (!own) return {};
  for (int r = 0; r < kResources; ++r) {
    if (players_[owner].resource(r) < own->ally_cost[static_cast<size_t>(r)]) return {};
  }
  bool hall = false;
  world_.pool<Building>().each([&](EntityId e, const Building& b) {
    const Health* h = world_.get<Health>(e);
    hall = hall || (b.complete && h && h->owner == owner && h->hp > 0 && rules_->defs[b.def].diplomacy);
  });
  if (!hall) return {};
  std::vector<std::string> allies;
  for (const FactionDef& f : rules_->factions) {
    if (f.ally) allies.push_back(f.id);
  }
  if (allies.empty()) return {};
  std::string pick;
  if (std::find(allies.begin(), allies.end(), forced) != allies.end()) {
    pick = forced;
  } else {
    std::vector<std::string> pref;
    const FactionDef* mine = rules_->faction(players_[owner].faction);
    const content::Value* fa = mine && mine->raw ? mine->raw->find("Faction") : nullptr;
    const content::Value* ai = fa ? fa->find("ai") : nullptr;
    if (const content::Value* ap = ai ? ai->find("ally_pref") : nullptr; ap && ap->is_array()) {
      for (const auto& a : ap->items()) {
        if (a.is_string() && std::find(allies.begin(), allies.end(), a.text()) != allies.end()) pref.push_back(a.text());
      }
    }
    if (pref.empty()) pref = allies;
    const bool top2 = pref.size() > 1 && combat_rng_.next() % 100u < 80u;
    const size_t n = top2 ? 2 : pref.size();
    pick = pref[combat_rng_.next() % n];
  }
  return choose_ally(owner, pick) ? pick : std::string{};
}

// --- főépület-képesség -----------------------------------------------------------------------------------------------
bool Sim::use_power(uint8_t owner, EntityId b, const std::string& id, Fixed x, Fixed y) {
  if (!rules_) return false;
  const int ai = rules_->ability_index(id);
  const Building* bd = world_.get<Building>(b);
  const Health* bh = world_.get<Health>(b);
  Caster* c = world_.get<Caster>(b);
  if (ai < 0 || !bd || !bh || !c || bh->owner != owner || bh->hp <= 0 || !bd->complete) return false;
  const auto ab = static_cast<uint16_t>(ai);
  const auto& pw = rules_->defs[bd->def].powers;
  if (std::find(pw.begin(), pw.end(), ab) == pw.end()) return false;
  if (world_.get<Captured>(b)) return false;   // elfoglalt épület képessége nem (D30)
  const AbilityDef& a = rules_->abilities[ab];
  for (const auto& [rid, rt] : c->ready) {
    if (rid == ab && rt > tick_) return false;
  }
  if (a.targeted && map_) {   // csak látható pontra (a régi G 1677)
    const int tx = x.floor_int(), ty = y.floor_int();
    const auto& vis = players_[owner].visible;
    if (tx < 0 || ty < 0 || tx >= map_->width() || ty >= map_->height() ||
        (!vis.empty() && !vis[static_cast<size_t>(ty * map_->width() + tx)])) {
      return false;
    }
  }
  for (int r = 0; r < kResources; ++r) {
    if (players_[owner].resource(r) < a.cost[static_cast<size_t>(r)]) return false;
  }
  Health* hh = world_.get<Health>(b);
  if (a.hq_hp > 0 && hh->hp <= a.hq_hp + 50) return false;   // a főépület túl sebesült
  pay(owner, a.cost);
  if (a.hq_hp > 0) hh->hp -= a.hq_hp;
  if (a.effect == "area") {   // területi mező a pontra (a főépület a forrás)
    const EntityId f = world_.create();
    world_.add(f, Position{x, y});
    world_.add(f, Field{owner, b, ab, tick_ + a.duration, tick_ + 1});
    if (a.reveal > 0) players_[owner].reveals.push_back({x.floor_int(), y.floor_int(), a.reveal, static_cast<int32_t>(tick_ + 10 * kTickHz)});   // 10 s-ra felfedi (a régi G 1697)
  } else if (a.effect == "summon" && a.unit != kNoDef) {   // idézés a pont köré
    for (int k = 0; k < a.count; ++k) {
      const core::Angle ang = core::Angle::from_units(k * 21900);   // 2,1 radián lépésenként
      const Fixed sx = x + core::cos(ang) * Fixed::from_ratio(20, 32), sy = y + core::sin(ang) * Fixed::from_ratio(20, 32);
      const EntityId u = spawn_unit(a.unit, owner, sx, sy);
      if (u == kNoEntity) continue;
      place_at_free(u, sx, sy);
      world_.add(u, Lifetime{tick_ + summon_life(owner, a.lifetime)});
    }
  } else if (a.effect == "wind") {   // szél: a saját egységek önerősítést kapnak, az ellenség lassul
    const int bi = rules_->ability_index(a.buff);
    std::vector<EntityId> own;
    world_.pool<Unit>().each([&](EntityId e, const Unit&) {
      const Health* h = world_.get<Health>(e);
      const Position& p = *world_.get<Position>(e);
      if (!h || h->hp <= 0 || core::length(p.x - x, p.y - y) > a.radius) return;
      if (h->owner == owner) own.push_back(e);
      else if (hostile(owner, h->owner)) {
        if (Afflict* af = world_.get<Afflict>(e)) {
          af->slow_until = tick_ + a.duration;
          af->slow_mult = a.has_slow ? a.slow : Fixed::from_ratio(1, 2);
        }
      }
    });
    if (bi >= 0) {
      for (const EntityId e : own) give_buff(e, static_cast<uint16_t>(bi), a.duration);
    }
  } else if (a.effect == "levy") {   // mozgósítás: minden (nem bent lévő) munkás megkapja az önerősítést
    const int bi = rules_->ability_index(a.buff);
    if (bi < 0) return false;
    std::vector<EntityId> ws;
    world_.pool<Worker>().each([&](EntityId e, const Worker&) {
      const Health* h = world_.get<Health>(e);
      if (h && h->owner == owner && h->hp > 0 && !sheltered(e) && world_.get<Caster>(e)) ws.push_back(e);
    });
    for (const EntityId e : ws) give_buff(e, static_cast<uint16_t>(bi), a.duration);
  } else if (!mod_use_power(owner, b, ab, x, y)) {   // a népi hatások (áradás, bányagyorsítás, sietés) a népmodulban
    refund(owner, a.cost, 1000);
    if (a.hq_hp > 0) hh->hp += a.hq_hp;
    return false;
  }
  bool found = false;
  for (auto& [rid, rt] : c->ready) {
    if (rid == ab) {
      rt = tick_ + a.cooldown;
      found = true;
    }
  }
  if (!found) {
    c->ready.emplace_back(ab, tick_ + a.cooldown);
    std::sort(c->ready.begin(), c->ready.end());
  }
  return true;
}

// --- elfoglalás, megszállás (M7 10. lépés; D30) ----------------------------------------------------------------------
int Sim::occupy_capacity(const Building& b) const {
  const int side = std::max(b.w, b.h);
  for (const auto& [s, n] : rules_->capture.slots) {
    if (side <= s) return n;
  }
  return 0;
}

bool Sim::occupied_against(EntityId b, uint8_t owner) {
  const Occupants* o = world_.get<Occupants>(b);
  return o && !o->units.empty() && o->owner != owner;
}

bool Sim::seize_valid(EntityId e, const Seize& s) {
  const Health* h = world_.get<Health>(e);
  const Building* bd = world_.get<Building>(s.building);
  const Health* bh = world_.get<Health>(s.building);
  if (!rules_ || !h || h->hp <= 0 || !bd || !bh || bh->hp <= 0 || !bd->complete || s.ability >= rules_->abilities.size()) return false;
  const uint8_t uo = h->owner, bo = bh->owner;
  const Occupants* oc = world_.get<Occupants>(s.building);
  const bool hostile_occ = oc && !oc->units.empty() && oc->owner != uo;
  if (bo == uo) return hostile_occ;          // visszafoglalás: a saját épületből kiűzi a megszállókat
  if (!hostile(uo, bo)) return false;        // csak ellenséges vagy semleges épület
  if (s.occupy) return !hostile_occ && static_cast<int>(oc ? oc->units.size() : 0) < occupy_capacity(*bd);
  for (const FactionDef& f : rules_->factions) {
    if (f.hq == bd->def) return false;       // a főépület nem foglalható el
  }
  return int64_t{bh->hp} * 65536 < int64_t{bh->max} * rules_->abilities[s.ability].below.raw();
}

void Sim::release_units(const Building& bd, const std::vector<EntityId>& out, bool half) {
  const Fixed cx = Fixed::from_int(bd.tx) + Fixed::from_ratio(bd.w, 2);
  const Fixed cy = Fixed::from_int(bd.ty) + Fixed::from_ratio(bd.h, 2);
  for (size_t k = 0; k < out.size(); ++k) {
    const EntityId e = out[k];
    if (!world_.alive(e)) continue;
    world_.pool<Sheltered>().remove(e);
    if (half) {
      if (Health* h = world_.get<Health>(e)) h->hp = std::max(1, h->hp / 2);
    }
    if (nav_) {
      world_.add(e, Body{});
      world_.add(e, Path{});
      const auto air = nav_->nav().table().movement_class("air");
      const EntityDef& d = rules_->defs[world_.get<Unit>(e)->def];
      set_body(e, d.radius, d.flying && air ? *air : 0xFF);
    }
    // a régi: k × 0,9 radiánnal körbe, a lábnyom alsó részénél (a 60%-ánál), a legközelebbi szabad helyre
    const core::Angle a = core::Angle::from_units(static_cast<int32_t>(k) * 9387);
    place_at_free(e, cx + core::cos(a) * Fixed::from_ratio(bd.w * 7, 10),
                  cy + Fixed::from_ratio(bd.h, 10) + core::sin(a) * Fixed::from_ratio(10, 32));
  }
}

bool Sim::evict(EntityId b, bool half) {
  const Occupants* o = world_.get<Occupants>(b);
  const Building* bd = world_.get<Building>(b);
  if (!o || !bd) return false;
  const std::vector<EntityId> out = o->units;
  const Building copy = *bd;
  world_.pool<Occupants>().remove(b);
  release_units(copy, out, half);
  return !out.empty();
}

void Sim::capture_building(EntityId e, EntityId b) {
  const uint8_t uo = world_.get<Health>(e)->owner;
  if (Producer* pr = world_.get<Producer>(b)) {   // a folyamatban lévő képzés és kutatás ára az előző gazdáé
    while (!pr->queue.empty()) {
      if (!cancel_queue_item(b, static_cast<int>(pr->queue.size()) - 1)) break;
      pr = world_.get<Producer>(b);
    }
  }
  if (world_.get<Garrison>(b)) unload(b);
  evict(b, false);
  world_.get<Health>(b)->owner = uo;
  if (!world_.get<Captured>(b)) world_.add(b, Captured{});
  if (Combat* c = world_.get<Combat>(b)) c->target = 0;
  world_.destroy(e);   // az elfoglaló beköltözik
}

void Sim::occupy_enter(EntityId e, EntityId b) {
  if (!world_.get<Occupants>(b)) world_.add(b, Occupants{});
  Position* p = world_.get<Position>(e);
  const Position* bp = world_.get<Position>(b);
  if (!p || !bp) return;
  stop(e);
  if (Combat* c = world_.get<Combat>(e)) {
    c->order = Combat::None;
    c->target = 0;
  }
  world_.pool<Body>().remove(e);
  world_.pool<Path>().remove(e);
  p->x = bp->x;
  p->y = bp->y;
  world_.add(e, Sheltered{b});
  Occupants* o = world_.get<Occupants>(b);
  o->owner = world_.get<Health>(e)->owner;
  o->units.push_back(e);
  o->cd.push_back(0);
}

void Sim::seize_system() {
  std::vector<EntityId> ids;
  world_.pool<Seize>().each([&](EntityId e, const Seize&) { ids.push_back(e); });
  for (const EntityId e : ids) {
    Seize* s = world_.get<Seize>(e);
    if (!s) continue;
    if (!seize_valid(e, *s)) {
      world_.pool<Seize>().remove(e);
      halt(e);
      continue;
    }
    const EntityId b = s->building;
    const Building& bd = *world_.get<Building>(b);
    const int32_t hp = world_.get<Health>(e)->hp;
    const uint8_t uo = world_.get<Health>(e)->owner;
    const Position& p = *world_.get<Position>(e);
    if (rect_gap(p.x, p.y, bd) > rules_->defs[world_.get<Unit>(e)->def].radius + kReach) {
      s->start = -1;
      if (incapacitated(e)) continue;
      const Movement* mv = world_.get<Movement>(e);
      if (--s->repath <= 0 || !(mv && mv->moving)) {
        s->repath = kRepath;
        const Position bp = *world_.get<Position>(b);
        order_move(e, bp.x, bp.y);
      }
      continue;
    }
    if (s->start < 0) {   // a varázslás kezdete
      s->start = tick_;
      s->start_hp = hp;
      halt(e);
      if (Combat* c = world_.get<Combat>(e)) {
        c->order = Combat::None;
        c->target = 0;
      }
      continue;
    }
    if (hp < s->start_hp || incapacitated(e)) {   // sebzés (vagy kábulás) megszakítja
      world_.pool<Seize>().remove(e);
      continue;
    }
    if (tick_ - s->start < rules_->abilities[s->ability].cast) continue;
    const bool occ = s->occupy != 0;
    world_.pool<Seize>().remove(e);
    if (world_.get<Health>(b)->owner == uo) evict(b, false);   // visszafoglalás
    else if (occ) occupy_enter(e, b);
    else capture_building(e, b);
  }
}

void Sim::occupation_system() {
  std::vector<EntityId> ids;
  world_.pool<Occupants>().each([&](EntityId b, const Occupants&) { ids.push_back(b); });
  for (const EntityId b : ids) {
    Occupants* o = world_.get<Occupants>(b);
    const Health* bh = world_.get<Health>(b);
    const Building* bd = world_.get<Building>(b);
    if (!o || !bh || !bd || bh->hp <= 0) continue;   // a ledőlőből a halálkor jönnek ki (fél életerővel)
    for (size_t i = o->units.size(); i-- > 0;) {
      if (!world_.alive(o->units[i])) {
        o->units.erase(o->units.begin() + static_cast<std::ptrdiff_t>(i));
        o->cd.erase(o->cd.begin() + static_cast<std::ptrdiff_t>(i));
      }
    }
    if (o->units.empty()) {
      world_.pool<Occupants>().remove(b);
      continue;
    }
    if (bh->owner != o->owner && int64_t{bh->hp} * 65536 < int64_t{bh->max} * rules_->capture.evict_below.raw()) {
      evict(b, false);   // 50% alatti életerőnél kiűzik őket
      continue;
    }
    const uint8_t owner = o->owner;
    const Building box = *bd;
    for (size_t i = 0;; ++i) {   // a távolharcos megszállók kilőnek
      o = world_.get<Occupants>(b);
      if (!o || i >= o->units.size()) break;
      if (o->cd[i] > 0) {
        --o->cd[i];
        continue;
      }
      const EntityId u = o->units[i];
      const Unit* uu = world_.get<Unit>(u);
      if (!uu) continue;
      const uint16_t udef = uu->def;
      const AttackDef& a = rules_->defs[udef].attack;
      if (!a.has || !a.ranged()) continue;
      const Fixed range = stat(u, "Attack.range", a.range);
      EntityId best = kNoEntity;
      int64_t best_d = 0;
      world_.pool<Unit>().each([&](EntityId t, const Unit& tu) {
        const Health* th = world_.get<Health>(t);
        if (!th || th->hp <= 0 || !hostile(owner, th->owner) || sheltered(t)) return;   // a megszálló az épületből lát
        if (rules_->defs[tu.def].flying ? !a.air : !a.ground) return;
        const Position& tp = *world_.get<Position>(t);
        const Fixed g = rect_gap(tp.x, tp.y, box) - rules_->defs[tu.def].radius;
        if (g > range + kRangeSlack) return;
        if (best == kNoEntity || g.raw() < best_d || (g.raw() == best_d && t < best)) {
          best = t;
          best_d = g.raw();
        }
      });
      if (best == kNoEntity) continue;
      o->cd[i] = a.cooldown;
      const int64_t dmg = milli_of(stat(u, "Attack.damage", a.damage));
      launch_shot(u, owner, udef, best, dmg, a.projectile, a.splash, range);
    }
  }
}

// --- előléptetés (D30) -----------------------------------------------------------------------------------------------
std::vector<Modifier> Sim::rank_mods(uint8_t rank) const {
  std::vector<Modifier> out;
  if (!rules_ || rank == 0 || rank > rules_->ranks.size()) return out;
  const Rules::Rank& r = rules_->ranks[rank - 1];
  auto mk = [&](Modifier::Op op, const char* path, Fixed v) {
    Modifier m;
    m.op = op;
    m.path = path;
    m.value = v;
    out.push_back(m);
  };
  mk(Modifier::Mul, "Health.max", Fixed::from_int(1) + r.hp);
  mk(Modifier::Mul, "Attack.damage", Fixed::from_int(1) + r.damage);
  if (r.armor) mk(Modifier::Add, "Health.armor", Fixed::from_int(r.armor));
  return out;
}

void Sim::promote(EntityId e, int64_t value) {
  if (!rules_ || rules_->ranks.empty() || !e || !world_.alive(e) || value <= 0) return;
  const Unit* u = world_.get<Unit>(e);
  if (!u || world_.get<HeroState>(e) || world_.get<Worker>(e) || world_.get<Lifetime>(e) || !world_.get<Combat>(e)) return;
  const EntityDef& d = rules_->defs[u->def];
  int64_t cost = 0;
  for (const int32_t c : d.cost) cost += c;
  if (!world_.get<Veteran>(e)) world_.add(e, Veteran{});
  Veteran* v = world_.get<Veteran>(e);
  v->xp = static_cast<int32_t>(std::min<int64_t>(int64_t{v->xp} + value, 2000000000));
  bool up = false;
  while (v->rank < rules_->ranks.size()) {
    const int64_t thr = (cost * 1000 * rules_->ranks[v->rank].cost_mult.raw()) >> 16;   // ezred tapasztalat
    if (v->xp < thr) break;
    ++v->rank;
    up = true;
  }
  if (up) hero_recalc(e, false);
}

}  // namespace totu::sim
