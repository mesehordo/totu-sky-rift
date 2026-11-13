// SPDX-License-Identifier: GPL-3.0-or-later
// Hősök, tárgyak, semleges táborok (M7, ADR-008 2. lépés; D25). A régi játék szerint (H 14–160, 240–300; G 3475–3544;
// spec/HOSOK_TARGYAK_TABOROK.md HOS-1–4, TARGY-1–2, TABOR-1–2):
//   - tapasztalat ölésért (a célpont `Hero.xp`-je; hősnél 100 + 60 × szint; különben 8 + 0,18 × költség / darab,
//     épületnél a fele; idézett áldozat nem ad), a gyilkos játékos 12 csempén belüli hősei egyenlően osztoznak;
//   - szintlépés: +1 képességpont, növekedés (`Hero.growth`), +25% életerő; a képesség n. szintje a 2n − 1. hősszinttől,
//     a végső a 6.-tól; az elesett hős állapota megmarad, újraképzése idő × (0,6 + 0,12 × szint);
//   - leltár 6 hely: fogyó (gyógyítás, energia, tapasztalat, örök bónusz) és viselt (D24-módosítók; cserénél az
//     életerő aránya marad);
//   - táborok: a Vadon (semleges, 0. játékos) egységei a táboruknál (1,5 csempén belül) 5 csempés körben keresnek
//     célpontot, egy támadásra az egész tábor felriad; 7 csempén túl visszatérnek, közben nem támadnak és 10%/s-mal
//     gyógyulnak; a kiirtott tábor egyszer zsákmányt dob (3. szinttől 60%-kal egy főzetet is).
#include <algorithm>

#include "core/angle.hpp"
#include "map/map.hpp"
#include "map/pathfinder.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

const Fixed kReach = Fixed::from_ratio(22, 32);
constexpr int kScan = 12;

Modifier add_mod(const char* path, Fixed v) {
  Modifier m;
  m.op = Modifier::Add;
  m.path = path;
  m.value = v;
  return m;
}

}  // namespace

// --- statisztika ------------------------------------------------------------------------------------------------------
std::vector<Modifier> Sim::hero_mods(EntityId e) {
  std::vector<Modifier> out;
  const HeroState* hs = world_.get<HeroState>(e);
  const Unit* u = world_.get<Unit>(e);
  if (!hs || !u) return out;
  const EntityDef& d = rules_->defs[u->def];
  const Fixed lv = Fixed::from_int(hs->level - 1);
  out.push_back(add_mod("Health.max", d.grow_hp * lv + Fixed::from_int(hs->perm_hp)));
  out.push_back(add_mod("Attack.damage", d.grow_damage * lv + Fixed::from_int(hs->perm_damage)));
  out.push_back(add_mod("Health.armor", d.grow_armor * lv));
  out.push_back(add_mod("Energy.max", d.grow_energy * lv));
  out.push_back(add_mod("Health.shield", d.grow_shield * lv));
  for (const uint16_t it : hs->items) {
    if (it == kNoItem || it >= rules_->items.size() || !rules_->items[it].equip) continue;
    out.insert(out.end(), rules_->items[it].modifiers.begin(), rules_->items[it].modifiers.end());
  }
  return out;
}

Fixed Sim::stat(EntityId e, const char* path, Fixed base) {
  const Health* h = world_.get<Health>(e);
  const Unit* u = world_.get<Unit>(e);
  const Building* b = u ? nullptr : world_.get<Building>(e);
  if (!h || (!u && !b) || !rules_) return base;
  const EntityDef& d = rules_->defs[u ? u->def : b->def];
  if (!world_.get<HeroState>(e)) return research_stat(h->owner, path, base, d);
  const std::vector<Modifier> hm = hero_mods(e);
  return apply_modifiers({&players_[h->owner].research_mods, &hm}, path, base, d);
}

