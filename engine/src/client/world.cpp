// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/world.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "content/loader.hpp"

namespace totu::client {

using core::Fixed;

namespace {

float to_px(Fixed v) { return static_cast<float>(v.raw()) / 65536.0f * World::kTilePx; }
Fixed to_fixed(float px) { return Fixed::from_raw(static_cast<int32_t>(px / World::kTilePx * 65536.0f)); }

const uint8_t kTeams[8][4] = {{220, 40, 40, 255},  {40, 90, 230, 255},  {40, 180, 60, 255}, {230, 200, 40, 255},
                              {160, 60, 200, 255}, {240, 130, 30, 255}, {40, 200, 200, 255}, {235, 235, 235, 255}};
// bemutató: a játékosok népe (a sprite-lapok előtagja)
const char* kFactions[8] = {"elf_", "cro_", "urs_", "imp_", "wul_", "huk_", "oon_", "dsz_"};

}  // namespace

bool World::load(const std::string& content_dir, const std::string& map_path, uint64_t seed, std::string* error) {
  content::Vfs vfs;
  vfs.add_layer("totu", std::filesystem::path(content_dir) / "base");
  content::ContentSet set;
  content::ContentLoader(vfs, std::filesystem::path(content_dir) / "schema").load(set);
  std::vector<std::string> errors;
  if (!terrain_.load(set, errors)) {
    if (error) *error = errors.empty() ? "terepadat" : errors.front();
    return false;
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

void World::spawn_demo(int players, int per_player) {
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
      const auto* pos = sim_->world().get<sim::Position>(e);
      if (pos) vis_[e] = Visual{to_px(pos->x), to_px(pos->y), p % 2 == 0 ? 0 : 4, false, -1};
    }
  }
}

void World::order_demo_attack() {
  std::map<uint8_t, std::vector<sim::EntityId>> armies;
  sim_->world().pool<sim::Health>().each([&](sim::EntityId e, const sim::Health& hh) { armies[hh.owner].push_back(e); });
  const Fixed cx = Fixed::from_ratio(map_->width(), 2), cy = Fixed::from_ratio(map_->height(), 2);
  for (auto& [owner, ids] : armies) {
    sim_->order_group_move(ids, owner % 2 ? cx + Fixed::from_int(6) : cx - Fixed::from_int(6), cy,
                           owner % 2 ? sim::Formation::Auto : sim::Formation::Line);
  }
}

void World::step() {
  // az előző helyek a simításhoz
  sim_->world().pool<sim::Position>().each([&](sim::EntityId e, const sim::Position& p) {
    Visual& v = vis_[e];
    v.px = to_px(p.x);
    v.py = to_px(p.y);
  });
  sim_->step();
  // irány és mozgás a megtett útból
  for (auto it = vis_.begin(); it != vis_.end();) {
    const auto* p = sim_->world().alive(it->first) ? sim_->world().get<sim::Position>(it->first) : nullptr;
    if (!p) {
      if (fx_.size() < 4096) fx_.push_back({it->second.px, it->second.py, true});
      it = vis_.erase(it);
      continue;
    }
    if (const auto* hh = sim_->world().get<sim::Health>(it->first)) {
      if (it->second.hp >= 0 && hh->hp < it->second.hp && fx_.size() < 4096) {
        fx_.push_back({to_px(p->x), to_px(p->y) - 6.0f, false});
      }
      it->second.hp = hh->hp;
    }
    const float dx = to_px(p->x) - it->second.px, dy = to_px(p->y) - it->second.py;
    it->second.moving = dx * dx + dy * dy > 0.01f;
    if (it->second.moving) {
      const float a = std::atan2(dy, dx);   // a képernyőn lefelé nő az y: K = 0, DK = 1, D = 2 …
      it->second.dir = (static_cast<int>(std::lround(a / (3.14159265f / 4.0f))) + 8) % 8;
    }
    ++it;
  }
  selected_.erase(std::remove_if(selected_.begin(), selected_.end(), [&](sim::EntityId e) { return !sim_->world().alive(e); }),
                  selected_.end());
  if (sim_->tick() % 5 == 0) update_fog();
}

void World::update_fog() {
  const int w = map_->width(), h = map_->height();
  if (fog_.empty()) fog_.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
  for (auto& f : fog_) {
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
        if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= kSight * kSight) fog_[static_cast<size_t>(y * w + x)] = 255;
      }
    }
  });
  fog_dirty_ = true;
}

const std::vector<uint8_t>& World::fog(bool* changed) {
  if (fog_.empty()) update_fog();
  if (changed) *changed = fog_dirty_;
  fog_dirty_ = false;
  return fog_;
}

