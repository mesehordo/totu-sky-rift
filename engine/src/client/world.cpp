// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/world.hpp"

#include "client/sigil.hpp"

#include <algorithm>
#include <cmath>

namespace totu::client {

using core::Fixed;

namespace {

Fixed to_fixed(float px) { return Fixed::from_raw(static_cast<int32_t>(px / World::kTilePx * 65536.0f)); }

// bemutató: a játékosok népe (a sprite-lapok előtagja)
const char* kFactions[8] = {"elf_", "cro_", "urs_", "imp_", "wul_", "huk_", "oon_", "dsz_"};

}  // namespace

void World::update() {
  std::shared_ptr<const Snapshot> prev, cur;
  runner_.latest(&prev, &cur);
  const size_t fx_before = fx_.size();
  runner_.take_fx(fx_);
  for (size_t k = fx_before; k < fx_.size() && cur; ++k) {   // halál: a régi `blood` (nagy egységnél `explosion`) képe
    if (fx_[k].death) vfx_.push_back({fx_[k].big ? "explosion" : "blood", fx_[k].x, fx_[k].y, cur->tick, 0});
  }
  if (cur == cur_ && prev == prev_) return;
  const bool new_tick = !cur_ || cur->tick != cur_->tick;
  prev_ = std::move(prev);
  cur_ = std::move(cur);
  if (new_tick) spawn_vfx();
  if (!new_tick && !vis_.empty()) return;
  // a látványállapot átvétele azonosító szerint (mindkét lista rendezett): irány és mozgás a megtett útból
  std::vector<Visual> next;
  next.reserve(cur_->units.size());
  size_t i = 0, j = 0;
  for (const SnapUnit& u : cur_->units) {
    while (i < vis_.size() && vis_[i].id < u.id) ++i;
    while (j < prev_->units.size() && prev_->units[j].id < u.id) ++j;
    Visual v = i < vis_.size() && vis_[i].id == u.id ? vis_[i] : Visual{};
    if (v.id != u.id) {   // új egység: a kezdő irány a pálya oldala szerint (a bemutató seregei egymás felé néznek)
      v.id = u.id;
      v.dir = u.owner % 2 == 1 ? 0 : 4;
      v.sheet = -2;
    }
    const bool had = j < prev_->units.size() && prev_->units[j].id == u.id;
    v.px = had ? prev_->units[j].x : u.x;
    v.py = had ? prev_->units[j].y : u.y;
    const float dx = u.x - v.px, dy = u.y - v.py;
    v.moving = dx * dx + dy * dy > 0.01f;
    if (v.moving) {
      const float a = std::atan2(dy, dx);   // a képernyőn lefelé nő az y: K = 0, DK = 1, D = 2 …
      v.dir = (static_cast<int>(std::lround(a / (3.14159265f / 4.0f))) + 8) % 8;
    } else if (u.has_face && (u.face_x - u.x) * (u.face_x - u.x) + (u.face_y - u.y) * (u.face_y - u.y) > 1.0f) {
      const float a = std::atan2(u.face_y - u.y, u.face_x - u.x);   // állva a célpontja felé néz (támadás, varázslás)
      v.dir = (static_cast<int>(std::lround(a / (3.14159265f / 4.0f))) + 8) % 8;
    }
    next.push_back(v);
  }
  vis_.swap(next);
  selected_.erase(std::remove_if(selected_.begin(), selected_.end(),
                                 [&](sim::EntityId e) { const auto* u = cur_->find(e); return !u || u->hp <= 0; }),
                  selected_.end());
}

float World::live_alpha() const {
  if (!cur_) return 0;
  return static_cast<float>(std::clamp((runner_.now() - cur_->published) / runner_.tick_seconds(), 0.0, 1.0));
}

const std::vector<uint8_t>& World::fog(bool* changed) {
  static const std::vector<uint8_t> kEmpty;
  if (!cur_ || !cur_->fog) {
    if (changed) *changed = false;
    return kEmpty;
  }
  if (changed) *changed = cur_->fog != fog_seen_;
  fog_seen_ = cur_->fog;
  return *cur_->fog;
}

void World::lights(std::vector<render::Light>& out, float alpha) const {
  if (!cur_) return;
  for (size_t i = 0; i < cur_->units.size(); ++i) {
    const SnapUnit& u = cur_->units[i];
    if (u.id % 6 != 0 || u.kind != SnapUnit::kUnit) continue;
    const Visual& v = vis_[i];
    render::Light l;
    l.x = v.px + (u.x - v.px) * alpha;
    l.y = v.py + (u.y - v.py) * alpha - 20.0f;   // a fáklya a fej magasságában
    l.radius = 90.0f;
    l.r = 255; l.g = 190; l.b = 110;
    out.push_back(l);
  }
}

void World::select_box(float x0, float y0, float x1, float y1) {
  if (x0 > x1) std::swap(x0, x1);
  if (y0 > y1) std::swap(y0, y1);
  selected_.clear();
  selected_building_ = 0;
  last_move_ = false;
  if (!cur_) return;
  for (const SnapUnit& u : cur_->units) {
    if (u.owner != session_.local_player() || u.hp <= 0 || u.kind != SnapUnit::kUnit) continue;
    if (u.x >= x0 - 8 && u.x <= x1 + 8 && u.y >= y0 - 8 && u.y <= y1 + 8) selected_.push_back(u.id);
  }
  if (!selected_.empty()) return;
  for (const SnapUnit& u : cur_->units) {   // egység nélkül: a pont alatti saját épület
    if (u.owner != session_.local_player() || u.hp <= 0 || u.kind != SnapUnit::kBuilding) continue;
    const float bx = static_cast<float>(u.tx) * kTilePx, by = static_cast<float>(u.ty) * kTilePx;
    if (x0 >= bx && x0 < bx + static_cast<float>(u.w) * kTilePx && y0 >= by && y0 < by + static_cast<float>(u.h) * kTilePx) {
      selected_building_ = u.id;
    }
  }
}

void World::set_selection(const std::vector<sim::EntityId>& ids) {
  selected_.clear();
  selected_building_ = 0;
  last_move_ = false;
  if (!cur_) return;
  for (const sim::EntityId id : ids) {
    const SnapUnit* u = cur_->find(id);
    if (!u) continue;
    if (u->kind == SnapUnit::kUnit) selected_.push_back(id);
    else if (u->kind == SnapUnit::kBuilding && !selected_building_) selected_building_ = id;
  }
  if (!selected_.empty()) selected_building_ = 0;
}

bool World::ability_hotkey(char key, float wx, float wy) {
  const sim::Rules* r = session_.rules();
  if (!r || selected_.empty() || !cur_) return false;
  // a kijelöltek sablonjainak képességei közül az első, amelynek ez a gyorsbillentyűje
  for (const sim::EntityId id : selected_) {
    const SnapUnit* u = cur_->find(id);
    if (!u || u->def >= r->defs.size()) continue;
    for (const uint16_t ab : r->defs[u->def].abilities) {
      if (r->abilities[ab].hotkey != key) continue;
      sim::Command c;
      c.kind = sim::Command::Kind::Ability;
      c.units = selected_;
      c.def = r->abilities[ab].id;
      c.x = to_fixed(wx);
      c.y = to_fixed(wy);
      c.target = enemy_at(wx, wy);
      post(c);
      return true;
    }
  }
  return false;
}

void World::post(sim::Command c) {
  using K = sim::Command::Kind;
  const bool unit_order = c.kind == K::Move || c.kind == K::GroupMove || c.kind == K::Gather || c.kind == K::Stop ||
                          c.kind == K::Build || c.kind == K::Attack || c.kind == K::AttackMove || c.kind == K::Hold ||
                          c.kind == K::Ability || c.kind == K::Garrison || c.kind == K::Repair || c.kind == K::Patrol;
  if (queue_mode && unit_order) c.flag = static_cast<uint8_t>(c.flag | sim::Command::kQueue);
  runner_.post(std::move(c));
}

void World::order_patrol(float wx, float wy) {
  if (selected_.empty()) return;
  sim::Command c;
  c.kind = sim::Command::Kind::Patrol;
  c.units = selected_;
  c.x = to_fixed(wx);
  c.y = to_fixed(wy);
  post(c);
}

void World::order_line(float wx0, float wy0, float wx1, float wy1) {
  if (selected_.empty() || !cur_) return;
  // a kijelöltek a vonal irányára vetített helyük szerint rendezve (kevesebb keresztezés), majd egyenletesen elosztva
  const float dx = wx1 - wx0, dy = wy1 - wy0;
  std::vector<std::pair<float, sim::EntityId>> order;
  for (const sim::EntityId id : selected_) {
    const SnapUnit* u = cur_->find(id);
    if (u) order.emplace_back(u->x * dx + u->y * dy, id);
  }
  std::sort(order.begin(), order.end());
  const size_t n = order.size();
  for (size_t i = 0; i < n; ++i) {
    const float t = n == 1 ? 0.5f : static_cast<float>(i) / static_cast<float>(n - 1);
    sim::Command c;
    c.kind = sim::Command::Kind::Move;
    c.units = {order[i].second};
    c.x = to_fixed(wx0 + dx * t);
    c.y = to_fixed(wy0 + dy * t);
    post(c);
  }
  note_move(wx0 + dx / 2, wy0 + dy / 2);
}

bool World::selection_center(float& wx, float& wy) const {
  if (!cur_ || selected_.empty()) return false;
  float sx = 0, sy = 0;
  int n = 0;
  for (const sim::EntityId id : selected_) {
    if (const SnapUnit* u = cur_->find(id)) {
      sx += u->x;
      sy += u->y;
      ++n;
    }
  }
  if (n == 0) return false;
  wx = sx / static_cast<float>(n);
  wy = sy / static_cast<float>(n);
  return true;
}

bool World::order_rally(float wx, float wy) {
  if (!selected_.empty() || !selected_building_ || !cur_) return false;
  const SnapUnit* b = cur_->find(selected_building_);
  const sim::Rules* r = session_.rules();
  if (!b || b->owner != session_.local_player() || !r || b->def >= r->defs.size() || r->defs[b->def].produces.empty()) return false;
  sim::Command c;
  c.kind = sim::Command::Kind::Rally;
  c.units = {selected_building_};
  c.x = to_fixed(wx);
  c.y = to_fixed(wy);
  post(c);
  return true;
}

void World::order_move(float wx, float wy, sim::Formation f) {
  if (selected_.empty()) return;
  sim::Command c;
  c.kind = sim::Command::Kind::GroupMove;
  c.units = selected_;
  c.x = to_fixed(wx);
  c.y = to_fixed(wy);
  c.formation = static_cast<uint8_t>(f);
  post(c);
  note_move(wx, wy);   // a futtató során át a következő tickben (mint a hálózati játékban és a visszajátszásban)
}

sim::EntityId World::enemy_at(float wx, float wy) const {
  if (!cur_) return 0;
  sim::EntityId best = 0;
  float best_d = 1e9f;
  for (const SnapUnit& u : cur_->units) {
    if (u.owner == session_.local_player() || u.owner == 0 || u.hp <= 0 || u.kind == SnapUnit::kShot || u.kind == SnapUnit::kResource) continue;
    float d;
    if (u.kind == SnapUnit::kBuilding) {
      const float x0 = static_cast<float>(u.tx) * kTilePx, y0 = static_cast<float>(u.ty) * kTilePx;
      const float x1 = x0 + static_cast<float>(u.w) * kTilePx, y1 = y0 + static_cast<float>(u.h) * kTilePx;
      if (wx < x0 || wx > x1 || wy < y0 - 24 || wy > y1) continue;
      d = 0;
    } else {
      d = std::hypot(u.x - wx, (u.y - 12.0f) - wy);   // a figura törzse a talp fölött
      if (d > 18.0f) continue;
    }
    if (d < best_d) {
      best_d = d;
      best = u.id;
    }
  }
  return best;
}

sim::EntityId World::garrison_at(float wx, float wy) const {
  if (!cur_) return 0;
  for (const SnapUnit& u : cur_->units) {
    if (u.kind != SnapUnit::kBuilding || u.owner != session_.local_player() || u.slots == 0 || !u.complete || u.hp <= 0) continue;
    const float x0 = static_cast<float>(u.tx) * kTilePx, y0 = static_cast<float>(u.ty) * kTilePx;
    const float x1 = x0 + static_cast<float>(u.w) * kTilePx, y1 = y0 + static_cast<float>(u.h) * kTilePx;
    if (wx >= x0 && wx < x1 && wy >= y0 && wy < y1) return u.id;
  }
  return 0;
}

void World::order_garrison(sim::EntityId building) {
  if (selected_.empty() || !building) return;
  sim::Command c;
  c.kind = sim::Command::Kind::Garrison;
  c.units = selected_;
  c.target = building;
  post(c);
}

void World::order_unload(float wx, float wy) {
  const sim::EntityId b = garrison_at(wx, wy);
  if (!b) return;
  sim::Command c;
  c.kind = sim::Command::Kind::Unload;
  c.target = b;
  post(c);
}

void World::order_attack(sim::EntityId target) {
  if (selected_.empty() || !target) return;
  sim::Command c;
  c.kind = sim::Command::Kind::Attack;
  c.units = selected_;
  c.target = target;
  post(c);
}

void World::order_attack_move(float wx, float wy) {
  if (selected_.empty()) return;
  sim::Command c;
  c.kind = sim::Command::Kind::AttackMove;
  c.units = selected_;
  c.x = to_fixed(wx);
  c.y = to_fixed(wy);
  post(c);
  note_move(wx, wy);
}

int World::hovered(float alpha, float hx, float hy, float* out_cx, float* out_y, float* out_w) const {
  if (!cur_) return -1;
  const sim::Rules* rules = session_.rules();
  // az egér alatti entitás: előbb egység (a figura doboza), aztán épület (a lábnyom, fölfelé a kép magasságáig)
  int best = -1;
  float best_d = 1e9f;
  for (size_t i = 0; i < cur_->units.size(); ++i) {
    const SnapUnit& u = cur_->units[i];
    if (u.kind != SnapUnit::kUnit || u.max_hp <= 0 || u.hp <= 0) continue;
    const Visual& v = vis_[i];
    const float x = v.px + (u.x - v.px) * alpha, y = v.py + (u.y - v.py) * alpha;
    const float rad = rules && u.def < rules->defs.size() ? static_cast<float>(rules->defs[u.def].radius.to_double()) * kTilePx : 10.0f;
    const float half = std::max(10.0f, rad * 1.1f);
    if (hx < x - half || hx > x + half || hy < y - std::max(36.0f, rad * 2.6f) || hy > y + 4) continue;
    const float d = std::fabs(hx - x) + std::fabs(hy - (y - rad));
    if (d < best_d) { best_d = d; best = static_cast<int>(i); }
  }
  if (best < 0) {
    for (size_t i = 0; i < cur_->units.size(); ++i) {
      const SnapUnit& u = cur_->units[i];
      if (u.kind != SnapUnit::kBuilding || u.max_hp <= 0 || u.hp <= 0) continue;
      const float bx = static_cast<float>(u.tx) * kTilePx, by = static_cast<float>(u.ty) * kTilePx;
      const float bw = static_cast<float>(u.w) * kTilePx, bh = static_cast<float>(u.h) * kTilePx;
      if (hx >= bx && hx < bx + bw && hy >= by - bh * 0.5f && hy < by + bh) best = static_cast<int>(i);
    }
  }
  if (best < 0) return -1;
  const SnapUnit& u = cur_->units[static_cast<size_t>(best)];
  float cx, y, w;
  if (u.kind == SnapUnit::kBuilding) {
    w = std::max(20.0f, static_cast<float>(u.w) * kTilePx * 0.7f);
    cx = (static_cast<float>(u.tx) + static_cast<float>(u.w) / 2) * kTilePx;
    y = static_cast<float>(u.ty) * kTilePx - 30.0f;   // a lábnyom fölött, a helyőrség jelzői fölött
  } else {
    const Visual& v = vis_[static_cast<size_t>(best)];
    const float rad = rules && u.def < rules->defs.size() ? static_cast<float>(rules->defs[u.def].radius.to_double()) * kTilePx : 10.0f;
    w = std::max(20.0f, rad * 2.5f);
    cx = v.px + (u.x - v.px) * alpha;
    y = v.py + (u.y - v.py) * alpha - std::max(40.0f, rad * 2.8f);
  }
  if (out_cx) *out_cx = cx;
  if (out_y) *out_y = y;
  if (out_w) *out_w = w;
  return best;
}

void World::draw_bars(render::SpriteBatch& batch, const render::Texture& white, float alpha, float hx, float hy) const {
  float cx = 0, y = 0, w = 0;
  const int best = hovered(alpha, hx, hy, &cx, &y, &w);
  if (best < 0) return;
  const SnapUnit& u = cur_->units[static_cast<size_t>(best)];
  const float x0 = cx - w / 2;
  auto bar = [&](float yy, float hh, float frac, uint8_t r, uint8_t g, uint8_t b) {
    render::SpriteInstance bg;
    bg.x = x0; bg.y = yy; bg.w = w; bg.h = hh;
    bg.tint[0] = 10; bg.tint[1] = 10; bg.tint[2] = 10;
    batch.draw(white, nullptr, bg);
    render::SpriteInstance fg = bg;
    fg.w = w * std::clamp(frac, 0.0f, 1.0f);
    fg.tint[0] = r; fg.tint[1] = g; fg.tint[2] = b;
    batch.draw(white, nullptr, fg);
  };
  if (u.shield_max > 0) bar(y - 4, 3, static_cast<float>(u.shield) / static_cast<float>(u.shield_max), 90, 170, 255);
  const float f = static_cast<float>(u.hp) / static_cast<float>(u.max_hp);
  if (f > 0.6f) bar(y, 4, f, 80, 220, 80);
  else if (f > 0.3f) bar(y, 4, f, 230, 200, 60);
  else bar(y, 4, f, 230, 60, 50);
  if (u.energy_max > 0 && u.kind == SnapUnit::kUnit && u.owner == session_.local_player()) {
    bar(y + 5, 3, static_cast<float>(u.energy) / static_cast<float>(u.energy_max), 200, 110, 255);
  }
}

std::vector<World::Label> World::labels(float alpha, double time, float hx, float hy) const {
  std::vector<Label> out;
  if (!cur_) return out;
  float cx = 0, y = 0, w = 0;
  const int best = hovered(alpha, hx, hy, &cx, &y, &w);
  if (best >= 0) {   // idézett egység: a hátralévő idő a sávok alatt (a régi: „N mp”)
    const SnapUnit& u = cur_->units[static_cast<size_t>(best)];
    if (u.kind == SnapUnit::kUnit && u.life_until > cur_->tick) {
      const bool energy = u.energy_max > 0 && u.owner == session_.local_player();
      Label l;
      l.kind = Label::Countdown;
      l.wx = cx;
      l.wy = y + (energy ? 9.0f : 5.0f);
      l.value = static_cast<int>((u.life_until - cur_->tick + 29) / 30);
      out.push_back(std::move(l));
    }
  }
  if (cur_->mission.active) {   // a jelzőfény felirata a kristály fölött
    const float bob = static_cast<float>(std::sin(time * 2.4)) * 3.0f;
    for (const auto& o : cur_->mission.objectives) {
      if (!o.placed || o.state != 1 || o.marker.empty()) continue;
      Label l;
      l.kind = Label::Beacon;
      l.wx = o.x;
      l.wy = o.y - 46.0f + bob - 9.0f - 6.0f;
      l.key = o.marker;
      out.push_back(std::move(l));
    }
  }
  for (const auto& g : cur_->items) {   // az egér alatti tárgy neve
    if (std::hypot(hx - g.x, hy - (g.y - 6.0f)) >= 16.0f) continue;
    Label l;
    l.kind = Label::Item;
    l.wx = g.x;
    l.wy = g.y - 38.0f + 16.0f;
    l.value = g.rarity;
    l.key = g.item;
    out.push_back(std::move(l));
    break;
  }
  return out;
}

void World::draw_ground_marks(render::SpriteBatch& batch, float alpha, double time) const {
  const sim::Rules* rules = session_.rules();
  if (!cur_ || !rules || !marks_.hero_tint) return;
  for (size_t i = 0; i < cur_->units.size(); ++i) {
    const SnapUnit& u = cur_->units[i];
    if (u.kind != SnapUnit::kUnit || u.hp <= 0 || u.def >= rules->defs.size() || rules->defs[u.def].flying) continue;
    const Visual& v = vis_[i];
    const float x = v.px + (u.x - v.px) * alpha, y = v.py + (u.y - v.py) * alpha;
    const float rad = static_cast<float>(rules->defs[u.def].radius.to_double()) * kTilePx;
    if (u.hero_level > 0) {   // a hős talajjele (a régi: max(30, 3 × sugár), lassan pulzálva)
      const float w = std::max(30.0f, rad * 3.0f), k = w / 48.0f;
      const auto col = team_color(u.owner);
      const float pulse = 0.85f + 0.15f * static_cast<float>(std::sin(time * 2.4 + static_cast<double>(u.id)));
      render::SpriteInstance s;
      s.w = 56 * k;
      s.h = 34 * k;
      s.x = x - s.w / 2;
      s.y = y + rad * 0.35f - s.h / 2;
      s.tint[3] = static_cast<uint8_t>(255 * pulse);
      if (marks_.hero_fixed) batch.draw(*marks_.hero_fixed, nullptr, s);
      s.tint[0] = col[0]; s.tint[1] = col[1]; s.tint[2] = col[2];
      batch.draw(*marks_.hero_tint, nullptr, s);
    }
    const float mw = std::max(22.0f, rad * 2.7f), mk = mw / 40.0f;
    float extra = 0;
    auto mark = [&](const render::Texture* t, int layer) {
      if (!t) return;
      render::SpriteInstance s;
      s.w = (54 + extra) * mk;
      s.h = 38 * mk;
      s.x = x - s.w / 2;
      s.y = y + 2 - s.h / 2;
      s.layer[0] = static_cast<uint8_t>(layer);
      batch.draw(*t, nullptr, s);
      extra += 8;
    };
    if (!u.toggles.empty()) mark(marks_.chant, static_cast<int>(time * 8.0) & 7);
    if (u.hidden) mark(marks_.hidden, 0);
  }
}

void World::draw_hero_badges(render::SpriteBatch& batch, float alpha, double time) const {
  const sim::Rules* rules = session_.rules();
  if (!cur_ || !rules || !marks_.badge_frame || !marks_.badge_symbol) return;
  for (size_t i = 0; i < cur_->units.size(); ++i) {
    const SnapUnit& u = cur_->units[i];
    if (u.kind != SnapUnit::kUnit || u.hp <= 0 || u.hero_level == 0 || u.def >= rules->defs.size()) continue;
    const Visual& v = vis_[i];
    const float x = v.px + (u.x - v.px) * alpha, y = v.py + (u.y - v.py) * alpha;
    const float rad = static_cast<float>(rules->defs[u.def].radius.to_double()) * kTilePx;
    const float top = y - std::max(48.0f, rad * 3.4f) - 6;   // a fej és a csík fölött
    const float bob = static_cast<float>(std::sin(time * 2.0 + static_cast<double>(u.id))) * 1.5f;
    const auto col = team_color(u.owner);
    render::SpriteInstance s;
    s.w = s.h = 21;
    s.x = x - 10.5f;
    s.y = top - 21 - 3 + bob;
    batch.draw(*marks_.badge_frame, nullptr, s);
    s.tint[0] = col[0]; s.tint[1] = col[1]; s.tint[2] = col[2];
    batch.draw(*marks_.badge_symbol, nullptr, s);
  }
}

void World::draw_objective_rings(render::SpriteBatch& batch, double time) const {
  if (!cur_ || !obj_ring_ || !cur_->mission.active) return;
  const float pulse = 0.85f + 0.15f * static_cast<float>(std::sin(time * 3.0));
  for (const auto& o : cur_->mission.objectives) {
    if (!o.placed || o.state != 1) continue;
    const float r = std::max(18.0f, o.r), k = r / 64.0f;
    render::SpriteInstance s;
    s.w = (2 * 64 + 20) * k;
    s.h = (64 * 1.2f + 20) * k;
    s.x = o.x - s.w / 2;
    s.y = o.y - s.h / 2;
    s.layer[0] = static_cast<uint8_t>(static_cast<int>(time * 6.0 / 3.0) % 12);
    s.tint[3] = static_cast<uint8_t>(255 * pulse);
    batch.draw(*obj_ring_, nullptr, s);
  }
}

void World::draw_objective_beacons(render::SpriteBatch& batch, double time) const {
  if (!cur_ || !obj_beacon_ || !cur_->mission.active) return;
  const float bob = static_cast<float>(std::sin(time * 2.4)) * 3.0f;
  for (const auto& o : cur_->mission.objectives) {
    if (!o.placed || o.state != 1) continue;
    render::SpriteInstance s;   // a kristály közepe 46 képponttal a cél fölött (a régi szerint)
    s.w = 16;
    s.h = 64;
    s.x = o.x - 8;
    s.y = o.y - 46 + bob - 12;
    batch.draw(*obj_beacon_, nullptr, s);
  }
}

void World::draw_ground_items(render::SpriteBatch& batch, const render::Texture& white, double time) const {
  // a régi `draw_ground_items`: árnyék, pulzáló fényudvar a ritkaság színében, fölötte lebegő ikon
  if (!cur_) return;
  static const uint8_t kRar[3][3] = {{230, 220, 190}, {110, 170, 255}, {255, 190, 70}};
  for (const auto& g : cur_->items) {
    const uint8_t* col = kRar[std::min<uint8_t>(g.rarity, 2)];
    const float bob = static_cast<float>(std::sin(time * 3.0 + static_cast<double>(g.id))) * 2.0f;
    render::SpriteInstance sh;   // árnyék
    sh.x = g.x - 9;
    sh.y = g.y + 4;
    sh.w = 18;
    sh.h = 6;
    sh.tint[0] = sh.tint[1] = sh.tint[2] = 0;
    sh.tint[3] = 150;
    if (item_glow_) batch.draw(*item_glow_, nullptr, sh);
    if (item_glow_) {
      render::SpriteInstance gl;
      gl.x = g.x - 18;
      gl.y = g.y - 22 + bob;
      gl.w = gl.h = 36;
      gl.tint[0] = col[0]; gl.tint[1] = col[1]; gl.tint[2] = col[2];
      gl.tint[3] = static_cast<uint8_t>(60 + 40 * std::sin(time * 4.0 + static_cast<double>(g.id)));
      batch.draw(*item_glow_, nullptr, gl);
    }
    render::SpriteInstance ic;
    const auto it = item_icons_.find(g.item);
    if (it != item_icons_.end() && it->second) {
      ic.x = g.x - 11;
      ic.y = g.y - 15 + bob;
      ic.w = ic.h = 22;
      batch.draw(*it->second, nullptr, ic);
    } else {   // ikon nélkül: kis négyzet a ritkaság színében
      ic.x = g.x - 6;
      ic.y = g.y - 10 + bob;
      ic.w = ic.h = 12;
      ic.tint[0] = col[0]; ic.tint[1] = col[1]; ic.tint[2] = col[2];
      batch.draw(white, nullptr, ic);
    }
  }
}

void World::draw_ambient(render::SpriteBatch& batch, const Camera& cam, int sw, int sh, double time) const {
  if (!amb_glow_ || !amb_leaf_ || !amb_dot_) return;
  if (amb_.empty()) {   // a régi: 34 szentjánosbogár, 22 levél, 30 pollen, rögzített véletlenből
    uint64_t st = 0x9E3779B97F4A7C15ULL;
    auto rnd = [&]() {
      st ^= st << 13;
      st ^= st >> 7;
      st ^= st << 17;
      return static_cast<float>(st >> 40) / static_cast<float>(1ULL << 24);
    };
    for (const auto& [kind, n] : {std::pair<uint8_t, int>{0, 34}, {1, 22}, {2, 30}}) {
      for (int i = 0; i < n; ++i) amb_.push_back({kind, rnd(), rnd(), rnd(), rnd()});
    }
  }
  const float t = static_cast<float>(std::fmod(time, 10000.0));
  const float M = 60, BW = static_cast<float>(sw) / cam.zoom + 2 * M, BH = static_cast<float>(sh) / cam.zoom + 2 * M;
  auto wrap = [](float v, float m) { return std::fmod(std::fmod(v, m) + m, m); };
  for (const AmbPart& p : amb_) {
    float x, y;
    if (p.kind == 1) {
      x = p.a * 4000 + t * (16 + p.c * 14) + std::sin(t * (1.2f + p.d) + p.c * 6) * 22;
      y = p.b * 4000 + t * (22 + p.d * 14);
    } else {
      x = p.a * 4000 + (p.c - 0.5f) * 14 * t + std::sin(t * (0.7f + p.c) + p.d * 9) * 18;
      y = p.b * 4000 + (p.d - 0.5f) * 10 * t + std::cos(t * (0.9f + p.d) + p.c * 7) * 12;
    }
    const float wx = cam.x + wrap(x - cam.x, BW) - M, wy = cam.y + wrap(y - cam.y, BH) - M;
    if (cur_ && cur_->fog && session_.has_sim()) {   // felfedezetlen helyen nem látszik
      const int tx = static_cast<int>(wx / kTilePx), ty = static_cast<int>(wy / kTilePx);
      const int mw = map().width(), mh = map().height();
      if (tx < 0 || ty < 0 || tx >= mw || ty >= mh) continue;
      const size_t k = static_cast<size_t>(ty) * static_cast<size_t>(mw) + static_cast<size_t>(tx);
      if (k < cur_->fog->size() && (*cur_->fog)[k] == 0) continue;
    }
    render::SpriteInstance s;
    if (p.kind == 0) {   // szentjánosbogár: villog, a gyengébb fázisban kisebb
      const float pulse = 0.5f + 0.5f * std::sin(t * (1.5f + p.c * 2) + p.d * 20);
      if (pulse < 0.25f) continue;
      const float sz = pulse > 0.6f ? 12.0f : 7.0f;
      s.x = wx - sz / 2;
      s.y = wy - sz / 2;
      s.w = s.h = sz;
      s.tint[0] = 255; s.tint[1] = 240; s.tint[2] = 140;
      batch.draw(*amb_glow_, nullptr, s);
    } else if (p.kind == 1) {   // falevél: forog
      static const uint8_t kLeaf[3][3] = {{176, 110, 40}, {196, 150, 52}, {140, 80, 36}};
      const int var = static_cast<int>(p.c * 3) % 3;
      const float ang = std::fmod(t * (90 + p.d * 120) + p.a * 360, 360.0f);
      s.x = wx;
      s.y = wy;
      s.w = s.h = 8;
      s.layer[0] = static_cast<uint8_t>(static_cast<int>(ang / 22.5f) & 7);
      s.tint[0] = kLeaf[var][0]; s.tint[1] = kLeaf[var][1]; s.tint[2] = kLeaf[var][2];
      batch.draw(*amb_leaf_, nullptr, s);
    } else {   // pollen
      s.x = wx;
      s.y = wy;
      s.w = s.h = 2.5f;
      s.tint[0] = 240; s.tint[1] = 240; s.tint[2] = 200;
      batch.draw(*amb_dot_, nullptr, s);
    }
  }
}

void World::draw_doodads(render::SpriteBatch& batch, render::Atlas* atlas, const Camera& cam, int sw, int sh) {
  if (ground_ && session_.has_sim()) {   // a fedőréteg: földutak és talajfoltok (a régi `Scenery`)
    render::SpriteInstance g;
    g.w = static_cast<float>(map().width()) * kTilePx;
    g.h = static_cast<float>(map().height()) * kTilePx;
    batch.draw(*ground_, nullptr, g);
  }
  if (module_ && session_.has_sim()) {   // a népmechanikák: fertőzött talaj, tavacskák (a régi `draw_creep`, `draw_ponds`)
    render::SpriteInstance g;
    g.w = static_cast<float>(map().width()) * kTilePx;
    g.h = static_cast<float>(map().height()) * kTilePx;
    batch.draw(*module_, nullptr, g);
  }
  if (!atlas || !cur_) return;
  const int si = sheet_by_id(atlas, "doodads");
  if (si < 0) return;
  const auto& sheet = atlas->data().sheets[static_cast<size_t>(si)];
  if (!doodads_done_) {
    doodads_done_ = true;
    const map::Map& m = map();
    const auto& tt = session_.terrain().terrains();
    const int W = m.width(), H = m.height();
    // a képlap nevei: virag_sarga 0, virag_piros 1, virag_kek 2, fucsomo 3, kavics 4, gomba 5, csont 6, tusko 7,
    // faronk 8, bokor 9, nad 10, tavirozsa 11, kristaly 12, koponya 13
    enum : uint8_t { kSarga = 0, kPiros, kKek, kFu, kKavics, kGomba, kCsont, kTusko, kRonk, kBokor, kNad, kTavi, kKrist, kKoponya };
    auto kind = [&](int x, int y) {   // 0 egyéb, 1 fű, 2 föld, 3 víz, 4 út
      if (!m.in_bounds(x, y)) return 0;
      const uint16_t t = m.terrain(x, y);
      if (t >= tt.size()) return 0;
      const std::string& id = tt[t].id;
      if (id == "grass" || id == "grass_lush" || id == "grass_dry" || id == "meadow" || id == "forest_floor") return 1;
      if (id == "dirt" || id == "mud" || id == "gravel" || id == "sand") return 2;
      if (id == "shallow_water" || id == "deep_water") return 3;
      if (id == "road" || id == "paved") return 4;
      return 0;
    };
    std::vector<uint8_t> wood(static_cast<size_t>(W) * static_cast<size_t>(H), 0);   // az erdő közelsége (3 csempén belül)
    for (const SnapUnit& u : cur_->units) {
      if (u.kind != SnapUnit::kResource || u.res != sim::kWood) continue;
      const int cx = static_cast<int>(u.x / kTilePx), cy = static_cast<int>(u.y / kTilePx);
      for (int dy = -3; dy <= 3; ++dy) {
        for (int dx = -3; dx <= 3; ++dx) {
          if (m.in_bounds(cx + dx, cy + dy)) wood[static_cast<size_t>(cy + dy) * static_cast<size_t>(W) + static_cast<size_t>(cx + dx)] = 1;
        }
      }
    }
    uint64_t st = 0x2545F4914F6CDD1DULL ^ (static_cast<uint64_t>(W) << 32) ^ static_cast<uint64_t>(H);
    auto rnd = [&]() {
      st ^= st << 13;
      st ^= st >> 7;
      st ^= st << 17;
      return static_cast<float>(st >> 40) / static_cast<float>(1ULL << 24);
    };
    auto hash01 = [](int x, int y) {   // a rét-zaj rácspontjai
      uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u;
      h = (h ^ (h >> 13)) * 1274126177u;
      return static_cast<float>((h ^ (h >> 16)) & 0xffff) / 65535.0f;
    };
    auto meadow = [&](int x, int y) {   // 6 csempés cellák, simítva
      const float fx = static_cast<float>(x) / 6.0f, fy = static_cast<float>(y) / 6.0f;
      const int ix = static_cast<int>(fx), iy = static_cast<int>(fy);
      const float ax = fx - static_cast<float>(ix), ay = fy - static_cast<float>(iy);
      const float sx = ax * ax * (3 - 2 * ax), sy = ay * ay * (3 - 2 * ay);
      const float a = hash01(ix, iy) * (1 - sx) + hash01(ix + 1, iy) * sx, b = hash01(ix, iy + 1) * (1 - sx) + hash01(ix + 1, iy + 1) * sx;
      return a * (1 - sy) + b * sy;
    };
    auto near = [&](int x, int y, int k) {
      return kind(x + 1, y) == k || kind(x - 1, y) == k || kind(x, y + 1) == k || kind(x, y - 1) == k;
    };
    auto pick = [&](std::initializer_list<uint8_t> l) { return *(l.begin() + static_cast<int>(rnd() * static_cast<float>(l.size())) % static_cast<int>(l.size())); };
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        const int k = kind(x, y);
        int f = -1;
        if (k == 3) {
          if (near(x, y, 1) && rnd() < 0.18f) f = kTavi;
        } else if (k == 1 || k == 2 || k == 4) {
          if ((near(x, y, 3)) && rnd() < 0.35f) {
            f = kNad;
          } else if (k == 4) {   // úton csak néhány kavics
            if (rnd() < 0.06f) f = kKavics;
          } else if (k == 1) {
            const float md = meadow(x, y);
            if (wood[static_cast<size_t>(y) * static_cast<size_t>(W) + static_cast<size_t>(x)]) {
              if (rnd() < 0.32f) f = pick({kBokor, kGomba, kTusko, kRonk, kFu, kBokor, kGomba});
            } else {
              const float cl = md > 0.42f ? 0.2f + 2.0f * std::max(0.0f, (md - 0.42f) / 0.3f) : 0.25f;   // a régi `cluster`
              if (rnd() < 0.16f * cl) {
                f = md > 0.5f ? pick({kSarga, kPiros, kKek, kFu}) : pick({kFu, kFu, kSarga, kPiros, kKek, kBokor, kKavics, kGomba, kTusko, kRonk});
              }
            }
          } else {
            if (rnd() < 0.1f) f = pick({kKavics, kKavics, kKavics, kFu, kFu, kTusko, kCsont});
            else if (rnd() < 0.006f) f = kKoponya;
          }
        }
        if (f >= 0) {
          const float jx = std::floor(rnd() * 17.0f) - 8.0f, jy = std::floor(rnd() * 17.0f) - 8.0f;
          doodads_.push_back({static_cast<float>(x) * kTilePx + jx, static_cast<float>(y) * kTilePx + jy, static_cast<uint8_t>(f)});
        }
      }
    }
  }
  const float vx1 = cam.x + static_cast<float>(sw) / cam.zoom, vy1 = cam.y + static_cast<float>(sh) / cam.zoom;
  const uint8_t no_team[4] = {0, 0, 0, 0};
  for (const Doodad& d : doodads_) {
    if (d.x < cam.x - 64 || d.y < cam.y - 64 || d.x > vx1 + 64 || d.y > vy1 + 64) continue;
    atlas->draw(batch, sheet, 0, d.frame, d.x, d.y, 1.0f / sheet.scale, no_team, nullptr, -1.0f, 0);
  }
}

