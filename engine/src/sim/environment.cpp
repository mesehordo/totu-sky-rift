// SPDX-License-Identifier: GPL-3.0-or-later
// Napszak és időjárás (M6, ADR-007 7. lépés; D35, D24): a napszak a tickből (folytonos nappal↔éjjel átmenet,
// hajnal/alkony szerint); az időjárás saját véletlenszám-folyamból (ADR-004), a pálya éghajlata szerint
// (`map.json` → `climate`), 2–6 percig tartva, majd fokozatosan a következőre váltva. A hatások csak "hatással"
// módban élnek (`MatchSetup`); a "hangulat" mód csak a megjelenítőt (M5) érinti, itt semmit. Az általános és a faji
// (`FactionDef::env_daytime/env_weather`) hatások egyszerű szorzók (sebesség, látótáv, lőtáv, gyűjtés, építés),
// plusz néhány ritka, kis sebzés (zivatar villáma, homokvihar a nyílt terepen, hamueső az élőkön). Pályaváltozás
// ("hatással + pályaváltozás" módban): az eső sarat/pocsolyát hagy a vízparti dirt/sand/gravel csempéken, a hó
// befagyasztja a tavakat és a gázlókat (`ice`, amin át lehet kelni) – mindkettő fokozatos és visszafordítható;
// olvadáskor a jégen álló egység a partra szorul. A kutatás állandó statmódosítói (D24) is itt alkalmazódnak.
#include <algorithm>
#include <cmath>

#include "map/map.hpp"
#include "map/pathfinder.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

bool tag_match(const std::vector<std::string>& tags, const EntityDef& d) {
  if (tags.empty()) return true;
  for (const std::string& t : tags) {
    if (d.has_tag(t)) return true;
  }
  return false;
}

// a transition_ticks alatt lineárisan: 0 (prev_kind) → 1 (kind)
Fixed blend_of(const WeatherState& w, int64_t tick) {
  if (w.blend_end <= w.blend_start || tick >= w.blend_end) return Fixed::from_int(1);
  if (tick <= w.blend_start) return Fixed{};
  return Fixed::from_ratio(static_cast<int32_t>(tick - w.blend_start), static_cast<int32_t>(w.blend_end - w.blend_start));
}

// két szorzó a blend aránya szerint keveredik (1,0 = nincs hatás semleges elem)
Fixed lerp(Fixed a, Fixed b, Fixed t) { return a + (b - a) * t; }

}  // namespace

Fixed Sim::research_stat(uint8_t owner, const char* path, Fixed base, const EntityDef& d) const {
  if (!rules_ || owner == 0 || owner > kMaxPlayers) return base;
  const std::vector<Modifier>* list = &players_[owner].research_mods;
  return apply_modifiers({list}, path, base, d);
}

int32_t Sim::night_permille() const {
  if (!rules_) return 0;
  const int64_t cyc = std::max<int64_t>(1, rules_->env.day_cycle_ticks);
  // a 0. tick a nap 10%-ánál kezdődik (mint a megjelenítő alapértelmezett ciklusa): a meccs délelőtt indul, nem
  // éjfélkor
  const int64_t t = (tick_ + cyc / 10) % cyc;
  const double f = static_cast<double>(t) / static_cast<double>(cyc);
  // D35: hajnal 0–0,05, nappal 0,05–0,50 (dél 0,275), alkony 0,50–0,55, éjszaka 0,55–1; folytonos éjjel-faktor:
  // 0 (teljes nappal) .. 1000 (teljes éjjel), a hajnal/alkony alatt lineárisan átmenve
  if (f < 0.05) return static_cast<int32_t>((1.0 - f / 0.05) * 1000.0);        // hajnal vége felé világosodik
  if (f < 0.50) return 0;
  if (f < 0.55) return static_cast<int32_t>((f - 0.50) / 0.05 * 1000.0);      // alkonyul
  return 1000;
}

const EnvEffect* Sim::weather_effect() const {
  if (!rules_ || weather_mode_ == MatchSetup::WeatherMode::Off || weather_.kind.empty()) return nullptr;
  const auto it = rules_->env.weather.find(weather_.kind);
  return it == rules_->env.weather.end() ? nullptr : &it->second;
}

