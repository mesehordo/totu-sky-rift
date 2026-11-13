// SPDX-License-Identifier: GPL-3.0-or-later
#include "sim/sim.hpp"

#include <algorithm>
#include <chrono>

namespace totu::sim {

namespace {
constexpr uint64_t kCombatStream = 1;   // véletlenszám-folyamok rendszerenként (ADR-004)
constexpr uint64_t kWeatherStream = 2;
}

Sim::Sim(uint64_t seed) : weather_rng_(seed, kWeatherStream), combat_rng_(seed, kCombatStream) { register_components(); }

void Sim::register_components() {
  world_.register_component<Position>("Position");
  world_.register_component<Movement>("Movement");
  world_.register_component<Health>("Health");
  world_.register_component<Attack>("Attack");
}

void Sim::queue(Command c) {
  // stabil beszúrás: azonos ticken belül a beérkezés sorrendje marad
  auto it = std::upper_bound(pending_.begin(), pending_.end(), c.tick,
                             [](int64_t t, const Command& x) { return t < x.tick; });
  pending_.insert(it, std::move(c));
}

EntityId Sim::spawn(const Command& c) {
  const EntityId e = world_.create();
  world_.add(e, Position{c.x, c.y});
  world_.add(e, Movement{c.speed, c.x, c.y, false});
  world_.add(e, Health{c.hp, c.hp, c.owner});
  world_.add(e, Attack{c.damage, c.spread, c.range, c.cooldown, 0});
  if (nav_) {
    world_.add(e, Body{});
    world_.add(e, Path{});
    set_body(e, Fixed::from_ratio(2, 5), 0xFF);   // alapérték: szárazföldi
  }
  return e;
}

bool Sim::order_move(EntityId e, Fixed x, Fixed y) {
  if (!world_.alive(e)) return false;   // a parancs késhet: a végrehajtáskor is ellenőrzünk
  Movement* m = world_.get<Movement>(e);
  if (!m) return false;
  m->tx = x; m->ty = y; m->moving = true;
  if (nav_) nav_request(e, x, y);
  return true;
}

bool Sim::kill(EntityId e) {
  if (!world_.alive(e)) return false;
  Health* h = world_.get<Health>(e);
  if (!h || h->hp <= 0) return false;
  h->hp = 0;
  world_.destroy(e);
  died_.emplace_back(e, h->owner);
  return true;
}

void Sim::apply(const Command& c) {
  switch (c.kind) {
    case Command::Kind::Spawn: {
      const EntityId e = spawn(c);
      if (nav_ && (c.radius.raw() > 0 || c.move_class != 0xFF)) {
        set_body(e, c.radius.raw() > 0 ? c.radius : Fixed::from_ratio(2, 5), c.move_class);
      }
      break;
    }
    case Command::Kind::GroupMove: {
      // a munka abbahagyása (a rakomány marad; a tervezett építés költsége vissza); a bent építő nem mozdul
      std::vector<EntityId> units;
      for (const EntityId e : c.units) {
        if (interrupt(e)) units.push_back(e);
      }
      order_group_move(units, c.x, c.y, static_cast<Formation>(c.formation));
      break;
    }
    case Command::Kind::Gather:
      for (const EntityId e : c.units) {
        if (interrupt(e)) order_gather(e, c.target);
      }
      break;
    case Command::Kind::Stop:
      for (const EntityId e : c.units) {
        if (interrupt(e)) stop(e);
      }
      break;
    case Command::Kind::Build:
      if (rules_ && !c.units.empty()) order_build(c.units.front(), rules_->def(c.def), c.rx, c.ry);
      break;
    case Command::Kind::CancelBuild:
      cancel_build(c.target);
      break;
    case Command::Kind::Demolish:
      demolish(c.target);
      break;
    case Command::Kind::Train:
      if (rules_) train(c.owner, rules_->def(c.def), c.target);
      break;
    case Command::Kind::Research:
      research(c.owner, c.def);
      break;
    case Command::Kind::CancelQueue:
      cancel_queue_item(c.target, c.rx);
      break;
    case Command::Kind::Rally:
      for (const EntityId e : c.units) set_rally(e, c.x, c.y, c.target);
      break;
    case Command::Kind::Attack:
      for (const EntityId e : c.units) {
        if (interrupt(e)) order_attack(e, c.target);
      }
      break;
    case Command::Kind::AttackMove:
      for (const EntityId e : c.units) {
        if (interrupt(e)) order_attack_move(e, c.x, c.y);
      }
      break;
    case Command::Kind::Hold:
      for (const EntityId e : c.units) {
        if (interrupt(e)) order_hold(e);
      }
      break;
    case Command::Kind::Block:
      nav_block(c.rx, c.ry, c.rw, c.rh, c.flag != 0);
      break;
    case Command::Kind::Gate:
      nav_gate(c.rx, c.ry, c.rw, c.rh, c.flag);
      break;
    case Command::Kind::Move:
      for (const EntityId e : c.units) {
        if (interrupt(e)) order_move(e, c.x, c.y);
      }
      break;
    case Command::Kind::Ability:
      use_ability(c.units, c.def, c.x, c.y, c.target);
      break;
    case Command::Kind::Garrison:
      order_garrison(c.units, c.target);
      break;
    case Command::Kind::Unload:
      unload(c.target);
      break;
    case Command::Kind::ChoosePath:
      choose_path(c.owner, c.def);
      break;
    case Command::Kind::Power:
      use_power(c.owner, c.target, c.def, c.x, c.y);
      break;
    case Command::Kind::Learn:
      if (!c.units.empty()) learn(c.units.front(), c.def);
      break;
    case Command::Kind::Pickup:
      if (!c.units.empty()) order_pickup(c.units.front(), c.target);
      break;
    case Command::Kind::UseItem:
      if (!c.units.empty()) use_item(c.units.front(), c.rx);
      break;
    case Command::Kind::DropItem:
      if (!c.units.empty()) drop_item(c.units.front(), c.rx);
      break;
    case Command::Kind::Repair:
      order_repair(c.units, c.target);
      break;
    case Command::Kind::Pack:
      pack_building(c.target);
      break;
    case Command::Kind::Unpack:
      if (!c.units.empty()) order_unpack(c.units.front(), c.rx, c.ry);
      break;
  }
}

void Sim::report_deaths() {
  // index szerint, mert egy eseménykezelő újabb halált okozhat (kill), ami a lista végére kerül
  for (size_t i = 0; i < died_.size(); ++i) {
    const auto [id, owner] = died_[i];
    for (SimHooks* h : hooks_) h->on_death(*this, id, owner);
  }
  died_.clear();
}

void Sim::movement_system() {
  auto& pos = world_.pool<Position>();
  world_.pool<Movement>().each([&](EntityId e, Movement& m) {
    if (!m.moving) return;
    Position* p = pos.get(e);
    if (!p) return;
    const Fixed dx = m.tx - p->x, dy = m.ty - p->y;
    const Fixed d = core::length(dx, dy);
    if (d <= m.speed) {
      p->x = m.tx; p->y = m.ty; m.moving = false;
      return;
    }
    p->x += dx * m.speed / d;
    p->y += dy * m.speed / d;
  });
}

void Sim::combat_system() {
  auto& pos = world_.pool<Position>();
  auto& hp = world_.pool<Health>();
  world_.pool<Attack>().each([&](EntityId e, Attack& a) {
    if (a.ready_tick > tick_ || !world_.alive(e)) return;
    const Position* p = pos.get(e);
    const Health* me = hp.get(e);
    if (!p || !me || me->hp <= 0) return;
    // a legközelebbi élő ellenség a lőtávon belül; egyenlő távolságnál a kisebb azonosító (determinisztikus)
    const int64_t r2 = int64_t{a.range.raw()} * a.range.raw();
    EntityId best = kNoEntity;
    int64_t best_d2 = 0;
    auto consider = [&](EntityId o, const Health& oh) {
      if (o == e || oh.owner == me->owner || oh.hp <= 0) return;
      const Position* q = pos.get(o);
      if (!q) return;
      const int64_t d2 = core::length_sq_raw(q->x - p->x, q->y - p->y);
      if (d2 <= r2 && (best == kNoEntity || d2 < best_d2 || (d2 == best_d2 && o < best))) { best = o; best_d2 = d2; }
    };
    if (nav_) {
      // navigációval a térbeli rácsból (2 × 2 csempés vödrök, a mozgás után naprakész) – ugyanaz a szabály, mint a
      // teljes bejárásnál, de nem 1600 × 1600 pár
      const int reach = static_cast<int>(a.range.raw() >> 16) / 2 + 1;
      const int bx = std::clamp(static_cast<int>(p->x.raw() >> 16) / 2, 0, nav_bw_ - 1);
      const int by = std::clamp(static_cast<int>(p->y.raw() >> 16) / 2, 0, nav_bh_ - 1);
      for (int yy = std::max(0, by - reach); yy <= std::min(nav_bh_ - 1, by + reach); ++yy) {
        for (int xx = std::max(0, bx - reach); xx <= std::min(nav_bw_ - 1, bx + reach); ++xx) {
          for (const EntityId o : buckets_[static_cast<size_t>(yy * nav_bw_ + xx)]) {
            if (const Health* oh = hp.get(o)) consider(o, *oh);
          }
        }
      }
    } else {
      hp.each([&](EntityId o, const Health& oh) { consider(o, oh); });
    }
    if (best == kNoEntity) return;
    Health* th = hp.get(best);
    const auto spread = static_cast<uint32_t>(std::max(0, a.spread));
    th->hp -= a.damage + static_cast<int32_t>(combat_rng_.below(spread));
    a.ready_tick = tick_ + a.cooldown;
    if (th->hp <= 0) {
      world_.destroy(best);
      died_.emplace_back(best, th->owner);
    }
  });
}

void Sim::step() {
  // 1. a tick parancsai a beérkezés sorrendjében
  size_t n = 0;
  while (n < pending_.size() && pending_[n].tick <= tick_) ++n;
  for (size_t i = 0; i < n; ++i) apply(pending_[i]);
  pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(n));
  // 2. a csatlakozók (szkriptek) a csatlakoztatás sorrendjében
  for (SimHooks* h : hooks_) h->on_tick(*this);
  // 3. rendszerek rögzített sorrendben, majd a halál-események
  if (rules_) {   // M6: a munkások döntései (mozgásparancsok) a mozgás előtt, majd az építés haladása
    environment_system();   // napszak/időjárás (D35) – a többi rendszer ezt már frissen látja
    aura_system();          // módosítók: az aurák (a kapcsolók és az önerősítések a Caster-ben)
    economy_system();
    construction_system();
    production_system();
    if (tick_ % 15 == 0) stealth_system();   // a régi: 0,5 s
    if (tick_ % 6 == 0) vision_system();     // a régi: 0,2 s
  }
  if (nav_) {
    const auto t0 = std::chrono::steady_clock::now();
    nav_paths();
    const auto t1 = std::chrono::steady_clock::now();
    nav_movement();
    const auto t2 = std::chrono::steady_clock::now();
    nav_stats_.path_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
    nav_stats_.move_ms += std::chrono::duration<double, std::milli>(t2 - t1).count();
  } else {
    movement_system();
  }
  combat_system();
  if (rules_) {   // M6: a sablon-egységek harca és lövedékei, a képességek, majd a halál
    match_combat_system();
    ability_system();
    death_system();
  }
  report_deaths();
  // 4. a tick végén a törlések
  world_.flush();
  if (rules_) supply_system();
  ++tick_;
}

