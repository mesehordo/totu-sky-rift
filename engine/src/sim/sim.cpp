// SPDX-License-Identifier: GPL-3.0-or-later
#include "sim/sim.hpp"

#include <algorithm>
#include <chrono>

namespace totu::sim {

namespace {
constexpr uint64_t kCombatStream = 1;   // véletlenszám-folyamok rendszerenként (ADR-004)
}

Sim::Sim(uint64_t seed) : combat_rng_(seed, kCombatStream) { register_components(); }

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
    case Command::Kind::GroupMove:
      order_group_move(c.units, c.x, c.y, static_cast<Formation>(c.formation));
      break;
    case Command::Kind::Block:
      nav_block(c.rx, c.ry, c.rw, c.rh, c.flag != 0);
      break;
    case Command::Kind::Gate:
      nav_gate(c.rx, c.ry, c.rw, c.rh, c.flag);
      break;
    case Command::Kind::Move:
      for (const EntityId e : c.units) order_move(e, c.x, c.y);
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
  report_deaths();
  // 4. a tick végén a törlések
  world_.flush();
  ++tick_;
}

uint64_t Sim::hash() const {
  core::StateHash h;
  h.add_i64(tick_);
  h.add_u64(world_.hash());
  h.add_u64(combat_rng_.state());
  h.add_u64(combat_rng_.increment());
  for (const SimHooks* k : hooks_) h.add_u64(k->hash());   // csatlakozó nélkül a hash az M2-es marad
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
}

std::optional<Command> load_command(core::ByteReader& r, int format) {
  Command c;
  const uint8_t k = r.u8();
  if (k < 1 || k > (format >= 3 ? 5 : 2)) return std::nullopt;
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
  if (!r.ok()) return std::nullopt;
  return c;
}

std::vector<uint8_t> Sim::save() const {
  core::ByteWriter w;
  w.str("totu-sim");
  w.u32(3);   // séma (2: a csatlakozók állapotával; 3: M4-es parancsformátum)
  w.i64(tick_);
  w.u64(combat_rng_.state());
  w.u64(combat_rng_.increment());
  world_.save(w);
  w.u32(static_cast<uint32_t>(pending_.size()));
  for (const auto& c : pending_) save_command(w, c);
  w.u32(static_cast<uint32_t>(hooks_.size()));
  for (const SimHooks* k : hooks_) k->save(w);
  return w.data();
}

bool Sim::load(const std::vector<uint8_t>& data) {
  core::ByteReader r(data);
  if (r.str() != "totu-sim" || r.u32() != 3) return false;
  tick_ = r.i64();
  const uint64_t st = r.u64(), inc = r.u64();
  combat_rng_.restore(st, inc);
  if (!world_.load(r)) return false;
  pending_.clear();
  const uint32_t n = r.u32();
  for (uint32_t i = 0; i < n; ++i) {
    auto c = load_command(r);
    if (!c) return false;
    pending_.push_back(std::move(*c));
  }
  died_.clear();
  if (r.u32() != hooks_.size()) return false;   // ugyanazok a csatlakozók kellenek, mint mentéskor
  for (SimHooks* k : hooks_) {
    if (!k->load(r)) return false;
  }
  return r.ok() && r.at_end();
}

}  // namespace totu::sim
