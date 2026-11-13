// SPDX-License-Identifier: GPL-3.0-or-later
#include "sim/rules.hpp"

#include <algorithm>

namespace totu::sim {

namespace {

constexpr const char* kResIds[kResources] = {"wood", "stone", "meteor", "faith"};

Fixed fixed_of(const content::Value* v, Fixed def) {
  if (!v) return def;
  return v->as_fixed().value_or(def);
}

int32_t int_of(const content::Value* v, int32_t def) {
  if (!v) return def;
  if (const auto i = v->as_int()) return static_cast<int32_t>(*i);
  if (const auto f = v->as_fixed()) return f->floor_int();
  return def;
}

const content::Value* path(const content::Value& v, const char* a, const char* b = nullptr) {
  const content::Value* x = v.find(a);
  if (!x || !b) return x;
  return x->is_object() ? x->find(b) : nullptr;
}

void read_res_map(const content::Value* obj, std::array<int32_t, kResources>& out) {
  if (!obj || !obj->is_object()) return;
  for (const auto& [k, v] : obj->members()) {
    const int r = resource_index(k);
    if (r >= 0) out[static_cast<size_t>(r)] = int_of(&v, 0);
  }
}

uint8_t res_mask(const content::Value* arr) {
  uint8_t m = 0;
  if (!arr || !arr->is_array()) return m;
  for (const auto& v : arr->items()) {
    const int r = v.is_string() ? resource_index(v.text()) : -1;
    if (r >= 0) m = static_cast<uint8_t>(m | (1u << r));
  }
  return m;
}

}  // namespace

const char* resource_id(int r) { return r >= 0 && r < kResources ? kResIds[r] : "?"; }

int resource_index(const std::string& id) {
  for (int i = 0; i < kResources; ++i) {
    if (id == kResIds[i]) return i;
  }
  return -1;
}

int seconds_to_ticks(Fixed seconds) {
  const int64_t raw = static_cast<int64_t>(seconds.raw()) * kTickHz;
  return static_cast<int>((raw + (int64_t{1} << 15)) >> 16);
}

bool EntityDef::has_tag(const std::string& t) const { return std::find(tags.begin(), tags.end(), t) != tags.end(); }

std::optional<Rules> Rules::load(const content::Value& game, content::ContentSet& set, std::vector<std::string>& errors) {
  const size_t before = errors.size();
  Rules r;
  // nyersanyagok
  const content::Value* res = game.find("resources");
  if (!res || !res->is_object()) {
    errors.push_back("game.json: hiányzik a `resources`");
    return std::nullopt;
  }
  for (int i = 0; i < kResources; ++i) {
    const content::Value* d = res->find(kResIds[i]);
    if (!d || !d->is_object()) {
      errors.push_back(std::string("game.json: hiányzik a `resources.") + kResIds[i] + "`");
      continue;
    }
    ResourceDef& rd = r.resources[static_cast<size_t>(i)];
    rd.gather_time = fixed_of(d->find("gather_time"), Fixed::from_int(2));
    rd.carry = int_of(d->find("carry"), 0);
    rd.node_amount = int_of(d->find("node_amount"), 0);
    const content::Value* sp = d->find("special");
    rd.special = sp && sp->is_bool() && sp->as_bool();
  }
  read_res_map(game.find("start_resources"), r.start_resources);
  r.max_supply = int_of(game.find("max_supply"), 200);
  if (const content::Value* dr = game.find("demolish_refund")) {
    const Fixed f = fixed_of(dr, Fixed::from_ratio(7, 10));
    r.demolish_refund_permille = static_cast<int32_t>((static_cast<int64_t>(f.raw()) * 1000 + (1 << 15)) >> 16);
  }
  if (const content::Value* cr = game.find("build_cancel_refund")) {
    const Fixed f = fixed_of(cr, Fixed::from_ratio(3, 4));
    r.cancel_refund_permille = static_cast<int32_t>((static_cast<int64_t>(f.raw()) * 1000 + (1 << 15)) >> 16);
  }
  if (const content::Value* tiers = path(game, "upkeep", "tiers"); tiers && tiers->is_array()) {
    for (const auto& t : tiers->items()) {
      if (!t.is_array() || t.items().size() != 2) continue;
      const Fixed f = fixed_of(&t.items()[1], Fixed::from_int(1));
      r.upkeep_tiers.emplace_back(int_of(&t.items()[0], 0),
                                  static_cast<int32_t>((static_cast<int64_t>(f.raw()) * 1000 + (1 << 15)) >> 16));
    }
  }

  // egységek és épületek (a `template_` kezdetű absztrakt sablonok nélkül)
  auto& ents = set["entity"];
  for (const std::string& id : ents.ids()) {
    if (id.rfind("template_", 0) == 0) continue;
    const content::Value* v = ents.get(id);
    if (!v) continue;
    EntityDef d;
    d.id = id;
    const content::Value* kind = path(*v, "Identity", "kind");
    d.building = kind && kind->is_string() && kind->text() == "building";
    if (const content::Value* f = path(*v, "Identity", "faction"); f && f->is_string()) d.faction = f->text();
    if (const content::Value* tg = path(*v, "Identity", "tags"); tg && tg->is_array()) {
      for (const auto& t : tg->items()) {
        if (t.is_string()) d.tags.push_back(t.text());
      }
    }
    const content::Value* wk = path(*v, "Identity", "worker");
    d.worker = wk && wk->is_bool() && wk->as_bool();
    d.hp = std::max(1, int_of(path(*v, "Health", "max"), 1));
    d.armor = int_of(path(*v, "Health", "armor"), 0);
    d.sight = int_of(path(*v, "Vision", "sight"), 0);
    read_res_map(path(*v, "Cost", "resources"), d.cost);
    d.supply = int_of(path(*v, "Cost", "supply"), 0);
    d.time_ticks = seconds_to_ticks(fixed_of(path(*v, "Cost", "time"), Fixed{}));
    // a sebesség az adatban csempe / másodperc
    const Fixed sp = fixed_of(path(*v, "Movement", "speed"), Fixed::from_ratio(5, 2));
    d.speed = Fixed::from_raw(static_cast<int32_t>((static_cast<int64_t>(sp.raw()) + kTickHz / 2) / kTickHz));
    d.radius = fixed_of(path(*v, "Movement", "radius"), Fixed::from_ratio(4, 10));
    const content::Value* fl = path(*v, "Movement", "flying");
    d.flying = fl && fl->is_bool() && fl->as_bool();
    d.gathers = 0x7;   // fa, arany, meteorit (az áhítat nem lelőhelyről jön)
    if (const content::Value* w = v->find("Worker"); w && w->is_object()) {
      if (const content::Value* g = w->find("gathers"); g && g->is_array()) d.gathers = res_mask(g);
      read_res_map(w->find("carry"), d.carry);
    }
    if (const content::Value* fp = path(*v, "Footprint", "size"); fp && fp->is_array() && fp->items().size() == 2) {
      d.fw = int_of(&fp->items()[0], 0);
      d.fh = int_of(&fp->items()[1], 0);
    }
    d.supply_provides = int_of(path(*v, "Supply", "provides"), 0);
    const content::Value* hero = path(*v, "Identity", "hero");
    d.hero = hero && hero->is_bool() && hero->as_bool();
    if (const content::Value* rl = path(*v, "Researcher", "list"); rl && rl->is_array()) {
      for (const auto& x : rl->items()) {
        if (x.is_string()) d.researches.push_back(x.text());
      }
    }
    if (const content::Value* all = path(*v, "Requirements", "all"); all && all->is_array()) {
      for (const auto& q : all->items()) {
        if (!q.is_object()) continue;
        for (const auto& [k, val] : q.members()) {
          EntityDef::Req req;
          if (k == "building") req.kind = EntityDef::Req::Building;
          else if (k == "research") req.kind = EntityDef::Req::Research;
          else if (k == "path") req.kind = EntityDef::Req::Path;
          else if (k == "ascended") req.kind = EntityDef::Req::Ascended;
          else continue;
          if (val.is_string()) req.id = val.text();
          d.reqs.push_back(req);
        }
      }
    }
    d.dropoff = res_mask(path(*v, "Dropoff", "resources"));
    r.defs.push_back(std::move(d));
  }
  std::sort(r.defs.begin(), r.defs.end(), [](const EntityDef& a, const EntityDef& b) { return a.id < b.id; });
  // a képezhető egységek (a rendezés után, sorszámmal)
  for (EntityDef& d : r.defs) {
    const content::Value* v = ents.get(d.id);
    if (const content::Value* pu = v ? path(*v, "Producer", "units") : nullptr; pu && pu->is_array()) {
      for (const auto& x : pu->items()) {
        if (!x.is_string()) continue;
        const uint16_t u = r.def(x.text());
        if (u != kNoDef) d.produces.push_back(u);
      }
    }
  }
  // kutatások
  auto& rs = set["research"];
  for (const std::string& id : rs.ids()) {
    if (id.rfind("template_", 0) == 0) continue;
    const content::Value* v = rs.get(id);
    if (!v) continue;
    ResearchDef rd;
    rd.id = id;
    if (const content::Value* f = path(*v, "Identity", "faction"); f && f->is_string()) rd.faction = f->text();
    read_res_map(path(*v, "Cost", "resources"), rd.cost);
    rd.time_ticks = seconds_to_ticks(fixed_of(path(*v, "Cost", "time"), Fixed{}));
    if (const content::Value* all = path(*v, "Requirements", "all"); all && all->is_array()) {
      for (const auto& q : all->items()) {
        if (!q.is_object()) continue;
        for (const auto& [k, val] : q.members()) {
          EntityDef::Req req;
          if (k == "building") req.kind = EntityDef::Req::Building;
          else if (k == "research") req.kind = EntityDef::Req::Research;
          else if (k == "path") req.kind = EntityDef::Req::Path;
          else if (k == "ascended") req.kind = EntityDef::Req::Ascended;
          else continue;
          if (val.is_string()) req.id = val.text();
          rd.reqs.push_back(req);
        }
      }
    }
    r.researches.push_back(std::move(rd));
  }
  std::sort(r.researches.begin(), r.researches.end(), [](const ResearchDef& a, const ResearchDef& b) { return a.id < b.id; });

  // népek
  auto& facs = set["faction"];
  for (const std::string& id : facs.ids()) {
    if (id.rfind("faction_", 0) != 0) continue;
    const content::Value* v = facs.get(id);
    const content::Value* f = v ? v->find("Faction") : nullptr;
    if (!f || !f->is_object()) continue;
    FactionDef fd;
    fd.id = id.substr(8);
    auto def_of = [&](const char* key) {
      const content::Value* x = f->find(key);
      return x && x->is_string() ? r.def(x->text()) : kNoDef;
    };
    fd.hq = def_of("hq");
    fd.worker = def_of("worker");
    if (const content::Value* su = f->find("start_units"); su && su->is_object()) {
      for (const auto& [uid, n] : su->members()) {
        const uint16_t d = r.def(uid);
        if (d != kNoDef) fd.start_units.emplace_back(d, int_of(&n, 0));
      }
    }
    if (const content::Value* b = f->find("builder"); b && b->is_string()) fd.builder = b->text();
    if (const content::Value* dm = f->find("demolish"); dm && dm->is_bool()) fd.demolish = dm->as_bool();
    if (const content::Value* g = f->find("start_gather"); g && g->is_string() && resource_index(g->text()) >= 0) {
      fd.start_gather = static_cast<uint8_t>(resource_index(g->text()));
    }
    fd.start_resources = r.start_resources;
    if (const content::Value* sr = f->find("start_resources")) read_res_map(sr, fd.start_resources);
    r.factions.push_back(std::move(fd));
  }
  std::sort(r.factions.begin(), r.factions.end(), [](const FactionDef& a, const FactionDef& b) { return a.id < b.id; });
  if (errors.size() != before) return std::nullopt;
  return r;
}

uint16_t Rules::def(const std::string& id) const {
  const auto it = std::lower_bound(defs.begin(), defs.end(), id, [](const EntityDef& d, const std::string& v) { return d.id < v; });
  return it != defs.end() && it->id == id ? static_cast<uint16_t>(it - defs.begin()) : kNoDef;
}

const FactionDef* Rules::faction(const std::string& id) const {
  const auto it = std::lower_bound(factions.begin(), factions.end(), id,
                                   [](const FactionDef& f, const std::string& v) { return f.id < v; });
  return it != factions.end() && it->id == id ? &*it : nullptr;
}

const ResearchDef* Rules::research(const std::string& id) const {
  const int i = research_index(id);
  return i < 0 ? nullptr : &researches[static_cast<size_t>(i)];
}

int Rules::research_index(const std::string& id) const {
  const auto it = std::lower_bound(researches.begin(), researches.end(), id,
                                   [](const ResearchDef& x, const std::string& v) { return x.id < v; });
  return it != researches.end() && it->id == id ? static_cast<int>(it - researches.begin()) : -1;
}

int32_t Rules::upkeep_permille(int32_t used) const {
  if (upkeep_tiers.empty()) return 1000;
  for (const auto& [top, f] : upkeep_tiers) {
    if (used <= top) return f;
  }
  return upkeep_tiers.back().second;
}

}  // namespace totu::sim