void Sim::hero_recalc(EntityId e, bool keep_ratio) {
  Health* h = world_.get<Health>(e);
  const Unit* u = world_.get<Unit>(e);
  if (!h || !u) return;
  const EntityDef& d = rules_->defs[u->def];
  const int32_t nmax = std::max(1, stat(e, "Health.max", Fixed::from_int(d.hp)).floor_int());
  if (nmax == h->max) return;
  if (keep_ratio) {   // tárgycsere: az arány marad (különben a cserével gyógyítani lehetne)
    h->hp = static_cast<int32_t>(std::max<int64_t>(1, int64_t{h->hp} * nmax / std::max(1, h->max)));
  } else {
    h->hp = std::max(1, h->hp + (nmax - h->max));
  }
  h->max = nmax;
  h->hp = std::min(h->hp, h->max);
}

// --- képességek -------------------------------------------------------------------------------------------------------
std::vector<uint16_t> Sim::abilities_of(EntityId e) {
  if (const HeroState* hs = world_.get<HeroState>(e)) return hs->learned;
  const Unit* u = world_.get<Unit>(e);
  const Building* b = u ? nullptr : world_.get<Building>(e);
  if (!u && !b) return {};
  return rules_->defs[u ? u->def : b->def].abilities;
}

uint16_t Sim::ability_for(EntityId e, uint16_t base) {
  const HeroState* hs = world_.get<HeroState>(e);
  if (!hs) return base;
  for (const uint16_t a : hs->learned) {
    if (rules_->abilities[a].base == base) return a;
  }
  return kNoAbility;
}

bool Sim::learn(EntityId e, const std::string& id) {
  HeroState* hs = world_.get<HeroState>(e);
  const Unit* u = world_.get<Unit>(e);
  const int ai = rules_ ? rules_->ability_index(id) : -1;
  if (!hs || !u || ai < 0 || hs->points == 0) return false;
  const auto base = static_cast<uint16_t>(rules_->abilities[static_cast<size_t>(ai)].base);
  const auto& list = rules_->defs[u->def].abilities;
  if (std::find(list.begin(), list.end(), base) == list.end()) return false;
  const AbilityDef& a = rules_->abilities[base];
  uint8_t n = 0;
  auto it = std::find_if(hs->learned.begin(), hs->learned.end(), [&](uint16_t x) { return rules_->abilities[x].base == base; });
  if (it != hs->learned.end()) n = rules_->abilities[*it].level;
  if (n >= a.max_level) return false;
  const int need = a.ultimate ? rules_->heroes.ultimate_level : 2 * (n + 1) - 1;
  if (hs->level < need) return false;
  const int next = n == 0 ? base : rules_->ability_index(a.id + "@" + std::to_string(n + 1));
  if (next < 0) return false;
  if (it != hs->learned.end()) *it = static_cast<uint16_t>(next);
  else hs->learned.push_back(static_cast<uint16_t>(next));
  --hs->points;
  return true;
}

// --- tapasztalat --------------------------------------------------------------------------------------------------------
void Sim::add_xp(EntityId e, int32_t milli) {
  HeroState* hs = world_.get<HeroState>(e);
  if (!hs || milli <= 0) return;
  const auto& lv = rules_->heroes.levels;
  const int maxl = static_cast<int>(lv.size());
  if (hs->level >= maxl) return;
  hs->xp += milli;
  while (hs->level < maxl && hs->xp >= lv[hs->level] * 1000) {
    ++hs->level;
    ++hs->points;
    hero_recalc(e, false);
    if (Health* h = world_.get<Health>(e)) {   // +25% életerő
      h->hp = std::min(h->max, h->hp + (Fixed::from_int(h->max) * rules_->heroes.levelup_heal).floor_int());
    }
  }
  if (hs->level >= maxl) hs->xp = lv.back() * 1000;
}

