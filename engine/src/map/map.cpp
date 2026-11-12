// SPDX-License-Identifier: GPL-3.0-or-later
#include "map/map.hpp"

#include "core/hash.hpp"

namespace totu::map {

namespace {

// "id*db id*db …" → elemek listája; hibánál üres optional
std::optional<std::vector<std::string>> expand_rle(const std::string& text, size_t expected) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && (text[i] == ' ' || text[i] == '\n' || text[i] == '\t' || text[i] == ',')) ++i;
    if (i >= text.size()) break;
    size_t j = i;
    while (j < text.size() && text[j] != ' ' && text[j] != '\n' && text[j] != '\t' && text[j] != ',') ++j;
    const std::string tok = text.substr(i, j - i);
    i = j;
    const size_t star = tok.find('*');
    std::string id = star == std::string::npos ? tok : tok.substr(0, star);
    size_t count = 1;
    if (star != std::string::npos) {
      const std::string n = tok.substr(star + 1);
      if (n.empty() || n.size() > 7) return std::nullopt;
      count = 0;
      for (char c : n) {
        if (c < '0' || c > '9') return std::nullopt;
        count = count * 10 + static_cast<size_t>(c - '0');
      }
    }
    if (id.empty() || count == 0 || out.size() + count > expected) return std::nullopt;
    out.insert(out.end(), count, id);
  }
  if (out.size() != expected) return std::nullopt;
  return out;
}

std::optional<int> level_of(char c) {
  if (c == '-') return -1;
  if (c >= '0' && c <= '2') return c - '0';
  return std::nullopt;
}

std::optional<int> level_of(const std::string& s) {
  if (s == "-1") return -1;
  if (s.size() == 1) return level_of(s[0]);
  return std::nullopt;
}

bool read_rows(const content::Value* rows, int w, int h, std::vector<std::string>& out, const char* what,
               std::vector<std::string>& errors) {
  if (!rows || !rows->is_array() || rows->items().size() != static_cast<size_t>(h)) {
    errors.push_back(std::string(what) + ": a `rows` tömb " + std::to_string(h) + " sorból álljon");
    return false;
  }
  for (size_t y = 0; y < rows->items().size(); ++y) {
    const content::Value& r = rows->items()[y];
    if (!r.is_string() || r.text().size() != static_cast<size_t>(w)) {
      errors.push_back(std::string(what) + ": a(z) " + std::to_string(y + 1) + ". sor " + std::to_string(w) +
                       " jel hosszú legyen");
      return false;
    }
    out.push_back(r.text());
  }
  return true;
}

}  // namespace

Map::Map(int width, int height, uint16_t fill_terrain)
    : w_(width),
      h_(height),
      terrain_(static_cast<size_t>(width) * static_cast<size_t>(height), fill_terrain),
      level_(terrain_.size(), 0),
      cliff_(terrain_.size(), 0) {}