void World::lights(std::vector<render::Light>& out, float alpha) const {
  for (const auto& [e, v] : vis_) {
    if (e % 6 != 0) continue;
    const auto* p = sim_->world().get<sim::Position>(e);
    if (!p) continue;
    render::Light l;
    l.x = v.px + (to_px(p->x) - v.px) * alpha;
    l.y = v.py + (to_px(p->y) - v.py) * alpha - 20.0f;   // a fáklya a fej magasságában
    l.radius = 90.0f;
    l.r = 255; l.g = 190; l.b = 110;
    out.push_back(l);
  }
}

void World::select_box(float x0, float y0, float x1, float y1) {
  if (x0 > x1) std::swap(x0, x1);
  if (y0 > y1) std::swap(y0, y1);
  selected_.clear();
  sim_->world().pool<sim::Health>().each([&](sim::EntityId e, const sim::Health& hh) {
    if (hh.owner != 1 || hh.hp <= 0) return;
    const auto* p = sim_->world().get<sim::Position>(e);
    if (!p) return;
    const float x = to_px(p->x), y = to_px(p->y);
    if (x >= x0 - 8 && x <= x1 + 8 && y >= y0 - 8 && y <= y1 + 8) selected_.push_back(e);
  });
}

void World::order_move(float wx, float wy, sim::Formation f) {
  if (selected_.empty()) return;
  sim::Command c;
  c.kind = sim::Command::Kind::GroupMove;
  c.tick = sim_->tick();
  c.units = selected_;
  c.x = to_fixed(wx);
  c.y = to_fixed(wy);
  c.formation = static_cast<uint8_t>(f);
  sim_->queue(c);   // a parancssoron át (mint a hálózati játékban és a visszajátszásban)
}

bool World::prepare_atlas(render::Atlas& atlas, std::string* error) {
  std::vector<uint8_t> owners;
  sim_->world().pool<sim::Health>().each([&](sim::EntityId, const sim::Health& hh) {
    if (std::find(owners.begin(), owners.end(), hh.owner) == owners.end()) owners.push_back(hh.owner);
  });
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

void World::draw(render::SpriteBatch& batch, render::Atlas* atlas, const render::Texture& fallback, const Camera& cam,
                 int sw, int sh, float alpha, double time, bool shadow_pass) {
  const float vx1 = cam.x + static_cast<float>(sw) / cam.zoom, vy1 = cam.y + static_cast<float>(sh) / cam.zoom;
  struct Item {
    float x, y;
    sim::EntityId e;
  };
  std::vector<Item> items;
  for (auto& [e, v] : vis_) {
    const auto* p = sim_->world().get<sim::Position>(e);
    const auto* hh = sim_->world().get<sim::Health>(e);
    if (!p || !hh || hh->hp <= 0) continue;
    const float x = v.px + (to_px(p->x) - v.px) * alpha, y = v.py + (to_px(p->y) - v.py) * alpha;
    if (x < cam.x - 64 || y < cam.y - 64 || x > vx1 + 64 || y > vy1 + 96) continue;   // takarás
    items.push_back({x, y, e});
  }
  std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.y != b.y ? a.y < b.y : a.e < b.e; });
  // kijelölés jelzése (a sprite alatt; az árnyékmenetben nem)
  for (const sim::EntityId e : (shadow_pass ? std::vector<sim::EntityId>{} : selected_)) {
    const auto it = vis_.find(e);
    const auto* p = sim_->world().get<sim::Position>(e);
    if (it == vis_.end() || !p) continue;
    const float x = it->second.px + (to_px(p->x) - it->second.px) * alpha, y = it->second.py + (to_px(p->y) - it->second.py) * alpha;
    render::SpriteInstance in;
    in.x = x - 12;
    in.y = y - 5;
    in.w = 24;
    in.h = 10;
    in.tint[0] = 80; in.tint[1] = 255; in.tint[2] = 80; in.tint[3] = 160;
    batch.draw(fallback, nullptr, in);
  }
  drawn_ = 0;
  for (const auto& it : items) {
    Visual& v = vis_[it.e];
    const auto* hh = sim_->world().get<sim::Health>(it.e);
    const uint8_t owner = hh ? hh->owner : 0;
    if (v.sheet == -1 && atlas) v.sheet = sheet_for(atlas, it.e, owner);
    const uint8_t* team = kTeams[(owner + 7) % 8];
    if (atlas && v.sheet >= 0) {
      const auto& s = atlas->data().sheets[static_cast<size_t>(v.sheet)];
      const auto& a = s.anims.at(v.moving ? "walk" : "idle");
      const int frame = static_cast<int>(time * a.fps + static_cast<double>(it.e)) % std::max(1, a.frames);
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
