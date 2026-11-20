// SPDX-License-Identifier: GPL-3.0-or-later
// Meccs és gazdaság (M6, ADR-007 2. lépés): a meccs indítása a pályából, sablonból létrehozott egységek és épületek,
// a munkások gyűjtése (a régi játék `_o_gather`-je szerint: odamegy, rakodik, a legközelebbi leadóhelyre hord, a
// kimerült lelőhely után a közelben újat keres), bevétel a Hadtáppal (MECH-UPKEEP), létszám (MECH-SUPPLY).
#include <algorithm>
#include <unordered_map>

#include "core/angle.hpp"
#include "map/map.hpp"
#include "map/pathfinder.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

// „odaért”: a régi játékban 22 képpont a test széle és a cél téglalapja között (1 csempe = 32 képpont)
const Fixed kReach = Fixed::from_ratio(22, 32);
constexpr int kRetryDropTicks = 60;   // az elérhetetlen leadóhelyet ennyi ideig kihagyja (régi játék: 2 s)

Fixed half(int v) { return Fixed::from_ratio(v, 2); }

}  // namespace

void Sim::attach_rules(const Rules* rules) {
  if (rules_ || !rules) return;
  rules_ = rules;
  world_.register_component<Unit>("Unit");
  world_.register_component<Building>("Building");
  world_.register_component<ResourceNode>("ResourceNode");
  world_.register_component<Worker>("Worker");
  world_.register_component<Producer>("Producer");
  world_.register_component<Combat>("Combat");
  world_.register_component<Vital>("Vital");
  world_.register_component<Shot>("Shot");
  world_.register_component<Stealth>("Stealth");
  world_.register_component<Caster>("Caster");     // M6 8. lépés: képességek
  world_.register_component<Afflict>("Afflict");
  world_.register_component<Lifetime>("Lifetime");
  world_.register_component<Field>("Field");
  world_.register_component<Spell>("Spell");
  world_.register_component<Garrison>("Garrison");   // helyőrség (D39)
  world_.register_component<Sheltered>("Sheltered");
  world_.register_component<Enter>("Enter");
  world_.register_component<Mine>("Mine");   // bányaépület (Napfúró, Fertőzött tárna)
  world_.register_component<HeroState>("HeroState");   // hősök, tárgyak, táborok (M7)
  world_.register_component<GroundItem>("GroundItem");
  world_.register_component<Fetch>("Fetch");
  world_.register_component<Camp>("Camp");
  world_.register_component<CampMember>("CampMember");
  world_.register_component<Pack>("Pack");   // Wullof (M7 3. lépés)
  world_.register_component<Wagon>("Wagon");
  world_.register_component<SelfBuild>("SelfBuild");
  world_.register_component<RepairOrder>("RepairOrder");
  world_.register_component<Mounted>("Mounted");
  world_.register_component<Tapped>("Tapped");
  world_.register_component<Seize>("Seize");   // elfoglalás, megszállás, előléptetés (D30)
  world_.register_component<Occupants>("Occupants");
  world_.register_component<Veteran>("Veteran");
  world_.register_component<Captured>("Captured");
  modules_ = make_faction_modules();   // népmodulok (M7): a saját komponenseik rögzített sorrendben
  for (auto& m : modules_) m->register_components(world_);
}

int32_t Sim::carry_cap(const EntityDef& d, int kind, uint8_t owner) const {
  const int32_t base = d.carry[static_cast<size_t>(kind)] > 0 ? d.carry[static_cast<size_t>(kind)]
                                                              : rules_->resources[static_cast<size_t>(kind)].carry;
  // Worker.carry_pct (D24): a régi `stat()` szerint százalékpont-akkumulátor (0 = nincs kutatás)
  const Fixed pct = research_stat(owner, "Worker.carry_pct", Fixed{}, d);
  const Fixed v = Fixed::from_int(base) * (Fixed::from_int(1) + pct / Fixed::from_int(100));
  return std::max(1, v.floor_int());
}

EntityId Sim::spawn_unit(uint16_t def, uint8_t owner, Fixed x, Fixed y) {
  if (!rules_ || def >= rules_->defs.size() || owner > kMaxPlayers) return kNoEntity;   // H-23: a tulajdonos 0..8
  const EntityDef& d = rules_->defs[def];
  const EntityId e = world_.create();
  const int32_t hp = std::max(1, research_stat(owner, "Health.max", Fixed::from_int(d.hp), d).floor_int());
  world_.add(e, Position{x, y});
  world_.add(e, Movement{d.speed, x, y, false});
  world_.add(e, Health{hp, hp, owner});
  world_.add(e, Unit{def});
  if (d.worker) world_.add(e, Worker{});
  Vital vt;
  vt.shield = std::max(0, research_stat(owner, "Health.shield", Fixed::from_int(d.shield), d).floor_int()) * 1000;
  world_.add(e, vt);
  if (d.attack.has) world_.add(e, Combat{});
  world_.add(e, Stealth{});
  world_.add(e, Afflict{});
  if (owner >= 1 && owner <= kMaxPlayers) {   // falkaszellem (Wullof)
    if (const FactionDef* f = rules_->faction(players_[owner].faction); f && f->pack && !d.worker && !d.has_tag("livestock")) {
      world_.add(e, Pack{});
    }
  }
  if (d.hero) {   // hős: az elesett állapota (újraélesztés) vagy új, 1. szinten egy ponttal
    HeroState hs;
    if (owner <= kMaxPlayers) {
      auto& fl = players_[owner].fallen;
      const auto it = std::find_if(fl.begin(), fl.end(), [&](const auto& f) { return f.first == def; });
      if (it != fl.end()) {
        hs = it->second;
        fl.erase(it);
      }
    }
    world_.add(e, hs);
    hero_recalc(e, false);
    Health& hh = *world_.get<Health>(e);
    hh.hp = hh.max;
  }
  if (!d.abilities.empty() || d.energy_max > 0) {
    Caster cs;
    cs.energy = std::min(50, d.energy_max) * 1000;   // a régi: legfeljebb 50 energiával indul
    world_.add(e, cs);
  }
  if (nav_) {
    world_.add(e, Body{});
    world_.add(e, Path{});
    const auto air = nav_->nav().table().movement_class("air");
    set_body(e, d.radius, d.flying && air ? *air : 0xFF);
  }
  for (auto& m : modules_) m->on_unit_spawned(*this, e);
  return e;
}