Fixed Sim::env_speed_mult(uint8_t owner, const EntityDef& d) const {
  (void)d;   // jelenleg nincs címkeszűrt sebességhatás (a sight/range mintájára bővíthető)
  Fixed m = Fixed::from_int(1);
  if (!rules_ || weather_mode_ == MatchSetup::WeatherMode::Off || weather_mode_ == MatchSetup::WeatherMode::Mood) return m;
  const Fixed t = blend_of(weather_, tick_);
  if (const EnvEffect* e = weather_effect()) m = m * lerp(Fixed::from_int(1), e->speed_mult, t * weather_.intensity);
  if (owner >= 1 && owner <= kMaxPlayers && !weather_.kind.empty()) {
    if (const FactionDef* f = rules_->faction(players_[owner].faction)) {
      const auto it = f->env_weather.find(weather_.kind);
      if (it != f->env_weather.end()) m = m * lerp(Fixed::from_int(1), it->second.speed_mult, t * weather_.intensity);
    }
  }
  return m;
}

Fixed Sim::env_sight_mult(uint8_t owner, const EntityDef& d) const {
  Fixed m = Fixed::from_int(1);
  if (!rules_) return m;
  if (daytime_mode_ == MatchSetup::Daytime::Effect && !d.has_tag("ejjellato")) {
    const Fixed night = Fixed::from_ratio(night_permille(), 1000);
    m = m * lerp(Fixed::from_int(1), rules_->env.night_sight_mult, night);
  }
  if (weather_mode_ != MatchSetup::WeatherMode::Off && weather_mode_ != MatchSetup::WeatherMode::Mood) {
    const Fixed t = blend_of(weather_, tick_) * weather_.intensity;
    if (const EnvEffect* e = weather_effect(); e && tag_match(e->sight_tags, d)) m = m * lerp(Fixed::from_int(1), e->sight_mult, t);
    if (owner >= 1 && owner <= kMaxPlayers && !weather_.kind.empty()) {
      if (const FactionDef* f = rules_->faction(players_[owner].faction)) {
        const auto it = f->env_weather.find(weather_.kind);
        if (it != f->env_weather.end() && tag_match(it->second.sight_tags, d)) m = m * lerp(Fixed::from_int(1), it->second.sight_mult, t);
      }
    }
  }
  return m;
}

Fixed Sim::env_range_mult(uint8_t owner, const EntityDef& d) const {
  Fixed m = Fixed::from_int(1);
  if (!rules_ || weather_mode_ == MatchSetup::WeatherMode::Off || weather_mode_ == MatchSetup::WeatherMode::Mood) return m;
  const Fixed t = blend_of(weather_, tick_) * weather_.intensity;
  if (const EnvEffect* e = weather_effect(); e && tag_match(e->range_tags, d)) m = m * lerp(Fixed::from_int(1), e->range_mult, t);
  if (owner >= 1 && owner <= kMaxPlayers && !weather_.kind.empty()) {
    if (const FactionDef* f = rules_->faction(players_[owner].faction)) {
      const auto it = f->env_weather.find(weather_.kind);
      if (it != f->env_weather.end() && tag_match(it->second.range_tags, d)) m = m * lerp(Fixed::from_int(1), it->second.range_mult, t);
    }
  }
  return m;
}

Fixed Sim::env_gather_mult(uint8_t owner) const {
  Fixed m = Fixed::from_int(1);
  if (!rules_ || weather_mode_ == MatchSetup::WeatherMode::Off || weather_mode_ == MatchSetup::WeatherMode::Mood) return m;
  const Fixed t = blend_of(weather_, tick_) * weather_.intensity;
  if (const EnvEffect* e = weather_effect()) m = m * lerp(Fixed::from_int(1), e->gather_mult, t);
  if (owner >= 1 && owner <= kMaxPlayers && !weather_.kind.empty()) {
    if (const FactionDef* f = rules_->faction(players_[owner].faction)) {
      const auto it = f->env_weather.find(weather_.kind);
      if (it != f->env_weather.end()) m = m * lerp(Fixed::from_int(1), it->second.gather_mult, t);
    }
  }
  return m;
}

Fixed Sim::env_build_mult(uint8_t owner) const {
  Fixed m = Fixed::from_int(1);
  if (!rules_) return m;
  if (daytime_mode_ == MatchSetup::Daytime::Effect) {
    const Fixed night = Fixed::from_ratio(night_permille(), 1000);
    m = m * lerp(Fixed::from_int(1), rules_->env.night_build_mult, night);
  }
  if (weather_mode_ != MatchSetup::WeatherMode::Off && weather_mode_ != MatchSetup::WeatherMode::Mood) {
    const Fixed t = blend_of(weather_, tick_) * weather_.intensity;
    if (const EnvEffect* e = weather_effect()) m = m * lerp(Fixed::from_int(1), e->build_mult, t);
  }
  (void)owner;
  return m;
}