uint64_t Sim::hash() const {
  core::StateHash h;
  h.add_i64(tick_);
  h.add_u64(world_.hash());
  h.add_u64(combat_rng_.state());
  h.add_u64(combat_rng_.increment());
  for (const SimHooks* k : hooks_) h.add_u64(k->hash());   // csatlakozó nélkül a hash az M2-es marad
  if (rules_) {   // M6: a játékosállapot (szabályok nélkül a hash a korábbi marad)
    h.add_u64(upkeep_);
    h.add_u64(static_cast<uint8_t>(fog_));
    for (const PlayerState& p : players_) {
      h.add_u64(p.active);
      for (int r = 0; r < kResources; ++r) { h.add_i64(p.res[static_cast<size_t>(r)]); h.add_i64(p.gathered[static_cast<size_t>(r)]); }
      h.add_i64(p.supply_cap);
      h.add_i64(p.supply_used);
      h.add_u64(p.researched.size());
      for (const auto& r : p.researched) h.add_bytes(r);
      h.add_bytes(p.path);
      h.add_u64(p.ascended);
      h.add_u64(p.defeated);
      h.add_u64(p.fallen.size());
      for (const auto& [fd, hs] : p.fallen) { h.add_u64(fd); hs.hash(h); }
      h.add_bytes(std::string_view(reinterpret_cast<const char*>(p.visible.data()), p.visible.size()));
      h.add_bytes(std::string_view(reinterpret_cast<const char*>(p.explored.data()), p.explored.size()));
      h.add_u64(p.research_mods.size());
      for (const Modifier& m : p.research_mods) {
        h.add_bytes(m.path); h.add_u64(static_cast<uint8_t>(m.op)); h.add_i64(m.value.raw()); h.add_bytes(m.filter_id);
        h.add_u64(m.filter_tags.size());
        for (const auto& t : m.filter_tags) h.add_bytes(t);
      }
    }
    // napszak/időjárás (M6 7. lépés)
    h.add_u64(static_cast<uint8_t>(daytime_mode_)); h.add_u64(static_cast<uint8_t>(weather_mode_));
    h.add_bytes(weather_.kind); h.add_bytes(weather_.prev_kind);
    h.add_i64(weather_.change_at); h.add_i64(weather_.blend_start); h.add_i64(weather_.blend_end);
    h.add_i64(weather_.intensity.raw());
    h.add_u64(weather_rng_.state()); h.add_u64(weather_rng_.increment());
    h.add_u64(mud_patches_.size());
    for (const auto& t : mud_patches_) h.add_u64(t.converted);
    h.add_u64(ice_patches_.size());
    for (const auto& t : ice_patches_) h.add_u64(t.converted);
  }
  return h.value();
}