EntityId Sim::place_building(uint16_t def, uint8_t owner, int tx, int ty, bool complete) {
  if (!rules_ || def >= rules_->defs.size() || owner > kMaxPlayers) return kNoEntity;   // H-24: a tulajdonos 0..8
  const EntityDef& d = rules_->defs[def];
  const int w = std::max(1, d.fw), h = std::max(1, d.fh);
  if (map_ && (tx < 0 || ty < 0 || tx + w > map_->width() || ty + h > map_->height())) return kNoEntity;   // csak a térképre
  const EntityId e = world_.create();
  const int32_t hp = std::max(1, research_stat(owner, "Health.max", Fixed::from_int(d.hp), d).floor_int());
  world_.add(e, Position{Fixed::from_int(tx) + half(w), Fixed::from_int(ty) + half(h)});
  world_.add(e, Health{complete ? hp : std::max(1, hp / 10), hp, owner});   // félkészen 10% (a régi szerint)
  Building b{def, static_cast<int16_t>(tx), static_cast<int16_t>(ty), static_cast<int16_t>(w), static_cast<int16_t>(h),
             static_cast<uint8_t>(complete ? 1 : 0)};
  b.progress = complete ? std::max(1, d.time_ticks) * 1000 : 0;
  world_.add(e, b);
  if (!d.produces.empty() || !d.researches.empty()) world_.add(e, Producer{});
  Vital vt;
  vt.shield = std::max(0, research_stat(owner, "Health.shield", Fixed::from_int(d.shield), d).floor_int()) * 1000;
  world_.add(e, vt);
  if (d.attack.has || d.garrison > 0) world_.add(e, Combat{});   // a helyőrség fegyvert ad
  if (d.garrison > 0) world_.add(e, Garrison{});
  if (d.mine_slots > 0) {   // bányaépület: a Napfúróban a bent dolgozók helyőrségként (nem lőnek), a tárnában a számláló
    world_.add(e, Mine{});
    if (!d.mine_consume && d.garrison <= 0) world_.add(e, Garrison{});
  }
  world_.add(e, Afflict{});   // páncéltörés (épületen is)
  if (!d.powers.empty()) world_.add(e, Caster{});                  // főépület-képességek újratöltése
  if (nav_) nav_->add_obstacle({tx, ty, w, h});
  for (auto& m : modules_) m->on_building_placed(*this, e);
  return e;
}