void Sim::weather_pick_next() {
  if (!rules_ || !map_ || rules_->env.climates.empty()) {
    weather_.change_at = tick_ + 1000000000;   // nincs adat: ne próbálja minden tickben újra
    return;
  }
  const std::string climate = map_->climate().empty() ? "temperate" : map_->climate();
  const auto it = rules_->env.climates.find(climate);
  const std::vector<std::string>& kinds = it != rules_->env.climates.end() ? it->second : rules_->env.climates.begin()->second;
  std::string next;
  if (!kinds.empty()) next = kinds[weather_rng_.below(static_cast<uint32_t>(kinds.size()))];
  if (next == "derult") next.clear();   // a derült ég "nincs időjárás"
  weather_.prev_kind = weather_.kind;
  weather_.blend_start = tick_;
  weather_.blend_end = tick_ + std::max(1, rules_->env.transition_ticks);
  weather_.kind = next;
  weather_.intensity = weather_rng_.below(2) == 0 ? Fixed::from_ratio(1, 2) : Fixed::from_int(1);   // enyhe / erős
  const int32_t span = std::max(1, rules_->env.hold_max_ticks - rules_->env.hold_min_ticks);
  weather_.change_at = weather_.blend_end + rules_->env.hold_min_ticks + static_cast<int64_t>(weather_rng_.below(static_cast<uint32_t>(span)));
}

void Sim::build_terrain_patches() {
  mud_patches_.clear();
  ice_patches_.clear();
  if (!map_ || !nav_) return;
  const auto& table = nav_->nav().table();
  const auto is_water = [&](int x, int y) {
    if (!map_->in_bounds(x, y)) return false;
    const auto p = table.pass_class("shallow");
    const auto d = table.pass_class("deep");
    const uint8_t ps = table.terrains()[map_->terrain(x, y)].pass;
    return (p && ps == *p) || (d && ps == *d);
  };
  const int w = map_->width(), h = map_->height();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const uint16_t t = map_->terrain(x, y);
      if (is_water(x, y)) {
        ice_patches_.push_back({static_cast<int16_t>(x), static_cast<int16_t>(y), t, 0});
        continue;
      }
      if (!table.terrains()[t].buildable || table.terrains()[t].hides) continue;   // víz, szikla, erdő stb. nem sár
      bool near_water = false;
      for (int dy = -1; dy <= 1 && !near_water; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          if (is_water(x + dx, y + dy)) { near_water = true; break; }
        }
      }
      if (near_water) mud_patches_.push_back({static_cast<int16_t>(x), static_cast<int16_t>(y), t, 0});
    }
  }
}

namespace {
constexpr int kTerrainStep = 10;   // ennyi tickenként egy csempe alakul/alakul vissza (determinisztikus ütem)
}

void Sim::weather_terrain_tick() {
  if (!rules_ || !map_ || !nav_ || weather_mode_ != MatchSetup::WeatherMode::EffectTerrain) return;
  if (tick_ % kTerrainStep != 0) return;
  const bool raining = weather_.kind == "eso" || weather_.kind == "zivatar";
  const bool snowing = weather_.kind == "ho" || weather_.kind == "hovihar";
  const auto& table = nav_->nav().table();
  const auto mud_id = table.terrain("mud");
  bool touched = false;
  if (mud_id) {
    for (auto& t : mud_patches_) {
      if (raining && !t.converted) {
        map_->set_terrain(t.x, t.y, *mud_id);
        t.converted = 1;
        touched = true;
        break;   // egyszerre egy csempe (determinisztikus, lassú ütem)
      }
      if (!raining && t.converted) {
        map_->set_terrain(t.x, t.y, t.orig_terrain);
        t.converted = 0;
        touched = true;
        break;
      }
    }
  }
  const auto ice_id = table.terrain("ice");
  if (ice_id) {
    for (auto& t : ice_patches_) {
      if (snowing && !t.converted) {
        map_->set_terrain(t.x, t.y, *ice_id);
        t.converted = 1;
        touched = true;
        break;
      }
      if (!snowing && t.converted) {
        map_->set_terrain(t.x, t.y, t.orig_terrain);
        t.converted = 0;
        touched = true;
        // olvadáskor a jégen álló egység a partra szorul
        const Fixed cx = Fixed::from_int(t.x) + Fixed::from_ratio(1, 2), cy = Fixed::from_int(t.y) + Fixed::from_ratio(1, 2);
        std::vector<EntityId> on_ice;
        world_.pool<Body>().each([&](EntityId e, const Body& b) {
          if (b.grid < 0) return;
          const Position* p = world_.get<Position>(e);
          if (p && p->x >= Fixed::from_int(t.x) && p->x < Fixed::from_int(t.x + 1) && p->y >= Fixed::from_int(t.y) &&
              p->y < Fixed::from_int(t.y + 1)) {
            on_ice.push_back(e);
          }
        });
        // előbb a navrács, hogy a partra helyezés már a visszaolvadt vizet lássa
        nav_->terrain_changed({0, 0, map_->width(), map_->height()});
        for (const EntityId e : on_ice) place_at_free(e, cx, cy);
        break;
      }
    }
  }
  if (touched) nav_->terrain_changed({0, 0, map_->width(), map_->height()});
}

