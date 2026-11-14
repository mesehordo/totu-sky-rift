// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/hud_model.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <tuple>

#include "content/json.hpp"

namespace totu::client {

namespace {

constexpr int kTile = 32;                       // világ-képpont / csempe
constexpr const char* kResKeys[] = {"wood", "gold", "meteor", "faith"};
constexpr size_t kCtxFirst = 4;                 // S D F G X C V B: kontextus-akciók

core::Fixed to_fixed(float px) { return core::Fixed::from_raw(static_cast<int32_t>(px / kTile * 65536.0f)); }

const content::Value* path_of(const content::Value* v, std::initializer_list<const char*> keys) {
  for (const char* k : keys) {
    if (!v || !v->is_object()) return nullptr;
    v = v->find(k);
  }
  return v;
}

std::array<uint8_t, 3> color_of(const content::Value* v, std::array<uint8_t, 3> fallback) {
  if (!v || !v->is_array() || v->items().size() < 3) return fallback;
  std::array<uint8_t, 3> c{};
  for (size_t i = 0; i < 3; ++i) c[i] = static_cast<uint8_t>(std::clamp<long long>(v->items()[i].as_int().value_or(0), 0, 255));
  return c;
}

std::string text_of(const content::Value* v) { return v && v->is_string() ? v->text() : std::string(); }

template <class T>
bool contains(const std::vector<T>& v, const T& x) { return std::find(v.begin(), v.end(), x) != v.end(); }

std::string fmt_int(long long v) { return std::to_string(v); }

}  // namespace

std::string HudModel::tr(const std::string& key, std::initializer_list<std::string> args) const {
  std::string s = name("hud." + key);
  size_t at = 0;
  for (const std::string& a : args) {
    const size_t p = s.find("{}", at);
    if (p == std::string::npos) break;
    s.replace(p, 2, a);
    at = p + a.size();
  }
  return s;
}

void HudModel::bind(const Snapshot* snap, const sim::Rules* rules, NameFn names, PostFn post) {
  snap_ = snap;
  rules_ = rules;
  names_ = std::move(names);
  post_ = std::move(post);
}

void HudModel::set_screen(int sw, int sh) {
  if (sw == sw_ && sh == sh_ && L_.hud.w > 0) return;
  sw_ = sw;
  sh_ = sh;
  L_ = hud_layout(sw, sh);
}

// ---------------------------------------------------------------- segédek
void HudModel::cmd(sim::Command c) {
  if (post_) post_(c);
}

sim::Command HudModel::base_cmd(sim::Command::Kind k) const {
  sim::Command c;
  c.kind = k;
  c.owner = player;
  return c;
}

const sim::EntityDef* HudModel::def_of(const SnapUnit& u) const {
  return rules_ && u.def < rules_->defs.size() && u.kind != SnapUnit::kResource ? &rules_->defs[u.def] : nullptr;
}

const sim::FactionDef* HudModel::faction_of(uint8_t owner) const {
  if (!snap_ || !rules_ || owner > sim::Sim::kMaxPlayers) return nullptr;
  return rules_->faction(pl(owner).faction);
}

std::vector<std::string> HudModel::roster(const std::string& fid, const char* key) const {
  std::vector<std::string> out;
  const sim::FactionDef* f = rules_ ? rules_->faction(fid) : nullptr;
  if (!f || !f->raw) return out;
  if (const content::Value* v = path_of(f->raw.get(), {"Roster", key}); v && v->is_array()) {
    for (const auto& x : v->items()) out.push_back(text_of(&x));
  }
  return out;
}

std::vector<std::string> HudModel::ally_pack(const std::string& fid, const char* key) const {
  std::vector<std::string> out;
  const sim::FactionDef* f = rules_ && !fid.empty() ? rules_->faction(fid) : nullptr;
  if (!f || !f->raw) return out;
  if (const content::Value* v = path_of(f->raw.get(), {"Faction", "ally_pack", key}); v && v->is_array()) {
    for (const auto& x : v->items()) out.push_back(text_of(&x));
  }
  return out;
}

bool HudModel::def_hidden(const sim::EntityDef& d) const {
  const content::Value* h = path_of(d.raw.get(), {"Identity", "hidden"});
  return h && h->is_bool() && h->as_bool();
}

bool HudModel::can_afford(const std::array<int32_t, sim::kResources>& cost) const {
  if (!snap_) return false;
  const SnapPlayer& me = self();
  for (size_t k = 0; k < sim::kResources; ++k) {
    if (me.res[k] < cost[k]) return false;
  }
  return true;
}

bool HudModel::reqs_met(const std::vector<sim::EntityDef::Req>& reqs, std::vector<std::string>* miss) const {
  if (!snap_ || !rules_) return false;
  const SnapPlayer& me = self();
  bool ok = true;
  for (const auto& r : reqs) {
    bool has = false;
    std::string what;
    switch (r.kind) {
      case sim::EntityDef::Req::Building:
        for (const SnapUnit& u : snap_->units) {
          if (u.owner == player && u.kind == SnapUnit::kBuilding && u.complete && u.hp > 0 && u.def < rules_->defs.size() &&
              rules_->defs[u.def].id == r.id) {
            has = true;
            break;
          }
        }
        what = name("building." + r.id + ".name");
        break;
      case sim::EntityDef::Req::Research:
        has = contains(me.researched, r.id);
        what = name("research." + r.id + ".name");
        break;
      case sim::EntityDef::Req::Path:
        has = me.path == r.id;
        what = tr("req.path");
        break;
      case sim::EntityDef::Req::Ascended:
        has = me.ascended;
        what = tr("req.ascended");
        break;
      case sim::EntityDef::Req::Ally:
        has = r.id.empty() ? !me.ally.empty() : me.ally == r.id;
        what = tr("req.ally");
        break;
    }
    if (!has) {
      ok = false;
      if (miss) miss->push_back(what);
    }
  }
  return ok;
}

bool HudModel::path_visible(const sim::EntityDef& d) const {
  if (!snap_) return true;
  for (const auto& r : d.reqs) {
    if (r.kind == sim::EntityDef::Req::Path) return self().path == r.id;
  }
  return true;
}

std::string HudModel::res_name(int k) const {
  if (k < 0 || k >= sim::kResources) return {};
  return tr(std::string("res.") + kResKeys[k]);
}

std::array<uint8_t, 3> HudModel::res_color(int k) const {
  static const std::array<uint8_t, 3> fb[] = {{70, 160, 70}, {236, 190, 60}, {176, 70, 255}, {255, 214, 120}};
  if (k < 0 || k >= sim::kResources) return {255, 255, 255};
  return color_of(rules_ ? path_of(rules_->game_raw.get(), {"resources", kResKeys[k], "color"}) : nullptr,
                  fb[static_cast<size_t>(k)]);
}

std::array<uint8_t, 3> HudModel::player_color(uint8_t owner) const {
  const content::Value* pc = rules_ ? path_of(rules_->game_raw.get(), {"player_colors"}) : nullptr;
  if (owner == 0 || !pc || !pc->is_array() || pc->items().size() < owner) return {200, 200, 200};
  return color_of(&pc->items()[owner - 1u], {200, 200, 200});
}

std::array<uint8_t, 3> HudModel::faction_color(const std::string& fid, bool accent) const {
  const sim::FactionDef* f = rules_ ? rules_->faction(fid) : nullptr;
  const content::Value* v = f && f->raw ? path_of(f->raw.get(), {"Faction", accent ? "accent" : "color"}) : nullptr;
  return color_of(v, accent ? std::array<uint8_t, 3>{230, 190, 90} : std::array<uint8_t, 3>{90, 86, 78});
}

bool HudModel::has_faith() const {
  const sim::FactionDef* f = faction_of(player);
  return f && f->raw && path_of(f->raw.get(), {"Faction", "faith"}) != nullptr;
}

std::string HudModel::unit_name(const SnapUnit& u) const {
  if (u.kind == SnapUnit::kResource) return res_name(u.res);
  const sim::EntityDef* d = def_of(u);
  if (!d) return "?";
  return name((d->building ? "building." : "unit.") + d->id + ".name");
}

// ---------------------------------------------------------------- kijelölés
std::tuple<int, int, std::string> HudModel::type_order(const SnapUnit& u) const {
  const sim::EntityDef* d = def_of(u);
  if (!d) return {3, 999, ""};
  std::vector<std::string> ks;
  if (snap_ && u.owner <= sim::Sim::kMaxPlayers) {
    const std::string& fid = pl(u.owner).faction;
    ks = roster(fid, "units");
    const auto bl = roster(fid, "buildings");
    ks.insert(ks.end(), bl.begin(), bl.end());
  }
  const auto it = std::find(ks.begin(), ks.end(), d->id);
  return {d->hero ? 0 : (u.kind == SnapUnit::kUnit ? 1 : 2), it != ks.end() ? static_cast<int>(it - ks.begin()) : 999, d->id};
}

std::string HudModel::selection_kind() const {
  std::vector<const SnapUnit*> s;
  for (const sim::EntityId id : sel) {
    const SnapUnit* u = unit(id);
    if (u && (u->hp > 0 || u->kind == SnapUnit::kResource)) s.push_back(u);
  }
  if (s.empty()) return "none";
  if (s.size() > 1) return "multi";
  if (s[0]->kind == SnapUnit::kResource) return "resource";
  if (s[0]->kind == SnapUnit::kBuilding) return "building";
  const sim::EntityDef* d = def_of(*s[0]);
  return d && d->hero ? "hero" : "unit";
}

std::vector<SelCell> HudModel::sel_cells() const {
  std::vector<const SnapUnit*> s;
  for (const sim::EntityId id : sel) {
    const SnapUnit* u = unit(id);
    if (u && u->hp > 0 && u->kind != SnapUnit::kResource && u->kind != SnapUnit::kShot) s.push_back(u);
  }
  if (s.empty()) return {};
  if (s.size() == 1 && s[0]->kind == SnapUnit::kBuilding) {
    std::vector<SelCell> out{{SelCell::One, {s[0]->id}, 0}};
    if (s[0]->owner == player) {
      for (int i = 0; i < std::min<int>(s[0]->queue_n, 11); ++i) out.push_back({SelCell::Queue, {s[0]->id}, i});
    }
    return out;
  }
  std::sort(s.begin(), s.end(), [&](const SnapUnit* a, const SnapUnit* b) {
    const auto ka = type_order(*a), kb = type_order(*b);
    return ka != kb ? ka < kb : a->id < b->id;
  });
  std::vector<std::pair<std::string, std::vector<sim::EntityId>>> groups;
  for (const SnapUnit* u : s) {
    const std::string key = std::get<2>(type_order(*u));
    auto it = std::find_if(groups.begin(), groups.end(), [&](const auto& g) { return g.first == key; });
    if (it == groups.end()) groups.push_back({key, {u->id}});
    else it->second.push_back(u->id);
  }
  std::vector<SelCell> cells;
  if (groups.size() == 1) {   // egyféle típus: egyenként; vegyes sereg: típusonként összevonva
    for (const sim::EntityId e : groups[0].second) cells.push_back({SelCell::One, {e}, 0});
  } else {
    for (auto& g : groups) cells.push_back({SelCell::Group, g.second, 0});
  }
  if (cells.size() > 12) {
    SelCell more{SelCell::More, {}, 0};
    for (size_t i = 11; i < cells.size(); ++i) more.ents.insert(more.ents.end(), cells[i].ents.begin(), cells[i].ents.end());
    cells.resize(11);
    cells.push_back(more);
  }
  return cells;
}

const SnapUnit* HudModel::primary() const {
  const auto cells = sel_cells();
  if (!cells.empty()) return unit(cells[0].ents[0]);
  for (const sim::EntityId id : sel) {
    const SnapUnit* u = unit(id);
    if (u && (u->hp > 0 || u->kind == SnapUnit::kResource)) return u;
  }
  return nullptr;
}

std::vector<const SnapUnit*> HudModel::own_sel_units() const {
  std::vector<const SnapUnit*> out;
  for (const sim::EntityId id : sel) {
    const SnapUnit* u = unit(id);
    if (u && u->hp > 0 && u->kind == SnapUnit::kUnit && u->owner == player && def_of(*u)) out.push_back(u);
  }
  return out;
}

std::vector<const SnapUnit*> HudModel::own_sel_buildings() const {
  std::vector<const SnapUnit*> out;
  for (const sim::EntityId id : sel) {
    const SnapUnit* u = unit(id);
    if (u && u->hp > 0 && u->kind == SnapUnit::kBuilding && u->owner == player && def_of(*u)) out.push_back(u);
  }
  return out;
}

const SnapUnit* HudModel::sel_hero() const {
  const auto us = own_sel_units();
  return us.size() == 1 && def_of(*us[0])->hero ? us[0] : nullptr;
}

std::vector<uint16_t> HudModel::unit_abilities(const SnapUnit& u) const {
  std::vector<uint16_t> out;
  const sim::EntityDef* d = def_of(u);
  if (!d || !snap_) return out;
  if (d->hero) {   // hős: csak a megtanult (a szintváltozat)
    for (const uint16_t a : u.learned) {
      if (!contains(out, a)) out.push_back(a);
    }
    return out;
  }
  const auto& done = pl(u.owner).researched;
  for (const uint16_t a : d->abilities) {
    bool ok = true;
    for (const auto& [ab, rid] : d->ability_research) {
      if (ab == a && !contains(done, rid)) ok = false;
    }
    if (ok && !contains(out, a)) out.push_back(a);
  }
  return out;
}

std::vector<uint16_t> HudModel::sel_abilities() const {
  std::vector<uint16_t> out;
  for (const SnapUnit* u : own_sel_units()) {
    for (const uint16_t a : unit_abilities(*u)) {
      if (!contains(out, a)) out.push_back(a);
    }
  }
  return out;
}

// ---------------------------------------------------------------- parancsrács (nézetmodell)
HudSlot HudModel::make_slot(std::string id, std::string icon, std::string label, std::function<void()> fn) const {
  HudSlot s;
  s.id = std::move(id);
  s.icon = std::move(icon);
  s.label = std::move(label);
  s.fn = std::move(fn);
  return s;
}

CommandCard HudModel::paged(std::vector<HudSlot> items, std::optional<HudSlot> tail) {
  CommandCard slots;
  const size_t cap = tail ? 11 : 12;
  if (items.size() > cap) {
    const size_t per = cap - 1;
    const int pages = static_cast<int>((items.size() + per - 1) / per);
    page_no = ((page_no % pages) + pages) % pages;
    for (size_t i = 0; i < per && static_cast<size_t>(page_no) * per + i < items.size(); ++i) {
      slots[i] = std::move(items[static_cast<size_t>(page_no) * per + i]);
    }
    slots[tail ? 10 : 11] = make_slot("page_next", "next", tr("next_page", {std::to_string(page_no + 1), std::to_string(pages)}),
                                       [this] { ++page_no; });
  } else {
    for (size_t i = 0; i < items.size(); ++i) slots[i] = std::move(items[i]);
  }
  if (tail) slots[11] = std::move(*tail);
  return slots;
}

void HudModel::fill_ctx(CommandCard& slots, std::vector<HudSlot> ctx) {
  constexpr size_t n = 12 - kCtxFirst;
  if (ctx.size() <= n) {
    for (size_t i = 0; i < ctx.size(); ++i) slots[kCtxFirst + i] = std::move(ctx[i]);
    return;
  }
  const size_t per = n - 1;
  const int pages = static_cast<int>((ctx.size() + per - 1) / per);
  page_no = ((page_no % pages) + pages) % pages;
  for (size_t i = 0; i < per && static_cast<size_t>(page_no) * per + i < ctx.size(); ++i) {
    slots[kCtxFirst + i] = std::move(ctx[static_cast<size_t>(page_no) * per + i]);
  }
  slots[11] = make_slot("page_next", "next", tr("more_cmds", {std::to_string(page_no + 1), std::to_string(pages)}),
                       [this] { ++page_no; });
}

HudSlot HudModel::back_slot() {
  return make_slot("back", "back", tr("back"), [this] {
    cmd_page.clear();
    page_no = 0;
    learn_mode = false;
    if (mode.kind == HudMode::Place) mode = {};
  });
}

void HudModel::open_page(const std::string& page) {
  cmd_page = page;
  page_no = 0;
  if (page == "learn") learn_mode = true;
}

HudSlot HudModel::entry_slot(HudSlot::Kind kind, uint16_t def, int research) {
  const SnapPlayer& me = self();
  std::vector<std::string> miss;
  bool done = false;
  float prog = -1.0f;
  int cnt = 0;
  std::array<int32_t, sim::kResources> cost{};
  std::string label;
  if (kind == HudSlot::Building) {
    const sim::EntityDef& d = rules_->defs[def];
    reqs_met(d.reqs, &miss);
    cost = d.cost;
    label = name("building." + d.id + ".name");
  } else if (kind == HudSlot::Unit) {
    const sim::EntityDef& d = rules_->defs[def];
    reqs_met(d.reqs, &miss);
    bool prods = false;
    for (const SnapUnit& b : snap_->units) {
      if (b.owner != player || b.kind != SnapUnit::kBuilding || b.hp <= 0 || b.def >= rules_->defs.size()) continue;
      if (b.complete && contains(rules_->defs[b.def].produces, def)) prods = true;
      for (int i = 0; i < b.queue_n; ++i) {
        if (((b.queue_research >> i) & 1) == 0 && b.queue_def[static_cast<size_t>(i)] == def) {
          ++cnt;
          prog = std::max(prog, i == 0 ? static_cast<float>(b.queue_progress) / 1000.0f : 0.0f);
        }
      }
    }
    if (!prods) miss.push_back(tr("req.producer"));
    cost = d.cost;
    label = name("unit." + d.id + ".name");
  } else {
    const sim::ResearchDef& r = rules_->researches[static_cast<size_t>(research)];
    cost = r.cost;
    label = name("research." + r.id + ".name");
    if (contains(me.researched, r.id)) {
      done = true;
      miss = {tr("req.done")};
    } else {
      bool has = false;
      for (const SnapUnit& b : snap_->units) {
        if (b.owner != player || b.kind != SnapUnit::kBuilding || b.hp <= 0 || b.def >= rules_->defs.size()) continue;
        if (b.complete && contains(rules_->defs[b.def].researches, r.id)) has = true;
        for (int i = 0; i < b.queue_n; ++i) {
          if (((b.queue_research >> i) & 1) && b.queue_def[static_cast<size_t>(i)] == research) {
            ++cnt;
            if (prog < 0) prog = i == 0 ? static_cast<float>(b.queue_progress) / 1000.0f : 0.0f;
          }
        }
      }
      if (!has) miss.push_back(tr("req.building"));
      reqs_met(r.reqs, &miss);
    }
  }
  const bool avail = miss.empty();
  HudSlot s;
  s.kind = kind;
  s.def = def;
  s.research = research;
  s.id = (kind == HudSlot::Unit ? "unit:" : kind == HudSlot::Building ? "building:" : "research:") +
         (kind == HudSlot::Research ? rules_->researches[static_cast<size_t>(research)].id : rules_->defs[def].id);
  s.label = label;
  s.enabled = avail && can_afford(cost);
  s.active = kind == HudSlot::Building && mode.kind == HudMode::Place && mode.def == def;
  s.prog = prog;
  s.count = cnt;
  if (done) {
    s.cost = "✓";
    s.cost_col = {150, 230, 150};
  }
  s.tip_title = label;
  if (!miss.empty()) {
    std::string m;
    for (size_t i = 0; i < miss.size(); ++i) m += (i ? ", " : "") + miss[i];
    s.tip_text = tr("missing", {m});
  }
  s.fn = [this, kind, def, research, miss, cost] {
    if (kind == HudSlot::Building) {
      if (!miss.empty()) return;
      if (!can_afford(cost)) {
        for (size_t k = 0; k < sim::kResources; ++k) {
          if (self().res[k] < cost[k]) res_flash[k] = t;
        }
        return;
      }
      mode = {HudMode::Place, {}, def, 0};
    } else if (kind == HudSlot::Unit) {
      for (int n = shift ? 5 : 1; n > 0; --n) {
        sim::Command c = base_cmd(sim::Command::Kind::Train);
        c.def = rules_->defs[def].id;
        cmd(c);
      }
    } else {
      sim::Command c = base_cmd(sim::Command::Kind::Research);
      c.def = rules_->researches[static_cast<size_t>(research)].id;
      cmd(c);
    }
  };
  s.rfn = [this, kind, def, research] { cancel_type(kind, def, research); };
  return s;
}

void HudModel::cancel_type(HudSlot::Kind kind, uint16_t def, int research) {
  if (kind == HudSlot::Building) {
    if (mode.kind == HudMode::Place) mode = {};
    return;
  }
  // a sor utolsó ilyen eleme (egység), illetve az első (kutatás)
  for (const SnapUnit& b : snap_->units) {
    if (b.owner != player || b.kind != SnapUnit::kBuilding) continue;
    for (int i = kind == HudSlot::Unit ? b.queue_n - 1 : 0; i >= 0 && i < b.queue_n; i += kind == HudSlot::Unit ? -1 : 1) {
      const bool res = (b.queue_research >> i) & 1;
      const uint16_t want = kind == HudSlot::Unit ? def : static_cast<uint16_t>(research);
      if (res == (kind == HudSlot::Research) && b.queue_def[static_cast<size_t>(i)] == want) {
        sim::Command c = base_cmd(sim::Command::Kind::CancelQueue);
        c.target = b.id;
        c.rx = i;
        cmd(c);
        return;
      }
    }
  }
}

void HudModel::train_from(uint16_t def, std::vector<sim::EntityId> blds) {
  for (int n = shift ? 5 : 1; n > 0; --n) {
    const SnapUnit* best = nullptr;
    for (const sim::EntityId id : blds) {
      const SnapUnit* b = unit(id);
      if (b && b->hp > 0 && b->complete && (!best || b->queue_n < best->queue_n)) best = b;
    }
    sim::Command c = base_cmd(sim::Command::Kind::Train);
    c.def = rules_->defs[def].id;
    c.target = best ? best->id : 0;
    cmd(c);
  }
}

std::vector<HudSlot> HudModel::ally_slots() {
  std::vector<HudSlot> out;
  const sim::FactionDef* mine = faction_of(player);
  const std::array<int32_t, sim::kResources> cost = mine ? mine->ally_cost : std::array<int32_t, sim::kResources>{};
  std::string price;
  for (int k = 0; k < sim::kResources; ++k) {
    if (cost[static_cast<size_t>(k)]) price += (price.empty() ? "" : ", ") + res_name(k) + " " + fmt_int(cost[static_cast<size_t>(k)]);
  }
  for (const sim::FactionDef& f : rules_->factions) {
    if (!f.ally) continue;
    const std::string nm = faction_name(f.id);
    HudSlot s = make_slot("ally:" + f.id, "", tr("ally_choose", {nm}), [this, id = f.id] {
      sim::Command c = base_cmd(sim::Command::Kind::ChooseAlly);
      c.def = id;
      cmd(c);
    });
    s.enabled = can_afford(cost);
    s.ally = f.id;
    s.tip_title = s.label;
    s.tip_text = tr("ally_price", {name("faction." + f.id + ".tagline"), price});
    out.push_back(std::move(s));
  }
  return out;
}

void HudModel::use_ability(uint16_t ab) {
  using T = sim::AbilityDef::Type;
  const sim::AbilityDef& a = rules_->abilities[ab];
  std::vector<sim::EntityId> us;
  for (const SnapUnit* u : own_sel_units()) us.push_back(u->id);
  if (a.target_placement) {
    mode = {HudMode::AbilityPlace, a.id, sim::kNoDef, 0};
  } else if (a.type == T::Area || a.type == T::Raise || a.type == T::Bolt || a.type == T::Blink || a.type == T::Capture ||
             a.type == T::Occupy) {
    mode = {HudMode::Cast, a.id, sim::kNoDef, 0};
  } else if (a.type == T::Passive || a.type == T::Aura || a.type == T::Autoheal) {
    return;
  } else if (a.type == T::Garrison) {   // a legközelebbi szabad helyű saját helyőrség-épületbe
    sim::Command c = base_cmd(sim::Command::Kind::Garrison);
    c.units = us;
    cmd(c);
  } else {
    sim::Command c = base_cmd(sim::Command::Kind::Ability);
    c.units = us;
    c.def = a.id;
    cmd(c);
  }
}

CommandCard HudModel::command_card() {
  CommandCard slots;
  if (!snap_ || !rules_) return slots;
  using T = sim::AbilityDef::Type;
  const SnapPlayer& me = self();
  const auto us = own_sel_units();
  const auto bs = own_sel_buildings();
  const SnapUnit* hero = sel_hero();
  if (!hero || hero->hero_points <= 0) {
    learn_mode = false;
    if (cmd_page == "learn") cmd_page.clear();
  }
  if (learn_mode && hero) cmd_page = "learn";
  std::vector<const SnapUnit*> workers;   // csak az építő munkások építhetnek
  for (const SnapUnit* u : us) {
    if (def_of(*u)->worker && def_of(*u)->builds) workers.push_back(u);
  }
  if (cmd_page == "build" && workers.empty()) cmd_page.clear();
  if (cmd_page == "learn") {
    std::vector<HudSlot> items;
    const sim::EntityDef& hd = *def_of(*hero);
    for (const uint16_t ab : hd.abilities) {
      const sim::AbilityDef& base = rules_->abilities[rules_->abilities[ab].base];
      int cur = 0;
      for (const uint16_t l : hero->learned) {
        if (l < rules_->abilities.size() && rules_->abilities[l].base == rules_->abilities[ab].base) cur = rules_->abilities[l].level;
      }
      const bool ok = hero->hero_points > 0 && cur < base.max_level &&
                      (!base.ultimate || hero->hero_level >= rules_->heroes.ultimate_level);
      const std::string nm = name("ability." + base.id + ".name");
      HudSlot s = make_slot("learn:" + base.id, text_of(path_of(base.spec.get(), {"icon"})),
                            nm + "  " + std::to_string(cur) + "/" + std::to_string(base.max_level),
                            [this, hid = hero->id, id = base.id, pts = hero->hero_points] {
                              sim::Command c = base_cmd(sim::Command::Kind::Learn);
                              c.units = {hid};
                              c.def = id;
                              cmd(c);
                              if (pts <= 1) learn_mode = false;
                            });
      s.enabled = ok;
      s.ab_id = base.id;
      s.tip_title = nm;
      s.tip_text = name("ability." + base.id + ".desc");
      items.push_back(std::move(s));
    }
    return paged(std::move(items), back_slot());
  }
  if (cmd_page == "build") {
    std::vector<HudSlot> items;
    std::vector<std::string> ids = roster(me.faction, "buildings");
    const auto ap = ally_pack(me.ally, "buildings");   // a szövetséges nép épületei
    for (const std::string& bid : ids) {
      const uint16_t d = rules_->def(bid);
      if (d == sim::kNoDef || def_hidden(rules_->defs[d]) || !path_visible(rules_->defs[d])) continue;
      items.push_back(entry_slot(HudSlot::Building, d, -1));
    }
    for (const std::string& bid : ap) {
      const uint16_t d = rules_->def(bid);
      if (d != sim::kNoDef) items.push_back(entry_slot(HudSlot::Building, d, -1));
    }
    return paged(std::move(items), back_slot());
  }
  if (!us.empty()) {
    bool mobile = false, attack = false;
    for (const SnapUnit* u : us) {
      mobile = mobile || def_of(*u)->speed > core::Fixed{};
      attack = attack || def_of(*u)->attack.has;
    }
    auto ids = [this] {
      std::vector<sim::EntityId> v;
      for (const SnapUnit* u : own_sel_units()) v.push_back(u->id);
      return v;
    };
    if (mobile) {
      HudSlot s = make_slot("move", "move", tr("move"), [this] { mode = {HudMode::Move, {}, sim::kNoDef, 0}; });
      s.active = mode.kind == HudMode::Move;
      s.tip_text = tr("move.tip");
      slots[0] = std::move(s);
      HudSlot st = make_slot("stop", "stop", tr("stop"), [this, ids] {
        sim::Command c = base_cmd(sim::Command::Kind::Stop);
        c.units = ids();
        cmd(c);
      });
      st.tip_text = tr("stop.tip");
      slots[1] = std::move(st);
    }
    if (attack) {
      HudSlot s = make_slot("attack", "attack", tr("attack"), [this] { mode = {HudMode::Attack, {}, sim::kNoDef, 0}; });
      s.active = mode.kind == HudMode::Attack;
      s.tip_text = tr("attack.tip");
      slots[2] = std::move(s);
    }
    if (mobile) {
      HudSlot s = make_slot("hold", "hold", tr("hold"), [this, ids] {
        sim::Command c = base_cmd(sim::Command::Kind::Hold);
        c.units = ids();
        cmd(c);
      });
      s.tip_text = tr("hold.tip");
      slots[3] = std::move(s);
    }
    std::vector<HudSlot> ctx;
    if (!workers.empty()) {
      HudSlot s = make_slot("build_menu", "build", tr("build"), [this] { open_page("build"); });
      s.tip_text = tr("build.tip");
      ctx.push_back(std::move(s));
    }
    for (const SnapUnit* u : us) {
      if (u->packed == sim::kNoDef) continue;
      HudSlot s = make_slot("unpack", "tent", tr("unpack"),
                            [this, wid = u->id, pk = u->packed] { mode = {HudMode::Unpack, {}, pk, wid}; });
      s.active = mode.kind == HudMode::Unpack;
      s.tip_text = tr("unpack.tip");
      ctx.push_back(std::move(s));
      break;
    }
    if (hero && hero->hero_points > 0) {
      const std::string lab = tr("learn", {std::to_string(hero->hero_points)});
      HudSlot s = make_slot("learn_menu", "learn", lab, [this] { open_page("learn"); });
      s.active = true;
      s.tip_text = tr("learn.tip");
      ctx.push_back(std::move(s));
    }
    std::vector<HudSlot> act, pas;
    for (const uint16_t ab : sel_abilities()) {
      const sim::AbilityDef& a = rules_->abilities[ab];
      const sim::AbilityDef& base = rules_->abilities[a.base];
      const bool passive = a.type == T::Passive || a.type == T::Aura;
      bool on = false;
      std::vector<const SnapUnit*> casters;
      for (const SnapUnit* u : us) {
        if (contains(unit_abilities(*u), ab)) casters.push_back(u);
      }
      if (a.type == T::Toggle || a.type == T::Chant) {
        for (const SnapUnit* u : casters) on = on || contains(u->toggles, ab);
      } else if (a.target_placement) {
        on = mode.kind == HudMode::AbilityPlace && mode.ability == a.id;
      } else if (a.type == T::Area || a.type == T::Raise || a.type == T::Bolt || a.type == T::Blink) {
        on = mode.kind == HudMode::Cast && mode.ability == a.id;
      }
      auto ready_of = [&](const SnapUnit* u) {
        for (const auto& [x, tk] : u->ready) {
          if (x == ab) return tk;
        }
        return int64_t{0};
      };
      float cd = 0.0f;
      if (!casters.empty() && a.cooldown > 0) {
        int64_t left = INT64_MAX;
        for (const SnapUnit* u : casters) left = std::min(left, ready_of(u) - snap_->tick);
        cd = std::clamp(static_cast<float>(left) / static_cast<float>(a.cooldown), 0.0f, 1.0f);
      }
      bool enabled = passive;
      for (const SnapUnit* u : casters) {
        if (((a.type == T::Toggle || a.type == T::Chant) && contains(u->toggles, ab)) ||
            (ready_of(u) <= snap_->tick && u->energy >= a.energy && (a.hp_cost <= 0 || u->hp > a.hp_cost + 5))) {
          enabled = true;
        }
      }
      const std::string nm = name("ability." + base.id + ".name");
      HudSlot s = make_slot("ab:" + a.id, text_of(path_of(a.spec.get(), {"icon"})), nm,
                            passive ? std::function<void()>{} : std::function<void()>{[this, ab] { use_ability(ab); }});
      if (a.cost[sim::kFaith]) {
        s.cost = std::to_string(a.cost[sim::kFaith]);
        s.cost_col = {255, 214, 120};
      } else if (a.energy) {
        s.cost = std::to_string(a.energy);
        s.cost_col = {130, 170, 255};
      } else if (a.hp_cost) {
        s.cost = std::to_string(a.hp_cost);
        s.cost_col = {255, 110, 100};
      } else if (const content::Value* sp = path_of(a.spec.get(), {"spores"}); sp && sp->is_number()) {
        s.cost = sp->text();
        s.cost_col = {150, 240, 110};
      }
      s.active = on;
      s.cd = cd;
      s.enabled = enabled;
      s.passive = passive;
      s.ab_id = base.id;
      s.tip_title = nm;
      s.tip_text = name("ability." + base.id + ".desc");
      (passive ? pas : act).push_back(std::move(s));
    }
    for (auto& s : act) ctx.push_back(std::move(s));
    for (auto& s : pas) ctx.push_back(std::move(s));
    fill_ctx(slots, std::move(ctx));
    return slots;
  }
  if (!bs.empty()) {
    const SnapUnit* b0 = bs[0];
    const sim::EntityDef& d0 = *def_of(*b0);
    std::vector<sim::EntityId> same;
    for (const SnapUnit* b : bs) {
      if (b->def == b0->def) same.push_back(b->id);
    }
    std::vector<HudSlot> items;
    if (d0.diplomacy && b0->complete) {
      if (me.ally.empty()) return paged(ally_slots(), std::nullopt);
      const std::string nm = faction_name(me.ally);
      HudSlot s = make_slot("ally_info", "", tr("ally_current", {nm}), {});
      s.passive = true;
      s.ally = me.ally;
      s.tip_title = s.label;
      s.tip_text = name("faction." + me.ally + ".desc");
      slots[0] = std::move(s);
      return slots;
    }
    if (b0->complete) {
      std::vector<std::string> own = roster(me.faction, "units");
      for (const auto& u : ally_pack(me.ally, "units")) own.push_back(u);   // a szövetséges nép egységei
      for (const uint16_t u : d0.produces) {
        const sim::EntityDef& d = rules_->defs[u];
        if (def_hidden(d)) continue;
        if (contains(own, d.id) && path_visible(d)) {
          HudSlot s = entry_slot(HudSlot::Unit, u, -1);
          s.fn = [this, u, same] { train_from(u, same); };
          items.push_back(std::move(s));
        }
      }
      for (const std::string& rid : d0.researches) {
        const int ri = rules_->research_index(rid);
        if (ri >= 0) items.push_back(entry_slot(HudSlot::Research, sim::kNoDef, ri));
      }
      for (const uint16_t pw : d0.powers) {
        const sim::AbilityDef& a = rules_->abilities[pw];
        const bool ok = can_afford(a.cost) && (!a.hq_hp || b0->hp > a.hq_hp + 50);
        const std::string nm = name("ability." + rules_->abilities[a.base].id + ".name");
        HudSlot s = make_slot("bp:" + a.id, text_of(path_of(a.spec.get(), {"icon"})), nm,
                              [this, id = a.id, bid = b0->id, targeted = a.targeted] {
                                if (targeted) {
                                  mode = {HudMode::GCast, id, sim::kNoDef, bid};
                                } else {
                                  sim::Command c = base_cmd(sim::Command::Kind::Power);
                                  c.target = bid;
                                  c.def = id;
                                  cmd(c);
                                }
                              });
        s.enabled = ok;
        s.active = mode.kind == HudMode::GCast && mode.ability == a.id && mode.ent == b0->id;
        for (int k = 0; k < sim::kResources; ++k) {
          if (a.cost[static_cast<size_t>(k)]) {
            s.cost = std::to_string(a.cost[static_cast<size_t>(k)]);
            s.cost_col = res_color(k);
            break;
          }
        }
        if (s.cost.empty() && a.hq_hp) {
          s.cost = std::to_string(a.hq_hp);
          s.cost_col = {255, 110, 100};
        }
        s.ab_id = rules_->abilities[a.base].id;
        s.tip_title = nm;
        s.tip_text = name("ability." + s.ab_id + ".desc");
        items.push_back(std::move(s));
      }
      if (d0.packable) {
        HudSlot s = make_slot("pack", "pack", tr("pack"), [this] {
          for (const SnapUnit* b : own_sel_buildings()) {
            if (!def_of(*b)->packable || !b->complete) continue;
            sim::Command c = base_cmd(sim::Command::Kind::Pack);
            c.target = b->id;
            cmd(c);
          }
        });
        s.tip_text = tr("pack.tip");
        items.push_back(std::move(s));
      }
    }
    std::vector<sim::EntityId> gb;
    int inside = 0;
    for (const SnapUnit* b : bs) {
      if (b->garrison > 0) {
        gb.push_back(b->id);
        inside += b->garrison;
      }
    }
    if (!gb.empty()) {
      HudSlot s = make_slot("unload", "house", tr("unload", {std::to_string(inside)}), [this, gb] {
        for (const sim::EntityId id : gb) {
          sim::Command c = base_cmd(sim::Command::Kind::Unload);
          c.target = id;
          cmd(c);
        }
      });
      s.tip_text = tr("unload.tip");
      items.push_back(std::move(s));
    }
    const sim::FactionDef* f = faction_of(player);
    if (me.ascended && me.path.empty() && f && b0->def == f->hq) {
      HudSlot s = make_slot("path", "learn", tr("path"), [this] { path_hidden = false; });
      s.active = true;
      s.tip_text = tr("path.tip");
      items.push_back(std::move(s));
    }
    std::optional<HudSlot> tail;
    const SnapUnit* qb = nullptr;
    for (const sim::EntityId id : same) {
      const SnapUnit* b = unit(id);
      if (b && b->queue_n > 0 && (!qb || b->queue_n > qb->queue_n)) qb = b;
    }
    if (qb) {
      tail = make_slot("cancel", "stop", tr("cancel"), [this, qid = qb->id] {
        const SnapUnit* b = unit(qid);
        if (!b || b->queue_n == 0) return;
        sim::Command c = base_cmd(sim::Command::Kind::CancelQueue);
        c.target = qid;
        c.rx = b->queue_n - 1;
        cmd(c);
      });
      tail->tip_text = tr("cancel.tip");
    }
    return paged(std::move(items), std::move(tail));
  }
  return slots;
}

bool HudModel::path_pending() const {
  if (!snap_) return false;
  const SnapPlayer& me = self();
  const sim::FactionDef* f = faction_of(player);
  return me.ascended && me.path.empty() && f && !f->paths.empty() && !path_hidden;
}

bool HudModel::input_blocked() const {
  if (paused || text_focus || path_pending()) return true;
  return snap_ && (snap_->winner != 0 || self().defeated);
}

bool HudModel::activate_slot(int i, int button) {
  if (input_blocked() || i < 0 || i >= 12) return false;
  CommandCard slots = command_card();
  auto& sl = slots[static_cast<size_t>(i)];
  if (!sl) return false;
  const std::function<void()>& fn = button == 1 ? sl->fn : sl->rfn;
  if (!fn) return false;
  if (button == 1 && (sl->kind == HudSlot::Unit || sl->kind == HudSlot::Building) && sl->def != sim::kNoDef) {
    const auto& cost = rules_->defs[sl->def].cost;
    for (size_t k = 0; k < sim::kResources; ++k) {
      if (self().res[k] < cost[k]) res_flash[k] = t;
    }
  }
  slot_flash = i;
  slot_flash_t = t;
  fn();
  return true;
}

// ---------------------------------------------------------------- régiók
std::vector<HudRegion> HudModel::middle_regions() {
  std::vector<HudRegion> out;
  const SnapUnit* e = primary();
  const sim::EntityDef* d = e ? def_of(*e) : nullptr;
  const bool hero = e && d && e->kind == SnapUnit::kUnit && d->hero && e->owner == player;
  if (!hero) {   // a fajcímer
    std::string fid = snap_ ? self().faction : std::string();
    if (e && e->owner > 0 && e->owner <= sim::Sim::kMaxPlayers && e->kind != SnapUnit::kResource) {
      fid = pl(e->owner).faction;
    }
    HudRegion r{L_.inv, "emblem", faction_name(fid), {}, {}, faction_name(fid), name("faction." + fid + ".tagline")};
    out.push_back(std::move(r));
    return out;
  }
  for (size_t i = 0; i < L_.inv_cells.size(); ++i) {
    const uint16_t it = e->items[i];
    if (it == sim::kNoItem || !rules_ || it >= rules_->items.size()) continue;
    const std::string& iid = rules_->items[it].id;
    HudRegion r;
    r.r = L_.inv_cells[i];
    r.kind = "item";
    r.data = iid;
    r.left = [this, hid = e->id, i] {
      sim::Command c = base_cmd(sim::Command::Kind::UseItem);
      c.units = {hid};
      c.rx = static_cast<int32_t>(i);
      cmd(c);
    };
    r.right = [this, hid = e->id, i] {
      sim::Command c = base_cmd(sim::Command::Kind::DropItem);
      c.units = {hid};
      c.rx = static_cast<int32_t>(i);
      cmd(c);
    };
    r.tip_title = name("item." + iid + ".name");
    r.tip_text = name("item." + iid + ".desc");
    out.push_back(std::move(r));
  }
  return out;
}

void HudModel::sel_cell_click(const SelCell& c, int button) {
  if (c.kind == SelCell::Queue) {
    sim::Command cm = base_cmd(sim::Command::Kind::CancelQueue);
    cm.target = c.ents[0];
    cm.rx = c.idx;
    cmd(cm);
    return;
  }
  std::vector<sim::EntityId> ents;
  for (const sim::EntityId id : c.ents) {
    const SnapUnit* u = unit(id);
    if (u && u->hp > 0) ents.push_back(id);
  }
  if (button == 3 || (button == 1 && shift)) {   // kivétel a kijelölésből
    sel.erase(std::remove_if(sel.begin(), sel.end(), [&](sim::EntityId x) { return contains(ents, x); }), sel.end());
    sel_changed = true;
    return;
  }
  if (button == 1 && !ents.empty()) {
    sel = ents;
    cmd_page.clear();
    page_no = 0;
    sel_changed = true;
  }
}

std::vector<HudRegion> HudModel::sel_regions() {
  std::vector<HudRegion> out;
  const auto cells = sel_cells();
  for (size_t i = 0; i < cells.size() && i < L_.sel_cells.size(); ++i) {
    HudRegion r;
    r.r = L_.sel_cells[i];
    r.kind = "cell";
    const SelCell c = cells[i];
    r.left = [this, c] { sel_cell_click(c, 1); };
    r.right = [this, c] { sel_cell_click(c, c.kind == SelCell::Queue ? 1 : 3); };
    out.push_back(std::move(r));
  }
  return out;
}

std::vector<std::pair<Rect, std::string>> HudModel::path_cards() const {
  std::vector<std::pair<Rect, std::string>> out;
  const sim::FactionDef* f = faction_of(player);
  if (!f) return out;
  const double u = sh_ / 720.0;
  auto k = [u](double v) { return static_cast<int>(std::lround(v * u)); };
  const int cw = std::min(k(330), (sw_ - k(60)) / 3);
  const int head = k(70), foot = k(40);
  const int H = std::min(sh_, L_.hud.y - k(4));
  const int top = L_.topbar.bottom() + k(6);
  const int ch = std::min(k(440), H - top - head - foot - k(8));
  const int gap = k(16);
  const int tot = cw * 3 + gap * 2;
  const int y0 = std::max(top + head, top + head + (H - top - head - foot - ch) / 2);
  const int x0 = sw_ / 2 - tot / 2;
  for (size_t i = 0; i < f->paths.size() && i < 3; ++i) {
    out.push_back({Rect{x0 + static_cast<int>(i) * (cw + gap), y0, cw, ch}, f->paths[i].id});
  }
  return out;
}

std::vector<sim::EntityId> HudModel::idle_workers() const {
  std::vector<sim::EntityId> out;
  if (!snap_) return out;
  for (const SnapUnit& u : snap_->units) {
    const sim::EntityDef* d = def_of(u);
    if (u.owner == player && u.kind == SnapUnit::kUnit && u.hp > 0 && d && d->worker && u.order == SnapUnit::kRest &&
        u.amount == 0) {
      out.push_back(u.id);
    }
  }
  return out;
}

void HudModel::select_idle_worker() {
  auto ws = idle_workers();
  if (ws.empty()) return;
  std::sort(ws.begin(), ws.end());
  const sim::EntityId cur = sel.size() == 1 ? sel[0] : 0;
  const auto it = std::find_if(ws.begin(), ws.end(), [&](sim::EntityId x) { return x > cur; });
  const sim::EntityId nxt = it != ws.end() ? *it : ws[0];
  sel = {nxt};
  sel_changed = true;
  if (const SnapUnit* u = unit(nxt)) {
    jump = true;
    jump_x = u->x;
    jump_y = u->y;
  }
}

// ---------------------------------------------------------------- bemenet
bool HudModel::key_down(int key, bool repeat, bool mods) {
  if (key >= 'a' && key <= 'z') key -= 'a' - 'A';
  if (path_pending()) {   // irányválasztó ablak: csak a választás számít
    if (key == kKeyEscape) {
      path_hidden = true;
    } else if (key >= '1' && key <= '3') {
      const sim::FactionDef* f = faction_of(player);
      const size_t i = static_cast<size_t>(key - '1');
      if (f && i < f->paths.size()) {
        sim::Command c = base_cmd(sim::Command::Kind::ChoosePath);
        c.def = f->paths[i].id;
        cmd(c);
      }
    }
    return true;
  }
  if (repeat || held_.count(key)) return true;   // ugyanaz a lenyomás (ismétlés): nem hajtjuk végre újra
  held_.insert(key);
  if (paused || text_focus) return true;         // szünet / szövegbevitel: a játékparancsok némák
  if (key == kKeyEscape) {
    if (show_allies) show_allies = false;
    else if (mode.kind != HudMode::None) mode = {};
    else if (!cmd_page.empty() || learn_mode) {
      cmd_page.clear();
      page_no = 0;
      learn_mode = false;
    } else {
      return false;   // a hívó dönt (szünetmenü)
    }
    return true;
  }
  if (key == kKeyF8) {
    select_idle_worker();
    return true;
  }
  const int slot = key_slot(key);
  if (slot < 0 || mods) return false;
  activate_slot(slot);   // W E R T / S D F G / X C V B – ugyanaz, mint a kattintás
  return true;
}

void HudModel::key_up(int key) {
  if (key >= 'a' && key <= 'z') key -= 'a' - 'A';
  held_.erase(key);
}

bool HudModel::over_hud(float x, float y) const {
  if (L_.hud.contains(x, y) || L_.topbar.contains(x, y) || L_.idle.contains(x, y)) return true;
  if (has_faith() && L_.faith.contains(x, y)) return true;
  return false;
}

void HudModel::minimap_click(float x, float y, int button) {
  const Rect& r = L_.minimap;
  const float wx = (x - static_cast<float>(r.x)) / static_cast<float>(std::max(1, r.w)) * static_cast<float>(map_w_ * kTile);
  const float wy = (y - static_cast<float>(r.y)) / static_cast<float>(std::max(1, r.h)) * static_cast<float>(map_h_ * kTile);
  if (button == 1) {
    jump = true;
    jump_x = wx;
    jump_y = wy;
  } else if (button == 3) {
    std::vector<sim::EntityId> us;
    for (const SnapUnit* u : own_sel_units()) us.push_back(u->id);
    if (us.empty()) return;
    sim::Command c = base_cmd(mode.kind == HudMode::Attack ? sim::Command::Kind::AttackMove : sim::Command::Kind::GroupMove);
    c.units = us;
    c.x = to_fixed(wx);
    c.y = to_fixed(wy);
    c.formation = formation;
    cmd(c);
    mode = {};
  }
}

bool HudModel::click(float x, float y, int button) {
  if (path_pending()) {   // irányválasztó ablak
    if (button == 1) {
      for (const auto& [r, pid] : path_cards()) {
        if (r.contains(x, y)) {
          sim::Command c = base_cmd(sim::Command::Kind::ChoosePath);
          c.def = pid;
          cmd(c);
        }
      }
    }
    return true;
  }
  if (show_allies && button == 1 && !over_hud(x, y)) show_allies = false;
  if (!over_hud(x, y)) return false;
  if (L_.minimap.contains(x, y)) {
    minimap_click(x, y, button);
    return true;
  }
  if (button == 1 && L_.gear.contains(x, y)) {
    menu_requested = true;
    return true;
  }
  if (button == 1 && L_.allies.contains(x, y)) {
    show_allies = !show_allies;
    return true;
  }
  if (button == 1 && L_.idle.contains(x, y)) {
    select_idle_worker();
    return true;
  }
  for (size_t i = 0; i < L_.cmd_cells.size(); ++i) {
    if (L_.cmd_cells[i].contains(x, y)) {
      if (button == 1 || button == 3) activate_slot(static_cast<int>(i), button);
      return true;
    }
  }
  for (auto regs : {sel_regions(), middle_regions()}) {
    for (const HudRegion& r : regs) {
      if (!r.r.contains(x, y)) continue;
      const auto& fn = button == 1 ? r.left : button == 3 ? r.right : r.left;
      if (fn && (button == 1 || button == 3)) fn();
      return true;
    }
  }
  return true;
}

void HudModel::place_tile(float wx, float wy, uint16_t def, int* tx, int* ty) const {
  int fw = 1, fh = 1;
  if (rules_ && def < rules_->defs.size()) {
    fw = std::max(1, rules_->defs[def].fw);
    fh = std::max(1, rules_->defs[def].fh);
  }
  *tx = static_cast<int>(std::floor(wx / kTile)) - fw / 2;
  *ty = static_cast<int>(std::floor(wy / kTile)) - fh / 2;
}

bool HudModel::world_click(float wx, float wy, int button, sim::EntityId under) {
  if (mode.kind == HudMode::None) return false;
  if (button == 3) {
    mode = {};
    return true;
  }
  if (button != 1) return true;
  std::vector<sim::EntityId> us;
  for (const SnapUnit* u : own_sel_units()) us.push_back(u->id);
  const HudMode m = mode;
  switch (m.kind) {
    case HudMode::Place: {
      int tx = 0, ty = 0;
      place_tile(wx, wy, m.def, &tx, &ty);
      sim::Command c = base_cmd(sim::Command::Kind::Build);
      for (const SnapUnit* u : own_sel_units()) {
        if (def_of(*u)->worker && def_of(*u)->builds) {
          c.units = {u->id};
          break;
        }
      }
      c.def = rules_->defs[m.def].id;
      c.rx = tx;
      c.ry = ty;
      if (!c.units.empty()) cmd(c);
      break;
    }
    case HudMode::Unpack: {
      int tx = 0, ty = 0;
      place_tile(wx, wy, m.def, &tx, &ty);
      sim::Command c = base_cmd(sim::Command::Kind::Unpack);
      c.units = {m.ent};
      c.rx = tx;
      c.ry = ty;
      cmd(c);
      break;
    }
    case HudMode::Attack: {
      sim::Command c = base_cmd(under ? sim::Command::Kind::Attack : sim::Command::Kind::AttackMove);
      c.units = us;
      c.target = under;
      c.x = to_fixed(wx);
      c.y = to_fixed(wy);
      cmd(c);
      break;
    }
    case HudMode::Move: {
      sim::Command c = base_cmd(sim::Command::Kind::GroupMove);
      c.units = us;
      c.x = to_fixed(wx);
      c.y = to_fixed(wy);
      c.formation = formation;
      cmd(c);
      break;
    }
    case HudMode::Cast:
    case HudMode::AbilityPlace: {
      sim::Command c = base_cmd(sim::Command::Kind::Ability);
      c.units = us;
      c.def = m.ability;
      c.x = to_fixed(wx);
      c.y = to_fixed(wy);
      c.target = under;
      cmd(c);
      break;
    }
    case HudMode::GCast: {
      sim::Command c = base_cmd(sim::Command::Kind::Power);
      c.target = m.ent;
      c.def = m.ability;
      c.x = to_fixed(wx);
      c.y = to_fixed(wy);
      cmd(c);
      break;
    }
    case HudMode::None: break;
  }
  if (!shift || m.kind == HudMode::Unpack || m.kind == HudMode::GCast) mode = {};
  return true;
}

// ---------------------------------------------------------------- a karakterlap adatai
std::string HudModel::unit_type(const SnapUnit& u) const {
  if (u.kind == SnapUnit::kBuilding) return "epulet";
  const sim::EntityDef* d = def_of(u);
  return d ? d->utype : std::string();
}

std::pair<std::string, std::string> HudModel::type_info(const std::string& key) const {
  const content::Value* ut = rules_ ? path_of(rules_->game_raw.get(), {"unit_types"}) : nullptr;
  if (key == "epulet") {
    return {tr("type.buildings"), tr("type.building_desc")};
  }
  if (!path_of(ut, {"types", key.c_str()})) return {key, {}};
  return {tr("utype." + key + ".name"), tr("utype." + key + ".desc")};
}

std::pair<std::vector<std::pair<std::string, float>>, std::vector<std::pair<std::string, float>>> HudModel::counters(
    const SnapUnit& u, const std::string& key) const {
  std::vector<std::pair<std::string, float>> strong, weak;
  if (!rules_ || key.empty()) return {strong, weak};
  const auto fl = [](core::Fixed f) { return static_cast<float>(f.raw()) / 65536.0f; };
  const sim::EntityDef* d = def_of(u);
  if (key != "epulet" && !(d && d->type_nostrength)) {
    if (const auto it = rules_->type_matrix.find(key); it != rules_->type_matrix.end()) {
      for (const auto& [k, v] : it->second) strong.emplace_back(k, fl(v));
    }
  }
  for (const auto& [a, m] : rules_->type_matrix) {
    if (const auto it = m.find(key); it != m.end()) weak.emplace_back(a, fl(it->second));
  }
  auto by = [](const auto& a, const auto& b) { return a.second > b.second; };
  std::stable_sort(strong.begin(), strong.end(), by);
  std::stable_sort(weak.begin(), weak.end(), by);
  return {strong, weak};
}

std::vector<HudEffect> HudModel::active_effects(const SnapUnit& u) const {
  struct Info {
    const char* key;
    std::array<uint8_t, 3> col;
  };
  static const Info kInfo[13] = {
      {"burn", {255, 140, 60}},     {"poison", {140, 230, 90}},  {"frozen", {170, 205, 255}},
      {"slow", {180, 160, 255}},    {"wet", {130, 190, 255}},    {"rooted", {205, 170, 110}},
      {"asleep", {190, 195, 225}},  {"stun", {255, 230, 110}},   {"feared", {215, 180, 255}},
      {"blind", {170, 170, 175}},   {"silenced", {200, 160, 240}}, {"marked", {255, 110, 100}},
      {"sunder", {215, 130, 90}},
  };
  std::vector<HudEffect> out;
  if (!snap_) return out;
  const int64_t now = snap_->tick;
  for (size_t i = 0; i < 13; ++i) {
    const int64_t until = u.afflict[i];
    if (until <= now) continue;
    if (i == 3 && u.afflict[2] > now) continue;   // a fagyás már szerepel
    if (i == 7 && until - now > 30LL * 1000000) continue;
    const std::string k = std::string("fx.") + kInfo[i].key;
    out.push_back({tr(k + ".name"), static_cast<float>(until - now) / 30.0f, kInfo[i].col, tr(k + ".desc")});
  }
  for (const auto& [ab, until] : u.buffs) {
    if (until <= now || !rules_ || ab >= rules_->abilities.size()) continue;
    const std::string& bid = rules_->abilities[rules_->abilities[ab].base].id;
    out.push_back({name("ability." + bid + ".name"), static_cast<float>(until - now) / 30.0f, {255, 214, 110},
                   tr("fx.buff", {name("ability." + bid + ".desc")})});
  }
  return out;
}

std::string HudModel::status_text(const SnapUnit& u) const {
  if (!snap_ || !rules_ || u.owner != player) return {};
  const sim::EntityDef* d = def_of(u);
  if (!d) return {};
  auto join = [](const std::vector<std::string>& v) {
    std::string s;
    for (const auto& x : v) {
      if (x.empty()) continue;
      s += (s.empty() ? "" : "  •  ") + x;
    }
    return s;
  };
  if (u.kind == SnapUnit::kBuilding) {
    if (!u.complete) return tr("st.constructing");
    std::vector<std::string> notes;
    if (u.queue_n > 0) {
      const bool res = u.queue_research & 1;
      const uint16_t q = u.queue_def[0];
      std::string nm;
      int32_t time = 0;
      if (res && q < rules_->researches.size()) {
        nm = name("research." + rules_->researches[q].id + ".name");
        time = rules_->researches[q].time_ticks;
      } else if (!res && q < rules_->defs.size()) {
        nm = name("unit." + rules_->defs[q].id + ".name");
        time = rules_->defs[q].time_ticks;
      }
      const int left = static_cast<int>(std::lround(time * (1.0 - u.queue_progress / 1000.0) / 30.0));
      std::string s = tr("st.producing", {nm, std::to_string(std::max(0, left))});
      if (u.queue_n > 1) s += tr("st.queued", {std::to_string(u.queue_n - 1)});
      notes.push_back(s);
    }
    if (d->supply_provides) notes.push_back(tr("st.supply", {std::to_string(d->supply_provides)}));
    if (d->garrison) notes.push_back(tr("st.garrison", {std::to_string(u.garrison), std::to_string(d->garrison)}));
    if (!d->produces.empty() && u.queue_n == 0) notes.push_back(tr("st.rally"));
    return join(notes);
  }
  std::vector<std::string> bits;
  if (d->worker && u.amount > 0) {
    bits.push_back(tr("st.carry", {std::to_string(u.amount), res_name(u.res)}));
  } else {
    static const char* kOrder[] = {"rest", "move", "amove", "attack", "gather", "gobuild", "construct", "repair", "hold", "cast"};
    bits.push_back(u.order < 10 ? tr(std::string("st.") + kOrder[u.order]) : std::string());
  }
  return join(bits);
}

}  // namespace totu::client