void World::draw_placement_grid(render::SpriteBatch& batch, const render::Texture& white) const {
  if (!cur_ || ghost_def_ == sim::kNoDef) return;
  const auto& pp = cur_->place;
  const std::array<uint8_t, 3> col = {70, 130, 255};   // a tulajdonos kérése: a rács minden csapatnál kék
  auto rect = [&](float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    render::SpriteInstance in;
    in.x = x; in.y = y; in.w = w; in.h = h;
    in.tint[0] = r; in.tint[1] = g; in.tint[2] = b; in.tint[3] = a;
    batch.draw(white, nullptr, in);
  };
  if (pp.grid && pp.def == ghost_def_) {   // a lerakható csempék: halvány kitöltés, a peremük erősebb
    for (int y = 0; y < pp.gh; ++y) {
      for (int x = 0; x < pp.gw; ++x) {
        if (!(*pp.grid)[static_cast<size_t>(y * pp.gw + x)]) continue;
        const float px = static_cast<float>(pp.gx + x) * kTilePx, py = static_cast<float>(pp.gy + y) * kTilePx;
        rect(px, py, kTilePx, kTilePx, col[0], col[1], col[2], 55);
        rect(px, py, kTilePx, 1, col[0], col[1], col[2], 175);
        rect(px, py + kTilePx - 1, kTilePx, 1, col[0], col[1], col[2], 175);
        rect(px, py, 1, kTilePx, col[0], col[1], col[2], 175);
        rect(px + kTilePx - 1, py, 1, kTilePx, col[0], col[1], col[2], 175);
      }
    }
  }
  const sim::Rules* r = session_.rules();
  if (!r || ghost_def_ >= r->defs.size()) return;
  const float bw = static_cast<float>(std::max<int>(1, r->defs[ghost_def_].fw)) * kTilePx;
  const float bh = static_cast<float>(std::max<int>(1, r->defs[ghost_def_].fh)) * kTilePx;
  const float x0 = static_cast<float>(ghost_tx_) * kTilePx, y0 = static_cast<float>(ghost_ty_) * kTilePx;
  const uint8_t cr = ghost_ok_ ? 90 : 255, cg = ghost_ok_ ? 255 : 70, cb = ghost_ok_ ? 110 : 60;   // a régi keret színe
  rect(x0, y0, bw, 2, cr, cg, cb, 255);
  rect(x0, y0 + bh - 2, bw, 2, cr, cg, cb, 255);
  rect(x0, y0, 2, bh, cr, cg, cb, 255);
  rect(x0 + bw - 2, y0, 2, bh, cr, cg, cb, 255);
}

