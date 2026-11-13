// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/session.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>

#include "content/loader.hpp"

namespace totu::client {

using core::Fixed;

namespace {

constexpr float kTilePx = 32.0f;
float to_px(Fixed v) { return static_cast<float>(v.raw()) / 65536.0f * kTilePx; }

const auto kEpoch = std::chrono::steady_clock::now();

}  // namespace

const SnapUnit* Snapshot::find(sim::EntityId id) const {
  const auto it = std::lower_bound(units.begin(), units.end(), id, [](const SnapUnit& u, sim::EntityId v) { return u.id < v; });
  return it != units.end() && it->id == id ? &*it : nullptr;
}

bool Session::load(const std::string& content_dir, const std::string& map_path, uint64_t seed, std::string* error) {
  content::Vfs vfs;
  vfs.add_layer("totu", std::filesystem::path(content_dir) / "base");
  content::ContentSet set;
  content::ContentLoader(vfs, std::filesystem::path(content_dir) / "schema").load(set);
  std::vector<std::string> errors;
  if (!terrain_.load(set, errors)) {
    if (error) *error = errors.empty() ? "terepadat" : errors.front();
    return false;
  }
  {   // játékszabályok (M6): a game.json és a sablonok
    std::ifstream gin(std::filesystem::path(content_dir) / "base" / "game.json", std::ios::binary);
    const std::string gtext{std::istreambuf_iterator<char>(gin), std::istreambuf_iterator<char>()};
    const auto game = content::parse(gtext);
    std::vector<std::string> rerr;
    if (game) rules_ = sim::Rules::load(*game, set, rerr);
  }
  std::ifstream in(map_path, std::ios::binary);
  const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  const auto doc = content::parse(text);
  if (doc) map_ = map::Map::from_json(*doc, terrain_, errors);
  if (!map_) {
    if (error) *error = "a pálya nem tölthető be: " + map_path + (errors.empty() ? "" : " – " + errors.front());
    return false;
  }
  nav_ = std::make_unique<map::NavGrid>(*map_, terrain_);
  pf_ = std::make_unique<map::Pathfinder>(*nav_);
  sim_ = std::make_unique<sim::Sim>(seed);
  sim_->attach_navigation(pf_.get());
  return true;
}

void Session::spawn_demo(int players, int per_player) {
  const uint8_t land = terrain_.movement_class("land").value_or(0);
  const int w = map_->width(), h = map_->height();
  for (int p = 0; p < players; ++p) {
    const int side = 12;
    for (int i = 0; i < per_player; ++i) {
      sim::Command c;
      c.owner = static_cast<uint8_t>(p + 1);
      const int bx = p % 2 == 0 ? 4 : w - 4 - side, by = h / 2 - side / 2 + (p / 2) * (side + 2) - (players > 2 ? side : 0);
      c.x = Fixed::from_ratio(bx * 10 + (i % side) * 11, 10);
      c.y = Fixed::from_ratio(by * 10 + (i / side) * 11, 10);
      c.speed = Fixed::from_ratio(8 + i % 3, 100);
      c.hp = 120;
      c.damage = 6;
      c.spread = 4;
      c.cooldown = 30 + i % 10;
      c.range = Fixed::from_ratio(i % 4 == 0 ? 450 : 130, 100);
      const sim::EntityId e = sim_->spawn(c);
      sim_->set_body(e, Fixed::from_ratio(38, 100), land);
    }
  }
  last_.reset();   // a következő pillanatkép már az új egységekkel
}

bool Session::start_match(const std::vector<std::string>& factions, bool upkeep, std::string* error) {
  if (!rules_) {
    if (error) *error = "a játékszabályok nem tölthetők be";
    return false;
  }
  sim_->attach_rules(&*rules_);
  sim::MatchSetup setup;
  setup.factions = factions;
  setup.upkeep = upkeep;
  if (!sim_->setup_match(*map_, setup, error)) return false;
  last_.reset();
  fog_.reset();
  return true;
}

void Session::battle_demo(int per_side) {
  if (!rules_ || !sim_->rules()) return;
  const char* kinds[] = {"urs_pajzshordozo", "urs_berserker", "urs_szigonyvezo", "urs_dardaor", "urs_kardfogu",
                         "urs_mammut", "urs_szigonyvezo", "urs_berserker"};
  const int cx = map_->width() / 2, cy = map_->height() / 2;
  for (int side = 0; side < 2; ++side) {
    std::vector<sim::EntityId> army;
    for (int i = 0; i < per_side; ++i) {
      const int col = i / 6, row = i % 6;
      const int x = side == 0 ? cx - 7 - col : cx + 7 + col;
      const sim::EntityId e = sim_->spawn_unit(rules_->def(kinds[i % 8]), static_cast<uint8_t>(side + 1),
                                               Fixed::from_int(x) + Fixed::from_ratio(1, 2),
                                               Fixed::from_int(cy - 3 + row) + Fixed::from_ratio(1, 2));
      if (e != sim::kNoEntity) army.push_back(e);
    }
    for (const sim::EntityId e : army) {
      sim_->order_attack_move(e, Fixed::from_int(side == 0 ? cx + 10 : cx - 10), Fixed::from_int(cy));
    }
  }
  last_.reset();
}

void Session::order_demo_attack() {
  std::map<uint8_t, std::vector<sim::EntityId>> armies;
  sim_->world().pool<sim::Health>().each([&](sim::EntityId e, const sim::Health& hh) { armies[hh.owner].push_back(e); });
  const Fixed cx = Fixed::from_ratio(map_->width(), 2), cy = Fixed::from_ratio(map_->height(), 2);
  for (auto& [owner, ids] : armies) {
    sim_->order_group_move(ids, owner % 2 ? cx + Fixed::from_int(6) : cx - Fixed::from_int(6), cy,
                           owner % 2 ? sim::Formation::Auto : sim::Formation::Line);
  }
}

void Session::update_fog() {
  const int w = map_->width(), h = map_->height();
  auto fog = fog_ ? std::make_shared<std::vector<uint8_t>>(*fog_)
                  : std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
  for (auto& f : *fog) {
    if (f == 255) f = 128;   // a korábban látott rész felderített marad
  }
  constexpr int kSight = 8;
  sim_->world().pool<sim::Health>().each([&](sim::EntityId e, const sim::Health& hh) {
    if (hh.owner != 1 || hh.hp <= 0) return;
    const auto* p = sim_->world().get<sim::Position>(e);
    if (!p) return;
    const int cx = static_cast<int>(p->x.raw() >> 16), cy = static_cast<int>(p->y.raw() >> 16);
    for (int y = std::max(0, cy - kSight); y <= std::min(h - 1, cy + kSight); ++y) {
      for (int x = std::max(0, cx - kSight); x <= std::min(w - 1, cx + kSight); ++x) {
        if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= kSight * kSight) (*fog)[static_cast<size_t>(y * w + x)] = 255;
      }
    }
  });
  fog_ = std::move(fog);
}