void Sim::grant_kill_xp(EntityId victim, uint8_t owner, EntityId killer) {
  const Health* vh = world_.get<Health>(victim);
  const Position* vp = world_.get<Position>(victim);
  if (!vh || !vp || owner == 0 || !hostile(owner, vh->owner) || world_.get<Lifetime>(victim)) return;
  const Unit* vu = world_.get<Unit>(victim);
  const Building* vb = vu ? nullptr : world_.get<Building>(victim);
  if (!vu && !vb) return;
  const EntityDef& d = rules_->defs[vu ? vu->def : vb->def];
  int64_t value;   // ezred tapasztalat (a régi `xp_value`)
  if (d.xp_value >= 0) {
    value = int64_t{d.xp_value} * 1000;
  } else if (const HeroState* vhs = world_.get<HeroState>(victim)) {
    value = int64_t{rules_->heroes.xp_hero_base + rules_->heroes.xp_hero_per_level * vhs->level} * 1000;
  } else {
    int64_t cost = 0;
    for (const int32_t c : d.cost) cost += c;
    const Fixed v = rules_->heroes.xp_unit_base + rules_->heroes.xp_cost_mult * Fixed::from_ratio(static_cast<int32_t>(cost), d.count);
    value = (int64_t{v.raw()} * 1000) >> 16;
    if (vb) value /= 2;
  }
  std::vector<EntityId> heroes;
  const int maxl = static_cast<int>(rules_->heroes.levels.size());
  world_.pool<HeroState>().each([&](EntityId e, const HeroState& hs) {
    const Health* h = world_.get<Health>(e);
    const Position* p = world_.get<Position>(e);
    if (!h || !p || h->owner != owner || h->hp <= 0 || hs.level >= maxl || e == victim) return;
    if (core::length(p->x - vp->x, p->y - vp->y) <= rules_->heroes.share_radius) heroes.push_back(e);
  });
  if (heroes.empty() && killer && world_.alive(killer)) {
    if (const HeroState* ks = world_.get<HeroState>(killer); ks && ks->level < maxl) heroes.push_back(killer);
  }
  if (heroes.empty()) return;
  std::sort(heroes.begin(), heroes.end());
  for (const EntityId h : heroes) add_xp(h, static_cast<int32_t>(value / static_cast<int64_t>(heroes.size())));
}

// --- tárgyak ----------------------------------------------------------------------------------------------------------
bool Sim::give_item(EntityId e, uint16_t item) {
  HeroState* hs = world_.get<HeroState>(e);
  if (!hs || item >= rules_->items.size()) return false;
  const int inv = std::clamp(rules_->heroes.inventory, 1, 6);
  for (int i = 0; i < inv; ++i) {
    if (hs->items[static_cast<size_t>(i)] != kNoItem) continue;
    hs->items[static_cast<size_t>(i)] = item;
    if (rules_->items[item].equip) hero_recalc(e, true);
    return true;
  }
  return false;   // tele
}

EntityId Sim::drop_ground_item(uint16_t item, Fixed x, Fixed y) {
  if (!rules_ || item >= rules_->items.size()) return kNoEntity;
  const EntityId g = world_.create();
  world_.add(g, Position{x, y});
  world_.add(g, GroundItem{item});
  return g;
}

bool Sim::use_item(EntityId e, int slot) {
  HeroState* hs = world_.get<HeroState>(e);
  Health* h = world_.get<Health>(e);
  if (!hs || !h || slot < 0 || slot >= 6 || hs->items[static_cast<size_t>(slot)] == kNoItem) return false;
  const ItemDef& it = rules_->items[hs->items[static_cast<size_t>(slot)]];
  if (it.equip) return false;   // a viselt tárgy magától hat
  Caster* c = world_.get<Caster>(e);
  if (it.heal > 0) {
    if (h->hp >= h->max) return false;   // teljes életerőnél elutasítva
    h->hp = std::min(h->max, h->hp + it.heal);
  }
  if (it.energy > 0) {
    const int32_t cap = stat(e, "Energy.max", Fixed::from_int(rules_->defs[world_.get<Unit>(e)->def].energy_max)).floor_int() * 1000;
    if (!c || c->energy >= cap) return false;
    c->energy = std::min(cap, c->energy + it.energy * 1000);
  }
  hs->items[static_cast<size_t>(slot)] = kNoItem;
  if (it.xp > 0) add_xp(e, it.xp * 1000);
  if (it.perm_hp || it.perm_damage) {
    hs->perm_hp += it.perm_hp;
    hs->perm_damage += it.perm_damage;
    hero_recalc(e, false);
  }
  return true;
}