void World::draw_planned(render::SpriteBatch& batch, const render::Texture& white, render::Atlas* atlas) {
  const sim::Rules* r = session_.rules();
  if (!cur_ || !r || cur_->planned.empty()) return;
  auto rect = [&](float x, float y, float w, float h, uint8_t a) {
    render::SpriteInstance in;
    in.x = x; in.y = y; in.w = w; in.h = h;
    in.tint[0] = 70; in.tint[1] = 130; in.tint[2] = 255; in.tint[3] = a;
    batch.draw(white, nullptr, in);
  };
  for (const auto& p : cur_->planned) {
    if (p.def >= r->defs.size()) continue;
    bool built = false;   // ahol már áll (épül) valami, ott nem kell a terv
    for (const SnapUnit& u : cur_->units) {
      if (u.kind == SnapUnit::kBuilding && u.tx == p.tx && u.ty == p.ty) built = true;
    }
    if (built) continue;
    const int fw = std::max<int>(1, r->defs[p.def].fw), fh = std::max<int>(1, r->defs[p.def].fh);
    for (int y = 0; y < fh; ++y) {
      for (int x = 0; x < fw; ++x) {
        const float px = static_cast<float>(p.tx + x) * kTilePx, py = static_cast<float>(p.ty + y) * kTilePx;
        rect(px, py, kTilePx, kTilePx, 55);
        rect(px, py, kTilePx, 1, 175);
        rect(px, py + kTilePx - 1, kTilePx, 1, 175);
        rect(px, py, 1, kTilePx, 175);
        rect(px + kTilePx - 1, py, 1, kTilePx, 175);
      }
    }
    if (atlas) {
      const int s = sheet_by_id(atlas, art_id(*r, p.def));
      if (s >= 0) {
        const auto& gs = atlas->data().sheets[static_cast<size_t>(s)];
        const uint8_t tint[4] = {255, 255, 255, 170};
        const uint8_t no_team[4] = {0, 0, 0, 0};
        atlas->draw(batch, gs, 0, 0, static_cast<float>(p.tx) * kTilePx, static_cast<float>(p.ty) * kTilePx, 1.0f / gs.scale, no_team, tint);
      }
    }
  }
}