std::vector<std::pair<std::string, uint64_t>> Sim::component_hashes() const {
  auto out = world_.component_hashes();
  for (size_t i = 0; i < hooks_.size(); ++i) out.emplace_back("Szkript" + std::to_string(i + 1), hooks_[i]->hash());
  return out;
}

bool Command::needs_v3() const {
  return kind != Kind::Spawn && kind != Kind::Move ? true : (radius.raw() != 0 || move_class != 0xFF);
}

bool Command::needs_v4() const {
  return kind == Kind::Gather || kind == Kind::Stop || kind == Kind::Build || kind == Kind::CancelBuild ||
         kind == Kind::Demolish || kind == Kind::Train || kind == Kind::Research || kind == Kind::CancelQueue ||
         kind == Kind::Rally || kind == Kind::Attack || kind == Kind::AttackMove || kind == Kind::Hold ||
         kind == Kind::Ability || kind == Kind::Garrison || kind == Kind::Unload || kind == Kind::ChoosePath ||
         kind == Kind::Power || kind == Kind::Learn || kind == Kind::Pickup || kind == Kind::UseItem ||
         kind == Kind::DropItem || kind == Kind::Repair || kind == Kind::Pack || kind == Kind::Unpack || target != 0 ||
         !def.empty();
}

void save_command(core::ByteWriter& w, const Command& c, int format) {
  w.u8(static_cast<uint8_t>(c.kind));
  w.i64(c.tick);
  w.u8(c.owner);
  w.i32(c.x.raw()); w.i32(c.y.raw());
  w.i32(c.hp); w.i32(c.speed.raw()); w.i32(c.damage); w.i32(c.spread); w.i32(c.cooldown); w.i32(c.range.raw());
  w.u32(static_cast<uint32_t>(c.units.size()));
  for (const EntityId e : c.units) w.u32(e);
  if (format >= 3) {
    w.i32(c.radius.raw());
    w.u8(c.move_class);
    w.u8(c.formation);
    w.i32(c.rx); w.i32(c.ry); w.i32(c.rw); w.i32(c.rh);
    w.u8(c.flag);
  }
  if (format >= 4) {
    w.u32(c.target);
    w.str(c.def);
  }
}

