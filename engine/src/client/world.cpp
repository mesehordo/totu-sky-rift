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
  if (!cur_) return;
  for (const SnapUnit& u : cur_->units) {
    if (u.owner != 1 || u.hp <= 0 || u.kind != SnapUnit::kUnit) continue;
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
    if (r->defs[i].building && r->defs[i].faction == faction && r->defs[i].fw > 0) out.push_back(static_cast<uint16_t>(i));
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
    const uint8_t* team = kTeams[(owner + 7) % 8];
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