bool Sim::drop_item(EntityId e, int slot) {
  HeroState* hs = world_.get<HeroState>(e);
  const Position* p = world_.get<Position>(e);
  if (!hs || !p || slot < 0 || slot >= 6 || hs->items[static_cast<size_t>(slot)] == kNoItem) return false;
  const uint16_t item = hs->items[static_cast<size_t>(slot)];
  hs->items[static_cast<size_t>(slot)] = kNoItem;
  // véletlen irányba, 18 képpontra (a régi szerint)
  const core::Angle a = core::Angle::from_units(static_cast<int32_t>(combat_rng_.next() & 0xFFFF));
  drop_ground_item(item, p->x + core::cos(a) * Fixed::from_ratio(18, 32), p->y + core::sin(a) * Fixed::from_ratio(18, 32));
  if (rules_->items[item].equip) hero_recalc(e, true);
  return true;
}

bool Sim::order_pickup(EntityId e, EntityId item) {
  if (!world_.get<HeroState>(e) || !world_.get<GroundItem>(item)) return false;
  interrupt(e);
  world_.add(e, Fetch{item, 0});
  return true;
}

void Sim::item_system() {
  std::vector<EntityId> ids;
  world_.pool<Fetch>().each([&](EntityId e, const Fetch&) { ids.push_back(e); });
  for (const EntityId e : ids) {
    Fetch* f = world_.get<Fetch>(e);
    const Health* h = world_.get<Health>(e);
    if (!f || !h || h->hp <= 0) continue;
    const GroundItem* g = world_.alive(f->item) ? world_.get<GroundItem>(f->item) : nullptr;
    if (!g) {
      world_.pool<Fetch>().remove(e);
      continue;
    }
    const Position& p = *world_.get<Position>(e);
    const Position& ip = *world_.get<Position>(f->item);
    const Fixed r = rules_->defs[world_.get<Unit>(e)->def].radius;
    if (core::length(ip.x - p.x, ip.y - p.y) <= r + kReach) {
      const EntityId item = f->item;
      world_.pool<Fetch>().remove(e);
      halt(e);
      if (give_item(e, g->item)) world_.destroy(item);   // tele leltárnál ott marad
      continue;
    }
    const Movement* mv = world_.get<Movement>(e);
    if (--f->repath <= 0 || !(mv && mv->moving)) {
      order_move(e, ip.x, ip.y);
      f->repath = 24;
    }
  }
}

