// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/world.hpp"

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
  runner_.take_fx(fx_);
  if (cur == cur_ && prev == prev_) return;
  const bool new_tick = !cur_ || cur->tick != cur_->tick;
  prev_ = std::move(prev);
  cur_ = std::move(cur);
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
  return static_cast<float>(std::clamp((runner_.now() - cur_->published) / SimRunner::kTickSeconds, 0.0, 1.0));
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
  if (!cur_) return;
  for (const SnapUnit& u : cur_->units) {
    if (u.owner != 1 || u.hp <= 0 || u.kind != SnapUnit::kUnit) continue;
    if (u.x >= x0 - 8 && u.x <= x1 + 8 && u.y >= y0 - 8 && u.y <= y1 + 8) selected_.push_back(u.id);
  }
  if (!selected_.empty()) return;
  for (const SnapUnit& u : cur_->units) {   // egység nélkül: a pont alatti saját épület
    if (u.owner != 1 || u.hp <= 0 || u.kind != SnapUnit::kBuilding) continue;
    const float bx = static_cast<float>(u.tx) * kTilePx, by = static_cast<float>(u.ty) * kTilePx;
    if (x0 >= bx && x0 < bx + static_cast<float>(u.w) * kTilePx && y0 >= by && y0 < by + static_cast<float>(u.h) * kTilePx) {
      selected_building_ = u.id;
    }
  }
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
      runner_.post(c);
      return true;
    }
  }
  return false;
}

void World::order_move(float wx, float wy, sim::Formation f) {
  if (selected_.empty()) return;
  sim::Command c;
  c.kind = sim::Command::Kind::GroupMove;
  c.units = selected_;
  c.x = to_fixed(wx);
  c.y = to_fixed(wy);
  c.formation = static_cast<uint8_t>(f);
  runner_.post(c);   // a futtató során át a következő tickben (mint a hálózati játékban és a visszajátszásban)
}