std::shared_ptr<const Snapshot> Session::snapshot() {
  if (last_ && last_->tick == sim_->tick()) return last_;
  if (!fog_) update_fog();
  auto s = std::make_shared<Snapshot>();
  s->tick = sim_->tick();
  const bool match = sim_->rules() != nullptr;
  auto& w = sim_->world();
  w.pool<sim::Position>().each([&](sim::EntityId e, const sim::Position& p) {
    const auto* hh = w.get<sim::Health>(e);
    SnapUnit u;
    u.id = e;
    u.x = to_px(p.x);
    u.y = to_px(p.y);
    if (hh) {
      u.hp = hh->hp;
      u.max_hp = hh->max;
      u.owner = hh->owner;
    }
    if (match) {
      if (const auto* sh = w.get<sim::Shot>(e)) {   // lövedék
        u.kind = SnapUnit::kShot;
        u.res = sh->kind;
        u.owner = sh->owner;
        u.hp = 1;
        u.sx = to_px(sh->sx);
        u.sy = to_px(sh->sy);
        const float tot = std::hypot(to_px(sh->ax) - u.sx, to_px(sh->ay) - u.sy);
        const float done = std::hypot(u.x - u.sx, u.y - u.sy);
        u.progress = static_cast<uint16_t>(tot > 0.5f ? std::clamp(done / tot, 0.0f, 1.0f) * 1000.0f : 1000.0f);
        if (sh->kind < sim_->rules()->projectiles.size()) {   // parabola: a félúton a legmagasabb
          const float arc = static_cast<float>(sim_->rules()->projectiles[sh->kind].arc.raw()) / 65536.0f;
          const float t = static_cast<float>(u.progress) / 1000.0f;
          u.lift = arc * tot * 4.0f * t * (1.0f - t);
        }
      } else if (const auto* b = w.get<sim::Building>(e)) {
        u.kind = SnapUnit::kBuilding;
        u.def = b->def;
        u.tx = b->tx; u.ty = b->ty; u.w = b->w; u.h = b->h;
        u.complete = b->complete != 0;
        const int64_t total = int64_t{std::max(1, sim_->rules()->defs[b->def].time_ticks)} * 1000;
        u.progress = static_cast<uint16_t>(std::min<int64_t>(1000, int64_t{b->progress} * 1000 / total));
      } else if (const auto* n = w.get<sim::ResourceNode>(e)) {
        u.kind = SnapUnit::kResource;
        u.tx = n->tx; u.ty = n->ty; u.w = u.h = n->size;
        u.res = n->kind;
        u.amount = n->amount;
      } else if (const auto* un = w.get<sim::Unit>(e)) {
        u.def = un->def;
        if (const auto* k = w.get<sim::Worker>(e)) {
          if (k->inside) return;   // bent épít: nem látszik
          u.amount = k->carry;
          u.res = k->carry_kind;
        }
      }
    }
    s->units.push_back(u);
  });
  if (match) {
    for (int i = 0; i <= sim::Sim::kMaxPlayers; ++i) {
      const sim::PlayerState& p = sim_->player(i);
      SnapPlayer& o = s->players[static_cast<size_t>(i)];
      o.active = p.active;
      for (int r = 0; r < sim::kResources; ++r) o.res[static_cast<size_t>(r)] = p.resource(r);
      o.supply_cap = p.supply_cap;
      o.supply_used = p.supply_used;
    }
  }
  std::sort(s->units.begin(), s->units.end(), [](const SnapUnit& a, const SnapUnit& b) { return a.id < b.id; });
  s->fog = fog_;
  last_ = s;
  return last_;
}