bool Sim::setup_match(map::Map& m, const MatchSetup& setup, std::string* error) {
  auto fail = [&](const std::string& msg) {
    if (error) *error = msg;
    return false;
  };
  if (!rules_) return fail("nincsenek szabályadatok (attach_rules)");
  // előbb minden feltétel, csak utána bármilyen változás (H-25: hibánál a világ érintetlen; H-26: egyszer)
  if (match_started_) return fail("a meccs már elindult");
  {
    const auto& st = m.objects().starts;
    std::vector<std::pair<int, int>> used;
    for (size_t i = 0; i < setup.factions.size() && i < static_cast<size_t>(kMaxPlayers); ++i) {
      if (setup.factions[i].empty()) continue;
      const FactionDef* f = rules_->faction(setup.factions[i]);
      if (!f) return fail("ismeretlen nép: " + setup.factions[i]);
      if (!setup.bases) continue;   // M11: küldetés – kezdőhely és főépület nem kell
      if (i >= st.size()) return fail("a pályán kevesebb a kezdőhely, mint a játékos");
      if (std::find(used.begin(), used.end(), st[i]) != used.end()) return fail("két játékos ugyanazon a kezdőhelyen");   // H-38
      used.push_back(st[i]);
      if (f->hq == kNoDef) return fail("a népnek nincs főépülete: " + f->id);
      const EntityDef& hq = rules_->defs[f->hq];
      if (std::max(1, hq.fw) > m.width() || std::max(1, hq.fh) > m.height()) return fail("a pálya kisebb a főépületnél");   // H-03
      for (const auto& [def, n] : f->start_units) {
        if (n < 0 || n > 200) return fail("túl sok kezdőegység: " + f->id);   // H-89
      }
    }
    for (const map::MapResource& r : m.objects().resources) {
      const int kind = resource_index(r.kind);
      if (kind < 0 || rules_->resources[static_cast<size_t>(kind)].special) return fail("ismeretlen lelőhely: " + r.kind);
    }
  }
  match_started_ = true;
  upkeep_ = setup.upkeep;
  fog_ = setup.fog;
  map_ = &m;
  daytime_mode_ = setup.daytime;
  weather_mode_ = setup.weather;
  weather_ = WeatherState{};
  weather_.change_at = tick_;   // az első időjárás az első environment_system()-ben eldől
  const auto& starts = m.objects().starts;
  {   // a választott kezdőhelyek: érvényesek és különbözők
    std::vector<int8_t> used;
    for (size_t i = 0; i < setup.starts.size() && i < setup.factions.size(); ++i) {
      if (setup.factions[i].empty() || setup.starts[i] < 0) continue;
      if (static_cast<size_t>(setup.starts[i]) >= starts.size()) return fail("érvénytelen kezdőhely");
      if (std::find(used.begin(), used.end(), setup.starts[i]) != used.end()) return fail("két játékosnak ugyanaz a kezdőhelye");
      used.push_back(setup.starts[i]);
    }
  }
  // a kezdőhelyek kiosztása (a régi `_starts`): a választott marad; a többi – ha a pálya jelöli a két oldalt, és
  // legfeljebb két csapat van – a csapata oldalán az első szabad, különben a sorszáma szerinti (ha szabad)
  std::vector<int> start_of(static_cast<size_t>(kMaxPlayers) + 1, -1);
  {
    std::vector<bool> taken(starts.size(), false);
    std::vector<uint8_t> teams_used;
    for (size_t i = 0; i < setup.factions.size() && i < static_cast<size_t>(kMaxPlayers); ++i) {
      if (setup.factions[i].empty()) continue;
      if (i < setup.starts.size() && setup.starts[i] >= 0 && static_cast<size_t>(setup.starts[i]) < starts.size()) {
        start_of[i + 1] = setup.starts[i];
        taken[static_cast<size_t>(setup.starts[i])] = true;
      }
      const uint8_t t = i < setup.teams.size() ? setup.teams[i] : 0;
      if (t != 0 && std::find(teams_used.begin(), teams_used.end(), t) == teams_used.end()) teams_used.push_back(t);
    }
    std::sort(teams_used.begin(), teams_used.end());
    const auto& sides = m.objects().start_sides;
    const bool by_side = sides.size() == starts.size() && !teams_used.empty() && teams_used.size() <= 2;
    for (size_t i = 0; i < setup.factions.size() && i < static_cast<size_t>(kMaxPlayers); ++i) {
      if (setup.factions[i].empty() || start_of[i + 1] >= 0) continue;
      const uint8_t t = i < setup.teams.size() ? setup.teams[i] : 0;
      int pick = -1;
      if (by_side && t != 0) {
        const int side = static_cast<int>(std::find(teams_used.begin(), teams_used.end(), t) - teams_used.begin());
        for (size_t k = 0; k < starts.size() && pick < 0; ++k) {
          if (!taken[k] && sides[k] == side) pick = static_cast<int>(k);
        }
      }
      if (pick < 0 && i < starts.size() && !taken[i]) pick = static_cast<int>(i);
      for (size_t k = 0; k < starts.size() && pick < 0; ++k) {
        if (!taken[k]) pick = static_cast<int>(k);
      }
      if (pick >= 0) {
        start_of[i + 1] = pick;
        taken[static_cast<size_t>(pick)] = true;
      }
    }
  }
  std::vector<uint8_t> playing;
  for (size_t i = 0; i < setup.factions.size() && i < static_cast<size_t>(kMaxPlayers); ++i) {
    if (setup.factions[i].empty()) continue;
    if (!rules_->faction(setup.factions[i])) return fail("ismeretlen nép: " + setup.factions[i]);
    if (setup.bases && i >= starts.size()) return fail("a pályán kevesebb a kezdőhely, mint a játékos");
    PlayerState& p = players_[i + 1];
    p = PlayerState{};
    p.faction = setup.factions[i];
    p.active = true;
    p.team = i < setup.teams.size() && setup.teams[i] <= 4 ? setup.teams[i] : 0;
    const FactionDef& f = *rules_->faction(p.faction);
    for (int r = 0; r < kResources; ++r) {   // küldetésben a nyersanyagot a küldetés adja; Gyors: kétszeres
      p.res[static_cast<size_t>(r)] = setup.bases ? int64_t{f.start_resources[static_cast<size_t>(r)]} * 1000 *
                                                        (setup.tempo == MatchSetup::Tempo::Rush ? 2 : 1) : 0;
      // a régi „Nyersanyag” beállítás: kevés × 0,6, sok × 2 (+150 meteorit alább)
      if (setup.bases && setup.resources == 0) p.res[static_cast<size_t>(r)] = p.res[static_cast<size_t>(r)] * 6 / 10;
      if (setup.bases && setup.resources == 2) p.res[static_cast<size_t>(r)] *= 2;
    }
    if (setup.bases && setup.resources == 2) p.res[kMeteor] += 150 * 1000;
    playing.push_back(static_cast<uint8_t>(i + 1));
  }
  // nyersanyagok: előbb a lelőhelyek, majd a fák (a pálya sorrendjében)
  std::vector<map::TileRect> obstacles;
  auto add_node = [&](int kind, int x, int y, int size, int amount) {
    const EntityId e = world_.create();
    world_.add(e, Position{Fixed::from_int(x) + half(size), Fixed::from_int(y) + half(size)});
    world_.add(e, ResourceNode{static_cast<uint8_t>(kind), static_cast<int16_t>(x), static_cast<int16_t>(y),
                               static_cast<int16_t>(size), amount});
    obstacles.push_back({x, y, size, size});
  };
  for (const map::MapResource& r : m.objects().resources) {
    const int kind = resource_index(r.kind);
    const int amount = r.amount >= 0 ? r.amount : rules_->resources[static_cast<size_t>(kind)].node_amount;
    if (amount <= 0) continue;   // H-36: az üres lelőhely nem foglal helyet
    add_node(kind, r.x, r.y, r.size, amount);
  }
  for (const auto& [x, y] : m.objects().trees) add_node(kWood, x, y, 1, rules_->resources[kWood].node_amount);
  for (const auto& d : m.objects().decor) obstacles.push_back({d.tx, d.ty, d.w, d.h});   // díszletépület: utat zár
  if (nav_) nav_->add_obstacles(obstacles);   // egyszerre: a nagy erdős pályákon is gyors indulás
  // játékosonként a főépület és a kezdő egységek (a régi játék szerint: a pálya közepe felé, ívben)
  const Fixed cx = half(m.width()), cy = half(m.height());
  auto_defeat_ = setup.bases;
  climate_ = setup.climate;
  // M12: a meccs módja
  mode_ = MatchMode{};
  mode_.victory = static_cast<uint8_t>(setup.victory);
  mode_.tempo = static_cast<uint8_t>(setup.tempo);
  {
    const int32_t def = setup.tempo == MatchSetup::Tempo::Survival ? 15 : setup.victory == MatchSetup::Victory::Hill ? 3 : 20;
    mode_.minutes = std::clamp(setup.minutes > 0 ? setup.minutes : def, 1, 240);
  }
  if (setup.tempo == MatchSetup::Tempo::Calm) mode_.peace_until = tick_ + 10LL * 60 * kTickHz;
  if (setup.tempo == MatchSetup::Tempo::Survival) {
    mode_.peace_players = true;
    mode_.next_wave = tick_ + 3LL * 60 * kTickHz;   // az első hullám a 3. percben
  }
  for (const uint8_t owner : playing) {
    if (!setup.bases) break;   // M11: küldetés – a kezdő egységeket a küldetés helyezi el
    const FactionDef& f = *rules_->faction(players_[owner].faction);
    if (f.hq == kNoDef) return fail("a népnek nincs főépülete: " + f.id);
    const EntityDef& hq = rules_->defs[f.hq];
    const int w = std::max(1, hq.fw), h = std::max(1, hq.fh);
    size_t si = owner - 1u;   // a kiosztott kezdőhely (lásd fent)
    if (start_of[owner] >= 0) si = static_cast<size_t>(start_of[owner]);
    const auto [sx, sy] = starts[si];
    const int tx = std::clamp(sx - w / 2, 0, m.width() - w), ty = std::clamp(sy - h / 2, 0, m.height() - h);
    const EntityId b = place_building(f.hq, owner, tx, ty, true);
    if (b == kNoEntity) return fail("a főépület nem helyezhető el");   // H-03 (az előzetes ellenőrzés után nem fordul elő)
    const Position bp = *world_.get<Position>(b);
    const core::Angle base = core::atan2(cy - bp.y, cx - bp.x);
    for (const auto& [def, n] : f.start_units) {
      for (int k = 0; k < n; ++k) {
        // a régi képlet: szög + (k − n/2) × 0,35 rad; 0,35 rad = 3651 szögegység (2^16 = 360°)
        const core::Angle a = base + core::Angle::from_units((2 * k - n) * 3651 / 2);
        const Fixed rx = half(w) + Fixed::from_ratio(40, 32), ry = half(h) + Fixed::from_ratio(40, 32);
        // H-39: a pálya szélénél a pályán belül marad
        const Fixed ux = std::clamp(bp.x + core::cos(a) * rx, Fixed::from_ratio(1, 2), Fixed::from_int(m.width()) - Fixed::from_ratio(1, 2));
        const Fixed uy = std::clamp(bp.y + core::sin(a) * ry, Fixed::from_ratio(1, 2), Fixed::from_int(m.height()) - Fixed::from_ratio(1, 2));
        const EntityId u = spawn_unit(def, owner, ux, uy);
        const EntityDef& ud = rules_->defs[def];
        if (!ud.worker) continue;
        const int kinds[4] = {f.start_gather, kGold, kWood, kMeteor};
        for (const int kind : kinds) {
          if (!(ud.gathers & (1u << kind))) continue;
          const EntityId r = nearest_resource(kind, bp.x, bp.y, Fixed::from_int(16), kNoEntity);
          if (r != kNoEntity) order_gather(u, r);
          break;
        }
      }
    }
  }
  navigation_changed();
  supply_system();
  stealth_system();
  vision_system();
  build_terrain_patches();
  for (int i = 0; i <= kMaxPlayers; ++i) rebuild_rules(static_cast<uint8_t>(i));
  ai_slots_ = setup.ai;
  for (const AiSlot& a : setup.ai) {   // az „Őrült” gépi ellenfél egyetlen csalása (M9)
    if (a.player >= 1 && a.player <= kMaxPlayers && a.difficulty == "orult") players_[a.player].cheat_gather = true;
  }
  if (setup.camps) spawn_camps(m);   // a Vadon táborai (D25)
  for (auto& mod : modules_) mod->on_match_start(*this);
  return true;
}

