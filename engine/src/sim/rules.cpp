// SPDX-License-Identifier: GPL-3.0-or-later
#include "sim/rules.hpp"

#include <algorithm>
#include <cstdlib>

namespace totu::sim {

namespace {

constexpr const char* kResIds[kResources] = {"wood", "gold", "meteor", "faith"};

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
  out.bounty_gold = fixed_of(obj->find("bounty_gold"), out.bounty_gold);
  if (const content::Value* rg = obj->find("rage"); rg && rg->is_object()) {
    out.has_rage = true;
    read_tags(rg->find("tags"), out.rage_tags);
    out.rage_max = fixed_of(rg->find("max"), Fixed{});
  }
  if (const content::Value* mo = obj->find("moon"); mo && mo->is_object()) {   // Holdtölte (a régi G 1098)
    out.has_moon = true;
    out.moon_period = std::max(1, seconds_to_ticks(fixed_of(mo->find("period"), Fixed::from_int(240))));
    out.moon_dur = seconds_to_ticks(fixed_of(mo->find("dur"), Fixed::from_int(60)));
    out.moon_mult = fixed_of(mo->find("mult"), Fixed::from_int(1));
  }
  if (const content::Value* hk = obj->find("heal_on_kill"); hk && hk->is_object()) {   // a régi G 1201
    out.has_heal_on_kill = true;
    read_tags(hk->find("tags"), out.hok_tags);
    out.hok_amount = int_of(hk->find("amount"), 0);
  }
  if (const content::Value* da = obj->find("detect_all")) out.detect_all = int_of(da, 0);
  if (const content::Value* ps = obj->find("pond_size")) out.pond_size = int_of(ps, 0);
  out.summon_life_mult = fixed_of(obj->find("summon_life_mult"), out.summon_life_mult);
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
  r.game_raw = std::make_shared<const content::Value>(game);
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