void World::draw_carry(render::SpriteBatch& batch, float alpha) const {
  const sim::Rules* rules = session_.rules();
  if (!cur_ || !sack_ || !rules || !rules->game_raw) return;
  const content::Value* resv = rules->game_raw->find("resources");
  for (size_t i = 0; i < cur_->units.size(); ++i) {
    const SnapUnit& u = cur_->units[i];
    if (u.kind != SnapUnit::kUnit || u.amount <= 0 || u.hp <= 0 || u.def >= rules->defs.size() || !rules->defs[u.def].worker) continue;
    std::array<uint8_t, 3> col{200, 200, 200};
    if (const content::Value* rv = resv ? resv->find(sim::resource_id(u.res)) : nullptr) {
      if (const content::Value* c = rv->find("color"); c && c->is_array() && c->items().size() >= 3) {
        for (size_t k = 0; k < 3; ++k) col[k] = static_cast<uint8_t>(std::clamp<long long>(c->items()[k].as_int().value_or(200), 0, 255));
      }
    }
    const Visual& v = vis_[i];
    const float x = v.px + (u.x - v.px) * alpha, y = v.py + (u.y - v.py) * alpha;
    const float rad = static_cast<float>(rules->defs[u.def].radius.to_double()) * kTilePx;
    render::SpriteInstance s;
    s.w = s.h = 12;
    s.x = x + rad * 0.55f - 6;
    s.y = y - rad * 1.25f - 6;
    s.tint[0] = col[0]; s.tint[1] = col[1]; s.tint[2] = col[2];
    batch.draw(*sack_, nullptr, s);
  }
}

std::array<uint8_t, 3> World::team_color(uint8_t owner) const {
  const sim::Rules* r = session_.rules();
  const content::Value* pc = r && r->game_raw ? r->game_raw->find("player_colors") : nullptr;
  if (owner == 0 || !pc || !pc->is_array() || pc->items().size() < owner) return {200, 200, 200};
  const content::Value& c = pc->items()[owner - 1u];
  std::array<uint8_t, 3> out{200, 200, 200};
  if (c.is_array() && c.items().size() >= 3) {
    for (size_t i = 0; i < 3; ++i) out[i] = static_cast<uint8_t>(std::clamp<long long>(c.items()[i].as_int().value_or(200), 0, 255));
  }
  return out;
}

void World::draw_rally_ground(render::SpriteBatch& batch, double time) const {
  if (!cur_ || !rally_.arrows || !rally_.sigil) return;
  const uint8_t me = session_.local_player();
  const std::array<uint8_t, 3> col = team_color(me);
  const std::array<uint8_t, 3> light = {static_cast<uint8_t>(std::min(255, col[0] + 70)), static_cast<uint8_t>(std::min(255, col[1] + 70)),
                                        static_cast<uint8_t>(std::min(255, col[2] + 70))};
  for (const sim::EntityId e : {selected_building_}) {
    const SnapUnit* b = cur_->find(e);
    if (!b || !b->has_rally || b->owner != me) continue;
    const float x0 = (static_cast<float>(b->tx) + static_cast<float>(b->w) / 2) * kTilePx;
    const float y0 = (static_cast<float>(b->ty) + static_cast<float>(b->h) / 2) * kTilePx;
    const float rx = b->rally_x, ry = b->rally_y;
    const float dx = rx - x0, dy = ry - y0, d = std::sqrt(dx * dx + dy * dy);
    if (d <= 14) continue;
    // menetelő nyilak (15 képpontonként, 18 képpont/mp), a régi `_draw_marching_arrows`
    const float ux = dx / d, uy = dy / d;
    const int dir = static_cast<int>(std::lround(std::atan2(uy, ux) / 6.2831853 * 16.0)) & 15;
    for (float k = std::fmod(static_cast<float>(time) * 18.0f, 15.0f); k < d - 14; k += 15.0f) {
      render::SpriteInstance a;
      a.x = x0 + ux * k - 8;
      a.y = y0 + uy * k - 8;
      a.w = a.h = 16;
      a.layer[0] = static_cast<uint8_t>(dir);
      a.tint[0] = light[0]; a.tint[1] = light[1]; a.tint[2] = light[2];
      batch.draw(*rally_.arrows, nullptr, a);
    }
    render::SpriteInstance s;   // kis pecsét a célnál (26 képpontos gyűrű)
    s.w = 38;
    s.h = 27;
    s.x = rx - s.w / 2;
    s.y = ry + 2 - s.h / 2;
    s.tint[0] = col[0]; s.tint[1] = col[1]; s.tint[2] = col[2];
    batch.draw(*rally_.sigil, nullptr, s);
    // a zászló árnyékát az árnyékmenet veti, a nappal együtt forogva (D52)
  }
}

void World::draw_move_indicator(render::SpriteBatch& batch, float alpha, double time) const {
  if (!cur_ || !last_move_ || !rally_.arrows || !rally_.move_sigil) return;
  float sx = 0, sy = 0;
  int n = 0;
  bool going = false;
  for (const sim::EntityId e : selected_) {
    const SnapUnit* u = cur_->find(e);
    if (!u || u->owner != session_.local_player()) continue;
    const Visual& v = vis_[static_cast<size_t>(u - cur_->units.data())];
    sx += v.px + (u->x - v.px) * alpha;
    sy += v.py + (u->y - v.py) * alpha;
    ++n;
    going = going || u->order == SnapUnit::kMove || u->order == SnapUnit::kAttackMove;
  }
  if (n == 0 || !going) return;
  const float x0 = sx / static_cast<float>(n), y0 = sy / static_cast<float>(n);
  const float dx = last_move_x_ - x0, dy = last_move_y_ - y0, d = std::sqrt(dx * dx + dy * dy);
  const uint8_t light[3] = {130, 255, 162};   // a régi (60, 205, 92) + 70
  if (d > 20) {
    const float ux = dx / d, uy = dy / d;
    const int dir = static_cast<int>(std::lround(std::atan2(uy, ux) / 6.2831853 * 16.0)) & 15;
    for (float k = std::fmod(static_cast<float>(time) * 18.0f, 15.0f); k < d - 20; k += 15.0f) {
      render::SpriteInstance a;
      a.x = x0 + ux * k - 8;
      a.y = y0 + uy * k - 8;
      a.w = a.h = 16;
      a.layer[0] = static_cast<uint8_t>(dir);
      a.tint[0] = light[0]; a.tint[1] = light[1]; a.tint[2] = light[2];
      batch.draw(*rally_.arrows, nullptr, a);
    }
  }
  render::SpriteInstance s;
  s.w = 50;
  s.h = 34;
  s.x = last_move_x_ - s.w / 2;
  s.y = last_move_y_ - s.h / 2;
  batch.draw(*rally_.move_sigil, nullptr, s);
}

std::array<uint8_t, 3> World::faction_accent(const std::string& fid) const {
  const sim::Rules* r = session_.rules();
  const sim::FactionDef* f = r ? r->faction(fid) : nullptr;
  const content::Value* fv = f && f->raw ? f->raw->find("Faction") : nullptr;
  const content::Value* v = fv ? fv->find("accent") : nullptr;
  std::array<uint8_t, 3> out{230, 190, 90};
  if (v && v->is_array() && v->items().size() >= 3) {
    for (size_t i = 0; i < 3; ++i) out[i] = static_cast<uint8_t>(std::clamp<long long>(v->items()[i].as_int().value_or(200), 0, 255));
  }
  return out;
}

void World::draw_banners(render::SpriteBatch& batch, double time) const {
  // D51: minden látott, nem semleges épület jobb alsó sarkánál a tulajdonos lobogója – a népe stílusában (címer
  // nélkül), a csapatszínben, a nép kiemelőszínű szegélyével és csúcsdíszével; a fázis épületenként eltolva
  if (!cur_ || !banner_.pole || !banner_.cloth) return;
  for (const SnapUnit& u : cur_->units) {
    if (u.kind != SnapUnit::kBuilding || u.owner == 0 || u.hp <= 0) continue;
    const std::array<uint8_t, 3> team = team_color(u.owner);
    const std::string& fid = cur_->players[std::min<size_t>(u.owner, sim::Sim::kMaxPlayers)].faction;
    const std::array<uint8_t, 3> acc = faction_accent(fid);
    const int style = banner_style(fid);
    const float fx = static_cast<float>(u.tx + u.w) * kTilePx - 10.0f, fy = static_cast<float>(u.ty + u.h) * kTilePx - 6.0f;
    const int ph = (static_cast<int>(time * 5.0 / 6.2831853 * 8.0) + static_cast<int>(u.id * 3)) & 7;
    render::SpriteInstance s;
    s.x = fx - 5;
    s.y = fy - 54;
    s.w = 20;
    s.h = 56;
    s.foot = fy;   // a napszakkal forgó árnyék innen vetül (D52)
    s.layer[0] = static_cast<uint8_t>(style);
    batch.draw(*banner_.pole, nullptr, s);
    render::SpriteInstance c = s;
    c.layer[0] = static_cast<uint8_t>(style * 8 + ph);
    c.tint[0] = team[0]; c.tint[1] = team[1]; c.tint[2] = team[2];
    batch.draw(*banner_.cloth, nullptr, c);
    if (banner_.trim) {
      render::SpriteInstance t = c;
      t.tint[0] = acc[0]; t.tint[1] = acc[1]; t.tint[2] = acc[2];
      batch.draw(*banner_.trim, nullptr, t);
    }
    if (banner_.knob) {
      render::SpriteInstance k = s;
      k.tint[0] = acc[0]; k.tint[1] = acc[1]; k.tint[2] = acc[2];
      batch.draw(*banner_.knob, nullptr, k);
    }
  }
}