void Sim::restore_obstacles(map::Map& m) {
  map_ = &m;
  if (!nav_ || !rules_) return;
  std::vector<map::TileRect> obstacles;
  world_.pool<ResourceNode>().each([&](EntityId, const ResourceNode& n) { obstacles.push_back({n.tx, n.ty, n.size, n.size}); });
  world_.pool<Building>().each([&](EntityId, const Building& b) { obstacles.push_back({b.tx, b.ty, b.w, b.h}); });
  for (const auto& d : m.objects().decor) obstacles.push_back({d.tx, d.ty, d.w, d.h});
  nav_->add_obstacles(obstacles);
  for (const auto& op : nav_ops_) {   // a mentett Block/Gate parancsok, a kiadás sorrendjében (H-29)
    const map::TileRect rc{op[1], op[2], op[3], op[4]};
    if (op[0] == 0) {
      if (op[5] != 0) nav_->add_obstacle(rc);
      else nav_->remove_obstacle(rc);
    } else {
      nav_->set_gate(rc, static_cast<uint8_t>(op[5]));
    }
  }
  // nincs újratervezés: a mentett utak a mentés pillanatának akadályaival készültek, érvényesek (ugyanúgy folytatódik)
  // a mentett pályaváltozások (D35: eső → sár, hó → jég) újraalkalmazása a friss (még változatlan) pályán; a
  // jelöltlistát (`mud_patches_`/`ice_patches_`) a betöltés már feltöltötte ugyanabban a sorrendben
  const auto& table = nav_->nav().table();
  const auto mud_id = table.terrain("mud");
  const auto ice_id = table.terrain("ice");
  bool any = false;
  for (const auto& t : mud_patches_) {
    if (t.converted && mud_id) { m.set_terrain(t.x, t.y, *mud_id); any = true; }
  }
  for (const auto& t : ice_patches_) {
    if (t.converted && ice_id) { m.set_terrain(t.x, t.y, *ice_id); any = true; }
  }
  if (any) nav_->terrain_changed({0, 0, m.width(), m.height()});
}

bool Sim::order_gather(EntityId e, EntityId res) {
  if (!rules_ || !world_.alive(e) || !world_.alive(res)) return false;
  if (const ResourceNode* rn = world_.get<ResourceNode>(res); rn && !mod_can_gather(e, rn->kind)) return false;
  Worker* w = world_.get<Worker>(e);
  const Unit* u = world_.get<Unit>(e);
  const ResourceNode* n = world_.get<ResourceNode>(res);
  const Position* rp = world_.get<Position>(res);
  if (!w || !u || !n || !rp || n->amount <= 0) return false;
  if (!(rules_->defs[u->def].gathers & (1u << n->kind))) return false;
  w->state = Worker::Go;
  w->res = res;
  w->move_to = 0;
  w->fails = 0;
  w->last_kind = n->kind;
  w->lx = rp->x;
  w->ly = rp->y;
  return true;
}

