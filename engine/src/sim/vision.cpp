// SPDX-License-Identifier: GPL-3.0-or-later
// Látótér és lopakodás (M6, ADR-007 6. lépés; a régi játék szerint: G 3043–3088, 1287–1345). Játékosonként
// csempénkénti láthatóság (6 tickenként) és felderítettség (megmarad); a köd beállítása (normál, felderített,
// nincs) a meccs indításakor. A magaslat (D33, ADR-006): a látótáv szintenként +1 (mélységben −1); magasabb szintre
// csak közelről (2 csempe) lát fel; a mélységben állót fentről csak 6 csempén belülről látja; a fentről lövő 2 s-ig
// mindenkinek látható. A repülő a szinteket figyelmen kívül hagyja. Rejtőzés (15 tickenként): a rejtett egységet
// a felfedő (egység vagy kész épület `Vision.detect` sugárral) és a 2,2 csempén belüli ellenség fedi fel. A
// napszak hatása (éjjel −25%, D35) a 7. lépésben.
#include <algorithm>

#include "map/map.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

void Sim::update_vision(uint8_t player) {
  if (!map_) return;
  PlayerState& p = players_[player];
  const int w = map_->width(), h = map_->height();
  const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
  if (p.explored.size() != n) p.explored.assign(n, fog_ == MatchSetup::Fog::Explored ? 1 : 0);
  if (fog_ == MatchSetup::Fog::None) {
    p.visible.assign(n, 1);
    p.explored.assign(n, 1);
    return;
  }
  p.visible.assign(n, 0);
  const auto& el = rules_->elevation;
  auto see_from = [&](EntityId e, int sight, bool flying) {
    const Position* pos = world_.get<Position>(e);
    if (!pos) return;
    const int cx = std::clamp(pos->x.floor_int(), 0, w - 1), cy = std::clamp(pos->y.floor_int(), 0, h - 1);
    const int lv = flying ? 0 : level_at(e);
    int r = sight;
    if (!flying) r += lv > 0 ? lv * el.vision_per_level : lv < 0 ? el.depression_vision : 0;
    r = std::max(1, r);
    const int up2 = el.uphill_presence * el.uphill_presence, down2 = el.depression_hidden_beyond * el.depression_hidden_beyond;
    for (int dy = -r; dy <= r; ++dy) {
      const int y = cy + dy;
      if (y < 0 || y >= h) continue;
      for (int dx = -r; dx <= r; ++dx) {
        const int x = cx + dx;
        const int d2 = dx * dx + dy * dy;
        if (x < 0 || x >= w || d2 > r * r) continue;
        if (!flying) {
          const int tl = map_->level(x, y);
          if (tl > lv && d2 > up2) continue;     // magasabb szintre csak közelről
          if (tl < lv && tl < 0 && d2 > down2) continue;   // a mélységbe fentről csak közelről
        }
        const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
        p.visible[i] = 1;
        p.explored[i] = 1;
      }
    }
  };
  world_.pool<Health>().each([&](EntityId e, const Health& hh) {
    if (hh.owner != player || hh.hp <= 0) return;
    if (const Unit* u = world_.get<Unit>(e)) {
      const EntityDef& d = rules_->defs[u->def];
      see_from(e, std::max(1, d.sight), d.flying);
    } else if (const Building* b = world_.get<Building>(e)) {
      see_from(e, std::max(1, rules_->defs[b->def].sight) + std::max(b->w, b->h) / 2, false);
    }
  });
}

void Sim::vision_system() {
  for (int i = 1; i <= kMaxPlayers; ++i) {
    if (players_[static_cast<size_t>(i)].active) update_vision(static_cast<uint8_t>(i));
  }
}

void Sim::stealth_system() {
  // rejtett: az adat szerint (a faji rejtőzések – erdő, éjszaka, beásás – az M7-ben ide kapcsolódnak)
  std::vector<EntityId> hidden;
  world_.pool<Stealth>().each([&](EntityId e, Stealth& s) {
    const Unit* u = world_.get<Unit>(e);
    s.hidden = u && rules_->defs[u->def].always_hidden ? 1 : 0;
    s.detected = 0;
    if (s.hidden) hidden.push_back(e);
  });
  if (hidden.empty()) return;
  const Fixed near = Fixed::from_ratio(22, 10);
  for (const EntityId hdn : hidden) {
    const Position* hp = world_.get<Position>(hdn);
    const Health* hh = world_.get<Health>(hdn);
    Stealth* st = world_.get<Stealth>(hdn);
    if (!hp || !hh || !st) continue;
    world_.pool<Health>().each([&](EntityId o, const Health& oh) {
      if (oh.hp <= 0 || !hostile(oh.owner, hh->owner) || (st->detected & (1u << oh.owner))) return;
      const Position* op = world_.get<Position>(o);
      if (!op) return;
      int det = 0;
      bool unit = false;
      if (const Unit* ou = world_.get<Unit>(o)) {
        det = rules_->defs[ou->def].detect;
        unit = true;
      } else if (const Building* ob = world_.get<Building>(o); ob && ob->complete) {
        det = rules_->defs[ob->def].detect;
      }
      const Fixed d = core::length(op->x - hp->x, op->y - hp->y);
      if ((det > 0 && d <= Fixed::from_int(det)) || (unit && d <= near)) st->detected = static_cast<uint16_t>(st->detected | (1u << oh.owner));
    });
  }
}

bool Sim::is_visible(uint8_t player, EntityId e) {
  if (!world_.alive(e)) return false;
  const Health* h = world_.get<Health>(e);
  if (h && h->owner == player) return true;
  if (const Worker* w = world_.get<Worker>(e); w && w->inside) return false;   // épületben: nem látszik
  if (!rules_ || !map_ || fog_ == MatchSetup::Fog::None || player == 0 || player > kMaxPlayers) return true;
  const Stealth* st = world_.get<Stealth>(e);
  if (st && st->revealed_until > tick_) return true;   // a fentről lövő (D33)
  if (st && st->hidden && !(st->detected & (1u << player))) return false;
  const PlayerState& p = players_[player];
  if (p.visible.empty()) return true;
  int x = 0, y = 0;
  if (const Building* b = world_.get<Building>(e)) {
    // épület: bármely lábnyom-csempéje látszik
    for (int yy = b->ty; yy < b->ty + b->h; ++yy) {
      for (int xx = b->tx; xx < b->tx + b->w; ++xx) {
        if (p.visible[static_cast<size_t>(yy) * static_cast<size_t>(map_->width()) + static_cast<size_t>(xx)]) return true;
      }
    }
    return false;
  }
  const Position* pos = world_.get<Position>(e);
  if (!pos) return false;
  x = std::clamp(pos->x.floor_int(), 0, map_->width() - 1);
  y = std::clamp(pos->y.floor_int(), 0, map_->height() - 1);
  return p.visible[static_cast<size_t>(y) * static_cast<size_t>(map_->width()) + static_cast<size_t>(x)] != 0;
}

}  // namespace totu::sim