namespace {

// a láng helyei csempében a lábnyom bal felső sarkától: a sablon `Visual.fire` pontjai, különben az épület
// azonosítójából számolt, a lábnyomon (és fölötte, a tetőn) elosztott pontok
void fire_spots(const SnapUnit& u, const sim::EntityDef* d, int n, std::vector<std::array<float, 3>>& out) {
  out.clear();
  for (int k = 0; k < n; ++k) {
    float fx, fy;
    if (d && !d->fire_points.empty()) {
      const auto& p = d->fire_points[static_cast<size_t>(k) % d->fire_points.size()];
      fx = static_cast<float>(p[0].raw()) / 65536.0f;
      fy = static_cast<float>(p[1].raw()) / 65536.0f;
      if (static_cast<size_t>(k) >= d->fire_points.size()) fx += (k & 1 ? 0.3f : -0.3f);   // több láng, mint pont: mellé
    } else {
      uint32_t h = static_cast<uint32_t>(u.id) * 2654435761u + static_cast<uint32_t>(k) * 40503u;
      h ^= h >> 15;
      h *= 2246822519u;
      h ^= h >> 13;
      const float a = static_cast<float>(h & 0xffff) / 65535.0f, b = static_cast<float>(h >> 16) / 65535.0f;
      fx = static_cast<float>(u.w) * (0.18f + 0.64f * a);
      fy = static_cast<float>(u.h) * (0.42f + 0.4f * b);   // a kép a lábnyom alsó részén áll
    }
    out.push_back({fx, fy, 0.7f + 0.2f * static_cast<float>((k * 7) % 3) / 2.0f});
  }
}

bool fx_visible(const Snapshot& s, const SnapUnit& u, int mw) {
  if (!s.fog) return true;
  const int cx = u.tx + u.w / 2, cy = u.ty + u.h / 2;
  const size_t k = static_cast<size_t>(cy) * static_cast<size_t>(mw) + static_cast<size_t>(cx);
  return cx >= 0 && cy >= 0 && k < s.fog->size() && (*s.fog)[k] == 255;
}

constexpr int kFlamesPerStage[4] = {0, 1, 3, 5};

}  // namespace

void World::draw_building_fx(render::SpriteBatch& batch, render::Atlas* atlas, double time) {
  if (!cur_) return;
  const sim::Rules* r = session_.rules();
  const float t = static_cast<float>(std::fmod(time, 10000.0));
  const int mw = map().width();
  for (const SnapUnit& u : cur_->units) {
    if (u.kind != SnapUnit::kBuilding || !u.complete || u.hp <= 0 || !fx_visible(*cur_, u, mw)) continue;
    const sim::EntityDef* d = r && u.def < r->defs.size() ? &r->defs[u.def] : nullptr;
    const float ox = static_cast<float>(u.tx) * kTilePx, oy = static_cast<float>(u.ty) * kTilePx;
    if (d) {   // a sablon animációs rétege: kéményfüst és atlaszlapos elemek
      for (size_t i = 0; i < d->overlays.size(); ++i) {
        const auto& o = d->overlays[i];
        const float px = ox + static_cast<float>(o.x.raw()) / 65536.0f * kTilePx, py = oy + static_cast<float>(o.y.raw()) / 65536.0f * kTilePx;
        const float sc = static_cast<float>(o.scale.raw()) / 65536.0f;
        if (o.effect != "smoke" && o.effect != "fire" && atlas) {   // egy atlaszlap animációja (pl. modból)
          const int si = sheet_by_id(atlas, o.effect);
          if (si < 0) continue;
          const auto& s = atlas->data().sheets[static_cast<size_t>(si)];
          int col = 0;
          if (!s.anims.empty()) {
            const auto a = s.anims.find("idle");
            const auto& an = a != s.anims.end() ? a->second : s.anims.begin()->second;
            col = an.start + static_cast<int>(t * std::max(1.0f, static_cast<float>(an.fps))) % std::max(1, an.frames);
          }
          const uint8_t no_team[4] = {0, 0, 0, 0};
          atlas->draw(batch, s, 0, col, px, py, sc / s.scale, no_team, nullptr, 0.0f);
        }
      }
    }
  }
}

void World::draw_building_flames(render::SpriteBatch& batch, double time) const {
  if (!cur_ || !fire_.flame) return;
  const sim::Rules* r = session_.rules();
  const float t = static_cast<float>(std::fmod(time, 10000.0));
  const int mw = map().width();
  const float dim = 1.0f - 0.55f * static_cast<float>(std::clamp(cur_->night, 0, 1000)) / 1000.0f;   // éjjel a füst sötétebb
  // egy füstoszlop: a gomolyok emelkednek, nőnek, a szél kissé elviszi, és elhalványulnak (állapot nélkül, az időből)
  auto smoke = [&](float px, float py, float scale, uint8_t shade, float dens, uint32_t seed) {
    if (!fire_.puff) return;
    constexpr int kN = 7;
    for (int k = 0; k < kN; ++k) {
      float p = t * 0.32f + static_cast<float>(k) / kN + static_cast<float>(seed % 97) * 0.0137f;
      p -= std::floor(p);
      const float sz = (12.0f + 30.0f * p) * scale;
      const float x = px + p * p * 16.0f * scale + std::sin(t * 1.3f + static_cast<float>(k) * 2.1f + static_cast<float>(seed)) * 3.0f * p;
      const float y = py - p * 64.0f * scale;
      const float a = (1.0f - p) * std::min(1.0f, p * 6.0f) * dens;
      if (a <= 0.01f) continue;
      render::SpriteInstance s;
      s.x = x - sz / 2;
      s.y = y - sz / 2;
      s.w = s.h = sz;
      s.tint[0] = s.tint[1] = s.tint[2] = static_cast<uint8_t>(std::lround(shade * dim));
      s.tint[3] = static_cast<uint8_t>(std::lround(std::clamp(a, 0.0f, 1.0f) * 255.0f));
      batch.draw(*fire_.puff, nullptr, s);
    }
  };
  auto flame = [&](float px, float py, float sc, int k, uint32_t seed) {
    const float flick = 1.0f + 0.08f * std::sin(t * 9.0f + static_cast<float>(seed % 31));
    if (fire_.glow) {   // a láng fénye (közel additív)
      render::SpriteInstance g;
      const float gs = 30.0f * sc * flick;
      g.x = px - gs / 2;
      g.y = py - 8.0f * sc - gs / 2;
      g.w = g.h = gs;
      g.tint[0] = 200; g.tint[1] = 80; g.tint[2] = 20;
      batch.draw(*fire_.glow, nullptr, g);
    }
    render::SpriteInstance s;
    s.w = 16.0f * sc;
    s.h = 24.0f * sc * flick;
    s.x = px - s.w / 2;
    s.y = py - s.h;
    s.layer[0] = static_cast<uint8_t>((static_cast<int>(t * 12.0f) + k * 3 + static_cast<int>(seed % 8)) % client::kFlameFrames);
    batch.draw(*fire_.flame, nullptr, s);
  };
  std::vector<std::array<float, 3>> spots;
  for (const SnapUnit& u : cur_->units) {
    if (u.kind != SnapUnit::kBuilding || !u.complete || u.hp <= 0 || !fx_visible(*cur_, u, mw)) continue;
    const sim::EntityDef* d = r && u.def < r->defs.size() ? &r->defs[u.def] : nullptr;
    const float ox = static_cast<float>(u.tx) * kTilePx, oy = static_cast<float>(u.ty) * kTilePx;
    if (d) {   // a sablon állandó lángjai (pl. kohó)
      for (size_t i = 0; i < d->overlays.size(); ++i) {
        const auto& o = d->overlays[i];
        if (o.effect == "smoke") {
          smoke(ox + static_cast<float>(o.x.raw()) / 65536.0f * kTilePx, oy + static_cast<float>(o.y.raw()) / 65536.0f * kTilePx,
                static_cast<float>(o.scale.raw()) / 65536.0f, 225, 0.8f, static_cast<uint32_t>(u.id * 5 + i));
          continue;
        }
        if (o.effect != "fire") continue;
        flame(ox + static_cast<float>(o.x.raw()) / 65536.0f * kTilePx, oy + static_cast<float>(o.y.raw()) / 65536.0f * kTilePx,
              static_cast<float>(o.scale.raw()) / 65536.0f, static_cast<int>(i), static_cast<uint32_t>(u.id + i));
      }
    }
    const int stage = client::burn_stage(u.hp, u.max_hp);
    if (stage == 0) continue;
    if (stage >= 2) {   // az égő épület füstje (50% alatt), a lángok mögött
      fire_spots(u, d, stage == 3 ? 3 : 1, spots);
      for (size_t k = 0; k < spots.size(); ++k) {
        smoke(ox + spots[k][0] * kTilePx, oy + spots[k][1] * kTilePx - 12.0f, stage == 3 ? 1.35f : 1.0f, 150, 1.0f,
              static_cast<uint32_t>(u.id * 11 + k));
      }
    }
    fire_spots(u, d, kFlamesPerStage[stage], spots);
    const float grow = stage == 3 ? 1.35f : stage == 2 ? 1.15f : 1.0f;
    for (size_t k = 0; k < spots.size(); ++k) {
      flame(ox + spots[k][0] * kTilePx, oy + spots[k][1] * kTilePx, spots[k][2] * grow, static_cast<int>(k), static_cast<uint32_t>(u.id * 13 + k));
    }
  }
}

std::string World::element_sheet(uint16_t ability, const char* kind) const {
  const sim::Rules* r = session_.rules();
  std::string el = "arcane";
  if (r && ability < r->abilities.size() && r->abilities[ability].element != sim::Element::None) {
    el = sim::element_name(r->abilities[ability].element);
  }
  return "vfx_" + el + "_" + kind;
}

void World::spawn_vfx() {
  const sim::Rules* r = session_.rules();
  if (!cur_ || !prev_ || !r) return;
  const int64_t now = cur_->tick;
  vfx_.erase(std::remove_if(vfx_.begin(), vfx_.end(), [&](const Vfx& v) { return now - v.t0 > 90; }), vfx_.end());
  for (const SnapUnit& u : cur_->units) {
    if (u.kind != SnapUnit::kUnit) continue;
    if (u.cast_tick == now && u.cast_ability < r->abilities.size()) {   // varázslás (a régi `cast` / `enova`)
      const sim::AbilityDef& a = r->abilities[u.cast_ability];
      if (a.type == sim::AbilityDef::Type::Nova) {
        vfx_.push_back({element_sheet(u.cast_ability, "nova"), u.x, u.y, now, 2.0f * static_cast<float>(a.radius.raw()) / 65536.0f * kTilePx});
      } else if (a.type != sim::AbilityDef::Type::Toggle && a.type != sim::AbilityDef::Type::Aura) {
        vfx_.push_back({element_sheet(u.cast_ability, "cast"), u.x, u.y, now, 0});
      }
    }
    if (u.hero_level > 0) {   // szintlépés
      const SnapUnit* p = prev_->find(u.id);
      if (p && p->hero_level > 0 && u.hero_level > p->hero_level) vfx_.push_back({"levelup", u.x, u.y, now, 0});
    }
  }
  for (const SnapUnit& p : prev_->units) {   // a varázslövedék becsapódása: az előző képben még repült, most nincs
    if (p.kind != SnapUnit::kShot || p.spell >= r->abilities.size() || cur_->find(p.id)) continue;
    const float splash = static_cast<float>(r->abilities[p.spell].splash.raw()) / 65536.0f * kTilePx;
    vfx_.push_back({element_sheet(p.spell, "impact"), p.x, p.y - p.lift, now, splash > 4.0f ? splash * 2.0f : 0.0f});
  }
}

void World::draw_vfx(render::SpriteBatch& batch, render::Atlas* atlas, float alpha) {
  if (!cur_ || !atlas) return;
  const uint8_t no_team[4] = {0, 0, 0, 0};
  for (const Vfx& v : vfx_) {
    const int si = sheet_by_id(atlas, v.sheet);
    if (si < 0) continue;
    const auto& s = atlas->data().sheets[static_cast<size_t>(si)];
    if (s.anims.empty()) continue;
    const auto it = s.anims.find("play");
    const auto& an = it != s.anims.end() ? it->second : s.anims.begin()->second;
    const double el = (static_cast<double>(cur_->tick - v.t0) + alpha) / 30.0;
    const int frame = static_cast<int>(el * std::max(1, an.fps));
    if (el < 0 || frame >= an.frames) continue;   // egyszer lejátszva
    const float base_w = static_cast<float>(s.frame_w) / s.scale;
    const float k = v.width > 0 && base_w > 0 ? v.width / base_w : 1.0f;
    atlas->draw(batch, s, 0, an.start + frame, v.x, v.y, k / s.scale, no_team, nullptr, 0.0f);
  }
}