std::shared_ptr<const Snapshot> Session::step(const std::vector<sim::Command>& commands, std::vector<FxEvent>& fx) {
  const auto before = snapshot();
  for (sim::Command c : commands) {
    c.tick = sim_->tick();
    sim_->queue(c);   // a parancssoron át (mint a hálózati játékban és a visszajátszásban)
  }
  sim_->step();
  if (sim_->tick() % 5 == 0) update_fog();
  const auto after = snapshot();
  // látványesemények azonosító szerinti sorrendben: eltűnt egység → halál (az előző helyén), csökkent életerő → találat
  size_t j = 0;
  for (const SnapUnit& a : before->units) {
    if (a.kind != SnapUnit::kUnit) continue;   // a kivágott fa és a kimerült lelőhely nem „hal meg”
    while (j < after->units.size() && after->units[j].id < a.id) ++j;
    if (j >= after->units.size() || after->units[j].id != a.id) {
      fx.push_back({a.x, a.y, true});
      continue;
    }
    const SnapUnit& b = after->units[j];
    if (b.hp < a.hp) fx.push_back({b.x, b.y - 6.0f, false});
  }
  return after;
}

// ---------------------------------------------------------------------------------------------------------------

double SimRunner::now() const {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - kEpoch).count();
}

void SimRunner::run_one() {
  std::vector<sim::Command> cmds;
  {
    std::lock_guard<std::mutex> lock(mu_);
    cmds.swap(pending_);
    if (!cur_) cur_ = s_.snapshot();
  }
  fx_scratch_.clear();
  const auto t0 = std::chrono::steady_clock::now();
  auto snap = s_.step(cmds, fx_scratch_);
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  auto pub = std::make_shared<Snapshot>(*snap);   // a közzététel ideje a futtató órája szerint
  pub->published = now();
  std::lock_guard<std::mutex> lock(mu_);
  prev_ = cur_;
  cur_ = std::move(pub);
  if (fx_.size() < 16384) fx_.insert(fx_.end(), fx_scratch_.begin(), fx_scratch_.end());
  if (tick_ms_.size() < 1000000) tick_ms_.push_back(ms);
}

void SimRunner::step_sync() { run_one(); }

void SimRunner::loop() {
  using clock = std::chrono::steady_clock;
  const auto period = std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(kTickSeconds));
  auto next = clock::now() + period;
  while (!stop_.load()) {
    run_one();
    const auto t = clock::now();
    if (t < next) {
      std::this_thread::sleep_until(next);
      next += period;
    } else {
      next = t + period;   // lemaradás: nem pótol (a szimuláció lassul, a kép nem akad)
    }
  }
}

void SimRunner::start() {
  if (thread_.joinable()) return;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!cur_) {
      auto s = std::make_shared<Snapshot>(*s_.snapshot());
      s->published = now();
      cur_ = s;
    }
  }
  stop_ = false;
  thread_ = std::thread([this] { loop(); });
}

void SimRunner::stop() {
  stop_ = true;
  if (thread_.joinable()) thread_.join();
}

void SimRunner::post(sim::Command c) {
  std::lock_guard<std::mutex> lock(mu_);
  pending_.push_back(std::move(c));
}

void SimRunner::latest(std::shared_ptr<const Snapshot>* prev, std::shared_ptr<const Snapshot>* cur) {
  std::lock_guard<std::mutex> lock(mu_);
  if (!cur_) cur_ = s_.snapshot();   // szinkron mód, még egy tick sem futott
  *cur = cur_;
  *prev = prev_ ? prev_ : cur_;
}

void SimRunner::take_fx(std::vector<FxEvent>& out) {
  std::lock_guard<std::mutex> lock(mu_);
  out.insert(out.end(), fx_.begin(), fx_.end());
  fx_.clear();
}

void SimRunner::take_tick_ms(std::vector<double>& out) {
  std::lock_guard<std::mutex> lock(mu_);
  out.insert(out.end(), tick_ms_.begin(), tick_ms_.end());
  tick_ms_.clear();
}

}  // namespace totu::client