std::optional<Command> load_command(core::ByteReader& r, int format) {
  Command c;
  const uint8_t k = r.u8();
  if (k < 1 || k > (format >= 4 ? 29 : format >= 3 ? 5 : 2)) return std::nullopt;
  c.kind = static_cast<Command::Kind>(k);
  c.tick = r.i64();
  c.owner = r.u8();
  c.x = Fixed::from_raw(r.i32()); c.y = Fixed::from_raw(r.i32());
  c.hp = r.i32(); c.speed = Fixed::from_raw(r.i32()); c.damage = r.i32(); c.spread = r.i32(); c.cooldown = r.i32();
  c.range = Fixed::from_raw(r.i32());
  const uint32_t n = r.u32();
  if (n > 100000) return std::nullopt;
  for (uint32_t i = 0; i < n && r.ok(); ++i) c.units.push_back(r.u32());
  if (format >= 3) {
    c.radius = Fixed::from_raw(r.i32());
    c.move_class = r.u8();
    c.formation = r.u8();
    c.rx = r.i32(); c.ry = r.i32(); c.rw = r.i32(); c.rh = r.i32();
    c.flag = r.u8();
    if (c.formation >= kFormationCount) return std::nullopt;
  }
  if (format >= 4) {
    c.target = r.u32();
    c.def = r.str();
  }
  if (!r.ok()) return std::nullopt;
  return c;
}

