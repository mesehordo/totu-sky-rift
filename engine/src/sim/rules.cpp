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

// másodpercenkénti érték tickenként (kerekítve)
Fixed per_tick(Fixed per_second) {
  return Fixed::from_raw(static_cast<int32_t>((static_cast<int64_t>(per_second.raw()) + kTickHz / 2) / kTickHz));
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

void read_tags(const content::Value* arr, std::vector<std::string>& out) {
  if (!arr || !arr->is_array()) return;
  for (const auto& v : arr->items()) {
    if (v.is_string()) out.push_back(v.text());
  }
}

// `Modifiers`: [{"path", "op": add|mul|replace, "value", "filter": {"tags": [...], "id": "..."}}]
void parse_modifiers(const content::Value* arr, std::vector<Modifier>& out) {
  if (!arr || !arr->is_array()) return;
  for (const auto& m : arr->items()) {
    if (!m.is_object()) continue;
    Modifier mod;
    if (const content::Value* p = m.find("path"); p && p->is_string()) mod.path = p->text();
    if (const content::Value* o = m.find("op"); o && o->is_string()) {
      mod.op = o->text() == "mul" ? Modifier::Mul : o->text() == "replace" ? Modifier::Replace : Modifier::Add;
    }
    mod.value = fixed_of(m.find("value"), Fixed{});
    if (const content::Value* f = m.find("filter"); f && f->is_object()) {
      read_tags(f->find("tags"), mod.filter_tags);
      if (const content::Value* id = f->find("id"); id && id->is_string()) mod.filter_id = id->text();
    }
    if (!mod.path.empty()) out.push_back(std::move(mod));
  }
}

// egy napszak/időjárás hatásblokk (`game.json` → `environment.weather.<id>`, vagy a faji `environment.*.<id>`)
EnvEffect parse_env_effect(const content::Value& v) {
  EnvEffect e;
  e.speed_mult = fixed_of(v.find("speed_mult"), e.speed_mult);
  e.sight_mult = fixed_of(v.find("sight_mult"), e.sight_mult);
  e.range_mult = fixed_of(v.find("range_mult"), e.range_mult);
  e.gather_mult = fixed_of(v.find("gather_mult"), e.gather_mult);
  e.build_mult = fixed_of(v.find("build_mult"), e.build_mult);
  read_tags(v.find("range_tags"), e.range_tags);
  read_tags(v.find("sight_tags"), e.sight_tags);
  e.lightning_per_min = int_of(v.find("lightning_per_min"), 0);
  e.lightning_dmg = int_of(v.find("lightning_dmg"), 0);
  e.open_dmg_per_min = int_of(v.find("open_dmg_per_min"), 0);
  e.open_dmg = int_of(v.find("open_dmg"), 0);
  e.organic_dmg_per_min = int_of(v.find("organic_dmg_per_min"), 0);
  e.organic_dmg = int_of(v.find("organic_dmg"), 0);
  return e;
}

void parse_env_map(const content::Value* obj, std::map<std::string, EnvEffect>& out) {
  if (!obj || !obj->is_object()) return;
  for (const auto& [k, v] : obj->members()) {
    if (v.is_object()) out[k] = parse_env_effect(v);
  }
}

// `Rules` (KUT-RULE): a ma használt kulcsok – az ismeretlen kulcs a régi játékban is csak adat volt
void parse_rules(const content::Value* obj, const Rules& r, RuleSet& out) {
  if (!obj || !obj->is_object()) return;
  if (const content::Value* g = obj->find("garrison_dmg")) out.garrison_dmg = int_of(g, -1);
  if (const content::Value* sa = obj->find("supply_add"); sa && sa->is_object()) {
    out.has_supply_add = true;
    for (const auto& [bid, n] : sa->members()) {
      const uint16_t d = r.def(bid);
      if (d != kNoDef) out.supply_add.emplace_back(d, int_of(&n, 0));
    }
  }
  if (const content::Value* lr = obj->find("low_hp_rage"); lr && lr->is_object()) {
    out.has_low_hp_rage = true;
    out.lhr_below = fixed_of(lr->find("below"), Fixed::from_ratio(2, 5));
    out.lhr_mult = fixed_of(lr->find("mult"), Fixed::from_ratio(6, 5));
  }
  if (const content::Value* nw = obj->find("no_weak"); nw && nw->is_object()) {
    out.has_no_weak = true;
    for (const auto& [who, list] : nw->members()) {
      std::vector<std::string> classes;
      read_tags(&list, classes);
      out.no_weak.emplace_back(who, std::move(classes));
    }
  }
  if (const content::Value* rg = obj->find("rage"); rg && rg->is_object()) {
    out.has_rage = true;
    read_tags(rg->find("tags"), out.rage_tags);
    out.rage_max = fixed_of(rg->find("max"), Fixed{});
  }
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
  if (const content::Value* m = path(game, "unit_types", "matrix"); m && m->is_object()) {
    for (const auto& [att, row] : m->members()) {
      if (!row.is_object()) continue;
      for (const auto& [tgt, val] : row.members()) r.type_matrix[att][tgt] = fixed_of(&val, Fixed::from_int(1));
    }
  }
  r.shield_regen_delay = seconds_to_ticks(fixed_of(game.find("shield_regen_delay"), Fixed::from_int(10)));
  {
    const Fixed rate = fixed_of(game.find("shield_regen_rate"), Fixed::from_int(2));   // pajzs / s
    r.shield_regen_per_tick = Fixed::from_raw(static_cast<int32_t>((static_cast<int64_t>(rate.raw()) + kTickHz / 2) / kTickHz));
  }
  if (const content::Value* el = game.find("elevation_rules"); el && el->is_object()) {
    auto permille = [&](const char* k, int32_t def) {
      const content::Value* x = el->find(k);
      if (!x) return def;
      const Fixed f = fixed_of(x, Fixed{});
      return static_cast<int32_t>((static_cast<int64_t>(f.raw()) * 1000 + (1 << 15)) >> 16);
    };
    r.elevation.ranged_range_per_level = fixed_of(el->find("ranged_range_per_level"), r.elevation.ranged_range_per_level);
    r.elevation.ranged_range_max = fixed_of(el->find("ranged_range_max"), r.elevation.ranged_range_max);
    r.elevation.building_range_per_level = fixed_of(el->find("building_range_per_level"), r.elevation.building_range_per_level);
    r.elevation.uphill_miss_per_level = permille("uphill_miss_per_level", 250);
    r.elevation.uphill_miss_max = permille("uphill_miss_max", 400);
    r.elevation.damage_per_level = permille("damage_per_level", 100);
    r.elevation.vision_per_level = int_of(el->find("vision_per_level"), 1);
    r.elevation.depression_vision = int_of(el->find("depression_vision"), -1);
    r.elevation.uphill_presence = int_of(el->find("uphill_vision_needs_presence"), 2);
    r.elevation.reveal_shooter_ticks = seconds_to_ticks(fixed_of(el->find("reveal_shooter_seconds"), Fixed::from_int(2)));
    r.elevation.depression_hidden_beyond = int_of(el->find("depression_hidden_beyond"), 6);
  }
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

  // lövedékfajták (D40) – a sablonok előtt, mert a támadások ezekre hivatkoznak
  if (const content::Value* pr = game.find("projectiles"); pr && pr->is_object()) {
    for (const auto& [k, v] : pr->members()) {
      if (!v.is_object()) continue;   // a leírás és más nem objektum mezők
      ProjectileKind pk;
      pk.id = k;
      const Fixed sp = fixed_of(v.find("speed"), Fixed::from_int(15));
      pk.speed = Fixed::from_raw(static_cast<int32_t>((static_cast<int64_t>(sp.raw()) + kTickHz / 2) / kTickHz));
      auto flag = [&](const char* key) {
        const content::Value* x = v.find(key);
        return x && x->is_bool() && x->as_bool();
      };
      pk.homing = flag("homing");
      pk.ground = flag("ground");
      pk.line = flag("line");
      pk.never_miss = flag("never_miss");
      pk.returns = flag("returns");
      pk.arc = fixed_of(v.find("arc"), Fixed{});
      pk.bounces = int_of(v.find("bounces"), 0);
      pk.bounce_range = fixed_of(v.find("bounce_range"), pk.bounce_range);
      const Fixed bf = fixed_of(v.find("bounce_falloff"), Fixed::from_ratio(3, 4));
      pk.bounce_falloff = static_cast<int32_t>((static_cast<int64_t>(bf.raw()) * 1000 + (1 << 15)) >> 16);
      pk.impact = fixed_of(v.find("impact"), pk.impact);
      pk.range_mult = fixed_of(v.find("range_mult"), pk.range_mult);
      if (!pk.homing && !pk.ground && !pk.line) pk.homing = true;
      r.projectiles.push_back(pk);
    }
  }

  // elemhatások és a képességek elemei (`statuses.json`)
  std::map<std::string, Element> ability_element;
  {
    auto& st = set["status"];
    auto status_fx = [&](const char* id) -> const content::Value* {
      const content::Value* v = st.get(id);
      return v ? v->find("Status") : nullptr;
    };
    if (const content::Value* ice = status_fx("status_ice")) {
      r.element_fx.ice_ticks = seconds_to_ticks(fixed_of(ice->find("dur"), Fixed::from_int(2)));
      r.element_fx.ice_slow = fixed_of(ice->find("slow"), r.element_fx.ice_slow);
    }
    if (const content::Value* fear = status_fx("status_fear")) {
      r.element_fx.fear_ticks = seconds_to_ticks(fixed_of(fear->find("dur"), Fixed::from_ratio(8, 5)));
      r.element_fx.fear_flee = fixed_of(fear->find("flee"), r.element_fx.fear_flee);
    }
    const content::Value* asg = st.meta("_hozzarendeles");
    if (const content::Value* ab = asg ? asg->find("abilities") : nullptr; ab && ab->is_object()) {
      for (const auto& [aid, el] : ab->members()) {
        if (!el.is_string()) continue;
        if (el.text() == "ice") ability_element[aid] = Element::Ice;
        else if (el.text() == "fear") ability_element[aid] = Element::Fear;
      }
    }
  }
  r.energy_regen_per_tick = per_tick(fixed_of(game.find("energy_regen"), Fixed::from_ratio(4, 5)));
  if (const content::Value* rf = game.find("rift"); rf && rf->is_object()) {
    r.rift.radius = fixed_of(rf->find("radius"), r.rift.radius);
    r.rift.energy_mult = fixed_of(rf->find("energy_mult"), r.rift.energy_mult);
    r.rift.mage_range = fixed_of(rf->find("mage_range"), r.rift.mage_range);
    r.rift.spell_mult = fixed_of(rf->find("spell_mult"), r.rift.spell_mult);
  }

  // képességek (a sablonok előtt: az egységek a sorszámukra hivatkoznak; az idézett egység a sablonok után)
  std::vector<std::pair<std::string, std::string>> summon_units;   // képesség → idézett sablon
  {
    auto& abs = set["ability"];
    for (const std::string& id : abs.ids()) {
      const content::Value* v = abs.get(id);
      const content::Value* spec = v ? v->find("Spec") : nullptr;
      if (!spec || !spec->is_object()) continue;
      const content::Value* lv0 = nullptr;   // az 1. szint (hős)
      if (const content::Value* lv = spec->find("levels"); lv && lv->is_array() && !lv->items().empty() &&
                                                           lv->items()[0].is_object()) {
        lv0 = &lv->items()[0];
      }
      auto key = [&](const char* k) -> const content::Value* {
        if (lv0) {
          if (const content::Value* x = lv0->find(k)) return x;
        }
        return spec->find(k);
      };
      auto flag = [&](const char* k) {
        const content::Value* x = key(k);
        return x && x->is_bool() && x->as_bool();
      };
      AbilityDef a;
      a.id = id;
      const content::Value* t = spec->find("type");
      const std::string type = t && t->is_string() ? t->text() : "";
      using T = AbilityDef::Type;
      a.type = type == "garrison" ? T::Garrison : type == "self_buff" ? T::SelfBuff : type == "toggle" ? T::Toggle
             : type == "nova" ? T::Nova : type == "chant" ? T::Chant : type == "aura" ? T::Aura
             : type == "autoheal" ? T::Autoheal : type == "bolt" ? T::Bolt : type == "summon" ? T::Summon
             : type == "area" ? T::Area : type == "passive" ? T::Passive : type == "gpower" ? T::Power : T::Unknown;
      // alapértékek típusonként (KEPESSEGEK.md B-2)
      const bool cd10 = a.type == T::Nova || a.type == T::SelfBuff;
      const Fixed cd_def = cd10 ? Fixed::from_int(10) : (a.type == T::Summon || a.type == T::Autoheal) ? Fixed::from_int(1) : Fixed{};
      a.cooldown = seconds_to_ticks(fixed_of(key("cooldown"), cd_def));
      const Fixed dur_def = a.type == T::Bolt ? Fixed::from_ratio(3, 2) : a.type == T::Nova ? Fixed::from_int(2) : Fixed::from_int(4);
      a.duration = seconds_to_ticks(fixed_of(key("duration"), dur_def));
      const Fixed rad_def = a.type == T::Area ? Fixed::from_int(2) : a.type == T::Nova ? Fixed::from_ratio(5, 2) : Fixed::from_int(7);
      a.radius = fixed_of(key("radius"), rad_def);
      a.range = fixed_of(key("range"), a.type == T::Autoheal ? Fixed::from_int(5) : Fixed::from_int(6));
      a.energy = int_of(key("energy"), 0);
      a.damage = fixed_of(key("damage"), (a.type == T::Nova || a.type == T::Bolt) ? Fixed::from_int(50) : Fixed{});
      a.dps = fixed_of(key("dps"), Fixed{});
      a.heal = fixed_of(key(a.type == T::Autoheal ? "amount" : "heal"), a.type == T::Autoheal ? Fixed::from_int(10) : Fixed{});
      a.stun = seconds_to_ticks(fixed_of(key("stun"), Fixed{}));
      if (const content::Value* sl = key("slow"); sl && sl->is_number()) {
        a.has_slow = true;
        a.slow = fixed_of(sl, Fixed{});
      }
      if (const content::Value* pj = key("projectile"); pj && pj->is_string()) {
        const int pi = r.projectile_index(pj->text());
        if (pi >= 0) a.projectile = static_cast<uint8_t>(pi + 1);
      }
      a.count = std::max(1, int_of(key("count"), 1));
      a.lifetime = seconds_to_ticks(fixed_of(key("lifetime"), Fixed::from_int(30)));
      a.target_placement = flag("target_placement");
      a.drop_work = flag("drop_work");
      a.ultimate = flag("ultimate");
      if (const content::Value* b = key("buff"); b && b->is_string()) a.buff = b->text();
      if (const content::Value* ef = key("effect"); ef && ef->is_string()) a.effect = ef->text();
      if (const content::Value* hk = key("hotkey"); hk && hk->is_string() && hk->text().size() == 1) a.hotkey = hk->text()[0];
      if (const auto it = ability_element.find(id); it != ability_element.end()) a.element = it->second;
      auto mod = [&](const char* k, Fixed def) {
        for (const content::Value* src : {lv0 ? lv0->find("modifiers") : nullptr, spec->find("modifiers")}) {
          if (const content::Value* x = src && src->is_object() ? src->find(k) : nullptr) return fixed_of(x, def);
        }
        return def;
      };
      a.mods.speed_mult = mod("speed_mult", Fixed::from_int(1));
      a.mods.damage_mult = mod("damage_mult", Fixed::from_int(1));
      a.mods.ranged_taken_mult = mod("ranged_taken_mult", Fixed::from_int(1));
      a.mods.armor_add = mod("armor_add", Fixed{});
      a.mods.range_add = mod("range_add", Fixed{});
      a.mods.regen_add = mod("regen_add", Fixed{});
      if (const content::Value* u = key("unit"); u && u->is_string()) summon_units.emplace_back(id, u->text());
      r.abilities.push_back(std::move(a));
    }
  }

  std::sort(r.abilities.begin(), r.abilities.end(), [](const AbilityDef& a, const AbilityDef& b) { return a.id < b.id; });

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
    if (const content::Value* a = v->find("Attack"); a && a->is_object()) {
      AttackDef& at = d.attack;
      at.has = true;
      at.damage = fixed_of(a->find("damage"), Fixed{});
      at.range = fixed_of(a->find("range"), Fixed::from_ratio(3, 10));
      at.cooldown = std::max(1, seconds_to_ticks(fixed_of(a->find("cooldown"), Fixed::from_int(1))));
      if (const content::Value* tg = a->find("targets"); tg && tg->is_array()) {
        at.ground = at.air = false;
        for (const auto& t : tg->items()) {
          if (t.is_string() && t.text() == "ground") at.ground = true;
          if (t.is_string() && t.text() == "air") at.air = true;
        }
      }
      if (const content::Value* pj = a->find("projectile"); pj && pj->is_string()) {
        int pi = r.projectile_index(pj->text());
        if (pi < 0) {   // ismeretlen fajta: követő lövedék (a régi játék minden lövedéke ilyen volt)
          ProjectileKind pk;
          pk.id = pj->text();
          r.projectiles.push_back(pk);
          pi = static_cast<int>(r.projectiles.size()) - 1;
        }
        at.projectile = static_cast<uint8_t>(pi + 1);
      }
      if (const content::Value* ps = a->find("proj_speed")) {   // egységenként felülírható
        const Fixed f = fixed_of(ps, Fixed::from_int(15));
        at.proj_speed = Fixed::from_raw(static_cast<int32_t>((static_cast<int64_t>(f.raw()) + kTickHz / 2) / kTickHz));
      }
      at.splash = fixed_of(a->find("splash"), Fixed{});
      for (const char* key : {"bonus", "type_bonus"}) {
        if (const content::Value* b = a->find(key); b && b->is_object()) {
          for (const auto& [k, val] : b->members()) {
            (key[0] == 'b' ? at.bonus : at.type_bonus).emplace_back(k, fixed_of(&val, Fixed{}));
          }
        }
      }
    }
    if (const content::Value* cl = path(*v, "Identity", "class"); cl && cl->is_string()) d.utype = cl->text();
    d.shield = int_of(path(*v, "Health", "shield"), 0);
    d.detect = int_of(path(*v, "Vision", "detect"), 0);
    const content::Value* ah = path(*v, "Vision", "always_hidden");
    d.always_hidden = ah && ah->is_bool() && ah->as_bool();
    d.shield_delay = seconds_to_ticks(fixed_of(path(*v, "Health", "shield_delay"), Fixed{}));
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
    // képességek (M6 8. lépés): az egység listája, majd a hősé
    auto add_ability = [&](const content::Value& x) {
      if (!x.is_string()) return;
      const int ai = r.ability_index(x.text());
      if (ai >= 0 && std::find(d.abilities.begin(), d.abilities.end(), ai) == d.abilities.end()) {
        d.abilities.push_back(static_cast<uint16_t>(ai));
      }
    };
    if (const content::Value* al = path(*v, "Abilities", "list"); al && al->is_array()) {
      for (const auto& x : al->items()) add_ability(x);
    }
    if (const content::Value* ha = path(*v, "Hero", "abilities"); ha && ha->is_array()) {
      for (const auto& x : ha->items()) add_ability(x);
    }
    if (const content::Value* ar = path(*v, "Abilities", "research"); ar && ar->is_object()) {
      for (const auto& [aid, rid] : ar->members()) {
        const int ai = r.ability_index(aid);
        if (ai >= 0 && rid.is_string()) d.ability_research.emplace_back(static_cast<uint16_t>(ai), rid.text());
      }
    }
    d.energy_max = int_of(path(*v, "Energy", "max"), 0);
    d.regen = fixed_of(path(*v, "Health", "regen"), Fixed{});
    d.garrison = int_of(path(*v, "Garrison", "slots"), 0);
    if (const content::Value* sh = path(*v, "Identity", "shape"); sh && sh->is_string()) d.shape = sh->text();
    const content::Value* rsrc = path(*v, "Mechanics", "rift_source");
    d.rift_source = rsrc && rsrc->is_bool() && rsrc->as_bool();
    if (const content::Value* pw = path(*v, "Powers", "list"); pw && pw->is_array()) {
      for (const auto& x : pw->items()) {
        const int ai = x.is_string() ? r.ability_index(x.text()) : -1;
        if (ai >= 0) d.powers.push_back(static_cast<uint16_t>(ai));
      }
    }
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
  for (const auto& [aid, uid] : summon_units) {
    const int ai = r.ability_index(aid);
    if (ai >= 0) r.abilities[static_cast<size_t>(ai)].unit = r.def(uid);
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
    parse_modifiers(v->find("Modifiers"), rd.modifiers);
    parse_rules(v->find("Rules"), r, rd.rules);
    r.researches.push_back(std::move(rd));
  }
  std::sort(r.researches.begin(), r.researches.end(), [](const ResearchDef& a, const ResearchDef& b) { return a.id < b.id; });

  // napszak és időjárás (D35, M6 7. lépés)
  if (const content::Value* env = game.find("environment"); env && env->is_object()) {
    r.env.day_cycle_ticks = std::max(1, int_of(env->find("day_cycle_ticks"), r.env.day_cycle_ticks));
    r.env.hold_min_ticks = int_of(env->find("hold_min_ticks"), r.env.hold_min_ticks);
    r.env.hold_max_ticks = int_of(env->find("hold_max_ticks"), r.env.hold_max_ticks);
    if (const content::Value* ht = env->find("hold_ticks"); ht && ht->is_array() && ht->items().size() == 2) {
      r.env.hold_min_ticks = int_of(&ht->items()[0], r.env.hold_min_ticks);
      r.env.hold_max_ticks = int_of(&ht->items()[1], r.env.hold_max_ticks);
    }
    r.env.transition_ticks = int_of(env->find("transition_ticks"), r.env.transition_ticks);
    r.env.night_build_mult = fixed_of(env->find("night_build_mult"), r.env.night_build_mult);
    r.env.night_sight_mult = fixed_of(env->find("night_sight_mult"), r.env.night_sight_mult);
    if (const content::Value* cl = env->find("climates"); cl && cl->is_object()) {
      for (const auto& [name, list] : cl->members()) {
        if (!list.is_array()) continue;
        std::vector<std::string> kinds;
        for (const auto& k : list.items()) {
          if (k.is_string()) kinds.push_back(k.text());
        }
        r.env.climates[name] = std::move(kinds);
      }
    }
    parse_env_map(env->find("weather_effects"), r.env.weather);
  }

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
    if (const content::Value* fe = f->find("environment"); fe && fe->is_object()) {
      parse_env_map(fe->find("daytime"), fd.env_daytime);
      parse_env_map(fe->find("weather"), fd.env_weather);
    }
    if (const content::Value* asc = f->find("ascension"); asc && asc->is_object()) {
      fd.ascension_buildings = int_of(asc->find("buildings"), 3);
    }
    if (const content::Value* ps = v->find("Paths"); ps && ps->is_object()) {
      // a régi KUT-EFF hatás ({stat, tags, add}) D24 módosítóként
      static const std::map<std::string, std::string> kStat = {
          {"hp", "Health.max"}, {"armor", "Health.armor"}, {"damage", "Attack.damage"}, {"regen", "Health.regen"},
          {"range", "Attack.range"}, {"sight", "Vision.sight"}, {"speed_pct", "Movement.speed_pct"},
          {"shield", "Health.shield"}, {"carry_pct", "Worker.carry_pct"}};
      for (const auto& [pid, pv] : ps->members()) {
        if (!pv.is_object()) continue;
        PathDef pd;
        pd.id = pid;
        parse_rules(pv.find("rules"), r, pd.rules);
        if (const content::Value* ef = pv.find("effects"); ef && ef->is_array()) {
          for (const auto& e : ef->items()) {
            const content::Value* st = e.find("stat");
            const auto it = st && st->is_string() ? kStat.find(st->text()) : kStat.end();
            if (it == kStat.end()) continue;
            Modifier m;
            m.path = it->second;
            m.value = fixed_of(e.find("add"), Fixed{});
            read_tags(e.find("tags"), m.filter_tags);
            pd.effects.push_back(std::move(m));
          }
        }
        fd.paths.push_back(std::move(pd));
      }
    }
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

int Rules::ability_index(const std::string& id) const {
  const auto it = std::lower_bound(abilities.begin(), abilities.end(), id,
                                   [](const AbilityDef& x, const std::string& v) { return x.id < v; });
  return it != abilities.end() && it->id == id ? static_cast<int>(it - abilities.begin()) : -1;
}

const PathDef* FactionDef::path(const std::string& pid) const {
  for (const PathDef& p : paths) {
    if (p.id == pid) return &p;
  }
  return nullptr;
}

void RuleSet::merge(const RuleSet& o) {
  if (o.garrison_dmg >= 0) garrison_dmg = o.garrison_dmg;
  if (o.has_supply_add) {
    has_supply_add = true;
    supply_add = o.supply_add;
  }
  if (o.has_low_hp_rage) {
    has_low_hp_rage = true;
    lhr_below = o.lhr_below;
    lhr_mult = o.lhr_mult;
  }
  if (o.has_no_weak) {
    has_no_weak = true;
    no_weak = o.no_weak;
  }
  if (o.has_rage) {
    has_rage = true;
    rage_tags = o.rage_tags;
    rage_max = o.rage_max;
  }
}

bool AbilityMods::empty() const {
  const Fixed one = Fixed::from_int(1);
  return speed_mult == one && damage_mult == one && ranged_taken_mult == one && armor_add.raw() == 0 &&
         range_add.raw() == 0 && regen_add.raw() == 0;
}

int Rules::projectile_index(const std::string& id) const {
  for (size_t i = 0; i < projectiles.size(); ++i) {
    if (projectiles[i].id == id) return static_cast<int>(i);
  }
  return -1;
}

Fixed Rules::type_mult(const std::string& attacker, const std::string& target) const {
  const auto a = type_matrix.find(attacker);
  if (a == type_matrix.end()) return Fixed::from_int(1);
  const auto t = a->second.find(target);
  return t == a->second.end() ? Fixed::from_int(1) : t->second;
}

namespace {
// a régi `stat()` szűrője: címke, a sablon azonosítója, `all`, `building` (minden épület), `tower` (támadó épület)
bool modifier_matches(const Modifier& m, const EntityDef& d) {
  if (!m.filter_id.empty()) return m.filter_id == d.id;
  if (m.filter_tags.empty()) return true;
  for (const std::string& t : m.filter_tags) {
    if (d.has_tag(t) || t == d.id || t == "all" || (d.building && t == "building") ||
        (d.building && d.attack.has && t == "tower")) {
      return true;
    }
  }
  return false;
}
}  // namespace

Fixed apply_modifiers(const std::vector<const std::vector<Modifier>*>& lists, const std::string& path, Fixed base,
                      const EntityDef& d) {
  Fixed add{}, mul = Fixed::from_int(1);
  bool has_replace = false;
  Fixed replace{};
  for (const std::vector<Modifier>* list : lists) {
    if (!list) continue;
    for (const Modifier& m : *list) {
      if (m.path != path || !modifier_matches(m, d)) continue;
      if (m.op == Modifier::Add) add = add + m.value;
      else if (m.op == Modifier::Mul) mul = mul * m.value;
      else { has_replace = true; replace = m.value; }
    }
  }
  if (has_replace) return replace;
  return (base + add) * mul;
}

int32_t Rules::upkeep_permille(int32_t used) const {
  if (upkeep_tiers.empty()) return 1000;
  for (const auto& [top, f] : upkeep_tiers) {
    if (used <= top) return f;
  }
  return upkeep_tiers.back().second;
}

}  // namespace totu::sim