void World::draw_areas(render::SpriteBatch& batch, render::Atlas* atlas, float alpha) {
  // a régi `draw_area`: a képesség saját képe több, eredeti méretű példányban a terület mentén; különben az elem
  // `area` lapja a terület méretére nagyítva; a végén elhalványul
  const sim::Rules* r = session_.rules();
  if (!cur_ || !atlas || !r) return;
  const uint8_t no_team[4] = {0, 0, 0, 0};
  const double now = static_cast<double>(cur_->tick) + alpha;
  for (const auto& ar : cur_->areas) {
    if (ar.ability >= r->abilities.size()) continue;
    const sim::AbilityDef& a = r->abilities[ar.ability];
    const double age = (now - static_cast<double>(ar.start)) / 30.0, left = (static_cast<double>(ar.until) - now) / 30.0;
    uint8_t tint[4] = {255, 255, 255, static_cast<uint8_t>(255.0 * std::clamp(std::min(left / 0.5, age / 0.3), 0.0, 1.0))};
    const bool own = !a.sprite.empty();
    const int si = sheet_by_id(atlas, own ? a.sprite : element_sheet(ar.ability, "area"));
    if (si < 0) continue;
    const auto& s = atlas->data().sheets[static_cast<size_t>(si)];
    if (s.anims.empty()) continue;
    const auto it = s.anims.find("play");
    const auto& an = it != s.anims.end() ? it->second : s.anims.begin()->second;
    auto frame_at = [&](double t) { return an.start + static_cast<int>(std::max(0.0, t) * std::max(1, an.fps)) % std::max(1, an.frames); };
    if (!own) {
      const float base_w = static_cast<float>(s.frame_w) / s.scale;
      const float k = base_w > 0 ? ar.r * 2.2f / base_w : 1.0f;
      atlas->draw(batch, s, 0, frame_at(age), ar.x, ar.y, k / s.scale, no_team, tint, 0.0f);
      continue;
    }
    const int n = std::max(1, static_cast<int>((ar.r / 26.0f) * (ar.r / 26.0f) * 1.2f));
    for (int k = 0; k < n; ++k) {
      float ox = 0, oy = 0;
      if (k > 0) {
        const float t = static_cast<float>(k) / static_cast<float>(std::max(1, n - 1)) * 6.283f + 0.4f;
        ox = std::cos(t) * ar.r * 0.55f;
        oy = std::sin(t) * ar.r * 0.45f;
      }
      atlas->draw(batch, s, 0, frame_at(age + k * 0.17), ar.x + ox, ar.y + oy, 1.0f / s.scale, no_team, tint, 0.0f);
    }
  }
}

std::shared_ptr<const sim::ModuleView> World::take_view_change() {
  if (!cur_ || cur_->view == drawn_view_) return nullptr;
  drawn_view_ = cur_->view;
  return drawn_view_;
}

void World::draw_links(render::SpriteBatch& batch, const render::Texture& white, double time) const {
  // a régi: három kanyargó szál (sötét, világos, közép) az összekötött épületek között, rajtuk lüktető pötty a nép
  // kiemelőszínében; az idegen fonal csak akkor, ha mindkét végét látta
  if (!cur_ || !cur_->view) return;
  for (const auto& l : cur_->view->links) {
    const SnapUnit* a = cur_->find(l.a);
    const SnapUnit* b = cur_->find(l.b);
    if (!a || !b) continue;
    const float ax = (static_cast<float>(a->tx) + static_cast<float>(a->w) / 2) * kTilePx, ay = (static_cast<float>(a->ty) + static_cast<float>(a->h) / 2) * kTilePx;
    const float bx = (static_cast<float>(b->tx) + static_cast<float>(b->w) / 2) * kTilePx, by = (static_cast<float>(b->ty) + static_cast<float>(b->h) / 2) * kTilePx;
    const float d = std::max(1.0f, std::hypot(bx - ax, by - ay));
    const float nx = -(by - ay) / d, ny = (bx - ax) / d;
    static const uint8_t kStrand[3][3] = {{70, 55, 45}, {190, 175, 150}, {150, 135, 110}};
    static const float kOff[3] = {0.0f, 1.5f, -1.5f};
    const int n = std::max(4, static_cast<int>(d / 3.0f));
    for (int k = 0; k < 3; ++k) {
      for (int i = 0; i <= n; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(n);
        const float wv = std::sin(t * 3.14159f * 3 + static_cast<float>(k) * 2 + static_cast<float>(l.a)) * 2.2f * std::sin(t * 3.14159f);
        render::SpriteInstance s;
        const float sz = k == 0 ? 2.0f : 1.2f;
        s.x = ax + (bx - ax) * t + nx * (wv + kOff[k]) - sz / 2;
        s.y = ay + (by - ay) * t + ny * (wv + kOff[k]) - sz / 2;
        s.w = s.h = sz;
        s.tint[0] = kStrand[k][0]; s.tint[1] = kStrand[k][1]; s.tint[2] = kStrand[k][2];
        batch.draw(white, nullptr, s);
      }
    }
    const float ph = static_cast<float>(std::fmod(time * 0.6 + static_cast<double>(l.a) * 0.13, 1.0));
    const auto acc = faction_accent(cur_->players[std::min<size_t>(l.owner, sim::Sim::kMaxPlayers)].faction);
    render::SpriteInstance p;
    p.x = ax + (bx - ax) * ph - 2;
    p.y = ay + (by - ay) * ph - 2;
    p.w = p.h = 4;
    p.tint[0] = acc[0]; p.tint[1] = acc[1]; p.tint[2] = acc[2];
    batch.draw(white, nullptr, p);
  }
}

void World::draw_larva(render::SpriteBatch& batch, const render::Texture& white) const {
  // a régi `draw_building_marks`: a Kaptár fölött annyi kis, sötét szélű pötty, ahány lárva van
  if (!cur_ || !cur_->view) return;
  for (const auto& [id, n] : cur_->view->larva) {
    const SnapUnit* b = cur_->find(id);
    if (!b || b->kind != SnapUnit::kBuilding) continue;
    const float cx = (static_cast<float>(b->tx) + static_cast<float>(b->w) / 2) * kTilePx, y = (static_cast<float>(b->ty) + static_cast<float>(b->h) * 0.3f) * kTilePx;
    const int k = std::min(n, 9);
    for (int i = 0; i < k; ++i) {
      const float x = cx + (static_cast<float>(i) - static_cast<float>(k - 1) / 2) * 7.0f;
      render::SpriteInstance o;
      o.x = x - 3; o.y = y - 3; o.w = o.h = 6;
      o.tint[0] = 30; o.tint[1] = 24; o.tint[2] = 20;
      batch.draw(white, nullptr, o);
      render::SpriteInstance in = o;
      in.x += 1; in.y += 1; in.w = in.h = 4;
      in.tint[0] = 226; in.tint[1] = 214; in.tint[2] = 150;
      batch.draw(white, nullptr, in);
    }
  }
}

void World::draw_status_fx(render::SpriteBatch& batch, const render::Texture& white, float alpha, double time) const {
  const sim::Rules* rules = session_.rules();
  if (!cur_ || !rules) return;
  const float t = static_cast<float>(std::fmod(time, 10000.0));
  const render::Texture& dot = amb_dot_ ? *amb_dot_ : white;
  auto rect = [&](float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b) {
    render::SpriteInstance s;
    s.x = x; s.y = y; s.w = w; s.h = h;
    s.tint[0] = r; s.tint[1] = g; s.tint[2] = b;
    batch.draw(white, nullptr, s);
  };
  auto circ = [&](float x, float y, float rr, uint8_t r, uint8_t g, uint8_t b) {
    render::SpriteInstance s;
    s.x = x - rr; s.y = y - rr; s.w = s.h = rr * 2;
    s.tint[0] = r; s.tint[1] = g; s.tint[2] = b;
    batch.draw(dot, nullptr, s);
  };
  auto outlined = [&](float x, float y, float rr, uint8_t r, uint8_t g, uint8_t b, uint8_t dr, uint8_t dg, uint8_t db) {
    circ(x + 0.6f, y + 0.6f, rr + 1.0f, dr, dg, db);
    circ(x, y, rr, r, g, b);
  };
  for (size_t i = 0; i < cur_->units.size(); ++i) {
    const SnapUnit& u = cur_->units[i];
    if (u.kind != SnapUnit::kUnit || u.hp <= 0 || u.def >= rules->defs.size()) continue;
    auto on = [&](int k) { return u.afflict[static_cast<size_t>(k)] > cur_->tick; };
    bool any = false;
    for (int k = 0; k < 13; ++k) any = any || on(k);
    if (!any) continue;
    const Visual& v = vis_[i];
    const float sx = v.px + (u.x - v.px) * alpha, sy = v.py + (u.y - v.py) * alpha;
    const float r = std::max(8.0f, static_cast<float>(rules->defs[u.def].radius.raw()) / 65536.0f * kTilePx);
    const float top = std::max(28.0f, r * 2.6f);
    const float ph = std::fmod(static_cast<float>(u.id) * 0.6180339f, 1.0f);
    if (on(2) || on(3)) {   // fagyott, lassult: jégszilánkok lassan körbe
      for (int k = 0; k < 4; ++k) {
        const float a = t * 0.9f + static_cast<float>(k) * 1.5708f + ph * 6;
        const float px = sx + std::cos(a) * r * 1.05f, py = sy - top * (0.25f + 0.18f * static_cast<float>(k % 2)) + std::sin(a) * r * 0.35f;
        const float kk = k % 2 ? 3.2f : 2.4f;
        rect(px - kk * 0.35f + 1, py - kk * 1.6f + 1, kk * 0.7f, kk * 3.2f, 24, 20, 46);
        rect(px - kk * 0.35f, py - kk * 1.6f, kk * 0.7f, kk * 3.2f, 226, 238, 255);
        rect(px - kk * 0.7f, py - kk * 0.35f, kk * 1.4f, kk * 0.7f, 226, 238, 255);
      }
    }
    if (on(0) && fire_.flame) {   // ég: lángnyelvek a figura alsó felén
      for (int k = 0; k < 5; ++k) {
        float q = t * 1.6f + static_cast<float>(k) * 0.37f + ph;
        q -= std::floor(q);
        render::SpriteInstance s;
        const float h = (11.0f + 3.0f * static_cast<float>(k % 2)) * (1 - q * 0.6f);
        s.w = h * 0.66f;
        s.h = h;
        s.x = sx + (static_cast<float>(k) - 2) * r * 0.42f + std::sin(t * 7 + static_cast<float>(k)) * 1.5f - s.w / 2;
        s.y = sy - top * (0.05f + 0.45f * q) - h;
        s.layer[0] = static_cast<uint8_t>((static_cast<int>(t * 12.0f) + k * 3) % client::kFlameFrames);
        batch.draw(*fire_.flame, nullptr, s);
      }
    }
    if (on(1)) {   // mérgezett: buborékok szállnak fel
      for (int k = 0; k < 4; ++k) {
        float q = t * 0.7f + static_cast<float>(k) * 0.25f + ph;
        q -= std::floor(q);
        const float px = sx + std::sin(static_cast<float>(k) * 2.3f + ph * 9) * r * 0.7f + std::sin(t * 3 + static_cast<float>(k)) * 1.2f;
        const float py = sy - top * (0.15f + 0.8f * q);
        const float rr = 1.6f + 1.6f * (1 - std::abs(q - 0.5f) * 2);
        outlined(px, py, rr, 150, 240, 90, 30, 70, 16);
        circ(px - rr * 0.35f, py - rr * 0.35f, std::max(0.8f, rr * 0.35f), 220, 255, 190);
      }
    }
    if (on(4)) {   // átázott: hulló vízcseppek
      for (int k = 0; k < 4; ++k) {
        float q = t * 1.3f + static_cast<float>(k) * 0.25f + ph;
        q -= std::floor(q);
        outlined(sx + (static_cast<float>(k) - 1.5f) * r * 0.55f, sy - top * (0.95f - 0.8f * q), 2.0f, 150, 205, 255, 16, 30, 60);
      }
    }
    if (on(5)) {   // gúzsba kötve: gyökérindák a láb körül
      for (int k = 0; k < 5; ++k) {
        const float a = static_cast<float>(k) * 1.2566f + ph * 3;
        const float bx = sx + std::cos(a) * r * 0.9f, by = sy + std::sin(a) * r * 0.3f;
        const float tx = sx + std::cos(a) * r * 0.35f, ty = sy - top * (0.22f + 0.06f * std::sin(t * 2 + static_cast<float>(k)));
        for (int j = 0; j <= 6; ++j) {
          const float f = static_cast<float>(j) / 6.0f;
          circ(bx + (tx - bx) * f + std::sin(a) * 4 * std::sin(f * 3.14159f), by + (ty - by) * f, 1.6f - f * 0.6f, 120, 92, 50);
        }
        circ(tx, ty, 2.0f, 90, 150, 60);
      }
    }
    if (on(8)) {   // megrémült: felkiáltójel a fej fölött, verejtékcseppek
      const float bob = std::sin(t * 9) * 1.5f, ex = sx, ey = sy - top - 10 + bob;
      rect(ex - 2, ey - 9, 5, 10, 40, 10, 10);
      rect(ex - 1, ey - 8, 3, 8, 255, 90, 80);
      rect(ex - 2, ey + 2, 5, 4, 40, 10, 10);
      rect(ex - 1, ey + 3, 3, 2, 255, 90, 80);
      for (int k = 0; k < 2; ++k) {
        float q = t * 2 + static_cast<float>(k) * 0.5f;
        q -= std::floor(q);
        circ(sx + (k ? r * 0.8f : -r * 0.8f), sy - top * (0.9f - 0.3f * q), 2.0f, 180, 220, 255);
      }
    }
    if (on(6)) {   // alszik: Z Z Z száll fel
      for (int k = 0; k < 3; ++k) {
        float q = t * 0.5f + static_cast<float>(k) / 3.0f + ph;
        q -= std::floor(q);
        const float zx = sx + r * 0.5f + q * 10, zy = sy - top - 4 - q * 18, zs = 3.0f + q * 3.0f;
        rect(zx, zy, zs * 2, 1.4f, 230, 235, 255);
        for (int j = 0; j < 4; ++j) rect(zx + zs * 2 - static_cast<float>(j + 1) * zs * 0.5f, zy + static_cast<float>(j + 1) * zs * 0.4f, 1.4f, 1.4f, 230, 235, 255);
        rect(zx, zy + zs * 2, zs * 2, 1.4f, 230, 235, 255);
      }
    }
    if (on(7)) {   // kábult: csillagok forognak a fej körül
      for (int k = 0; k < 3; ++k) {
        const float a = t * 4 + static_cast<float>(k) * 2.094f;
        const float px = sx + std::cos(a) * r * 0.9f, py = sy - top - 2 + std::sin(a) * 3;
        rect(px - 2.5f, py - 0.7f, 5, 1.4f, 255, 230, 110);
        rect(px - 0.7f, py - 2.5f, 1.4f, 5, 255, 230, 110);
      }
    }
    if (on(9)) {   // elvakult: sötét fátyol a fej körül
      for (int k = 0; k < 6; ++k) {
        const float a = t * 1.5f + static_cast<float>(k) * 1.047f;
        circ(sx + std::cos(a) * r * 0.8f, sy - top * 0.85f + std::sin(a) * 2, 3.0f, 40, 30, 60);
      }
    }
    if (on(10)) {   // elnémítva: áthúzott kör a fej fölött
      circ(sx, sy - top - 8, 5.0f, 60, 60, 70);
      circ(sx, sy - top - 8, 3.6f, 200, 200, 210);
      rect(sx - 4, sy - top - 8.7f, 8, 1.4f, 60, 60, 70);
    }
    if (on(11)) {   // megjelölt: piros célpont a fej fölött
      circ(sx, sy - top - 7, 4.0f, 255, 70, 60);
      circ(sx, sy - top - 7, 1.8f, 255, 230, 220);
    }
    if (on(12)) {   // páncéltörés: lepattanó szilánkok
      for (int k = 0; k < 3; ++k) {
        float q = t * 1.1f + static_cast<float>(k) / 3.0f + ph;
        q -= std::floor(q);
        rect(sx + (static_cast<float>(k) - 1) * r * 0.6f + q * 6, sy - top * 0.6f + q * 10, 2.4f, 2.4f, 170, 160, 150);
      }
    }
  }
}