sim::EntityId World::enemy_at(float wx, float wy) const {
  if (!cur_) return 0;
  sim::EntityId best = 0;
  float best_d = 1e9f;
  for (const SnapUnit& u : cur_->units) {
    if (u.owner == 1 || u.owner == 0 || u.hp <= 0 || u.kind == SnapUnit::kShot || u.kind == SnapUnit::kResource) continue;
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
    if (u.kind != SnapUnit::kBuilding || u.owner != 1 || u.slots == 0 || !u.complete || u.hp <= 0) continue;
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
  runner_.post(c);
}

void World::order_unload(float wx, float wy) {
  const sim::EntityId b = garrison_at(wx, wy);
  if (!b) return;
  sim::Command c;
  c.kind = sim::Command::Kind::Unload;
  c.target = b;
  runner_.post(c);
}

void World::order_attack(sim::EntityId target) {
  if (selected_.empty() || !target) return;
  sim::Command c;
  c.kind = sim::Command::Kind::Attack;
  c.units = selected_;
  c.target = target;
  runner_.post(c);
}

void World::order_attack_move(float wx, float wy) {
  if (selected_.empty()) return;
  sim::Command c;
  c.kind = sim::Command::Kind::AttackMove;
  c.units = selected_;
  c.x = to_fixed(wx);
  c.y = to_fixed(wy);
  runner_.post(c);
}

void World::draw_bars(render::SpriteBatch& batch, const render::Texture& white, float alpha) const {
  if (!cur_) return;
  for (size_t i = 0; i < cur_->units.size(); ++i) {
    const SnapUnit& u = cur_->units[i];
    if (u.kind == SnapUnit::kShot || u.kind == SnapUnit::kResource || u.max_hp <= 0 || u.hp <= 0) continue;
    const bool sel = std::find(selected_.begin(), selected_.end(), u.id) != selected_.end();
    if (u.hp >= u.max_hp && !sel) continue;
    float x, y, w;
    if (u.kind == SnapUnit::kBuilding) {
      w = static_cast<float>(u.w) * kTilePx * 0.7f;
      x = static_cast<float>(u.tx) * kTilePx + (static_cast<float>(u.w) * kTilePx - w) / 2;
      y = static_cast<float>(u.ty) * kTilePx - 8.0f;
    } else {
      const Visual& v = vis_[i];
      w = 22.0f;
      x = v.px + (u.x - v.px) * alpha - w / 2;
      y = v.py + (u.y - v.py) * alpha - 44.0f;
    }
    const float f = std::clamp(static_cast<float>(u.hp) / static_cast<float>(u.max_hp), 0.0f, 1.0f);
    render::SpriteInstance bg;
    bg.x = x - 1; bg.y = y - 1; bg.w = w + 2; bg.h = 5;
    bg.tint[0] = 15; bg.tint[1] = 12; bg.tint[2] = 10; bg.tint[3] = 200;
    batch.draw(white, nullptr, bg);
    render::SpriteInstance fg;
    fg.x = x; fg.y = y; fg.w = w * f; fg.h = 3;
    // zöld → sárga → piros (a régi játék színei szerint)
    fg.tint[0] = static_cast<uint8_t>(f > 0.5f ? (1.0f - f) * 2.0f * 230.0f : 230.0f);
    fg.tint[1] = static_cast<uint8_t>(f > 0.5f ? 210.0f : f * 2.0f * 210.0f);
    fg.tint[2] = 40;
    batch.draw(white, nullptr, fg);
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
    if (u.kind != SnapUnit::kBuilding || u.owner != 1 || u.slots == 0 || !u.complete) continue;
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
  runner_.post(c);
  return true;
}

std::vector<uint16_t> World::buildable_defs(const std::string& faction) const {
  std::vector<uint16_t> out;
  const sim::Rules* r = session_.rules();
  if (!r) return out;
  for (size_t i = 0; i < r->defs.size(); ++i) {
    const bool ally = cur_ && !cur_->players[1].ally.empty() && r->defs[i].faction == cur_->players[1].ally;   // D26
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
  for (const int i : sheets) need.push_back(&atlas.data().sheets[static_cast<size_t>(i)]);
  return atlas.prepare(need, error);
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
  if (u.kind == SnapUnit::kResource) return sheet_by_id(atlas, sim::resource_id(u.res));
  if (u.kind == SnapUnit::kShot) {   // lövedék: a fajta lapja (D40)
    const sim::Rules* r = session_.rules();
    return r && u.res < r->projectiles.size() ? sheet_by_id(atlas, r->projectiles[u.res].id) : -1;
  }
  const sim::Rules* r = session_.rules();
  if (u.def == sim::kNoDef || !r || u.def >= r->defs.size()) return -1;
  return sheet_by_id(atlas, r->defs[u.def].id);
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
  // kijelölés jelzése (a sprite alatt; az árnyékmenetben nem)
  if (!shadow_pass) {
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
          col = a->second.start + static_cast<int>(time * a->second.fps) % std::max(1, a->second.frames);
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
    if (atlas && v.sheet >= 0) {
      const auto& s = atlas->data().sheets[static_cast<size_t>(v.sheet)];
      if (!v.walk) {
        v.walk = &s.anims.at("walk");
        v.idle = &s.anims.at("idle");
      }
      const auto& a = v.moving ? *v.walk : *v.idle;
      const int frame = static_cast<int>(time * a.fps + static_cast<double>(u.id)) % std::max(1, a.frames);
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
    const int s = sheet_by_id(atlas, r->defs[ghost_def_].id);
    if (s >= 0) {
      const auto& gs = atlas->data().sheets[static_cast<size_t>(s)];
      const uint8_t tint[4] = {190, 255, 190, 150};
      const uint8_t no_team[4] = {0, 0, 0, 0};
      atlas->draw(batch, gs, 0, 0, static_cast<float>(ghost_tx_) * kTilePx, static_cast<float>(ghost_ty_) * kTilePx,
                  1.0f / gs.scale, no_team, tint);
    }
  }
}

}  // namespace totu::client
