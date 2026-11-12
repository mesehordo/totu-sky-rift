// SPDX-License-Identifier: GPL-3.0-or-later
#include "sim/sim.hpp"

#include <algorithm>

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

void Sim::apply(const Command& c) {
  switch (c.kind) {
    case Command::Kind::Spawn: {
      const EntityId e = world_.create();
      world_.add(e, Position{c.x, c.y});
      world_.add(e, Movement{c.speed, c.x, c.y, false});
      world_.add(e, Health{c.hp, c.hp, c.owner});
      world_.add(e, Attack{c.damage, c.spread, c.range, c.cooldown, 0});
      break;
    }
    case Command::Kind::Move:
      for (const EntityId e : c.units) {
        if (!world_.alive(e)) continue;   // a parancs késhet: a végrehajtáskor is ellenőrzünk
        if (Movement* m = world_.get<Movement>(e)) { m->tx = c.x; m->ty = c.y; m->moving = true; }
      }
      break;
  }
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
    hp.each([&](EntityId o, const Health& oh) {
      if (o == e || oh.owner == me->owner || oh.hp <= 0) return;
      const Position* q = pos.get(o);
      if (!q) return;
      const int64_t d2 = core::length_sq_raw(q->x - p->x, q->y - p->y);
      if (d2 <= r2 && (best == kNoEntity || d2 < best_d2)) { best = o; best_d2 = d2; }
    });
    if (best == kNoEntity) return;
    Health* th = hp.get(best);
    const auto spread = static_cast<uint32_t>(std::max(0, a.spread));
    th->hp -= a.damage + static_cast<int32_t>(combat_rng_.below(spread));
    a.ready_tick = tick_ + a.cooldown;
    if (th->hp <= 0) world_.destroy(best);
  });
}

void Sim::step() {
  // 1. a tick parancsai a beérkezés sorrendjében
  size_t n = 0;
  while (n < pending_.size() && pending_[n].tick <= tick_) ++n;
  for (size_t i = 0; i < n; ++i) apply(pending_[i]);
  pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(n));
  // 2. rendszerek rögzített sorrendben
  movement_system();
  combat_system();
  // 3. a tick végén a törlések
  world_.flush();
  ++tick_;
}

uint64_t Sim::hash() const {
  core::StateHash h;
  h.add_i64(tick_);
  h.add_u64(world_.hash());
  h.add_u64(combat_rng_.state());
  h.add_u64(combat_rng_.increment());
  return h.value();
}

void save_command(core::ByteWriter& w, const Command& c) {
  w.u8(static_cast<uint8_t>(c.kind));
  w.i64(c.tick);
  w.u8(c.owner);
  w.i32(c.x.raw()); w.i32(c.y.raw());
  w.i32(c.hp); w.i32(c.speed.raw()); w.i32(c.damage); w.i32(c.spread); w.i32(c.cooldown); w.i32(c.range.raw());
  w.u32(static_cast<uint32_t>(c.units.size()));
  for (const EntityId e : c.units) w.u32(e);
}

std::optional<Command> load_command(core::ByteReader& r) {
  Command c;
  const uint8_t k = r.u8();
  if (k != 1 && k != 2) return std::nullopt;
  c.kind = static_cast<Command::Kind>(k);
  c.tick = r.i64();
  c.owner = r.u8();
  c.x = Fixed::from_raw(r.i32()); c.y = Fixed::from_raw(r.i32());
  c.hp = r.i32(); c.speed = Fixed::from_raw(r.i32()); c.damage = r.i32(); c.spread = r.i32(); c.cooldown = r.i32();
  c.range = Fixed::from_raw(r.i32());
  const uint32_t n = r.u32();
  if (n > 100000) return std::nullopt;
  for (uint32_t i = 0; i < n && r.ok(); ++i) c.units.push_back(r.u32());
  if (!r.ok()) return std::nullopt;
  return c;
}

std::vector<uint8_t> Sim::save() const {
  core::ByteWriter w;
  w.str("totu-sim");
  w.u32(1);   // séma
  w.i64(tick_);
  w.u64(combat_rng_.state());
  w.u64(combat_rng_.increment());
  world_.save(w);
  w.u32(static_cast<uint32_t>(pending_.size()));
  for (const auto& c : pending_) save_command(w, c);
  return w.data();
}

bool Sim::load(const std::vector<uint8_t>& data) {
  core::ByteReader r(data);
  if (r.str() != "totu-sim" || r.u32() != 1) return false;
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
  return r.ok() && r.at_end();
}

}  // namespace totu::sim