void Sim::stop(EntityId e) {
  if (!world_.alive(e)) return;
  if (Worker* w = world_.get<Worker>(e)) w->state = Worker::Idle;
  Movement* m = world_.get<Movement>(e);
  const Position* p = world_.get<Position>(e);
  if (!m || !p) return;
  if (Path* path = world_.get<Path>(e)) {
    nav_arrive(e, *m, *path, *p);
  } else {
    m->moving = false;
    m->tx = p->x;
    m->ty = p->y;
  }
}

void Sim::income(uint8_t owner, int kind, int32_t amount, bool gathering) {
  PlayerState& p = players_[owner];
  int64_t milli = int64_t{amount} * 1000;
  if (kind == kGold && upkeep_) milli = int64_t{amount} * rules_->upkeep_permille(p.supply_used);   // Hadtáp
  if (gathering && p.cheat_gather) milli += milli / 2;   // „Őrült” gép: +50% gyűjtés (jelölt csalás)
  p.res[static_cast<size_t>(kind)] += milli;
  p.gathered[static_cast<size_t>(kind)] += milli;
}

EntityId Sim::nearest_resource(int kind, Fixed x, Fixed y, Fixed max_dist, EntityId exclude) {
  EntityId best = kNoEntity;
  int64_t best_d2 = int64_t{max_dist.raw()} * max_dist.raw();
  const auto& pos = world_.pool<Position>();
  world_.pool<ResourceNode>().each([&](EntityId e, const ResourceNode& n) {
    if (n.kind != kind || n.amount <= 0 || e == exclude) return;
    const Position* p = pos.get(e);
    if (!p) return;
    const int64_t d2 = core::length_sq_raw(p->x - x, p->y - y);
    if (d2 < best_d2 || (d2 == best_d2 && best != kNoEntity && e < best)) {
      best = e;
      best_d2 = d2;
    }
  });
  return best;
}

EntityId Sim::nearest_dropoff(uint8_t owner, int kind, Fixed x, Fixed y, EntityId exclude) {
  EntityId best = kNoEntity;
  int64_t best_d2 = 0;
  const auto& pos = world_.pool<Position>();
  const auto& hp = world_.pool<Health>();
  world_.pool<Building>().each([&](EntityId e, const Building& b) {
    if (e == exclude || !b.complete || !(rules_->defs[b.def].dropoff & (1u << kind))) return;
    const Health* h = hp.get(e);
    const Position* p = pos.get(e);
    if (!h || !p || h->owner != owner || h->hp <= 0) return;
    const int64_t d2 = core::length_sq_raw(p->x - x, p->y - y);
    if (best == kNoEntity || d2 < best_d2 || (d2 == best_d2 && e < best)) {
      best = e;
      best_d2 = d2;
    }
  });
  return best;
}

bool Sim::in_reach(EntityId unit, EntityId target) {
  const Position* p = world_.pool<Position>().get(unit);
  if (!p) return false;
  int x = 0, y = 0, w = 0, h = 0;
  if (const ResourceNode* n = world_.pool<ResourceNode>().get(target)) {
    x = n->tx; y = n->ty; w = h = n->size;
  } else if (const Building* b = world_.pool<Building>().get(target)) {
    x = b->tx; y = b->ty; w = b->w; h = b->h;
  } else {
    return false;
  }
  // a test széle és a téglalap közti távolság ≤ kReach
  const Fixed x0 = Fixed::from_int(x), y0 = Fixed::from_int(y), x1 = Fixed::from_int(x + w), y1 = Fixed::from_int(y + h);
  const Fixed dx = std::max({x0 - p->x, Fixed{}, p->x - x1});
  const Fixed dy = std::max({y0 - p->y, Fixed{}, p->y - y1});
  Fixed radius = Fixed::from_ratio(4, 10);
  if (const Unit* u = world_.pool<Unit>().get(unit)) radius = rules_->defs[u->def].radius;
  const Fixed lim = kReach + radius;
  return core::length_sq_raw(dx, dy) <= int64_t{lim.raw()} * lim.raw();
}

void Sim::remove_resource(EntityId e) {
  const ResourceNode* n = world_.get<ResourceNode>(e);
  if (!n) return;
  // a kivágott fa helye járható lesz; a meglévő utak érvényesek maradnak (csak nyílik hely), ezért nincs
  // általános újratervezés
  if (nav_) nav_->remove_obstacle({n->tx, n->ty, n->size, n->size});
  world_.destroy(e);
}

