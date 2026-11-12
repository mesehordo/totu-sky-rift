// SPDX-License-Identifier: GPL-3.0-or-later
#include "map/navgrid.hpp"

#include <algorithm>

#include "core/hash.hpp"

namespace totu::map {

NavConfig NavConfig::from_json(const content::Value* nav, std::vector<std::string>& errors) {
  NavConfig c;
  if (!nav) return c;
  if (const content::Value* v = nav->find("navcells_per_tile"); v && v->as_int() != kNavPerTile) {
    errors.push_back("navigation.navcells_per_tile: a motor csak 2-t támogat");
  }
  if (const content::Value* v = nav->find("clearance_cap")) {
    const auto n = v->as_int();
    if (!n || *n < 1 || *n > 64) errors.push_back("navigation.clearance_cap: 1 és 64 közötti egész");
    else c.clearance_cap = static_cast<int>(*n);
  }
  if (const content::Value* s = nav->find("size_classes"); s && s->is_array()) {
    std::vector<SizeClass> sizes;
    for (const auto& item : s->items()) {
      const content::Value* id = item.find("id");
      const content::Value* r = item.find("max_radius");
      const content::Value* cl = item.find("clearance");
      const auto rf = r ? r->as_fixed() : std::nullopt;
      const auto cn = cl ? cl->as_int() : std::nullopt;
      if (!id || !id->is_string() || !rf || !cn || *cn < 1 || *cn > c.clearance_cap) {
        errors.push_back("navigation.size_classes: minden elemhez id, max_radius és 1..clearance_cap közötti clearance kell");
        return c;
      }
      sizes.push_back({id->text(), *rf, static_cast<uint8_t>(*cn)});
    }
    std::sort(sizes.begin(), sizes.end(), [](const SizeClass& a, const SizeClass& b) { return a.max_radius < b.max_radius; });
    if (!sizes.empty()) c.sizes = std::move(sizes);
  }
  return c;
}

NavGrid::NavGrid(const Map& map, const TerrainTable& table, NavConfig config)
    : map_(map), table_(table), config_(std::move(config)), nw_(map.width() * kNavPerTile),
      nh_(map.height() * kNavPerTile) {
  const auto& mcs = table_.movement_classes();
  for (size_t i = 0; i < mcs.size(); ++i) {
    if (!mcs[i].flying) class_ids_.push_back(static_cast<uint8_t>(i));
  }
  const size_t n = static_cast<size_t>(nw_) * static_cast<size_t>(nh_);
  base_.assign(class_ids_.size(), std::vector<uint8_t>(n, 0));
  clear_.assign(class_ids_.size(), std::vector<uint8_t>(n, 0));
  blocked_.assign(n, 0);
  rebuild_all();
}

int NavGrid::grid_of(uint8_t movement_class) const {
  for (size_t g = 0; g < class_ids_.size(); ++g) {
    if (class_ids_[g] == movement_class) return static_cast<int>(g);
  }
  return -1;
}

int NavGrid::size_class(Fixed radius) const {
  for (size_t i = 0; i < config_.sizes.size(); ++i) {
    if (radius <= config_.sizes[i].max_radius) return static_cast<int>(i);
  }
  return static_cast<int>(config_.sizes.size()) - 1;
}

Fixed NavGrid::speed(int grid, int nx, int ny) const {
  if (!passable(grid, nx, ny)) return Fixed{};
  return table_.speed(class_ids_[static_cast<size_t>(grid)], map_.terrain(nx / kNavPerTile, ny / kNavPerTile));
}

void NavGrid::update_base(int grid, TileRect r) {
  const uint8_t mc = class_ids_[static_cast<size_t>(grid)];
  auto& base = base_[static_cast<size_t>(grid)];
  for (int ty = r.y; ty < r.y + r.h; ++ty) {
    for (int tx = r.x; tx < r.x + r.w; ++tx) {
      const uint8_t ok = map_.passable(table_, mc, tx, ty) ? 1 : 0;
      for (int sy = 0; sy < kNavPerTile; ++sy) {
        for (int sx = 0; sx < kNavPerTile; ++sx) {
          const size_t i = index(tx * kNavPerTile + sx, ty * kNavPerTile + sy);
          base[i] = static_cast<uint8_t>(ok && blocked_[i] == 0);
        }
      }
    }
  }
}

// Csebisev-távolság két menetben (előre és vissza), a felső határral levágva. A tartományon kívüli navcellák nem
// akadályok (a hívó gondoskodik róla, hogy a tartomány elég nagy legyen); a pálya széle akadály.
void NavGrid::recompute(int grid, int x0, int y0, int x1, int y1) {
  const auto& base = base_[static_cast<size_t>(grid)];
  auto& cl = clear_[static_cast<size_t>(grid)];
  const int cap = config_.clearance_cap;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      const size_t i = index(x, y);
      if (!base[i]) { cl[i] = 0; continue; }
      const int edge = std::min({x + 1, y + 1, nw_ - x, nh_ - y, cap});
      cl[i] = static_cast<uint8_t>(edge);
    }
  }
  auto relax = [&](int x, int y, int nx, int ny) {
    if (nx < x0 || ny < y0 || nx >= x1 || ny >= y1) return;
    const size_t i = index(x, y);
    const int v = cl[index(nx, ny)] + 1;
    if (v < cl[i]) cl[i] = static_cast<uint8_t>(v);
  };
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      if (!cl[index(x, y)]) continue;
      relax(x, y, x - 1, y);
      relax(x, y, x - 1, y - 1);
      relax(x, y, x, y - 1);
      relax(x, y, x + 1, y - 1);
    }
  }
  for (int y = y1 - 1; y >= y0; --y) {
    for (int x = x1 - 1; x >= x0; --x) {
      if (!cl[index(x, y)]) continue;
      relax(x, y, x + 1, y);
      relax(x, y, x + 1, y + 1);
      relax(x, y, x, y + 1);
      relax(x, y, x - 1, y + 1);
    }
  }
}