void World::draw_rally_flags(render::SpriteBatch& batch, double time) const {
  if (!cur_ || !rally_.pole || !rally_.cloth) return;
  const uint8_t me = session_.local_player();
  const std::array<uint8_t, 3> col = team_color(me);
  for (const sim::EntityId e : {selected_building_}) {
    const SnapUnit* b = cur_->find(e);
    if (!b || !b->has_rally || b->owner != me) continue;
    render::SpriteInstance p;
    p.x = b->rally_x - 3;
    p.y = b->rally_y - 30;
    p.w = 22;
    p.h = 32;
    p.foot = b->rally_y;   // a napszakkal forgó árnyék innen vetül (D52)
    batch.draw(*rally_.pole, nullptr, p);
    render::SpriteInstance c = p;   // a zászló a csapatszínben, lengve
    c.layer[0] = static_cast<uint8_t>(static_cast<int>(time * 5.0 / 6.2831853 * 8.0) & 7);
    c.tint[0] = col[0]; c.tint[1] = col[1]; c.tint[2] = col[2];
    batch.draw(*rally_.cloth, nullptr, c);
  }
}

void World::draw_garrison(render::SpriteBatch& batch, const render::Texture& white, float hover_wx, float hover_wy) const {
  if (!cur_) return;
  auto rect = [&](float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    render::SpriteInstance in;
    in.x = x; in.y = y; in.w = w; in.h = h;
    in.tint[0] = r; in.tint[1] = g; in.tint[2] = b; in.tint[3] = a;
    batch.draw(white, nullptr, in);
  };
  // egy fej: kerek fej (két egymásra tett téglalap) és váll, sötét körvonallal
  auto head = [&](float cx, float by, bool filled) {
    const uint8_t r = filled ? 235 : 120, g = filled ? 205 : 120, b = filled ? 120 : 125;
    rect(cx - 3.5f, by - 11.0f, 7.0f, 6.0f, 20, 16, 12, 220);
    rect(cx - 2.5f, by - 10.0f, 5.0f, 4.0f, r, g, b);
    rect(cx - 5.0f, by - 5.0f, 10.0f, 5.0f, 20, 16, 12, 220);
    rect(cx - 4.0f, by - 4.0f, 8.0f, 4.0f, r, g, b);
  };
  for (const SnapUnit& u : cur_->units) {
    if (u.kind != SnapUnit::kBuilding || u.owner != session_.local_player() || u.slots == 0 || !u.complete) continue;
    const float x0 = static_cast<float>(u.tx) * kTilePx, y0 = static_cast<float>(u.ty) * kTilePx;
    const float bw = static_cast<float>(u.w) * kTilePx, bh = static_cast<float>(u.h) * kTilePx;
    if (u.garrison > 0) {   // jel az épületen: kis zászló a bal felső sarokban
      rect(x0 + 3.0f, y0 + 2.0f, 2.0f, 12.0f, 40, 30, 20);
      rect(x0 + 5.0f, y0 + 2.0f, 8.0f, 6.0f, 235, 205, 120);
    }
    const bool hover = hover_wx >= x0 && hover_wx < x0 + bw && hover_wy >= y0 && hover_wy < y0 + bh;
    if (!hover) continue;
    const float step = 11.0f, total = step * static_cast<float>(u.slots);
    const float sx = x0 + bw / 2 - total / 2 + step / 2;
    for (int k = 0; k < u.slots; ++k) head(sx + step * static_cast<float>(k), y0 - 12.0f, k < u.garrison);
  }
}

bool World::order_build(uint16_t def, int tx, int ty) {
  const sim::Rules* r = session_.rules();
  if (selected_.empty() || !r || def >= r->defs.size()) return false;
  sim::Command c;
  c.kind = sim::Command::Kind::Build;
  c.units = {selected_.front()};
  c.def = r->defs[def].id;
  c.rx = tx;
  c.ry = ty;
  post(c);
  return true;
}

std::vector<uint16_t> World::buildable_defs(const std::string& faction) const {
  std::vector<uint16_t> out;
  const sim::Rules* r = session_.rules();
  if (!r) return out;
  for (size_t i = 0; i < r->defs.size(); ++i) {
    const bool ally = cur_ && !cur_->players[session_.local_player()].ally.empty() && r->defs[i].faction == cur_->players[session_.local_player()].ally;   // D26
    if (r->defs[i].building && (r->defs[i].faction == faction || ally) && r->defs[i].fw > 0) out.push_back(static_cast<uint16_t>(i));
  }
  return out;
}

bool World::prepare_atlas(render::Atlas& atlas, std::string* error) {
  update();
  std::vector<uint8_t> owners;
  std::vector<int> sheets;
  if (cur_) {
    for (const SnapUnit& u : cur_->units) {
      if (u.def != sim::kNoDef || u.kind != SnapUnit::kUnit) {   // meccs: a sablon vagy a nyersanyag lapja
        const int s = sheet_of(&atlas, u);
        if (s >= 0 && std::find(sheets.begin(), sheets.end(), s) == sheets.end()) sheets.push_back(s);
      } else if (std::find(owners.begin(), owners.end(), u.owner) == owners.end()) {
        owners.push_back(u.owner);
      }
    }
  }
  sheet_for(&atlas, 0, 1);   // a tulajdonosonkénti lista feltöltése (bemutató)
  std::vector<const render::AtlasSheet*> need;
  for (const uint8_t o : owners) {
    for (const int i : owner_sheets_[o]) need.push_back(&atlas.data().sheets[static_cast<size_t>(i)]);
  }
  if (const sim::Rules* r = session_.rules()) {   // a lövedékek lapjai is (az első lövésnél ne akadjon)
    for (const auto& pk : r->projectiles) {
      const int s = sheet_by_id(&atlas, pk.id);
      if (s >= 0 && std::find(sheets.begin(), sheets.end(), s) == sheets.end()) sheets.push_back(s);
    }
  }
  if (const int dd = sheet_by_id(&atlas, "doodads"); dd >= 0 && std::find(sheets.begin(), sheets.end(), dd) == sheets.end()) {
    sheets.push_back(dd);   // a terep apró díszletei (6. tétel)
  }
  for (const int i : sheets) need.push_back(&atlas.data().sheets[static_cast<size_t>(i)]);
  return atlas.prepare(need, error);
}

// a sablon képe: az `Identity.sprite`, ha meg van adva (változat, mod), különben az azonosító
const std::string& World::art_id(const sim::Rules& r, uint16_t def) {
  const sim::EntityDef& d = r.defs[def];
  return d.sprite.empty() ? d.id : d.sprite;
}

int World::sheet_by_id(render::Atlas* atlas, const std::string& id) {
  if (!atlas) return -1;
  if (sheet_ids_.empty()) {
    const auto& sheets = atlas->data().sheets;
    for (size_t i = 0; i < sheets.size(); ++i) sheet_ids_.emplace(sheets[i].id, static_cast<int>(i));
  }
  const auto it = sheet_ids_.find(id);
  return it == sheet_ids_.end() ? -1 : it->second;
}

int World::sheet_of(render::Atlas* atlas, const SnapUnit& u) {
  if (u.kind == SnapUnit::kResource) {
    // az arany lelőhelye a régi grafikában „stone” néven van (a régi játék köve lett az arany)
    const int s = sheet_by_id(atlas, sim::resource_id(u.res));
    return s >= 0 || u.res != sim::kGold ? s : sheet_by_id(atlas, "stone");
  }
  if (u.kind == SnapUnit::kShot) {   // lövedék: a fajta lapja (D40); varázslövedék: az elem `bolt` lapja (a régi)
    const sim::Rules* r = session_.rules();
    if (r && u.spell < r->abilities.size()) {
      const int vs = sheet_by_id(atlas, element_sheet(u.spell, "bolt"));
      if (vs >= 0) return vs;
    }
    return r && u.res < r->projectiles.size() ? sheet_by_id(atlas, r->projectiles[u.res].id) : -1;
  }
  const sim::Rules* r = session_.rules();
  if (u.def == sim::kNoDef || !r || u.def >= r->defs.size()) return -1;
  return sheet_by_id(atlas, art_id(*r, u.def));
}

int World::sheet_for(render::Atlas* atlas, sim::EntityId e, uint8_t owner) {
  if (!atlas) return -1;
  if (!sheets_ready_) {
    const auto& sheets = atlas->data().sheets;
    for (int p = 0; p < 8; ++p) {
      auto& list = owner_sheets_[static_cast<uint8_t>(p + 1)];
      for (size_t i = 0; i < sheets.size(); ++i) {
        const auto& s = sheets[i];
        if (s.type == "unit" && s.id.rfind(kFactions[p], 0) == 0 && s.anims.count("walk") && s.anims.count("idle")) {
          list.push_back(static_cast<int>(i));
        }
      }
    }
    sheets_ready_ = true;
  }
  const auto& list = owner_sheets_[owner];
  if (list.empty()) return -1;
  return list[e % list.size()];
}