std::vector<uint8_t> Sim::save() const {
  core::ByteWriter w;
  w.str("totu-sim");
  w.u32(9);   // séma (2: a csatlakozók állapotával; 3: M4-es parancsformátum; 4: M6 – parancs, játékosok; 5: napszak/időjárás; 6: képességek; 7: helyőrség; 8: vereség; 9: elesett hősök)
  w.i64(tick_);
  w.u64(combat_rng_.state());
  w.u64(combat_rng_.increment());
  world_.save(w);
  w.u32(static_cast<uint32_t>(pending_.size()));
  for (const auto& c : pending_) save_command(w, c, 4);
  w.u32(static_cast<uint32_t>(hooks_.size()));
  for (const SimHooks* k : hooks_) k->save(w);
  w.u8(rules_ ? 1 : 0);
  if (rules_) {
    w.u8(upkeep_ ? 1 : 0);
    w.u8(static_cast<uint8_t>(fog_));
    for (const PlayerState& p : players_) {
      w.u8(p.active ? 1 : 0);
      w.str(p.faction);
      for (int r = 0; r < kResources; ++r) { w.i64(p.res[static_cast<size_t>(r)]); w.i64(p.gathered[static_cast<size_t>(r)]); }
      w.i32(p.supply_cap);
      w.i32(p.supply_used);
      w.u32(static_cast<uint32_t>(p.researched.size()));
      for (const auto& r : p.researched) w.str(r);
      w.str(p.path);
      w.u8(p.ascended ? 1 : 0);
      w.u8(p.defeated ? 1 : 0);
      w.u32(static_cast<uint32_t>(p.fallen.size()));
      for (const auto& [fd, hs] : p.fallen) { w.u32(fd); hs.save(w); }
      w.str(std::string_view(reinterpret_cast<const char*>(p.visible.data()), p.visible.size()));
      w.str(std::string_view(reinterpret_cast<const char*>(p.explored.data()), p.explored.size()));
      w.u32(static_cast<uint32_t>(p.research_mods.size()));
      for (const Modifier& m : p.research_mods) {
        w.str(m.path); w.u8(static_cast<uint8_t>(m.op)); w.i32(m.value.raw()); w.str(m.filter_id);
        w.u32(static_cast<uint32_t>(m.filter_tags.size()));
        for (const auto& t : m.filter_tags) w.str(t);
      }
    }
    w.u8(static_cast<uint8_t>(daytime_mode_)); w.u8(static_cast<uint8_t>(weather_mode_));
    w.str(weather_.kind); w.str(weather_.prev_kind);
    w.i64(weather_.change_at); w.i64(weather_.blend_start); w.i64(weather_.blend_end); w.i32(weather_.intensity.raw());
    w.u64(weather_rng_.state()); w.u64(weather_rng_.increment());
    w.u32(static_cast<uint32_t>(mud_patches_.size()));
    for (const auto& t : mud_patches_) { w.i32(t.x); w.i32(t.y); w.u32(t.orig_terrain); w.u8(t.converted); }
    w.u32(static_cast<uint32_t>(ice_patches_.size()));
    for (const auto& t : ice_patches_) { w.i32(t.x); w.i32(t.y); w.u32(t.orig_terrain); w.u8(t.converted); }
  }
  return w.data();
}