// --- táborok ----------------------------------------------------------------------------------------------------------
void Sim::spawn_camps(map::Map& m) {
  const FactionDef* vadon = nullptr;
  for (const FactionDef& f : rules_->factions) {
    if (f.neutral && !f.camps.empty()) vadon = &f;
  }
  const auto& starts = m.objects().starts;
  if (!vadon || starts.empty()) return;
  const Fixed cx = Fixed::from_ratio(m.width(), 2), cy = Fixed::from_ratio(m.height(), 2);
  auto camp_def = [&](int level) -> const CampDef* {
    for (const CampDef& c : vadon->camps) {
      if (c.level == level) return &c;
    }
    return nullptr;
  };
  std::vector<std::pair<int, std::pair<Fixed, Fixed>>> spots;   // szint, hely
  for (const auto& [sx, sy] : starts) {   // kezdőhelyenként egy könnyű és egy közepes (tükrözve)
    const Fixed px = Fixed::from_int(sx) + Fixed::from_int(2), py = Fixed::from_int(sy) + Fixed::from_int(2);
    const Fixed dx = cx - px, dy = cy - py;
    const Fixed len = std::max(core::length(dx, dy), Fixed::from_int(1));
    const Fixed ux = dx / len, uy = dy / len;   // a közép felé
    const Fixed step = std::min(len * Fixed::from_ratio(9, 20), Fixed::from_int(18));
    spots.push_back({1, {px + ux * step - uy * Fixed::from_int(6), py + uy * step + ux * Fixed::from_int(6)}});
    spots.push_back({2, {px + ux * step + uy * Fixed::from_int(6), py + uy * step - ux * Fixed::from_int(6)}});
  }
  for (size_t i = 0; i < starts.size(); ++i) {   // a kezdőhelyek felezőin erősebbek
    const auto& a = starts[i];
    const auto& b = starts[(i + 1) % starts.size()];
    if (starts.size() < 3 && i > 0) break;
    const Fixed mx = Fixed::from_ratio(a.first + b.first + 4, 2), my = Fixed::from_ratio(a.second + b.second + 4, 2);
    if (starts.size() == 2) {   // két játékosnál a felező a középen át: oldalra tolva, két tábor
      const Fixed dx = Fixed::from_int(b.first - a.first), dy = Fixed::from_int(b.second - a.second);
      const Fixed len = std::max(core::length(dx, dy), Fixed::from_int(1));
      spots.push_back({4, {mx - dy / len * Fixed::from_int(10), my + dx / len * Fixed::from_int(10)}});
      spots.push_back({4, {mx + dy / len * Fixed::from_int(10), my - dx / len * Fixed::from_int(10)}});
    } else {
      spots.push_back({3, {mx, my}});
    }
  }
  spots.push_back({5, {cx, cy}});   // a főnöktábor középen
  const int land = nav_ ? nav_->nav().grid_of(nav_->nav().table().movement_class("land").value_or(0)) : -1;
  for (const auto& [level, pos] : spots) {
    const CampDef* cd = camp_def(level);
    if (!cd) continue;
    Fixed x = std::clamp(pos.first, Fixed::from_int(2), Fixed::from_int(m.width() - 2));
    Fixed y = std::clamp(pos.second, Fixed::from_int(2), Fixed::from_int(m.height() - 2));
    bool near_start = false;
    for (const auto& [sx, sy] : starts) {
      near_start = near_start || core::length(Fixed::from_int(sx + 2) - x, Fixed::from_int(sy + 2) - y) <
                                     Fixed::from_int(rules_->camps.min_start_dist);
    }
    if (near_start) continue;
    if (land >= 0) {   // járható helyre
      if (const auto cell = nav_->nearest(land, 1, 0, map::Pathfinder::cell_of(x, y))) {
        const auto c = map::Pathfinder::center_of(*cell);
        x = c.first;
        y = c.second;
      }
    }
    const EntityId camp = world_.create();
    world_.add(camp, Position{x, y});
    world_.add(camp, Camp{static_cast<uint8_t>(level), 0, 0});
    int k = 0;
    for (const auto& [def, n] : cd->units) {
      for (int i = 0; i < n; ++i, ++k) {
        const core::Angle a = core::Angle::from_units(k * 13107);   // körben, 72°-onként
        const Fixed ux = x + core::cos(a) * Fixed::from_ratio(1, 1), uy = y + core::sin(a) * Fixed::from_ratio(1, 1);
        const EntityId u = spawn_unit(def, 0, ux, uy);
        if (u == kNoEntity) continue;
        place_at_free(u, ux, uy);
        const Position& up = *world_.get<Position>(u);
        world_.add(u, CampMember{camp, up.x, up.y, 0});
      }
    }
  }
}