void NavGrid::rebuild_all() {
  for (int g = 0; g < classes(); ++g) {
    update_base(g, {0, 0, map_.width(), map_.height()});
    recompute(g, 0, 0, nw_, nh_);
  }
}

void NavGrid::after_change(TileRect r) {
  const int cap = config_.clearance_cap;
  const int x0 = r.x * kNavPerTile, y0 = r.y * kNavPerTile;
  const int x1 = (r.x + r.w) * kNavPerTile, y1 = (r.y + r.h) * kNavPerTile;
  // a megváltozott értékek a változás körüli `cap` sávban lehetnek; ezek helyes kiszámításához még egy `cap` sáv kell
  const int ox0 = std::max(0, x0 - cap), oy0 = std::max(0, y0 - cap);
  const int ox1 = std::min(nw_, x1 + cap), oy1 = std::min(nh_, y1 + cap);
  const int wx0 = std::max(0, x0 - 2 * cap), wy0 = std::max(0, y0 - 2 * cap);
  const int wx1 = std::min(nw_, x1 + 2 * cap), wy1 = std::min(nh_, y1 + 2 * cap);
  for (int g = 0; g < classes(); ++g) {
    update_base(g, r);
    auto& cl = clear_[static_cast<size_t>(g)];
    // a külső sáv régi értékeit félretesszük: a munkaterület széle hamis (túl nagy) értéket kaphat, azt nem írjuk vissza
    std::vector<uint8_t> keep;
    keep.reserve(static_cast<size_t>((wx1 - wx0) * (wy1 - wy0)));
    for (int y = wy0; y < wy1; ++y) {
      for (int x = wx0; x < wx1; ++x) keep.push_back(cl[index(x, y)]);
    }
    recompute(g, wx0, wy0, wx1, wy1);
    size_t k = 0;
    for (int y = wy0; y < wy1; ++y) {
      for (int x = wx0; x < wx1; ++x, ++k) {
        const bool inner = x >= ox0 && x < ox1 && y >= oy0 && y < oy1;
        if (!inner) cl[index(x, y)] = keep[k];
      }
    }
  }
}

void NavGrid::add_obstacle(TileRect r) {
  r.w = std::min(r.w, map_.width() - r.x);
  r.h = std::min(r.h, map_.height() - r.y);
  if (r.x < 0 || r.y < 0 || r.w <= 0 || r.h <= 0) return;
  for (int y = r.y * kNavPerTile; y < (r.y + r.h) * kNavPerTile; ++y) {
    for (int x = r.x * kNavPerTile; x < (r.x + r.w) * kNavPerTile; ++x) {
      uint8_t& b = blocked_[index(x, y)];
      if (b < 255) ++b;
    }
  }
  after_change(r);
}

void NavGrid::remove_obstacle(TileRect r) {
  r.w = std::min(r.w, map_.width() - r.x);
  r.h = std::min(r.h, map_.height() - r.y);
  if (r.x < 0 || r.y < 0 || r.w <= 0 || r.h <= 0) return;
  for (int y = r.y * kNavPerTile; y < (r.y + r.h) * kNavPerTile; ++y) {
    for (int x = r.x * kNavPerTile; x < (r.x + r.w) * kNavPerTile; ++x) {
      uint8_t& b = blocked_[index(x, y)];
      if (b > 0) --b;
    }
  }
  after_change(r);
}

void NavGrid::terrain_changed(TileRect r) {
  // a szintváltozás a szomszédos csempék szirtjét is érinti: egy csempével bővebb sáv
  r = {r.x - 1, r.y - 1, r.w + 2, r.h + 2};
  if (r.x < 0) { r.w += r.x; r.x = 0; }
  if (r.y < 0) { r.h += r.y; r.y = 0; }
  r.w = std::min(r.w, map_.width() - r.x);
  r.h = std::min(r.h, map_.height() - r.y);
  if (r.x < 0 || r.y < 0 || r.w <= 0 || r.h <= 0) return;
  after_change(r);
}

uint64_t NavGrid::hash() const {
  core::StateHash h;
  h.add_i64(nw_);
  h.add_i64(nh_);
  for (const auto& cl : clear_) {
    h.add_bytes(std::string_view(reinterpret_cast<const char*>(cl.data()), cl.size()));
  }
  return h.value();
}

}  // namespace totu::map