  // elemhatások, a képességek és az egységek elemei (`statuses.json`; a `kezi` felülírja, a null törli)
  std::map<std::string, Element> ability_element, unit_element;
  {
    auto& st = set["status"];
    auto fx = [&](const char* id) -> const content::Value* {
      const content::Value* v = st.get(id);
      return v ? v->find("Status") : nullptr;
    };
    auto ticks = [&](const content::Value* v, const char* k, int32_t def) {
      return v && v->find(k) ? seconds_to_ticks(fixed_of(v->find(k), Fixed{})) : def;
    };
    auto& e = r.element_fx;
    if (const content::Value* v = fx("status_ice")) {
      e.ice_ticks = ticks(v, "dur", e.ice_ticks);
      e.ice_slow = fixed_of(v->find("slow"), e.ice_slow);
    }
    if (const content::Value* v = fx("status_fear")) {
      e.fear_ticks = ticks(v, "dur", e.fear_ticks);
      e.fear_flee = fixed_of(v->find("flee"), e.fear_flee);
    }
    if (const content::Value* v = fx("status_fire")) {
      e.fire_ticks = ticks(v, "dur", e.fire_ticks);
      e.fire_ratio = fixed_of(v->find("ratio"), e.fire_ratio);
      e.fire_min = fixed_of(v->find("min_dps"), e.fire_min);
      e.fire_max = fixed_of(v->find("max_dps"), e.fire_max);
    }
    if (const content::Value* v = fx("status_poison")) {
      e.poison_ticks = ticks(v, "dur", e.poison_ticks);
      e.poison_ratio = fixed_of(v->find("ratio"), e.poison_ratio);
      e.poison_min = fixed_of(v->find("min_dps"), e.poison_min);
      e.poison_max = fixed_of(v->find("max_dps"), e.poison_max);
    }
    if (const content::Value* v = fx("status_lightning")) {
      e.shock_ticks = ticks(v, "dur", e.shock_ticks);
      e.shock_cd = ticks(v, "cooldown", e.shock_cd);
    }
    if (const content::Value* v = fx("status_water")) {
      e.wet_ticks = ticks(v, "dur", e.wet_ticks);
      e.shock_mult = fixed_of(v->find("shock_mult"), e.shock_mult);
    }
    if (const content::Value* v = fx("status_root")) e.root_ticks = ticks(v, "dur", e.root_ticks);
    if (const content::Value* v = fx("status_sleep")) e.sleep_ticks = ticks(v, "dur", e.sleep_ticks);
    if (const content::Value* v = fx("status_silence")) e.silence_ticks = ticks(v, "dur", e.silence_ticks);
    if (const content::Value* v = fx("status_blind")) {
      e.blind_ticks = ticks(v, "dur", e.blind_ticks);
      e.blind_miss = static_cast<int32_t>((static_cast<int64_t>(fixed_of(v->find("miss"), Fixed::from_ratio(1, 2)).raw()) * 1000) >> 16);
    }
    if (const content::Value* v = fx("status_mark")) {
      e.mark_ticks = ticks(v, "dur", e.mark_ticks);
      e.mark_mult = fixed_of(v->find("dmg_mult"), e.mark_mult);
    }
    if (const content::Value* v = fx("status_sunder")) {
      e.sunder_ticks = ticks(v, "dur", e.sunder_ticks);
      e.sunder_armor = int_of(v->find("armor"), e.sunder_armor);
    }
    if (const content::Value* v = fx("status_wind")) e.wind_push = fixed_of(v->find("push"), e.wind_push);
    const content::Value* asg = st.meta("_hozzarendeles");
    auto read_map = [&](const content::Value* m, std::map<std::string, Element>& out) {
      if (!m || !m->is_object()) return;
      for (const auto& [id, el] : m->members()) {
        if (el.is_string()) out[id] = element_of(el.text());
        else out[id] = Element::None;   // null: nincs eleme
      }
    };
    if (asg) {
      read_map(asg->find("abilities"), ability_element);
      read_map(asg->find("units"), unit_element);
      if (const content::Value* k = asg->find("kezi")) {
        read_map(k->find("abilities"), ability_element);
        read_map(k->find("units"), unit_element);
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
    for (const std::string& base_id : abs.ids()) {
      const content::Value* v = abs.get(base_id);
      const content::Value* spec = v ? v->find("Spec") : nullptr;
      if (!spec || !spec->is_object()) continue;
      // szintenként egy változat (hős, D25): az 1. szint az alap azonosító, a többi `id@n`
      const content::Value* lvs = spec->find("levels");
      const int nlev = lvs && lvs->is_array() && !lvs->items().empty() ? static_cast<int>(lvs->items().size()) : 1;
      for (int level = 1; level <= nlev; ++level) {
      const std::string id = level == 1 ? base_id : base_id + "@" + std::to_string(level);
      const content::Value* lv0 = nullptr;   // a szint adatai (a `levels[n]` felülírja az alapot)
      if (lvs && lvs->is_array() && static_cast<int>(lvs->items().size()) >= level && lvs->items()[static_cast<size_t>(level - 1)].is_object()) {
        lv0 = &lvs->items()[static_cast<size_t>(level - 1)];
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
      a.level = static_cast<uint8_t>(level);
      a.spec = std::make_shared<const content::Value>(*spec);
      a.max_level = static_cast<uint8_t>(nlev);
      const content::Value* t = spec->find("type");
      const std::string type = t && t->is_string() ? t->text() : "";
      using T = AbilityDef::Type;
      a.type = type == "garrison" ? T::Garrison : type == "self_buff" ? T::SelfBuff : type == "toggle" ? T::Toggle
             : type == "nova" ? T::Nova : type == "chant" ? T::Chant : type == "aura" ? T::Aura
             : type == "autoheal" ? T::Autoheal : type == "bolt" ? T::Bolt : type == "summon" ? T::Summon
             : type == "area" ? T::Area : type == "passive" ? T::Passive : type == "gpower" ? T::Power
             : type == "blink" ? T::Blink : type == "mount" ? T::Mount : type == "dismount" ? T::Dismount
             : type == "capture" ? T::Capture : type == "occupy" ? T::Occupy : T::Unknown;
      // alapértékek típusonként (KEPESSEGEK.md B-2)
      const bool cd10 = a.type == T::Nova || a.type == T::SelfBuff || a.type == T::Blink;
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
      for (int ri = 0; ri < kResources; ++ri) a.cost[static_cast<size_t>(ri)] = int_of(key(resource_id(ri)), 0);
      a.hq_hp = int_of(key("hq_hp"), 0);
      a.cast = seconds_to_ticks(fixed_of(key("cast"), Fixed{}));
      a.below = fixed_of(key("below"), Fixed::from_int(1));
      a.reveal = int_of(key("reveal"), 0);
      a.targeted = flag("target");
      a.hits_buildings = flag("buildings");
      a.land_damage = fixed_of(key("land_damage"), Fixed{});
      a.land_radius = fixed_of(key("land_radius"), a.land_radius);
      a.hp_cost = int_of(key("hp_cost"), 0);
      a.bounces = int_of(key("bounces"), 0);
      a.splash = fixed_of(key("splash"), Fixed{});
      if (const content::Value* pz = key("poison"); pz && pz->is_array() && pz->items().size() == 2) {
        a.poison_dps = fixed_of(&pz->items()[0], Fixed{});
        a.poison_ticks = seconds_to_ticks(fixed_of(&pz->items()[1], Fixed{}));
      }
      a.building_damage = fixed_of(key("building_damage"), Fixed{});
      a.self_destruct = flag("self_destruct");
      a.no_weak = flag("no_weak");
      a.after_slow = seconds_to_ticks(fixed_of(key("after_slow"), Fixed{}));
      if (const content::Value* ht = key("heal_tags"); ht && ht->is_array()) {
        for (const auto& tg : ht->items()) {
          if (tg.is_string()) a.heal_tags.push_back(tg.text());
        }
      }
      a.detect = fixed_of(key("detect"), Fixed{});
      if (const content::Value* rr = key("range_research"); rr && rr->is_object()) {
        for (const auto& [rid, rv] : rr->members()) a.range_research.emplace_back(rid, fixed_of(&rv, a.range));
      }
      if (const content::Value* hk = key("hotkey"); hk && hk->is_string() && hk->text().size() == 1) a.hotkey = hk->text()[0];
      if (const auto it = ability_element.find(base_id); it != ability_element.end()) a.element = it->second;
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
      for (auto [k, dst] : {std::pair<const char*, std::string*>{"into", &a.into_id}, {"mount", &a.mount_id}, {"rider", &a.rider_id}}) {
        if (const content::Value* x = key(k); x && x->is_string()) *dst = x->text();
      }
      read_tags(key("riders"), a.rider_ids);
      if (a.type == T::Mount && !key("range")) a.range = Fixed::from_int(3);
      r.abilities.push_back(std::move(a));
      }
    }
  }

  std::sort(r.abilities.begin(), r.abilities.end(), [](const AbilityDef& a, const AbilityDef& b) { return a.id < b.id; });
  for (AbilityDef& a : r.abilities) {   // az alap (1. szintű) változat sorszáma
    const int bi = r.ability_index(a.id.substr(0, a.id.find('@')));
    a.base = static_cast<uint16_t>(bi >= 0 ? bi : 0);
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
      if (const auto it = unit_element.find(id); it != unit_element.end()) at.element = it->second;
      at.slow = fixed_of(a->find("slow"), Fixed{});
      at.slow_ticks = seconds_to_ticks(fixed_of(a->find("slow_dur"), Fixed::from_ratio(3, 2)));
      if (const content::Value* pz = a->find("poison"); pz && pz->is_array() && pz->items().size() == 2) {
        at.poison_dps = fixed_of(&pz->items()[0], Fixed{});
        at.poison_ticks = seconds_to_ticks(fixed_of(&pz->items()[1], Fixed{}));
      }
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
          else if (k == "ally") req.kind = EntityDef::Req::Ally;
          else continue;
          if (val.is_string()) req.id = val.text();
          d.reqs.push_back(req);
        }
      }
    }
    d.dropoff = res_mask(path(*v, "Dropoff", "resources"));
    d.raw = std::make_shared<const content::Value>(*v);
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
    if (const content::Value* gr = path(*v, "Hero", "growth"); gr && gr->is_object()) {
      d.grow_hp = fixed_of(gr->find("hp"), Fixed{});
      d.grow_damage = fixed_of(gr->find("damage"), Fixed{});
      d.grow_armor = fixed_of(gr->find("armor"), Fixed{});
      d.grow_energy = fixed_of(gr->find("energy"), Fixed{});
      d.grow_shield = fixed_of(gr->find("shield"), Fixed{});
    }
    d.xp_value = int_of(path(*v, "Hero", "xp"), -1);
    d.count = std::max(1, int_of(path(*v, "Cost", "count"), 1));
    if (const content::Value* tw = path(*v, "Worker", "tap_wood"); tw && tw->is_object()) {
      d.tap_amount = int_of(tw->find("amount"), 8);
      d.tap_ticks = seconds_to_ticks(fixed_of(tw->find("time"), Fixed::from_int(10)));
      d.tap_drain = fixed_of(tw->find("drain"), Fixed::from_ratio(1, 2));
    }
    const content::Value* pk = path(*v, "Placement", "packable");
    d.packable = pk && pk->is_bool() && pk->as_bool();
    const content::Value* dip = path(*v, "Mechanics", "diplomacy");
    d.diplomacy = dip && dip->is_bool() && dip->as_bool();
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
  for (EntityDef& d : r.defs) {   // a régi (`Legacy`) darabkorlát és falka: a rendezés után (sorszámmal)
    const content::Value* v = ents.get(d.id);
    if (const content::Value* pb = v ? path(*v, "Legacy", "per_building") : nullptr; pb && pb->is_object()) {
      if (const content::Value* b = pb->find("building"); b && b->is_string()) d.per_building = r.def(b->text());
      d.per_building_max = int_of(pb->find("max"), 0);
    }
    if (const content::Value* sg = v ? path(*v, "Legacy", "spawn_group") : nullptr; sg && sg->is_object()) {
      if (const content::Value* u = sg->find("unit"); u && u->is_string()) d.spawn_unit = r.def(u->text());
      d.spawn_count = std::max(1, int_of(sg->find("count"), 3));
    }
  }
  for (AbilityDef& a : r.abilities) {   // felültetés / leszállás: a sablonok sorszáma
    if (!a.into_id.empty()) a.into = r.def(a.into_id);
    if (!a.mount_id.empty()) a.mount = r.def(a.mount_id);
    if (!a.rider_id.empty()) a.rider = r.def(a.rider_id);
    for (const auto& rid : a.rider_ids) {
      if (r.def(rid) != kNoDef) a.riders.push_back(r.def(rid));
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
          else if (k == "ally") req.kind = EntityDef::Req::Ally;
          else continue;
          if (val.is_string()) req.id = val.text();
          rd.reqs.push_back(req);
        }
      }
    }
    parse_modifiers(v->find("Modifiers"), rd.modifiers);
    parse_rules(v->find("Rules"), r, rd.rules);
    rd.raw = std::make_shared<const content::Value>(*v);
    r.researches.push_back(std::move(rd));
  }
  std::sort(r.researches.begin(), r.researches.end(), [](const ResearchDef& a, const ResearchDef& b) { return a.id < b.id; });

