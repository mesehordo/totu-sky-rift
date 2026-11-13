// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/world.hpp"

#include <algorithm>
#include <cmath>

namespace totu::client {

using core::Fixed;

namespace {

Fixed to_fixed(float px) { return Fixed::from_raw(static_cast<int32_t>(px / World::kTilePx * 65536.0f)); }

const uint8_t kTeams[8][4] = {{220, 40, 40, 255},  {40, 90, 230, 255},  {40, 180, 60, 255}, {230, 200, 40, 255},
                              {160, 60, 200, 255}, {240, 130, 30, 255}, {40, 200, 200, 255}, {235, 235, 235, 255}};
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
    if (u.id % 6 != 0) continue;
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
  if (!cur_) return;
  for (const SnapUnit& u : cur_->units) {
    if (u.owner != 1 || u.hp <= 0) continue;
    if (u.x >= x0 - 8 && u.x <= x1 + 8 && u.y >= y0 - 8 && u.y <= y1 + 8) selected_.push_back(u.id);
  }
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

bool World::prepare_atlas(render::Atlas& atlas, std::string* error) {
  update();
  std::vector<uint8_t> owners;
  if (cur_) {
    for (const SnapUnit& u : cur_->units) {
      if (std::find(owners.begin(), owners.end(), u.owner) == owners.end()) owners.push_back(u.owner);
    }
  }
  sheet_for(&atlas, 0, 1);   // a tulajdonosonkénti lista feltöltése
  std::vector<const render::AtlasSheet*> need;
  for (const uint8_t o : owners) {
    for (const int i : owner_sheets_[o]) need.push_back(&atlas.data().sheets[static_cast<size_t>(i)]);
  }
  return atlas.prepare(need, error);
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
    const Visual& v = vis_[i];
    const float x = v.px + (u.x - v.px) * alpha, y = v.py + (u.y - v.py) * alpha;
    if (x < cam.x - 64 || y < cam.y - 64 || x > vx1 + 64 || y > vy1 + 96) continue;   // takarás
    if (u.hp <= 0) continue;
    items_.push_back({x, y, i});
  }
  const auto& units = cur_->units;
  std::sort(items_.begin(), items_.end(),
            [&](const Item& a, const Item& b) { return a.y != b.y ? a.y < b.y : units[a.i].id < units[b.i].id; });
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
    if (v.sheet == -2) v.sheet = atlas ? sheet_for(atlas, u.id, owner) : -1;
    const uint8_t* team = kTeams[(owner + 7) % 8];
    if (atlas && v.sheet >= 0) {
      const auto& s = atlas->data().sheets[static_cast<size_t>(v.sheet)];
      if (!v.walk) {
        v.walk = &s.anims.at("walk");
        v.idle = &s.anims.at("idle");
      }
      const auto& a = v.moving ? *v.walk : *v.idle;
      const int frame = static_cast<int>(time * a.fps + static_cast<double>(u.id)) % std::max(1, a.frames);
      atlas->draw(batch, s, std::min(v.dir, s.directions - 1), a.start + frame, it.x, it.y, 1.0f / s.scale, team);
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
}

}  // namespace totu::client