void Sim::economy_system() {
  auto& workers = world_.pool<Worker>();
  // a lelőhelyeken épp kitermelő munkások (a munkahelyek foglaltsága)
  std::unordered_map<EntityId, int> working;
  workers.each([&](EntityId, const Worker& w) {
    if (w.state == Worker::Work && w.res) ++working[w.res];
  });
  auto slots_of = [&](uint8_t owner, int kind) {
    const FactionDef* f = owner >= 1 && owner <= kMaxPlayers ? rules_->faction(players_[owner].faction) : nullptr;
    return f && kind >= 0 && kind < kResources ? f->gather_slots[static_cast<size_t>(kind)] : 0;
  };
  workers.each([&](EntityId e, Worker& w) {
    if (w.state == Worker::Idle || !world_.alive(e)) return;
    const Position* pos = world_.get<Position>(e);
    const Movement* mv = world_.get<Movement>(e);
    const Unit* unit = world_.get<Unit>(e);
    const Health* hp = world_.get<Health>(e);
    if (!pos || !mv || !unit || !hp || hp->hp <= 0) return;
    const EntityDef& d = rules_->defs[unit->def];
    auto node_of = [&](EntityId r) -> const ResourceNode* {
      if (r == 0 || !world_.alive(r)) return nullptr;
      const ResourceNode* n = world_.get<ResourceNode>(r);
      return n && n->amount > 0 ? n : nullptr;
    };
    switch (w.state) {
      case Worker::Go: {
        const ResourceNode* n = node_of(w.res);
        if (!n) {   // kimerült: a közelben (12 csempe) ugyanilyet keres
          const EntityId nr = nearest_resource(w.last_kind, w.lx, w.ly, Fixed::from_int(12), kNoEntity);
          if (nr == kNoEntity) {
            stop(e);
            return;
          }
          w.res = nr;
          w.move_to = 0;
          n = world_.get<ResourceNode>(nr);
        }
        if (w.carry > 0 && w.carry_kind != n->kind) {   // más rakomány van nála: előbb leadja
          w.state = Worker::Return;
          w.move_to = 0;
          return;
        }
        if (!(d.gathers & (1u << n->kind))) {
          stop(e);
          return;
        }
        const Position& rp = *world_.get<Position>(w.res);
        w.last_kind = n->kind;
        w.lx = rp.x;
        w.ly = rp.y;
        if (in_reach(e, w.res)) {
          const bool tapping = n->kind == kWood && d.tap_amount > 0;   // a kéregcsapolás saját (fánként egy) szabálya
          if (const int slots = tapping ? 0 : slots_of(hp->owner, n->kind); slots > 0 && working[w.res] >= slots) {
            // tele: a 10 csempén belüli legközelebbi szabad, ugyanilyen lelőhely; ha nincs, sorban áll (a tanulmány 9.2)
            EntityId alt = kNoEntity;
            int64_t best = int64_t{Fixed::from_int(10).raw()} * Fixed::from_int(10).raw();
            world_.pool<ResourceNode>().each([&](EntityId o, const ResourceNode& on) {
              if (o == w.res || on.kind != n->kind || on.amount <= 0) return;
              if (const auto it = working.find(o); it != working.end() && it->second >= slots) return;
              const Position* op = world_.get<Position>(o);
              const int64_t d2 = op ? core::length_sq_raw(op->x - pos->x, op->y - pos->y) : best + 1;
              if (d2 < best) {
                best = d2;
                alt = o;
              }
            });
            if (alt != kNoEntity) {
              w.res = alt;
              w.move_to = 0;
              return;
            }
            if (w.fails == 0) emit(SimEvent::NodeFull, hp->owner, unit->def);
            w.fails = 1;   // vár (a jelzés egyszer)
            return;
          }
          w.fails = 0;
          ++working[w.res];
          // nagyobb rakomány: arányosan tovább rakodik; az időjárás (hőség, D35) lassíthatja
          const ResourceDef& rd = rules_->resources[n->kind];
          const int64_t t = int64_t{rd.gather_time.raw()} * carry_cap(d, n->kind, hp->owner) / std::max(1, rd.carry);
          const Fixed gm = env_gather_mult(hp->owner);
          const int64_t t2 = gm.raw() > 0 ? (t << 16) / gm.raw() : t;
          w.timer = std::max(1, seconds_to_ticks(Fixed::from_raw(static_cast<int32_t>(t2))));
          if (n->kind == kWood && d.tap_amount > 0) w.timer = std::max(1, d.tap_ticks);   // kéregcsapolás (Wullof)
          if (const Fixed mg = mod_gather(e); mg != Fixed::from_int(1) && mg.raw() > 0) {   // népmodul
            w.timer = std::max(1, (Fixed::from_int(w.timer) / mg).floor_int());
          }
          w.state = Worker::Work;
          w.move_to = 0;
          const uint8_t keep = w.state;
          stop(e);
          w.state = keep;
          return;
        }
        if (w.move_to != w.res) {
          order_move(e, rp.x, rp.y);
          w.move_to = w.res;
          return;
        }
        if (!mv->moving) {   // elérhetetlen vagy foglalt: újra, majd másikat keres
          w.move_to = 0;
          if (++w.fails > 3) {
            const EntityId nr = nearest_resource(n->kind, pos->x, pos->y, Fixed::from_int(14), w.res);
            if (nr == kNoEntity) {
              stop(e);
              return;
            }
            w.res = nr;
            w.fails = 0;
          }
        }
        return;
      }
      case Worker::Work: {
        ResourceNode* n = node_of(w.res) ? world_.get<ResourceNode>(w.res) : nullptr;
        if (!n) {
          w.state = Worker::Go;
          w.move_to = 0;
          return;
        }
        if (n->kind == kWood && d.tap_amount > 0) {   // kéregcsapolás: fánként egy csapoló, a fa lassan fogy, hordás nélkül
          Tapped* tp = world_.get<Tapped>(w.res);
          if (tp && tp->by != e && world_.alive(tp->by)) {
            const Worker* other = world_.get<Worker>(tp->by);
            if (other && other->res == w.res && other->state == Worker::Work) {   // foglalt: a közelben másik fa
              const EntityId nr = nearest_resource(kWood, pos->x, pos->y, Fixed::from_int(10), w.res);
              const Tapped* nt = nr != kNoEntity ? world_.get<Tapped>(nr) : nullptr;
              if (nr != kNoEntity && !(nt && nt->by != e && world_.alive(nt->by))) {
                w.res = nr;
                w.state = Worker::Go;
                w.move_to = 0;
              } else {
                stop(e);
              }
              return;
            }
          }
          if (!tp) world_.add(w.res, Tapped{e});
          else tp->by = e;
          if (--w.timer > 0) return;
          income(hp->owner, kWood, d.tap_amount, true);
          n = world_.get<ResourceNode>(w.res);
          n->amount -= std::max(1, (Fixed::from_int(d.tap_amount) * d.tap_drain).floor_int());
          w.timer = std::max(1, d.tap_ticks);
          if (n->amount <= 0) {
            remove_resource(w.res);
            w.state = Worker::Go;
            w.move_to = 0;
          }
          return;
        }
        if (--w.timer > 0) return;
        const int32_t take = std::min(carry_cap(d, n->kind, hp->owner), n->amount);
        n->amount -= take;
        w.carry_kind = n->kind;
        w.carry = take;
        if (n->amount <= 0) remove_resource(w.res);
        w.state = Worker::Return;
        w.move_to = 0;
        return;
      }
      case Worker::Return: {
        if (w.carry <= 0) {
          w.state = Worker::Go;
          w.move_to = 0;
          return;
        }
        const EntityId exclude = w.retry_tick > tick_ ? w.bad_drop : kNoEntity;
        const EntityId drop = nearest_dropoff(hp->owner, w.carry_kind, pos->x, pos->y, exclude);
        if (drop == kNoEntity) return;   // nincs leadóhely: vár
        if (in_reach(e, drop)) {
          income(hp->owner, w.carry_kind, w.carry, true);
          w.carry = 0;
          w.bad_drop = 0;
          w.state = Worker::Go;
          w.move_to = 0;
          const uint8_t keep = w.state;
          stop(e);
          w.state = keep;
          return;
        }
        if (w.move_to != drop) {
          const Position& dp = *world_.get<Position>(drop);
          order_move(e, dp.x, dp.y);
          w.move_to = drop;
          return;
        }
        if (!mv->moving) {   // nem érte el: ideiglenesen kihagyja
          w.bad_drop = drop;
          w.retry_tick = tick_ + kRetryDropTicks;
          w.move_to = 0;
        }
        return;
      }
      default:
        build_order_step(e, w);   // Build, Construct (build.cpp)
        return;
    }
  });
}