bool Sim::load(const std::vector<uint8_t>& data) {
  core::ByteReader r(data);
  if (r.str() != "totu-sim") return false;
  const uint32_t schema = r.u32();
  if (schema < 3 || schema > 9) return false;
  tick_ = r.i64();
  const uint64_t st = r.u64(), inc = r.u64();
  combat_rng_.restore(st, inc);
  if (!world_.load(r)) return false;
  pending_.clear();
  const uint32_t n = r.u32();
  for (uint32_t i = 0; i < n; ++i) {
    auto c = load_command(r, schema >= 4 ? 4 : 3);
    if (!c) return false;
    pending_.push_back(std::move(*c));
  }
  died_.clear();
  if (r.u32() != hooks_.size()) return false;   // ugyanazok a csatlakozók kellenek, mint mentéskor
  for (SimHooks* k : hooks_) {
    if (!k->load(r)) return false;
  }
  const bool with_rules = schema >= 4 && r.u8() != 0;
  if (with_rules != (rules_ != nullptr)) return false;   // a szabályokat a mentés előtt ugyanúgy csatolni kell
  if (with_rules) {
    upkeep_ = r.u8() != 0;
    fog_ = static_cast<MatchSetup::Fog>(std::min<uint8_t>(r.u8(), 2));
    for (PlayerState& p : players_) {
      p.active = r.u8() != 0;
      p.faction = r.str();
      for (int i = 0; i < kResources; ++i) { p.res[static_cast<size_t>(i)] = r.i64(); p.gathered[static_cast<size_t>(i)] = r.i64(); }
      p.supply_cap = r.i32();
      p.supply_used = r.i32();
      p.researched.clear();
      const uint32_t nr = r.u32();
      for (uint32_t i = 0; i < nr && i < 4096 && r.ok(); ++i) p.researched.push_back(r.str());
      p.path = r.str();
      p.ascended = r.u8() != 0;
      p.defeated = schema >= 8 && r.u8() != 0;
      p.fallen.clear();
      if (schema >= 9) {
        for (uint32_t i = 0, nf = r.u32(); i < nf && i < 64 && r.ok(); ++i) {
          const auto fd = static_cast<uint16_t>(r.u32());
          p.fallen.emplace_back(fd, HeroState::load(r));
        }
      }
      const std::string vis = r.str(), exp = r.str();
      p.visible.assign(vis.begin(), vis.end());
      p.explored.assign(exp.begin(), exp.end());
      p.research_mods.clear();
      if (schema >= 5) {
        const uint32_t nm = r.u32();
        for (uint32_t i = 0; i < nm && i < 4096 && r.ok(); ++i) {
          Modifier m;
          m.path = r.str();
          m.op = static_cast<Modifier::Op>(std::min<uint8_t>(r.u8(), 2));
          m.value = Fixed::from_raw(r.i32());
          m.filter_id = r.str();
          const uint32_t nt = r.u32();
          for (uint32_t t = 0; t < nt && t < 256 && r.ok(); ++t) m.filter_tags.push_back(r.str());
          p.research_mods.push_back(std::move(m));
        }
      }
    }
    for (int i = 0; i <= kMaxPlayers; ++i) rebuild_rules(static_cast<uint8_t>(i));   // származtatott (KUT-RULE)
    if (schema >= 5) {
      daytime_mode_ = static_cast<MatchSetup::Daytime>(std::min<uint8_t>(r.u8(), 2));
      weather_mode_ = static_cast<MatchSetup::WeatherMode>(std::min<uint8_t>(r.u8(), 3));
      weather_.kind = r.str();
      weather_.prev_kind = r.str();
      weather_.change_at = r.i64();
      weather_.blend_start = r.i64();
      weather_.blend_end = r.i64();
      weather_.intensity = Fixed::from_raw(r.i32());
      const uint64_t wst = r.u64(), winc = r.u64();
      weather_rng_.restore(wst, winc);
      mud_patches_.clear();
      const uint32_t nmud = r.u32();
      for (uint32_t i = 0; i < nmud && r.ok(); ++i) {
        TerrainPatch t;
        t.x = static_cast<int16_t>(r.i32()); t.y = static_cast<int16_t>(r.i32());
        t.orig_terrain = static_cast<uint16_t>(r.u32()); t.converted = r.u8();
        mud_patches_.push_back(t);
      }
      ice_patches_.clear();
      const uint32_t nice = r.u32();
      for (uint32_t i = 0; i < nice && r.ok(); ++i) {
        TerrainPatch t;
        t.x = static_cast<int16_t>(r.i32()); t.y = static_cast<int16_t>(r.i32());
        t.orig_terrain = static_cast<uint16_t>(r.u32()); t.converted = r.u8();
        ice_patches_.push_back(t);
      }
    }
  }
  return r.ok() && r.at_end();
}

}  // namespace totu::sim