  // tárgyak (D25)
  auto& its = set["item"];
  for (const std::string& id : its.ids()) {
    const content::Value* v = its.get(id);
    const content::Value* it = v ? v->find("Item") : nullptr;
    if (!it || !it->is_object()) continue;
    ItemDef d;
    d.id = id;
    const content::Value* k = it->find("kind");
    d.equip = k && k->is_string() && k->text() == "equip";
    const content::Value* ra = it->find("rarity");
    const std::string rar = ra && ra->is_string() ? ra->text() : "common";
    d.rarity = rar == "artifact" ? 2 : rar == "rare" ? 1 : 0;
    d.heal = int_of(it->find("heal"), 0);
    d.energy = int_of(it->find("energy"), 0);
    d.xp = int_of(it->find("xp"), 0);
    d.perm_hp = int_of(path(*it, "perm", "hp"), 0);
    d.perm_damage = int_of(path(*it, "perm", "damage"), 0);
    parse_modifiers(v->find("Modifiers"), d.modifiers);
    r.items.push_back(std::move(d));
  }
  std::sort(r.items.begin(), r.items.end(), [](const ItemDef& a, const ItemDef& b) { return a.id < b.id; });
  if (const content::Value* cp = game.find("capture"); cp && cp->is_object()) {   // D30
    if (const content::Value* sl = cp->find("occupy_slots"); sl && sl->is_array()) {
      r.capture.slots.clear();
      for (const auto& x : sl->items()) {
        if (x.is_array() && x.items().size() == 2) r.capture.slots.emplace_back(int_of(&x.items()[0], 0), int_of(&x.items()[1], 0));
      }
    }
    r.capture.evict_below = fixed_of(cp->find("evict_below"), r.capture.evict_below);
  }
  if (const content::Value* pm = game.find("promotion"); pm && pm->is_object()) {
    if (const content::Value* rk = pm->find("ranks"); rk && rk->is_array()) {
      for (const auto& x : rk->items()) {
        Rules::Rank k;
        if (const content::Value* id = x.find("id"); id && id->is_string()) k.id = id->text();
        k.cost_mult = fixed_of(x.find("cost_mult"), Fixed::from_int(2));
        k.hp = fixed_of(x.find("hp"), Fixed{});
        k.damage = fixed_of(x.find("damage"), Fixed{});
        k.armor = int_of(x.find("armor"), 0);
        r.ranks.push_back(k);
      }
    }
  }
  if (const content::Value* he = game.find("heroes"); he && he->is_object()) {
    if (const content::Value* lv = he->find("levels"); lv && lv->is_array()) {
      r.heroes.levels.clear();
      for (const auto& x : lv->items()) r.heroes.levels.push_back(int_of(&x, 0));
    }
    r.heroes.share_radius = fixed_of(he->find("xp_share_radius"), r.heroes.share_radius);
    r.heroes.inventory = int_of(he->find("inventory"), r.heroes.inventory);
    r.heroes.revive_base = fixed_of(he->find("revive_base"), r.heroes.revive_base);
    r.heroes.revive_per_level = fixed_of(he->find("revive_per_level"), r.heroes.revive_per_level);
    r.heroes.levelup_heal = fixed_of(he->find("levelup_heal"), r.heroes.levelup_heal);
    r.heroes.ultimate_level = int_of(he->find("ultimate_level"), r.heroes.ultimate_level);
    r.heroes.xp_hero_base = int_of(he->find("xp_hero_base"), r.heroes.xp_hero_base);
    r.heroes.xp_hero_per_level = int_of(he->find("xp_hero_per_level"), r.heroes.xp_hero_per_level);
    r.heroes.xp_unit_base = fixed_of(he->find("xp_unit_base"), r.heroes.xp_unit_base);
    r.heroes.xp_cost_mult = fixed_of(he->find("xp_cost_mult"), r.heroes.xp_cost_mult);
  }
  if (const content::Value* ca = game.find("camps"); ca && ca->is_object()) {
    r.camps.home = fixed_of(ca->find("home_radius"), r.camps.home);
    r.camps.search = fixed_of(ca->find("search_radius"), r.camps.search);
    r.camps.leash = fixed_of(ca->find("leash"), r.camps.leash);
    r.camps.regen_pct = fixed_of(ca->find("return_regen"), r.camps.regen_pct);
    r.camps.potion_level = int_of(ca->find("potion_from_level"), r.camps.potion_level);
    r.camps.potion_permille = static_cast<int32_t>((static_cast<int64_t>(fixed_of(ca->find("potion_chance"), Fixed::from_ratio(3, 5)).raw()) * 1000) >> 16);
    r.camps.min_start_dist = int_of(ca->find("min_start_dist"), r.camps.min_start_dist);
    if (const content::Value* rc = ca->find("recipe"); rc && rc->is_object()) {
      for (const auto& [lvl, rv] : rc->members()) {
        const int l = std::atoi(lvl.c_str());
        if (l < 1 || l > 5 || !rv.is_object()) continue;
        for (const auto& [role, n] : rv.members()) r.camps.recipe[static_cast<size_t>(l)].emplace_back(role, int_of(&n, 0));
      }
    }
  }

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
    fd.regen = fixed_of(f->find("regen"), Fixed{});
    fd.raw = std::make_shared<const content::Value>(*v);
    const content::Value* al = f->find("ally");
    fd.ally = al && al->is_bool() && al->as_bool();
    read_res_map(f->find("ally_cost"), fd.ally_cost);
    if (const content::Value* pb = f->find("pack_bonus"); pb && pb->is_object()) {
      fd.pack = true;
      fd.pack_radius = fixed_of(pb->find("radius"), fd.pack_radius);
      fd.pack_per = fixed_of(pb->find("per"), fd.pack_per);
      fd.pack_max = fixed_of(pb->find("max"), fd.pack_max);
      fd.pack_speed_per = fixed_of(pb->find("speed_per"), Fixed{});
    }
    fd.bounty = fixed_of(f->find("bounty"), Fixed{});
    if (const content::Value* br = f->find("bounty_res"); br && br->is_string() && resource_index(br->text()) >= 0) {
      fd.bounty_res = static_cast<uint8_t>(resource_index(br->text()));
    }
    auto fflag = [&](const char* k) {
      const content::Value* x = f->find(k);
      return x && x->is_bool() && x->as_bool();
    };
    fd.forest_stealth = fflag("forest_stealth");
    fd.repair = fflag("repair");
    if (const content::Value* wg = f->find("wagon"); wg && wg->is_string()) fd.wagon = r.def(wg->text());
    fd.unpack_ticks = seconds_to_ticks(fixed_of(f->find("unpack_time"), Fixed::from_int(8)));
    const content::Value* neu = f->find("neutral");
    fd.neutral = neu && neu->is_bool() && neu->as_bool();
    if (const content::Value* cm = f->find("camps"); cm && cm->is_object()) {
      for (const auto& [lvl, cv] : cm->members()) {
        if (!cv.is_object()) continue;
        CampDef c;
        c.level = std::atoi(lvl.c_str());
        if (const content::Value* us = cv.find("units"); us && us->is_object()) {
          for (const auto& [uid, n] : us->members()) {
            const uint16_t ud = r.def(uid);
            if (ud != kNoDef) c.units.emplace_back(ud, int_of(&n, 1));
          }
        }
        if (const content::Value* lt = cv.find("loot"); lt && lt->is_array()) {
          for (const auto& x : lt->items()) {
            if (!x.is_string()) continue;
            c.loot.push_back(x.text() == "artifact" ? 2 : x.text() == "rare" ? 1 : 0);
          }
        }
        fd.camps.push_back(std::move(c));
      }
      std::sort(fd.camps.begin(), fd.camps.end(), [](const CampDef& a, const CampDef& b) { return a.level < b.level; });
    }
    // kisnép-tábor (TABOR-1, `Camp`): szintenként a nép saját összetétele (`levels`), ha nincs, a szerepköreiből
    // (`roles`) a `game.json` recept szerint; a zsákmány a Vadoné (a táborkezelés a Vadon szintjéből veszi)
    if (const content::Value* cp = v->find("Camp"); cp && cp->is_object() && fd.camps.empty()) {
      const content::Value* roles = cp->find("roles");
      const content::Value* levels = cp->find("levels");
      for (int l = 1; l <= 5; ++l) {
        CampDef c;
        c.level = l;
        auto add = [&](const std::string& uid, int n) {
          const uint16_t ud = r.def(uid);
          if (ud == kNoDef || n <= 0) return;
          for (auto& [d, k] : c.units) {
            if (d == ud) {
              k += n;
              return;
            }
          }
          c.units.emplace_back(ud, n);
        };
        const content::Value* fixed = levels ? levels->find(std::to_string(l)) : nullptr;
        if (fixed && fixed->is_object()) {
          for (const auto& [uid, n] : fixed->members()) add(uid, int_of(&n, 0));
        } else if (roles && roles->is_object()) {
          for (const auto& [role, n] : r.camps.recipe[static_cast<size_t>(l)]) {
            if (const content::Value* u = roles->find(role); u && u->is_string()) add(u->text(), n);
          }
        }
        if (!c.units.empty()) fd.camps.push_back(std::move(c));
      }
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

Element element_of(const std::string& n) {
  static const char* kNames[] = {"", "ice", "fear", "fire", "lightning", "poison", "water", "root", "sleep", "blind", "wind",
                                 "sunder", "silence", "mark"};
  for (int i = 1; i < static_cast<int>(Element::Count); ++i) {
    if (n == kNames[i]) return static_cast<Element>(i);
  }
  return Element::None;
}

int Rules::item_index(const std::string& id) const {
  const auto it = std::lower_bound(items.begin(), items.end(), id, [](const ItemDef& x, const std::string& v) { return x.id < v; });
  return it != items.end() && it->id == id ? static_cast<int>(it - items.begin()) : -1;
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
  if (o.bounty_gold.raw() > 0) bounty_gold = o.bounty_gold;
  if (o.has_rage) {
    has_rage = true;
    rage_tags = o.rage_tags;
    rage_max = o.rage_max;
  }
  if (o.has_moon) {
    has_moon = true;
    moon_period = o.moon_period;
    moon_dur = o.moon_dur;
    moon_mult = o.moon_mult;
  }
  if (o.has_heal_on_kill) {
    has_heal_on_kill = true;
    hok_tags = o.hok_tags;
    hok_amount = o.hok_amount;
  }
  if (o.detect_all > 0) detect_all = o.detect_all;
  if (o.pond_size > 0) pond_size = o.pond_size;
  if (o.summon_life_mult != Fixed::from_int(1)) summon_life_mult = o.summon_life_mult;
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