int Sim::winner() const {
  if (mode_.survived) {   // Túlélés: a csapat nyert (a legkisebb sorszámú életben maradt játékos)
    for (int i = 1; i <= kMaxPlayers; ++i) {
      if (players_[static_cast<size_t>(i)].active && !players_[static_cast<size_t>(i)].defeated) return i;
    }
  }
  // a régi szabály: vége, ha az életben maradók mind szövetségesek (egy csapat vagy egy játékos); a győztes a
  // legkisebb sorszámú életben maradt (a csapattársai is nyertek – allied()). Legalább két oldal kell a kezdetén.
  int first = 0, sides = 0;
  std::array<bool, 5> team_seen{};
  for (int i = 1; i <= kMaxPlayers; ++i) {
    const PlayerState& p = players_[static_cast<size_t>(i)];
    if (!p.active) continue;
    if (p.team == 0) ++sides;
    else if (!team_seen[std::min<size_t>(p.team, 4)]) {
      team_seen[std::min<size_t>(p.team, 4)] = true;
      ++sides;
    }
  }
  if (sides < 2) return 0;
  for (int i = 1; i <= kMaxPlayers; ++i) {
    const PlayerState& p = players_[static_cast<size_t>(i)];
    if (!p.active || p.defeated) continue;
    if (first == 0) first = i;
    else if (!allied(static_cast<uint8_t>(first), static_cast<uint8_t>(i))) return 0;
  }
  return first;
}

bool Sim::defeat_player(uint8_t p) {
  if (p < 1 || p > kMaxPlayers) return false;
  PlayerState& ps = players_[p];
  if (!ps.active || ps.defeated) return false;
  ps.defeated = true;
  return true;
}

bool Sim::declare_winner(uint8_t p) {
  if (p < 1 || p > kMaxPlayers || !players_[p].active || players_[p].defeated || winner() != 0) return false;
  for (int i = 1; i <= kMaxPlayers; ++i) {
    PlayerState& o = players_[static_cast<size_t>(i)];
    if (o.active && !o.defeated && hostile(p, static_cast<uint8_t>(i))) o.defeated = true;
  }
  return true;
}

void Sim::supply_system() {
  std::array<int32_t, kMaxPlayers + 1> cap{}, used{};
  std::array<int64_t, kMaxPlayers + 1> part{};   // ezredrész-létszám (H-04)
  const auto& hp = world_.pool<Health>();
  world_.pool<Building>().each([&](EntityId e, const Building& b) {
    const Health* h = hp.get(e);
    if (!b.complete || !h || h->hp <= 0 || h->owner > kMaxPlayers) return;
    cap[h->owner] += rules_->defs[b.def].supply_provides;   // (az egységeké lent: emu)
    for (const auto& [bd, add] : rules_of(h->owner).supply_add) {   // Kiadós tanya
      if (bd == b.def) cap[h->owner] += add;
    }
  });
  world_.pool<Unit>().each([&](EntityId e, const Unit& u) {
    const Health* h = hp.get(e);
    if (!h || h->hp <= 0 || h->owner > kMaxPlayers) return;
    const EntityDef& ud = rules_->defs[u.def];
    if (world_.get<Lifetime>(e)) return;   // idézett lény (az állandó is): nem foglal létszámot (WC3; a tulajdonos döntése)
    if (ud.count > 1 && ud.spawn_unit == kNoDef) part[h->owner] += int64_t{ud.supply} * 1000 / ud.count;   // H-04: a pár tagja a fele (a régi supply/count)
    else used[h->owner] += ud.supply;
    cap[h->owner] += rules_->defs[u.def].supply_provides;   // létszámot adó egység (Wullof emu)
  });
  for (int i = 0; i <= kMaxPlayers; ++i) used[static_cast<size_t>(i)] += static_cast<int32_t>((part[static_cast<size_t>(i)] + 999) / 1000);
  world_.pool<Producer>().each([&](EntityId e, const Producer& pr) {   // a sorban lévő egységek is (MECH-SUPPLY)
    const Health* h = hp.get(e);
    if (!h || h->owner > kMaxPlayers) return;
    for (const QueueItem& q : pr.queue) used[h->owner] += q.supply;
  });
  // vereség: akinek nincs több (kész vagy félkész) épülete
  std::array<int32_t, kMaxPlayers + 1> buildings{};
  world_.pool<Building>().each([&](EntityId e, const Building&) {
    const Health* h = hp.get(e);
    if (h && h->hp > 0 && h->owner <= kMaxPlayers) ++buildings[h->owner];
  });
  for (int i = 1; i <= kMaxPlayers; ++i) {
    PlayerState& p = players_[static_cast<size_t>(i)];
    if (auto_defeat_ && p.active && !p.defeated && buildings[static_cast<size_t>(i)] == 0) p.defeated = true;
  }
  if (mode_.active()) mode_system();   // M12: győzelmi feltétel, hullámok
  for (int i = 1; i <= kMaxPlayers; ++i) cap[static_cast<size_t>(i)] += mod_supply(static_cast<uint8_t>(i));
  for (int i = 0; i <= kMaxPlayers; ++i) {
    players_[static_cast<size_t>(i)].supply_cap = std::min(cap[static_cast<size_t>(i)], rules_->max_supply);
    players_[static_cast<size_t>(i)].supply_used = used[static_cast<size_t>(i)];
  }
}