void World::collect(const Camera& cam, int sw, int sh, float alpha) {
  const ItemsKey key{tick(), alpha, cam.x, cam.y, cam.zoom, sw, sh};
  if (key == items_key_) return;   // ugyanaz a képkocka (árnyék, majd fő menet)
  items_key_ = key;
  items_.clear();
  if (!cur_) return;
  const float vx1 = cam.x + static_cast<float>(sw) / cam.zoom, vy1 = cam.y + static_cast<float>(sh) / cam.zoom;
  for (size_t i = 0; i < cur_->units.size(); ++i) {
    const SnapUnit& u = cur_->units[i];
    if (u.kind == SnapUnit::kShot) {   // lövedék: a hely simítva; a kulcs a talaj (a magasban is a talajhoz rendez)
      const Visual& v = vis_[i];
      const float x = v.px + (u.x - v.px) * alpha, y = v.py + (u.y - v.py) * alpha;
      if (x < cam.x - 64 || y < cam.y - 64 || x > vx1 + 64 || y > vy1 + 96) continue;
      items_.push_back({x, y, y + 8.0f, i});
      continue;
    }
    if (u.kind != SnapUnit::kUnit) {   // épület és lelőhely: a lábnyom szerint (a kép fölé nyúlik)
      const float x = static_cast<float>(u.tx) * kTilePx, y = static_cast<float>(u.ty) * kTilePx;
      const float x1 = static_cast<float>(u.tx + u.w) * kTilePx, y1 = static_cast<float>(u.ty + u.h) * kTilePx;
      if (x1 < cam.x - 96 || x > vx1 + 96 || y1 < cam.y - 32 || y > vy1 + 256) continue;
      if (u.kind == SnapUnit::kBuilding && u.hp <= 0) continue;
      items_.push_back({x, y, y1 - 0.5f, i});
      continue;
    }
    const Visual& v = vis_[i];
    const float x = v.px + (u.x - v.px) * alpha, y = v.py + (u.y - v.py) * alpha;
    if (x < cam.x - 64 || y < cam.y - 64 || x > vx1 + 64 || y > vy1 + 96) continue;   // takarás
    if (u.hp <= 0) continue;
    items_.push_back({x, y, y, i});
  }
  const auto& units = cur_->units;
  std::sort(items_.begin(), items_.end(),
            [&](const Item& a, const Item& b) { return a.key != b.key ? a.key < b.key : units[a.i].id < units[b.i].id; });
}

void World::draw(render::SpriteBatch& batch, render::Atlas* atlas, const render::Texture& fallback, const Camera& cam,
                 int sw, int sh, float alpha, double time, bool shadow_pass) {
  collect(cam, sw, sh, alpha);
  if (!cur_) return;
  // kijelölés jelzése (a sprite alatt; az árnyékmenetben nem): a régi pecsét – saját zöld, idegen piros; a mérete
  // a figuráé (max(24, 2,5 × sugár)), a talppont alatt 0,45 sugárral, lassan pulzálva (82–100 %)
  const sim::Rules* rules = session_.rules();
  if (!shadow_pass && sigil_own_ && rules) {
    const float pulse = 0.82f + 0.18f * static_cast<float>(std::sin(time * 3.0));
    for (const sim::EntityId e : selected_) {
      const SnapUnit* u = cur_->find(e);
      if (!u || u->def >= rules->defs.size()) continue;
      const Visual& v = vis_[static_cast<size_t>(u - cur_->units.data())];
      const float x = v.px + (u->x - v.px) * alpha, y = v.py + (u->y - v.py) * alpha;
      const float rad = static_cast<float>(rules->defs[u->def].radius.to_double()) * kTilePx;
      const float w = std::max(24.0f, rad * 2.5f);
      const float k = w / sigil_base_;
      const uint8_t me = session_.local_player();
      const render::Texture* tex = u->owner == me ? sigil_own_
                                   : sigil_ally_ && cur_->allied(me, u->owner) ? sigil_ally_ : sigil_other_;
      render::SpriteInstance in;
      in.w = (sigil_base_ + 12.0f) * k;
      in.h = (std::floor(sigil_base_ * 0.6f) + 12.0f) * k;
      in.x = x - in.w / 2;
      in.y = y + rad * 0.45f - in.h / 2;
      in.tint[3] = static_cast<uint8_t>(255.0f * pulse);
      batch.draw(*tex, nullptr, in);
    }
  } else if (!shadow_pass) {   // pecsét nélkül (próbák): a régi lapos folt
    for (const sim::EntityId e : selected_) {
      const SnapUnit* u = cur_->find(e);
      if (!u) continue;
      const Visual& v = vis_[static_cast<size_t>(u - cur_->units.data())];
      const float x = v.px + (u->x - v.px) * alpha, y = v.py + (u->y - v.py) * alpha;
      render::SpriteInstance in;
      in.x = x - 12;
      in.y = y - 5;
      in.w = 24;
      in.h = 10;
      in.tint[0] = 80; in.tint[1] = 255; in.tint[2] = 80; in.tint[3] = 160;
      batch.draw(fallback, nullptr, in);
    }
  }
  drawn_ = 0;
  // holttestek (a régi `draw_corpses`): a halálanimáció lejátszva, a végén megállva; a figurák alatt
  if (atlas && !cur_->corpses.empty()) {
    const sim::Rules* rr = session_.rules();
    for (const auto& c : cur_->corpses) {
      if (!rr || c.def >= rr->defs.size()) continue;
      const int si = sheet_by_id(atlas, art_id(*rr, c.def));
      if (si < 0) continue;
      const auto& s = atlas->data().sheets[static_cast<size_t>(si)];
      const auto d = s.anims.find("death");
      if (d == s.anims.end()) continue;
      const int frame = std::min(d->second.frames - 1, static_cast<int>(c.age * static_cast<float>(d->second.fps)));
      const uint8_t* team = teams_[(c.owner + 7u) % 8u].data();
      atlas->draw(batch, s, 0, d->second.start + std::max(0, frame), c.x, c.y, 1.0f / s.scale, team, nullptr, -1.0f,
                  shadow_pass ? 0 : render::kFxInnerShadow);
    }
  }
  for (const auto& it : items_) {
    const SnapUnit& u = cur_->units[it.i];
    Visual& v = vis_[it.i];
    const uint8_t owner = u.owner;
    const bool match = u.def != sim::kNoDef || u.kind != SnapUnit::kUnit;
    if (v.sheet == -2) v.sheet = !atlas ? -1 : match ? sheet_of(atlas, u) : sheet_for(atlas, u.id, owner);
    const uint8_t* team = teams_[(owner + 7u) % 8u].data();
    if (u.kind == SnapUnit::kShot) {   // lövedék: az ív magasságában, az árnyéka a talajon
      if (atlas && v.sheet >= 0) {
        const auto& s = atlas->data().sheets[static_cast<size_t>(v.sheet)];
        const float ang = std::atan2(u.y - u.sy, u.x - u.sx);
        const int dir = (static_cast<int>(std::lround(ang / (3.14159265f / 4.0f))) + 8) % 8;
        const uint8_t no_team[4] = {0, 0, 0, 0};
        atlas->draw(batch, s, std::min(dir, s.directions - 1), 0, it.x, it.y - u.lift, 1.0f / s.scale, no_team, nullptr,
                    it.y, shadow_pass ? 0 : render::kFxInnerShadow);
      } else if (!shadow_pass) {
        render::SpriteInstance in;
        in.x = it.x - 3;
        in.y = it.y - u.lift - 3;
        in.w = in.h = 6;
        in.tint[0] = 240; in.tint[1] = 220; in.tint[2] = 160;
        batch.draw(fallback, nullptr, in);
      }
      continue;
    }
    if (u.kind != SnapUnit::kUnit) {   // épület és lelőhely
      const float foot = static_cast<float>(u.ty + u.h) * kTilePx;
      if (atlas && v.sheet >= 0) {
        const auto& s = atlas->data().sheets[static_cast<size_t>(v.sheet)];
        int col = 0;
        if (u.kind == SnapUnit::kResource) {   // fa: a csempéből számolt változat
          col = static_cast<int>((static_cast<unsigned>(u.tx) * 7u + static_cast<unsigned>(u.ty) * 13u) % static_cast<unsigned>(std::max(1, s.columns)));
        } else if (const auto b = s.anims.find("build"); !u.complete && b != s.anims.end()) {   // építési fázis
          col = b->second.start + std::min(b->second.frames - 1, u.progress * b->second.frames / 1000);
        } else if (const auto a = s.anims.find("idle"); a != s.anims.end()) {
          col = a->second.start;   // D49: az elkészült épület állókép; az animációi a külön rétegen (draw_building_fx)
        }
        const uint8_t no_team[4] = {0, 0, 0, 0};
        const bool tree = u.kind == SnapUnit::kResource && u.res == sim::kWood;
        if (shadow_pass && !tree) {   // épület és lelőhely: oszloponkénti talp, rövidebb árnyék (a régi játék szerint)
          atlas->draw_shadow_columns(batch, s, 0, col, it.x, it.y, 1.0f / s.scale, 72);
        } else if (shadow_pass) {     // fa: a kép aljáról, a festett talajfolt nélkül
          atlas->draw(batch, s, 0, col, it.x, it.y, 1.0f / s.scale, no_team, nullptr, 0.0f, render::kFxTreeShadow);
        } else {
          atlas->draw(batch, s, 0, col, it.x, it.y, 1.0f / s.scale, u.kind == SnapUnit::kBuilding ? team : no_team,
                      nullptr, foot, render::kFxInnerShadow);
        }
      } else if (!shadow_pass) {   // tartalék: a lábnyom csapatszínű (lelőhelynél nyersanyagszínű) téglalapja
        render::SpriteInstance in;
        in.x = it.x + 2;
        in.y = it.y + 2;
        in.w = static_cast<float>(u.w) * kTilePx - 4;
        in.h = static_cast<float>(u.h) * kTilePx - 4;
        if (u.kind == SnapUnit::kBuilding) {
          in.tint[0] = team[0]; in.tint[1] = team[1]; in.tint[2] = team[2];
        } else {
          static const uint8_t kRes[4][3] = {{70, 160, 70}, {236, 190, 60}, {176, 70, 255}, {255, 214, 120}};
          in.tint[0] = kRes[u.res % 4][0]; in.tint[1] = kRes[u.res % 4][1]; in.tint[2] = kRes[u.res % 4][2];
        }
        batch.draw(fallback, nullptr, in);
      }
      ++drawn_;
      continue;
    }
    if (atlas && v.sheet >= 0 && !atlas->data().sheets[static_cast<size_t>(v.sheet)].anims.empty()) {
      const auto& s = atlas->data().sheets[static_cast<size_t>(v.sheet)];
      if (!v.walk) {   // H-69: a hiányzó animáció helyett a másik, végül az első (modolt atlasz)
        const auto wi = s.anims.find("walk"), ii = s.anims.find("idle");
        const auto* any = &s.anims.begin()->second;
        v.walk = wi != s.anims.end() ? &wi->second : ii != s.anims.end() ? &ii->second : any;
        v.idle = ii != s.anims.end() ? &ii->second : v.walk;
        const auto at = s.anims.find("attack"), ca = s.anims.find("cast"), wk = s.anims.find("work");
        v.attack = at != s.anims.end() ? &at->second : nullptr;
        v.cast = ca != s.anims.end() ? &ca->second : nullptr;
        v.work = wk != s.anims.end() ? &wk->second : nullptr;
      }
      // a régi `unit_anim` sorrendje: varázslás (0,6 s), támadás (0,5 s) – egyszer lejátszva –, munka (gyűjtés a
      // lelőhelynél, építés, javítás), járás, állás
      const double since_cast = (static_cast<double>(cur_->tick - u.cast_tick) + alpha) / 30.0;
      const double since_attack = (static_cast<double>(cur_->tick - u.attack_tick) + alpha) / 30.0;
      const render::AtlasAnim* pick = v.moving ? v.walk : v.idle;
      double once = -1;
      if (v.cast && since_cast >= 0 && since_cast < 0.6) {
        pick = v.cast;
        once = since_cast;
      } else if (v.attack && since_attack >= 0 && since_attack < 0.5) {
        pick = v.attack;
        once = since_attack;
      } else if (v.work && !v.moving &&
                 (u.order == SnapUnit::kGather || u.order == SnapUnit::kConstruct || u.order == SnapUnit::kRepair)) {
        pick = v.work;
      }
      const auto& a = *pick;
      const int frame = once >= 0 ? std::min(a.frames - 1, static_cast<int>(once * a.fps))
                                  : static_cast<int>(time * a.fps + static_cast<double>(u.id)) % std::max(1, a.frames);
      atlas->draw(batch, s, std::min(v.dir, s.directions - 1), a.start + frame, it.x, it.y, 1.0f / s.scale, team, nullptr,
                  -1.0f, shadow_pass ? 0 : render::kFxInnerShadow);
    } else if (!shadow_pass) {   // tartalék: csapatszínű kör
      render::SpriteInstance in;
      in.x = it.x - 8;
      in.y = it.y - 14;
      in.w = 16;
      in.h = 16;
      in.tint[0] = team[0]; in.tint[1] = team[1]; in.tint[2] = team[2];
      batch.draw(fallback, nullptr, in);
    }
    ++drawn_;
  }
  // az építés kísértetképe (áttetsző; az érvényességet a szimuláció dönti el)
  const sim::Rules* r = session_.rules();
  if (!shadow_pass && atlas && r && ghost_def_ < r->defs.size()) {
    const int s = sheet_by_id(atlas, art_id(*r, ghost_def_));
    if (s >= 0) {
      const auto& gs = atlas->data().sheets[static_cast<size_t>(s)];
      const uint8_t ok_tint[4] = {190, 255, 190, 150}, no_tint[4] = {255, 110, 100, 160};
      const uint8_t* tint = ghost_ok_ ? ok_tint : no_tint;
      const uint8_t no_team[4] = {0, 0, 0, 0};
      atlas->draw(batch, gs, 0, 0, static_cast<float>(ghost_tx_) * kTilePx, static_cast<float>(ghost_ty_) * kTilePx,
                  1.0f / gs.scale, no_team, tint);
    }
  }
}

}  // namespace totu::client