std::optional<Map> Map::from_json(const content::Value& doc, const TerrainTable& table,
                                  std::vector<std::string>& errors) {
  const size_t before = errors.size();
  const content::Value* wv = doc.find("width");
  const content::Value* hv = doc.find("height");
  const auto w = wv ? wv->as_int() : std::nullopt;
  const auto h = hv ? hv->as_int() : std::nullopt;
  if (!w || !h || *w < 1 || *h < 1 || *w > kMaxMapSize || *h > kMaxMapSize) {
    errors.push_back("a pálya `width` és `height` értéke 1 és 512 közötti egész legyen");
    return std::nullopt;
  }
  if (table.terrains().empty()) {
    errors.push_back("nincs betöltött terepfajta");
    return std::nullopt;
  }
  const int W = static_cast<int>(*w), H = static_cast<int>(*h);
  Map m(W, H, 0);
  const size_t n = static_cast<size_t>(W) * static_cast<size_t>(H);

  // terep
  const content::Value* tv = doc.find("terrain");
  if (!tv || !tv->is_object()) {
    errors.push_back("hiányzik a `terrain` (rows vagy rle)");
    return std::nullopt;
  }
  if (const content::Value* rle = tv->find("rle")) {
    auto items = rle->is_string() ? expand_rle(rle->text(), n) : std::nullopt;
    if (!items) {
      errors.push_back("terrain.rle: hibás vagy nem " + std::to_string(n) + " csempét ad");
      return std::nullopt;
    }
    for (size_t i = 0; i < n; ++i) {
      const auto t = table.terrain((*items)[i]);
      if (!t) {
        errors.push_back("terrain.rle: ismeretlen terepfajta: " + (*items)[i]);
        return std::nullopt;
      }
      m.terrain_[i] = *t;
    }
  } else {
    std::vector<std::string> rows;
    if (!read_rows(tv->find("rows"), W, H, rows, "terrain", errors)) return std::nullopt;
    const content::Value* legend = tv->find("legend");
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        const char c = rows[static_cast<size_t>(y)][static_cast<size_t>(x)];
        std::optional<uint16_t> t;
        const content::Value* entry = legend ? legend->find(std::string(1, c)) : nullptr;
        if (entry && entry->is_string()) t = table.terrain(entry->text());
        else if (!entry) t = table.terrain_by_char(c);
        if (!t) {
          errors.push_back("terrain: ismeretlen jel `" + std::string(1, c) + "` (" + std::to_string(x + 1) + ". oszlop, " +
                           std::to_string(y + 1) + ". sor)");
          return std::nullopt;
        }
        m.terrain_[m.index(x, y)] = *t;
      }
    }
  }

  // szintek
  if (const content::Value* lv = doc.find("levels")) {
    if (const content::Value* rle = lv->find("rle")) {
      auto items = rle->is_string() ? expand_rle(rle->text(), n) : std::nullopt;
      if (!items) {
        errors.push_back("levels.rle: hibás vagy nem " + std::to_string(n) + " csempét ad");
        return std::nullopt;
      }
      for (size_t i = 0; i < n; ++i) {
        const auto l = level_of((*items)[i]);
        if (!l) {
          errors.push_back("levels.rle: a szint -1, 0, 1 vagy 2 lehet");
          return std::nullopt;
        }
        m.level_[i] = static_cast<int8_t>(*l);
      }
    } else {
      std::vector<std::string> rows;
      if (!read_rows(lv->find("rows"), W, H, rows, "levels", errors)) return std::nullopt;
      for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
          const auto l = level_of(rows[static_cast<size_t>(y)][static_cast<size_t>(x)]);
          if (!l) {
            errors.push_back("levels: a jel `-`, `0`, `1` vagy `2` lehet (" + std::to_string(x + 1) + ". oszlop, " +
                             std::to_string(y + 1) + ". sor)");
            return std::nullopt;
          }
          m.level_[m.index(x, y)] = static_cast<int8_t>(*l);
        }
      }
    }
  }

  const content::Value* ac = doc.find("auto_cliffs");
  if (!ac || !ac->is_bool() || ac->as_bool()) m.compute_cliffs(table);
  if (errors.size() != before) return std::nullopt;
  return m;
}

void Map::compute_cliffs(const TerrainTable& table) {
  const auto ramp = table.pass_class("ramp");
  for (int y = 0; y < h_; ++y) {
    for (int x = 0; x < w_; ++x) {
      uint8_t c = 0;
      const bool is_ramp = ramp && table.terrains()[terrain(x, y)].pass == *ramp;
      if (!is_ramp) {
        const int lv = level(x, y);
        for (int dy = -1; dy <= 1 && !c; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            if ((dx || dy) && in_bounds(x + dx, y + dy) && level(x + dx, y + dy) < lv) { c = 1; break; }
          }
        }
      }
      cliff_[index(x, y)] = c;
    }
  }
}

bool Map::passable(const TerrainTable& table, uint8_t movement_class, int x, int y) const {
  if (!in_bounds(x, y)) return false;
  if (table.movement_classes()[movement_class].flying) return true;
  return !cliff(x, y) && table.passable(movement_class, terrain(x, y));
}

uint64_t Map::hash() const {
  core::StateHash h;
  h.add_i64(w_);
  h.add_i64(h_);
  for (size_t i = 0; i < terrain_.size(); ++i) {
    h.add_u64(terrain_[i] | static_cast<uint64_t>(static_cast<uint8_t>(level_[i])) << 16 |
              static_cast<uint64_t>(cliff_[i]) << 24);
  }
  return h.value();
}

}  // namespace totu::map
