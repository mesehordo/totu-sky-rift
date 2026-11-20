// SPDX-License-Identifier: GPL-3.0-or-later
#include "map/map.hpp"

#include <cstdlib>
#include <algorithm>
#include <charconv>
#include <limits>

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
  // (egyszerű érték: a GCC az optional-ra téves „inicializálatlan” figyelmeztetést ad)
  const long long w = (wv ? wv->as_int() : std::nullopt).value_or(0);
  const long long h = (hv ? hv->as_int() : std::nullopt).value_or(0);
  if (w < 1 || h < 1 || w > kMaxMapSize || h > kMaxMapSize) {
    errors.push_back("a pálya `width` és `height` értéke 1 és 512 közötti egész legyen");
    return std::nullopt;
  }
  if (table.terrains().empty()) {
    errors.push_back("nincs betöltött terepfajta");
    return std::nullopt;
  }
  const int W = static_cast<int>(w), H = static_cast<int>(h);
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

  // éghajlat (D35): "climate": "temperate" | "snowy" | "desert" | "swampy" | "volcanic" (üres = "temperate")
  if (const content::Value* cl = doc.find("climate"); cl && cl->is_string()) m.climate_ = cl->text();

  // kisnép-táborok (TABOR-1): "camps": {"1": "pinguu", "4": "tokla"} – szintenként (1–5) melyik nép tábora; a többi a Vadoné
  if (const content::Value* cp = doc.find("camps"); cp && cp->is_object()) {
    for (const auto& [lvl, who] : cp->members()) {
      int l = 0;   // H-22: csak a teljes, számjegyekből álló kulcs érvényes
      const auto [pe, pec] = std::from_chars(lvl.data(), lvl.data() + lvl.size(), l);
      if (pec != std::errc{} || pe != lvl.data() + lvl.size()) l = 0;
      if (l < 1 || l > 5 || !who.is_string()) errors.push_back("camps: szint (1–5) → kisnép kell");
      else m.objects_.camp_peoples.emplace_back(l, who.text());
    }
  }

  // a kezdőhelyek oldala (a régi `teams_hint`, a „Fent és lent”-féle pályákon): [0, 0, 1, 1]
  if (const content::Value* ts = doc.find("teams"); ts && ts->is_array()) {
    for (const auto& t : ts->items()) m.objects_.start_sides.push_back(static_cast<int>(std::clamp<long long>(t.as_int().value_or(0), 0, 1)));
  }
  // háttérszereplők (a régi `ambient`): [{"id", "n", "at": [x, y], "idle", "path": [[x, y] …], "shelter": [x, y]}]
  if (const content::Value* am = doc.find("ambient"); am && am->is_array()) {
    auto xy = [&](const content::Value* v, int& x, int& y) {
      if (!v || !v->is_array() || v->items().size() != 2) return false;
      x = static_cast<int>(std::clamp<long long>(v->items()[0].as_int().value_or(-1), -1, 100000));
      y = static_cast<int>(std::clamp<long long>(v->items()[1].as_int().value_or(-1), -1, 100000));
      return x >= 0 && y >= 0 && x < m.w_ && y < m.h_;
    };
    for (const auto& a : am->items()) {
      const content::Value* id = a.find("id");
      MapAmbient ma;
      if (!id || !id->is_string() || !xy(a.find("at"), ma.tx, ma.ty)) continue;
      ma.id = id->text();
      if (const content::Value* cnt = a.find("n")) ma.n = static_cast<int>(std::clamp<long long>(cnt->as_int().value_or(1), 1, 12));
      if (const content::Value* i = a.find("idle"); i && i->is_bool()) ma.idle = i->as_bool();
      if (const content::Value* p = a.find("path"); p && p->is_array()) {
        for (const auto& q : p->items()) {
          int x = 0, y = 0;
          if (xy(&q, x, y)) ma.path.emplace_back(x, y);
        }
      }
      ma.has_shelter = xy(a.find("shelter"), ma.sx, ma.sy);
      m.objects_.ambient.push_back(std::move(ma));
    }
  }
  // díszletépületek (a régi `decor`): [{"id", "tx", "ty", "w", "h", "frame", "ruin"}] – a pályán belül
  if (const content::Value* dc = doc.find("decor"); dc && dc->is_array()) {
    for (const auto& d : dc->items()) {
      const content::Value* id = d.find("id");
      if (!id || !id->is_string()) continue;
      MapDecor md;
      md.id = id->text();
      auto iv = [&](const char* k, int def) {
        const content::Value* v = d.find(k);
        return v ? static_cast<int>(std::clamp<long long>(v->as_int().value_or(def), -100000, 100000)) : def;
      };
      md.tx = iv("tx", 0);
      md.ty = iv("ty", 0);
      md.w = std::clamp(iv("w", 1), 1, 16);
      md.h = std::clamp(iv("h", 1), 1, 16);
      md.frame = std::max(0, iv("frame", 0));
      if (const content::Value* r = d.find("ruin"); r && r->is_bool()) md.ruin = r->as_bool();
      if (md.tx < 0 || md.ty < 0 || md.tx + md.w > m.w_ || md.ty + md.h > m.h_) {
        errors.push_back("a díszlet a pályán kívül: " + md.id);
        continue;
      }
      m.objects_.decor.push_back(std::move(md));
    }
  }
  // pályaobjektumok (M6): "objects": {"starts": [[x, y], …], "trees": {"rows": [...]} (`T` = fa),
  // "resources": [{"kind": "gold", "x": …, "y": …, "size": 2, "amount": …}, …]}
  if (const content::Value* ob = doc.find("objects"); ob && ob->is_object()) {
    // H-37: az int tartományára szorítva (a túl nagy érték így a határellenőrzésen bukik el, nem tekeredik érvényesbe)
    auto int_of = [](const content::Value* v, long long def) {
      return std::clamp<long long>((v ? v->as_int() : std::nullopt).value_or(def), std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    };
    if (const content::Value* st = ob->find("starts"); st && st->is_array()) {
      for (const auto& p : st->items()) {
        if (!p.is_array() || p.items().size() != 2) {
          errors.push_back("objects.starts: [x, y] párok kellenek");
          continue;
        }
        const long long x = int_of(&p.items()[0], -1), y = int_of(&p.items()[1], -1);
        if (x < 0 || y < 0 || x >= W || y >= H) errors.push_back("objects.starts: a pályán kívül");
        else m.objects_.starts.emplace_back(static_cast<int>(x), static_cast<int>(y));
      }
    }
    if (const content::Value* tr = ob->find("trees"); tr && tr->is_object()) {
      const content::Value* rv = tr->find("rows");
      if (!rv || !rv->is_array() || rv->items().size() != static_cast<size_t>(H)) {
        errors.push_back("objects.trees.rows: pontosan `height` sor kell");
      } else {
        for (int y = 0; y < H; ++y) {
          const content::Value& row = rv->items()[static_cast<size_t>(y)];
          if (!row.is_string() || row.text().size() != static_cast<size_t>(W)) {
            errors.push_back("objects.trees.rows: a " + std::to_string(y + 1) + ". sor nem `width` hosszú");
            break;
          }
          for (int x = 0; x < W; ++x) {
            if (row.text()[static_cast<size_t>(x)] == 'T') m.objects_.trees.emplace_back(x, y);
          }
        }
      }
    }
    if (const content::Value* rs = ob->find("resources"); rs && rs->is_array()) {
      for (const auto& r : rs->items()) {
        const content::Value* kind = r.is_object() ? r.find("kind") : nullptr;
        if (!kind || !kind->is_string()) {
          errors.push_back("objects.resources: minden lelőhelynek kell `kind`");
          continue;
        }
        MapResource mr;
        mr.kind = kind->text();
        mr.x = static_cast<int>(int_of(r.find("x"), -1));
        mr.y = static_cast<int>(int_of(r.find("y"), -1));
        mr.size = static_cast<int>(int_of(r.find("size"), 2));
        mr.amount = static_cast<int>(int_of(r.find("amount"), -1));
        if (mr.size < 1 || mr.size > 8 || mr.x < 0 || mr.y < 0 || mr.x + mr.size > W || mr.y + mr.size > H) {
          errors.push_back("objects.resources: `" + mr.kind + "` a pályán kívül vagy hibás méretű");
          continue;
        }
        m.objects_.resources.push_back(mr);
      }
    }
  }
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