void Sim::camp_system() {
  std::vector<EntityId> camps;
  world_.pool<Camp>().each([&](EntityId c, const Camp&) { camps.push_back(c); });
  for (const EntityId camp : camps) {
    Camp& cp = *world_.get<Camp>(camp);
    const Position cpos = *world_.get<Position>(camp);
    std::vector<EntityId> members;
    world_.pool<CampMember>().each([&](EntityId e, const CampMember& m) {
      const Health* h = world_.get<Health>(e);
      if (m.camp == camp && h && h->hp > 0) members.push_back(e);
    });
    std::sort(members.begin(), members.end());
    if (members.empty()) {   // kiirtva: egyszer zsákmány
      if (!cp.looted) {
        cp.looted = 1;
        const FactionDef* vadon = nullptr;
        for (const FactionDef& f : rules_->factions) {
          if (f.neutral) vadon = &f;
        }
        const CampDef* cd = nullptr;
        for (const CampDef& c : vadon ? vadon->camps : std::vector<CampDef>{}) {
          if (c.level == cp.level) cd = &c;
        }
        if (cd && !cd->loot.empty()) {
          const uint8_t rarity = cd->loot[combat_rng_.next() % cd->loot.size()];
          std::vector<uint16_t> pool;
          for (size_t i = 0; i < rules_->items.size(); ++i) {
            if (rules_->items[i].rarity == rarity) pool.push_back(static_cast<uint16_t>(i));
          }
          if (!pool.empty()) drop_ground_item(pool[combat_rng_.next() % pool.size()], cpos.x, cpos.y);
          if (cp.level >= rules_->camps.potion_level &&
              static_cast<int32_t>(combat_rng_.next() % 1000u) < rules_->camps.potion_permille) {
            const int potion = rules_->item_index("gyogyfozet");
            if (potion >= 0) drop_ground_item(static_cast<uint16_t>(potion), cpos.x + Fixed::from_ratio(1, 2), cpos.y);
          }
        }
      }
      continue;
    }
    // felriadás: ha bármelyik harcol, az egész tábor támadja ugyanazt
    EntityId foe = 0;
    for (const EntityId e : members) {
      const Combat* c = world_.get<Combat>(e);
      if (c && c->order == Combat::Attack && c->target && !world_.get<CampMember>(e)->returning) {
        foe = c->target;
        break;
      }
    }
    for (const EntityId e : members) {
      CampMember& m = *world_.get<CampMember>(e);
      Combat* c = world_.get<Combat>(e);
      Health& h = *world_.get<Health>(e);
      const Position& p = *world_.get<Position>(e);
      const Fixed home = core::length(p.x - m.hx, p.y - m.hy);
      if (m.returning) {   // vissza: nem támad, 10%/s-mal gyógyul
        if (c) {
          c->order = Combat::None;
          c->target = 0;
        }
        const int32_t heal = std::max(1, (Fixed::from_int(h.max) * rules_->camps.regen_pct).floor_int() / kTickHz);
        if (tick_ % 1 == 0) h.hp = std::min(h.max, h.hp + heal);
        if (home < Fixed::from_ratio(1, 2)) {
          m.returning = 0;
          halt(e);
        } else {
          const Movement* mv = world_.get<Movement>(e);
          if (!(mv && mv->moving)) order_move(e, m.hx, m.hy);
        }
        continue;
      }
      if (home > rules_->camps.leash) {   // túl messzire ment: vissza
        m.returning = 1;
        cp.alerted = 0;
        order_move(e, m.hx, m.hy);
        continue;
      }
      if (!c) continue;
      if (c->order == Combat::None && foe && world_.alive(foe)) {   // a tábor felriadt
        order_attack(e, foe);
        if (Combat* cc = world_.get<Combat>(e)) cc->autonomous = 0;
        cp.alerted = 1;
        continue;
      }
      // tétlen: csak a tábora mellett, 5 csempés körben keres (12 tickenként)
      if (c->order == Combat::None && home <= rules_->camps.home && (tick_ + e) % kScan == 0) {
        const EntityId t = find_enemy(e, rules_->camps.search);
        if (t != kNoEntity) {
          order_attack(e, t);
          if (Combat* cc = world_.get<Combat>(e)) cc->autonomous = 0;
        }
      }
    }
  }
}

}  // namespace totu::sim