void Sim::env_damage_ticks() {
  const EnvEffect* e = weather_effect();
  if (!e || !map_) return;
  auto roll = [&](int32_t per_min) {
    if (per_min <= 0) return false;
    // percenkénti esély tickenként, egyenletesen szétosztva (determinisztikus RNG-vel)
    const uint32_t chance_permille = static_cast<uint32_t>(std::min(1000, per_min * 1000 / (60 * kTickHz)));
    return weather_rng_.below(1000) < chance_permille;
  };
  if (e->lightning_per_min > 0 && roll(e->lightning_per_min)) {
    std::vector<EntityId> hi;
    world_.pool<Unit>().each([&](EntityId u, const Unit&) {
      const Health* h = world_.get<Health>(u);
      if (h && h->hp > 0 && level_at(u) >= 1) hi.push_back(u);
    });
    if (!hi.empty()) {
      const EntityId t = hi[weather_rng_.below(static_cast<uint32_t>(hi.size()))];
      deal_damage(0, 0, kNoDef, t, int64_t{e->lightning_dmg} * 1000, false);
    }
  }
  if (e->open_dmg_per_min > 0 && roll(e->open_dmg_per_min)) {
    const auto& table = nav_->nav().table();
    std::vector<EntityId> open;
    world_.pool<Unit>().each([&](EntityId u, const Unit&) {
      const Health* h = world_.get<Health>(u);
      const Position* p = world_.get<Position>(u);
      if (!h || h->hp <= 0 || !p) return;
      const int x = std::clamp(p->x.floor_int(), 0, map_->width() - 1), y = std::clamp(p->y.floor_int(), 0, map_->height() - 1);
      if (!table.terrains()[map_->terrain(x, y)].hides) open.push_back(u);
    });
    if (!open.empty()) {
      const EntityId t = open[weather_rng_.below(static_cast<uint32_t>(open.size()))];
      deal_damage(0, 0, kNoDef, t, int64_t{e->open_dmg} * 1000, false);
    }
  }
  if (e->organic_dmg_per_min > 0 && roll(e->organic_dmg_per_min)) {
    std::vector<EntityId> organic;
    world_.pool<Unit>().each([&](EntityId u, const Unit& un) {
      const Health* h = world_.get<Health>(u);
      if (h && h->hp > 0 && rules_->defs[un.def].has_tag("organic")) organic.push_back(u);
    });
    if (!organic.empty()) {
      const EntityId t = organic[weather_rng_.below(static_cast<uint32_t>(organic.size()))];
      deal_damage(0, 0, kNoDef, t, int64_t{e->organic_dmg} * 1000, false);
    }
  }
}

Sim::WeatherInfo Sim::weather_info() const {
  WeatherInfo i;
  i.kind = weather_.kind;
  i.prev_kind = weather_.prev_kind;
  i.intensity = weather_.intensity;
  i.blend = blend_of(weather_, tick_);
  return i;
}

void Sim::set_weather(const std::string& kind, Fixed intensity) {
  weather_.prev_kind = kind;
  weather_.kind = kind;
  weather_.blend_start = weather_.blend_end = tick_;
  weather_.intensity = intensity;
  weather_.change_at = tick_ + std::max(1, rules_ ? rules_->env.hold_min_ticks : 3600);
}

void Sim::environment_system() {
  if (!rules_) return;
  if (weather_mode_ != MatchSetup::WeatherMode::Off && tick_ >= weather_.change_at) weather_pick_next();
  weather_terrain_tick();
  env_damage_ticks();
}

}  // namespace totu::sim