// M12: a győzelmi feltételek és a tempóprofilok tickenkénti része (determinisztikus; a szimuláció állapotából)
void Sim::mode_system() {
  if (!map_ || winner() != 0) return;
  auto defeat_others = [&](uint8_t keep) {   // a győztes és a vele nem ellenséges játékosok maradnak
    for (int i = 1; i <= kMaxPlayers; ++i) {
      PlayerState& p = players_[static_cast<size_t>(i)];
      if (p.active && !p.defeated && hostile(keep, static_cast<uint8_t>(i))) p.defeated = true;
    }
  };
  // Főváros: a főépület elvesztése vereség
  if (mode_.victory == static_cast<uint8_t>(MatchSetup::Victory::Capital)) {
    std::array<bool, kMaxPlayers + 1> has{};
    world_.pool<Building>().each([&](EntityId e, const Building& b) {
      const Health* h = world_.get<Health>(e);
      if (!h || h->hp <= 0 || h->owner < 1 || h->owner > kMaxPlayers) return;
      const FactionDef* f = rules_->faction(players_[h->owner].faction);
      if (f && b.def == f->hq) has[h->owner] = true;
    });
    for (int i = 1; i <= kMaxPlayers; ++i) {
      PlayerState& p = players_[static_cast<size_t>(i)];
      if (p.active && !p.defeated && !has[static_cast<size_t>(i)]) p.defeated = true;
    }
  }
  const int64_t limit = int64_t{mode_.minutes} * 60 * kTickHz;
  // Dombkirály: a pálya közepének 8×8 csempéje; ha csak egy játékos (és szövetségesei) egységei vannak bent, nála gyűlik
  if (mode_.victory == static_cast<uint8_t>(MatchSetup::Victory::Hill) && tick_ % kTickHz == 0) {
    const Fixed cx = Fixed::from_ratio(map_->width(), 2), cy = Fixed::from_ratio(map_->height(), 2), r = Fixed::from_int(4);
    std::array<bool, kMaxPlayers + 1> in{};
    world_.pool<Unit>().each([&](EntityId e, const Unit&) {
      const Health* h = world_.get<Health>(e);
      const Position* p = world_.get<Position>(e);
      if (!h || !p || h->hp <= 0 || h->owner < 1 || h->owner > kMaxPlayers) return;
      if (p->x >= cx - r && p->x < cx + r && p->y >= cy - r && p->y < cy + r) in[h->owner] = true;
    });
    int holder = 0;
    bool contested = false;
    for (int i = 1; i <= kMaxPlayers; ++i) {
      if (!in[static_cast<size_t>(i)]) continue;
      if (holder == 0) holder = i;
      else if (hostile(static_cast<uint8_t>(holder), static_cast<uint8_t>(i))) contested = true;
    }
    if (holder && !contested) {
      mode_.hill[static_cast<size_t>(holder)] += kTickHz;
      if (mode_.hill[static_cast<size_t>(holder)] >= limit) defeat_others(static_cast<uint8_t>(holder));
    }
  }
  // Időkorlát: a legtöbb pont nyer (egyenlőségnél a kisebb sorszám)
  if (mode_.victory == static_cast<uint8_t>(MatchSetup::Victory::Timed) && tick_ >= limit) {
    std::array<int64_t, kMaxPlayers + 1> score{};
    for (int i = 1; i <= kMaxPlayers; ++i) {
      for (int k = 0; k < kResources; ++k) score[static_cast<size_t>(i)] += players_[static_cast<size_t>(i)].gathered[static_cast<size_t>(k)] / 1000;
    }
    world_.pool<Health>().each([&](EntityId e, const Health& h) {
      if (h.hp <= 0 || h.owner < 1 || h.owner > kMaxPlayers) return;
      if (world_.get<Building>(e)) score[h.owner] += 50;
      else if (world_.get<Unit>(e)) score[h.owner] += 10;
    });
    int best = 0;
    for (int i = 1; i <= kMaxPlayers; ++i) {
      const PlayerState& p = players_[static_cast<size_t>(i)];
      if (p.active && !p.defeated && (best == 0 || score[static_cast<size_t>(i)] > score[static_cast<size_t>(best)])) best = i;
    }
    if (best) defeat_others(static_cast<uint8_t>(best));
  }
  // Túlélés: a Vadon hullámai (90 s-onként, egyre több), a pálya sarkaiból a legközelebbi játékosépület felé
  if (mode_.tempo == static_cast<uint8_t>(MatchSetup::Tempo::Survival)) {
    if (tick_ >= limit) {
      mode_.survived = true;
      return;
    }
    if (tick_ >= mode_.next_wave) {
      mode_.next_wave = tick_ + 90LL * kTickHz;
      ++mode_.wave;
      std::vector<uint16_t> pool;
      for (size_t d = 0; d < rules_->defs.size(); ++d) {
        if (rules_->defs[d].faction == "vadon" && !rules_->defs[d].building && rules_->defs[d].attack.has) pool.push_back(static_cast<uint16_t>(d));
      }
      if (pool.empty()) return;
      const int corner = (mode_.wave - 1) % 4;
      const Fixed sx = Fixed::from_int(corner % 2 == 0 ? 3 : map_->width() - 3);
      const Fixed sy = Fixed::from_int(corner < 2 ? 3 : map_->height() - 3);
      EntityId target = kNoEntity;
      int64_t best = 0;
      world_.pool<Building>().each([&](EntityId e, const Building&) {
        const Health* h = world_.get<Health>(e);
        const Position* p = world_.get<Position>(e);
        if (!h || !p || h->hp <= 0 || h->owner < 1 || h->owner > kMaxPlayers) return;
        const int64_t d = core::length_sq_raw(p->x - sx, p->y - sy);
        if (target == kNoEntity || d < best || (d == best && e < target)) {
          target = e;
          best = d;
        }
      });
      const int n = std::min(3 + mode_.wave * 2, 40);
      for (int k = 0; k < n; ++k) {
        const uint16_t def = pool[static_cast<size_t>((mode_.wave + k) % static_cast<int>(pool.size()))];
        const EntityId u = spawn_unit(def, 0, sx, sy);
        if (u == kNoEntity) continue;
        place_at_free(u, sx, sy);
        if (const Position* tp = target != kNoEntity ? world_.get<Position>(target) : nullptr) order_attack_move(u, tp->x, tp->y);
      }
    }
  }
}

}  // namespace totu::sim
