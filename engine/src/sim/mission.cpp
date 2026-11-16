// SPDX-License-Identifier: GPL-3.0-or-later
#include "sim/mission.hpp"

#include <algorithm>
#include <iterator>
#include <set>

#include <filesystem>
#include <fstream>

#include "core/bytes.hpp"
#include "core/hash.hpp"
#include "sim/replay.hpp"
#include "sim/rules.hpp"

namespace totu::sim {

namespace {

using core::Fixed;
using content::Value;

constexpr int kMaxDepth = 8;          // tüzelésből tüzelés (végtelen lánc ellen)
constexpr int kTickBudget = 4000;     // akció tickenként
constexpr size_t kMaxMessages = 32;

std::string str(const Value& v, std::string_view k, std::string def = {}) {
  const Value* x = v.find(k);
  return x && x->is_string() ? x->text() : def;
}
int64_t num(const Value& v, std::string_view k, int64_t def = 0) {
  const Value* x = v.find(k);
  if (!x) return def;
  if (x->is_bool()) return x->as_bool() ? 1 : 0;
  return x->as_int().value_or(def);
}
Fixed fix(const Value& v, std::string_view k, Fixed def = {}) {
  const Value* x = v.find(k);
  return x ? x->as_fixed().value_or(def) : def;
}
bool flag(const Value& v, std::string_view k, bool def) {
  const Value* x = v.find(k);
  return x && x->is_bool() ? x->as_bool() : def;
}
bool compare(int64_t a, const std::string& op, int64_t b) {
  if (op == "eq" || op == "==") return a == b;
  if (op == "ne" || op == "!=") return a != b;
  if (op == "lt" || op == "<") return a < b;
  if (op == "le" || op == "<=") return a <= b;
  if (op == "gt" || op == ">") return a > b;
  return a >= b;   // ge (alapérték)
}

const std::set<std::string> kEvents = {"start", "time", "enter_region", "leave_region", "unit_dies", "count",
                                        "always", "dialogue_end", "choice", "objective_done", "timer", "cutscene_end"};
const std::set<std::string> kConds = {"var", "alive", "dead", "in_region", "count", "objective", "resource", "tick",
                                       "all", "any", "not"};
const std::set<std::string> kActions = {"dialogue", "objective", "spawn", "order", "kill", "remove", "reveal",
                                         "resources", "set_var", "add_var", "enable", "disable", "timer", "cutscene",
                                         "camera", "message", "letterbox", "subtitle", "win", "lose"};

// --- ellenőrzés ---------------------------------------------------------------------------------------------------
struct Checker {
  const MissionDef& m;
  const Rules* rules;
  std::vector<std::string>& errors;
  std::set<std::string> names, trigger_ids;
  void err(const std::string& where, const std::string& what) { errors.push_back(where + ": " + what); }
  void def_ref(const std::string& where, const std::string& def) {
    if (rules && !def.empty() && rules->def(def) == kNoDef) err(where, "ismeretlen sablon: " + def);
  }
  void place_ref(const std::string& where, const Value& v) {
    const std::string r = str(v, "region"), p = str(v, "point");
    if (!r.empty() && !m.region(r)) err(where, "ismeretlen régió: " + r);
    if (!p.empty() && !m.point(p)) err(where, "ismeretlen pont: " + p);
  }
  void unit_ref(const std::string& where, const Value& v) {
    const std::string u = str(v, "unit");
    if (!u.empty() && !names.count(u)) err(where, "ismeretlen egységnév: " + u);
    if (const Value* us = v.find("units"); us && us->is_array()) {
      for (const auto& x : us->items()) {
        if (x.is_string() && !names.count(x.text())) err(where, "ismeretlen egységnév: " + x.text());
      }
    }
    def_ref(where, str(v, "def"));
  }
  void cond(const std::string& where, const Value& c) {
    const std::string t = str(c, "type");
    if (!kConds.count(t)) return err(where, "ismeretlen feltétel: " + t);
    if (t == "all" || t == "any") {
      const Value* xs = c.find("of");
      if (!xs || !xs->is_array()) return err(where, "az `of` lista hiányzik");
      for (const auto& x : xs->items()) cond(where, x);
      return;
    }
    if (t == "not") {
      const Value* x = c.find("of");
      if (!x || !x->is_object()) return err(where, "az `of` feltétel hiányzik");
      return cond(where, *x);
    }
    if (t == "objective" && !m.objective(str(c, "id"))) err(where, "ismeretlen cél: " + str(c, "id"));
    if (t == "in_region") place_ref(where, c);
    unit_ref(where, c);
  }
  void action(const std::string& where, const Value& a) {
    const std::string t = str(a, "type");
    if (!kActions.count(t)) return err(where, "ismeretlen akció: " + t);
    if (t == "dialogue" && !m.dialogue(str(a, "id"))) err(where, "ismeretlen párbeszéd: " + str(a, "id"));
    if (t == "objective" && !m.objective(str(a, "id"))) err(where, "ismeretlen cél: " + str(a, "id"));
    if ((t == "enable" || t == "disable") && !trigger_ids.count(str(a, "trigger"))) err(where, "ismeretlen trigger: " + str(a, "trigger"));
    if (t == "cutscene" && !m.cutscenes.count(str(a, "id"))) err(where, "ismeretlen átvezető: " + str(a, "id"));
    if (t == "camera" && !m.point(str(a, "point"))) err(where, "ismeretlen kamerapont: " + str(a, "point"));
    if (t == "spawn") {
      if (str(a, "def").empty()) err(where, "a spawn sablon nélkül");
      if (str(a, "region").empty() && str(a, "point").empty()) err(where, "a spawn hely nélkül");
    }
    if (t == "order") {
      const std::string path = str(a, "path");
      if (!path.empty() && !m.path(path)) err(where, "ismeretlen útvonal: " + path);
    }
    place_ref(where, a);
    if (t != "spawn") unit_ref(where, a);
    else def_ref(where, str(a, "def"));
  }
};

std::array<int32_t, kResources> read_res(const Value* v) {
  std::array<int32_t, kResources> r{};
  if (!v || !v->is_object()) return r;
  for (int k = 0; k < kResources; ++k) {
    if (const Value* x = v->find(resource_id(k))) r[static_cast<size_t>(k)] = static_cast<int32_t>(x->as_int().value_or(0));
  }
  return r;
}

}  // namespace

// --- a leírás -----------------------------------------------------------------------------------------------------
const DialogueNode* MissionDialogue::node(const std::string& key) const {
  for (const auto& n : nodes) {
    if (n.id == key) return &n;
  }
  return nullptr;
}

template <class T>
static const T* find_id(const std::vector<T>& xs, const std::string& id) {
  for (const auto& x : xs) {
    if (x.id == id) return &x;
  }
  return nullptr;
}

const MissionRegion* MissionDef::region(const std::string& key) const { return find_id(regions, key); }
const MissionPoint* MissionDef::point(const std::string& key) const {
  if (const auto* p = find_id(points, key)) return p;
  return find_id(cameras, key);
}
const MissionPath* MissionDef::path(const std::string& key) const { return find_id(paths, key); }
const MissionDialogue* MissionDef::dialogue(const std::string& key) const { return find_id(dialogues, key); }
const MissionObjective* MissionDef::objective(const std::string& key) const { return find_id(objectives, key); }

std::optional<MissionDef> MissionDef::parse(const std::string& text, const Rules* rules, std::vector<std::string>& errors) {
  content::ParseError pe;
  auto parsed = content::parse(text, &pe);
  if (!parsed || !parsed->is_object()) {
    errors.push_back("mission.json: " + pe.message + " (" + std::to_string(pe.line) + ". sor)");
    return std::nullopt;
  }
  MissionDef m;
  m.doc = std::make_shared<const Value>(std::move(*parsed));
  const Value& d = *m.doc;
  const size_t before = errors.size();
  m.id = str(d, "id");
  m.name = str(d, "name");
  m.description = str(d, "description");
  m.map = str(d, "map");
  if (m.id.empty()) errors.push_back("a küldetésnek nincs azonosítója (`id`)");
  if (const Value* ps = d.find("players"); ps && ps->is_array()) {
    for (const auto& p : ps->items()) {
      MissionPlayer mp;
      mp.slot = static_cast<uint8_t>(std::clamp<int64_t>(num(p, "slot"), 0, Sim::kMaxPlayers));
      mp.faction = str(p, "faction");
      mp.human = flag(p, "human", false);
      mp.res = read_res(p.find("resources"));
      mp.ai = str(p, "ai");
      if (mp.slot == 0) errors.push_back("players: a hely 1–8");
      if (rules && !rules->faction(mp.faction)) errors.push_back("players: ismeretlen nép: " + mp.faction);
      m.players.push_back(std::move(mp));
    }
  }
  if (m.players.empty()) errors.push_back("a küldetésnek nincs játékosa");
  auto rect = [](const Value& v) {
    MissionRegion r;
    r.id = str(v, "id");
    r.x = static_cast<int>(num(v, "x"));
    r.y = static_cast<int>(num(v, "y"));
    r.w = static_cast<int>(std::max<int64_t>(1, num(v, "w", 1)));
    r.h = static_cast<int>(std::max<int64_t>(1, num(v, "h", 1)));
    return r;
  };
  if (const Value* rs = d.find("regions"); rs && rs->is_array()) for (const auto& r : rs->items()) m.regions.push_back(rect(r));
  auto point = [](const Value& v) { return MissionPoint{str(v, "id"), fix(v, "x"), fix(v, "y"), fix(v, "zoom")}; };
  if (const Value* ps = d.find("points"); ps && ps->is_array()) for (const auto& p : ps->items()) m.points.push_back(point(p));
  if (const Value* ps = d.find("cameras"); ps && ps->is_array()) for (const auto& p : ps->items()) m.cameras.push_back(point(p));
  if (const Value* ps = d.find("paths"); ps && ps->is_array()) {
    for (const auto& p : ps->items()) {
      MissionPath mp;
      mp.id = str(p, "id");
      if (const Value* pts = p.find("points"); pts && pts->is_array()) {
        for (const auto& q : pts->items()) {
          if (q.is_array() && q.items().size() == 2) {
            mp.points.emplace_back(q.items()[0].as_fixed().value_or(Fixed{}), q.items()[1].as_fixed().value_or(Fixed{}));
          }
        }
      }
      if (mp.points.empty()) errors.push_back("paths." + mp.id + ": nincs pontja");
      m.paths.push_back(std::move(mp));
    }
  }
  if (const Value* us = d.find("units"); us && us->is_array()) {
    for (const auto& u : us->items()) {
      MissionUnit mu{str(u, "name"), str(u, "def"), static_cast<uint8_t>(std::clamp<int64_t>(num(u, "owner"), 0, Sim::kMaxPlayers)),
                     fix(u, "x"), fix(u, "y")};
      m.units.push_back(std::move(mu));
    }
  }
  if (const Value* vs = d.find("variables"); vs && vs->is_object()) {
    for (const auto& [k, v] : vs->members()) m.variables.emplace_back(k, v.as_int().value_or(0));
  }
  if (const Value* os = d.find("objectives"); os && os->is_array()) {
    for (const auto& o : os->items()) {
      MissionObjective mo;
      mo.id = str(o, "id");
      mo.text = str(o, "text");
      const std::string k = str(o, "kind", "primary");
      mo.kind = k == "secondary" ? 1 : k == "hidden" ? 2 : 0;
      mo.visible = flag(o, "visible", mo.kind != 2);
      m.objectives.push_back(std::move(mo));
    }
  }
  if (const Value* ds = d.find("dialogues"); ds && ds->is_object()) {
    for (const auto& [id, dv] : ds->members()) {
      MissionDialogue md;
      md.id = id;
      if (const Value* ns = dv.find("nodes"); ns && ns->is_array()) {
        for (const auto& n : ns->items()) {
          DialogueNode dn;
          dn.id = str(n, "id");
          dn.speaker = str(n, "speaker");
          dn.portrait = str(n, "portrait");
          dn.text = str(n, "text");
          dn.next = str(n, "next");
          dn.ticks = static_cast<int32_t>(std::clamp<int64_t>(num(n, "ticks", 90), 1, 30 * 600));
          if (const Value* cs = n.find("choices"); cs && cs->is_array()) {
            for (const auto& c : cs->items()) {
              dn.choices.push_back(DialogueChoice{str(c, "text"), str(c, "next"), str(c, "var"), num(c, "value")});
            }
          }
          md.nodes.push_back(std::move(dn));
        }
      }
      if (md.nodes.empty()) errors.push_back("dialogues." + id + ": nincs csomópontja");
      m.dialogues.push_back(std::move(md));
    }
  }
  if (const Value* cs = d.find("cutscenes"); cs && cs->is_object()) {
    for (const auto& [id, cv] : cs->members()) m.cutscenes[id] = &cv;
  }
  if (const Value* ts = d.find("triggers"); ts && ts->is_array()) {
    for (const auto& t : ts->items()) m.triggers.push_back(&t);
  }
  // hivatkozások
  Checker ck{m, rules, errors, {}, {}};
  for (const auto& u : m.units) {
    if (!u.name.empty()) ck.names.insert(u.name);
    ck.def_ref("units", u.def);
    if (u.owner > 0 && std::none_of(m.players.begin(), m.players.end(), [&](const MissionPlayer& p) { return p.slot == u.owner; })) {
      errors.push_back("units." + u.name + ": a tulajdonos nem játékos: " + std::to_string(u.owner));
    }
  }
  std::set<std::string> seen;
  auto collect_spawn_names = [&](const Value* acts) {
    if (!acts || !acts->is_array()) return;
    for (const auto& a : acts->items()) {
      if (str(a, "type") == "spawn" && !str(a, "name").empty()) ck.names.insert(str(a, "name"));
    }
  };
  for (const Value* t : m.triggers) {
    const std::string id = str(*t, "id");
    if (id.empty()) errors.push_back("triggers: azonosító nélküli trigger");
    else if (!seen.insert(id).second) errors.push_back("triggers: ismétlődő azonosító: " + id);
    ck.trigger_ids.insert(id);
    collect_spawn_names(t->find("actions"));
  }
  for (const auto& [id, cv] : m.cutscenes) collect_spawn_names(cv->find("steps"));
  for (const auto& md : m.dialogues) {
    for (const auto& n : md.nodes) {
      if (!n.next.empty() && !md.node(n.next)) errors.push_back("dialogues." + md.id + "." + n.id + ": ismeretlen következő: " + n.next);
      for (const auto& c : n.choices) {
        if (!c.next.empty() && !md.node(c.next)) errors.push_back("dialogues." + md.id + "." + n.id + ": ismeretlen válasz-cél: " + c.next);
      }
    }
  }
  for (const Value* t : m.triggers) {
    const std::string where = "triggers." + str(*t, "id");
    const Value* ev = t->find("event");
    if (!ev || !ev->is_object()) {
      errors.push_back(where + ": nincs eseménye");
      continue;
    }
    const std::string et = str(*ev, "type");
    if (!kEvents.count(et)) errors.push_back(where + ": ismeretlen esemény: " + et);
    if ((et == "enter_region" || et == "leave_region") && !m.region(str(*ev, "region"))) errors.push_back(where + ": ismeretlen régió: " + str(*ev, "region"));
    if ((et == "dialogue_end" || et == "choice") && !m.dialogue(str(*ev, "id"))) errors.push_back(where + ": ismeretlen párbeszéd: " + str(*ev, "id"));
    if (et == "objective_done" && !m.objective(str(*ev, "id"))) errors.push_back(where + ": ismeretlen cél: " + str(*ev, "id"));
    if (et == "cutscene_end" && !m.cutscenes.count(str(*ev, "id"))) errors.push_back(where + ": ismeretlen átvezető: " + str(*ev, "id"));
    ck.unit_ref(where, *ev);
    if (const Value* cs = t->find("conditions"); cs && cs->is_array()) for (const auto& c : cs->items()) ck.cond(where, c);
    if (const Value* as = t->find("actions"); as && as->is_array()) for (const auto& a : as->items()) ck.action(where, a);
  }
  for (const auto& [id, cv] : m.cutscenes) {
    if (const Value* st = cv->find("steps"); st && st->is_array()) {
      for (const auto& a : st->items()) ck.action("cutscenes." + id, a);
    } else {
      errors.push_back("cutscenes." + id + ": nincs lépése");
    }
  }
  if (errors.size() > before) return std::nullopt;
  return m;
}

// --- a futó küldetés ----------------------------------------------------------------------------------------------
MissionHost::MissionHost(std::shared_ptr<const MissionDef> def) : def_(std::move(def)) {
  triggers_.resize(def_->triggers.size());
  for (size_t i = 0; i < def_->triggers.size(); ++i) triggers_[i].enabled = flag(*def_->triggers[i], "enabled", true);
  for (const auto& [k, v] : def_->variables) vars_[k] = v;
  for (const auto& o : def_->objectives) objectives_[o.id] = o.visible ? 1 : 0;
}

void MissionHost::note(const std::string& m) {
  if (log_.size() < 200) log_.push_back(m);
}

void MissionHost::start(Sim& s) {
  for (const auto& p : def_->players) {
    for (int k = 0; k < kResources; ++k) {
      if (p.res[static_cast<size_t>(k)] > 0) s.add_resources(p.slot, k, p.res[static_cast<size_t>(k)]);
    }
  }
  const Rules* r = s.rules();
  for (const auto& u : def_->units) {
    const uint16_t d = r ? r->def(u.def) : kNoDef;
    if (d == kNoDef) continue;
    EntityId e = kNoEntity;
    if (r->defs[d].building) {
      e = s.place_building(d, u.owner, u.x.floor_int(), u.y.floor_int(), true);
    } else {
      e = s.spawn_unit(d, u.owner, u.x, u.y);
      if (e != kNoEntity) s.place_at_free(e, u.x, u.y);
    }
    if (e != kNoEntity && !u.name.empty()) names_[u.name].push_back(e);
  }
}

int64_t MissionHost::var(const std::string& name) const {
  const auto it = vars_.find(name);
  return it == vars_.end() ? 0 : it->second;
}

uint8_t MissionHost::objective_state(const std::string& id) const {
  const auto it = objectives_.find(id);
  return it == objectives_.end() ? 0 : it->second;
}

const std::vector<EntityId>* MissionHost::named(const std::string& name) const {
  const auto it = names_.find(name);
  return it == names_.end() ? nullptr : &it->second;
}

bool MissionHost::in_region(Sim& s, EntityId e, const MissionRegion& r) const {
  const Position* p = s.world().get<Position>(e);
  if (!p) return false;
  const Fixed x0 = Fixed::from_int(r.x), y0 = Fixed::from_int(r.y);
  return p->x >= x0 && p->y >= y0 && p->x < x0 + Fixed::from_int(r.w) && p->y < y0 + Fixed::from_int(r.h);
}

std::vector<EntityId> MissionHost::resolve(Sim& s, const Value& a, EntityId trigger_unit) const {
  std::vector<EntityId> out;
  auto alive = [&](EntityId e) {
    const Health* h = s.world().alive(e) ? s.world().get<Health>(e) : nullptr;
    return h && h->hp > 0;
  };
  auto add_name = [&](const std::string& n) {
    if (const auto* v = named(n)) for (const EntityId e : *v) if (alive(e)) out.push_back(e);
  };
  if (const std::string u = str(a, "unit"); !u.empty()) add_name(u);
  if (const Value* us = a.find("units"); us && us->is_array()) {
    for (const auto& x : us->items()) if (x.is_string()) add_name(x.text());
  }
  if (flag(a, "trigger_unit", false) && trigger_unit != kNoEntity && alive(trigger_unit)) out.push_back(trigger_unit);
  if (const Value* o = a.find("owner"); o && str(a, "unit").empty() && !a.find("units")) {   // játékos szerint
    const uint8_t owner = static_cast<uint8_t>(o->as_int().value_or(0));
    const std::string def = str(a, "def");
    const uint16_t d = !def.empty() && s.rules() ? s.rules()->def(def) : kNoDef;
    const MissionRegion* reg = def_->region(str(a, "region"));
    s.world().pool<Unit>().each([&](EntityId e, const Unit& un) {
      const Health* h = s.world().get<Health>(e);
      if (!h || h->hp <= 0 || h->owner != owner || (d != kNoDef && un.def != d)) return;
      if (reg && !in_region(s, e, *reg)) return;
      out.push_back(e);
    });
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

bool MissionHost::check(Sim& s, const Value& c, EntityId unit) const {
  const std::string t = str(c, "type");
  if (t == "all" || t == "any") {
    const Value* xs = c.find("of");
    if (!xs || !xs->is_array()) return false;
    for (const auto& x : xs->items()) {
      const bool v = check(s, x, unit);
      if (t == "any" && v) return true;
      if (t == "all" && !v) return false;
    }
    return t == "all";
  }
  if (t == "not") {
    const Value* x = c.find("of");
    return x && !check(s, *x, unit);
  }
  const std::string op = str(c, "op", "ge");
  const int64_t value = num(c, "value");
  if (t == "var") return compare(var(str(c, "var")), op, value);
  if (t == "tick") return compare(s.tick(), op, value);
  if (t == "alive") return !resolve(s, c, unit).empty();
  if (t == "dead") return resolve(s, c, unit).empty();
  if (t == "objective") {
    const std::string st = str(c, "state", "done");
    const uint8_t want = st == "hidden" ? 0 : st == "active" ? 1 : st == "failed" ? 3 : 2;
    return objective_state(str(c, "id")) == want;
  }
  if (t == "in_region") {
    const MissionRegion* r = def_->region(str(c, "region"));
    if (!r) return false;
    for (const EntityId e : resolve(s, c, unit)) {
      if (in_region(s, e, *r)) return true;
    }
    return false;
  }
  if (t == "count") {   // élő egységek és épületek
    const uint8_t owner = static_cast<uint8_t>(num(c, "owner"));
    const std::string def = str(c, "def");
    const uint16_t d = !def.empty() && s.rules() ? s.rules()->def(def) : kNoDef;
    int64_t n = 0;
    s.world().pool<Health>().each([&](EntityId e, const Health& h) {
      if (h.hp <= 0 || h.owner != owner) return;
      const Unit* u = s.world().get<Unit>(e);
      const Building* b = s.world().get<Building>(e);
      if (!u && !b) return;
      if (d != kNoDef && (u ? u->def : b->def) != d) return;
      ++n;
    });
    return compare(n, op, value);
  }
  if (t == "resource") {
    const uint8_t owner = static_cast<uint8_t>(std::clamp<int64_t>(num(c, "owner"), 0, Sim::kMaxPlayers));
    const std::string res = str(c, "res", "gold");
    for (int k = 0; k < kResources; ++k) {
      if (res == resource_id(k)) return compare(s.player(owner).resource(k), op, value);
    }
    return false;
  }
  return false;
}

bool MissionHost::matches(Sim& s, const Value& ev, const Event& e) const {
  const std::string t = str(ev, "type");
  if (t == "time") {
    if (e.type != "tick") return false;
    const int64_t at = num(ev, "at", -1), every = num(ev, "every", 0);
    return (at >= 0 && s.tick() == at) || (every > 0 && s.tick() > 0 && s.tick() % every == 0);
  }
  if (t == "always") return e.type == "tick";
  if (t == "count") return e.type == "tick" && check(s, ev, kNoEntity);
  if (t != e.type) return false;
  if (t == "enter_region" || t == "leave_region") {
    if (str(ev, "region") != e.a) return false;
    if (const Value* o = ev.find("owner"); o && o->as_int().value_or(-1) != e.owner) return false;
    if (const std::string u = str(ev, "unit"); !u.empty()) {
      const auto* v = named(u);
      return v && std::find(v->begin(), v->end(), e.unit) != v->end();
    }
    return true;
  }
  if (t == "unit_dies") {
    if (const Value* o = ev.find("owner"); o && o->as_int().value_or(-1) != e.owner) return false;
    if (const std::string u = str(ev, "unit"); !u.empty()) {
      const auto* v = named(u);
      if (!v || std::find(v->begin(), v->end(), e.unit) == v->end()) return false;
    }
    if (const std::string def = str(ev, "def"); !def.empty()) {
      const Unit* un = s.world().get<Unit>(e.unit);
      const Building* b = s.world().get<Building>(e.unit);
      const uint16_t d = s.rules() ? s.rules()->def(def) : kNoDef;
      if (d == kNoDef || (un ? un->def : b ? b->def : kNoDef) != d) return false;
    }
    return true;
  }
  if (t == "choice") {
    if (str(ev, "id") != e.a) return false;
    const Value* v = ev.find("value");
    return !v || v->as_int().value_or(-1) == e.value;
  }
  if (t == "dialogue_end" || t == "objective_done" || t == "cutscene_end" || t == "timer") return str(ev, "id") == e.a;
  return t == "start";
}

void MissionHost::fire(Sim& s, const Event& e) {
  if (depth_ >= kMaxDepth) {
    ++errors_;
    note("túl mély triggerlánc: " + e.type);
    return;
  }
  ++depth_;
  // prioritás szerint (nagyobb előbb), egyenlőnél a leírás sorrendjében
  std::vector<size_t> order(def_->triggers.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
    return num(*def_->triggers[a], "priority") > num(*def_->triggers[b], "priority");
  });
  for (const size_t i : order) {
    TriggerState& ts = triggers_[i];
    const Value& t = *def_->triggers[i];
    if (!ts.enabled || ts.done) continue;
    const Value* ev = t.find("event");
    if (!ev || !matches(s, *ev, e)) continue;
    const int64_t cd = num(t, "cooldown");
    if (cd > 0 && ts.last >= 0 && s.tick() - ts.last < cd) continue;
    bool ok = true;
    if (const Value* cs = t.find("conditions"); cs && cs->is_array()) {
      for (const auto& c : cs->items()) ok = ok && check(s, c, e.unit);
    }
    if (!ok) continue;
    ts.last = s.tick();
    if (!flag(t, "repeat", false)) ts.done = true;
    if (const Value* as = t.find("actions"); as && as->is_array()) {
      for (const auto& a : as->items()) run_action(s, a, e.unit);
    }
  }
  --depth_;
}

void MissionHost::advance_dialogue(Sim& s, const std::string& next) {
  const MissionDialogue* d = def_->dialogue(talk_.dialogue);
  if (d && !next.empty() && d->node(next)) {
    talk_.node = next;
    talk_.started = s.tick();
    return;
  }
  const std::string ended = talk_.dialogue;
  talk_ = DialogueView{};
  if (!dialogue_queue_.empty()) {
    const std::string q = dialogue_queue_.front();
    dialogue_queue_.erase(dialogue_queue_.begin());
    if (const MissionDialogue* nd = def_->dialogue(q)) talk_ = DialogueView{q, nd->nodes.front().id, s.tick()};
  }
  if (!ended.empty()) fire(s, Event{"dialogue_end", ended});
}

bool MissionHost::run_action(Sim& s, const Value& a, EntityId trigger_unit) {
  if (--budget_ < 0) {
    if (budget_ == -1) {
      ++errors_;
      note("a tick akciókerete elfogyott");
    }
    return false;
  }
  const std::string t = str(a, "type");
  const Rules* rules = s.rules();
  auto where = [&](Fixed& x, Fixed& y) {
    if (const MissionRegion* r = def_->region(str(a, "region"))) {
      x = Fixed::from_int(r->x) + Fixed::from_ratio(r->w, 2);
      y = Fixed::from_int(r->y) + Fixed::from_ratio(r->h, 2);
      return true;
    }
    if (const MissionPoint* p = def_->point(str(a, "point"))) {
      x = p->x;
      y = p->y;
      return true;
    }
    return false;
  };
  if (t == "dialogue") {
    const MissionDialogue* d = def_->dialogue(str(a, "id"));
    if (!d) return false;
    if (talk_.dialogue.empty()) talk_ = DialogueView{d->id, d->nodes.front().id, s.tick()};
    else if (dialogue_queue_.size() < 16) dialogue_queue_.push_back(d->id);
    return true;
  }
  if (t == "objective") {
    const std::string id = str(a, "id"), st = str(a, "state", "show");
    if (!def_->objective(id)) return false;
    const uint8_t v = st == "hide" ? 0 : st == "done" ? 2 : st == "fail" ? 3 : 1;
    const uint8_t old = objectives_[id];
    objectives_[id] = v;
    if (v == 2 && old != 2) fire(s, Event{"objective_done", id});
    return true;
  }
  if (t == "spawn") {
    const uint16_t d = rules ? rules->def(str(a, "def")) : kNoDef;
    Fixed x, y;
    if (d == kNoDef || !where(x, y)) return false;
    const uint8_t owner = static_cast<uint8_t>(std::clamp<int64_t>(num(a, "owner"), 0, Sim::kMaxPlayers));
    const int64_t n = std::clamp<int64_t>(num(a, "count", 1), 1, 100);
    const std::string name = str(a, "name");
    for (int64_t k = 0; k < n; ++k) {
      const EntityId e = s.spawn_unit(d, owner, x, y);
      if (e == kNoEntity) continue;
      s.place_at_free(e, x, y);
      if (!name.empty()) names_[name].push_back(e);
    }
    return true;
  }
  if (t == "order") {
    const auto us = resolve(s, a, trigger_unit);
    if (us.empty()) return false;
    Command c;
    const std::string k = str(a, "order", "move");
    c.kind = k == "attack_move" ? Command::Kind::AttackMove : k == "stop" ? Command::Kind::Stop
             : k == "hold"      ? Command::Kind::Hold      : Command::Kind::Move;
    c.tick = s.tick() + 1;
    c.owner = 0;   // rendszerparancs
    c.units = us;
    if (k == "attack") {   // névvel adott célpont
      const Value* tg = a.find("target");
      const auto ts = resolve(s, tg ? *tg : a, trigger_unit);
      if (ts.empty()) return false;
      c.kind = Command::Kind::Attack;
      c.target = ts.front();
    } else if (c.kind == Command::Kind::Move || c.kind == Command::Kind::AttackMove) {
      Fixed x, y;
      if (const MissionPath* p = def_->path(str(a, "path"))) {   // útvonal: a végpontjára (az útvonalat a
        x = p->points.back().first;                              // szkript lépteti, ha járőrözés kell)
        y = p->points.back().second;
      } else if (!where(x, y)) {
        return false;
      }
      c.x = x;
      c.y = y;
    }
    s.queue(c);
    return true;
  }
  if (t == "kill" || t == "remove") {
    for (const EntityId e : resolve(s, a, trigger_unit)) {
      if (t == "kill") s.kill(e);
      else s.world().destroy(e);   // nyom nélkül (halál-esemény nincs)
    }
    return true;
  }
  if (t == "reveal") {
    const MissionRegion* r = def_->region(str(a, "region"));
    if (!r) return false;
    const uint8_t p = static_cast<uint8_t>(std::clamp<int64_t>(num(a, "player", 1), 1, Sim::kMaxPlayers));
    auto& rv = s.player_mut(p).reveals;
    if (rv.size() < 64) {
      rv.push_back({r->x + r->w / 2, r->y + r->h / 2, std::max(r->w, r->h) / 2 + 1,
                    static_cast<int32_t>(s.tick() + std::clamp<int64_t>(num(a, "ticks", 300), 1, 30 * 3600))});
    }
    return true;
  }
  if (t == "resources") {
    const uint8_t p = static_cast<uint8_t>(std::clamp<int64_t>(num(a, "player", 1), 0, Sim::kMaxPlayers));
    for (int k = 0; k < kResources; ++k) {
      if (const int64_t v = num(a, resource_id(k)); v != 0) s.add_resources(p, k, static_cast<int32_t>(std::clamp<int64_t>(v, -1000000, 1000000)));
    }
    return true;
  }
  if (t == "set_var") {
    vars_[str(a, "var")] = num(a, "value");
    return true;
  }
  if (t == "add_var") {
    vars_[str(a, "var")] += num(a, "value", 1);
    return true;
  }
  if (t == "enable" || t == "disable") {
    const std::string id = str(a, "trigger");
    for (size_t i = 0; i < def_->triggers.size(); ++i) {
      if (str(*def_->triggers[i], "id") != id) continue;
      triggers_[i].enabled = t == "enable";
      if (t == "enable") triggers_[i].done = false;
    }
    return true;
  }
  if (t == "timer") {
    timers_[str(a, "id")] = s.tick() + std::clamp<int64_t>(num(a, "ticks", 30), 1, 30LL * 3600 * 24);
    return true;
  }
  if (t == "cutscene") {
    if (!def_->cutscenes.count(str(a, "id"))) return false;
    cut_ = CutsceneView{};
    cut_.id = str(a, "id");
    cut_.started = s.tick();
    cut_step_ = 0;
    step_cutscene(s);
    return true;
  }
  if (t == "camera") {
    cut_.camera = str(a, "point");
    cut_.camera_ticks = static_cast<int32_t>(std::clamp<int64_t>(num(a, "ticks", 60), 0, 30 * 60));
    return true;
  }
  if (t == "letterbox") {
    cut_.letterbox = flag(a, "on", true);
    return true;
  }
  if (t == "subtitle") {
    cut_.speaker = str(a, "speaker");
    cut_.subtitle = str(a, "text");
    cut_.subtitle_until = s.tick() + std::clamp<int64_t>(num(a, "ticks", 90), 1, 30 * 60);
    return true;
  }
  if (t == "message") {
    if (messages_.size() >= kMaxMessages) messages_.erase(messages_.begin());
    messages_.push_back(Message{s.tick(), s.tick() + std::clamp<int64_t>(num(a, "ticks", 150), 1, 30 * 60), str(a, "text")});
    return true;
  }
  if (t == "win" || t == "lose") {
    if (result_ == Result::None) result_ = t == "win" ? Result::Won : Result::Lost;
    return true;
  }
  return false;
}

void MissionHost::step_cutscene(Sim& s) {
  if (cut_.id.empty()) return;
  const auto it = def_->cutscenes.find(cut_.id);
  if (it == def_->cutscenes.end()) return;
  const Value* steps = it->second->find("steps");
  if (!steps || !steps->is_array()) return;
  const auto& st = steps->items();
  while (cut_step_ < st.size() && s.tick() - cut_.started >= num(st[cut_step_], "t")) {
    run_action(s, st[cut_step_], kNoEntity);
    ++cut_step_;
    if (cut_.id.empty()) return;   // egy lépés másik átvezetőt indított vagy véget vetett neki
  }
  const int64_t end = num(*it->second, "length", st.empty() ? 0 : num(st.back(), "t"));
  if (cut_step_ >= st.size() && s.tick() - cut_.started >= end) {
    const std::string ended = cut_.id;
    const std::string cam = cut_.camera;
    cut_ = CutsceneView{};
    cut_.camera = cam;   // a kamera ott marad
    fire(s, Event{"cutscene_end", ended});
  }
}

void MissionHost::on_tick(Sim& s) {
  budget_ = kTickBudget;
  if (!started_) {
    started_ = true;
    fire(s, Event{"start", {}, 0, kNoEntity, 0});
  }
  // időzítők (lejárat, majd név szerint)
  std::vector<std::string> due;
  for (const auto& [id, at] : timers_) {
    if (at <= s.tick()) due.push_back(id);
  }
  for (const auto& id : due) {
    timers_.erase(id);
    fire(s, Event{"timer", id});
  }
  // régiók: belépés és kilépés (a régió sorrendjében, azonosító szerint)
  for (const MissionRegion& r : def_->regions) {
    std::vector<EntityId> now;
    s.world().pool<Unit>().each([&](EntityId e, const Unit&) {
      const Health* h = s.world().get<Health>(e);
      if (h && h->hp > 0 && !s.world().doomed(e) && in_region(s, e, r)) now.push_back(e);
    });
    std::sort(now.begin(), now.end());
    std::vector<EntityId>& was = inside_[r.id];
    std::vector<EntityId> in, out;
    std::set_difference(now.begin(), now.end(), was.begin(), was.end(), std::back_inserter(in));
    std::set_difference(was.begin(), was.end(), now.begin(), now.end(), std::back_inserter(out));
    was = now;
    for (const EntityId e : in) {
      const Health* h = s.world().get<Health>(e);
      fire(s, Event{"enter_region", r.id, h ? h->owner : uint8_t{0}, e});
    }
    for (const EntityId e : out) {
      const Health* h = s.world().alive(e) ? s.world().get<Health>(e) : nullptr;
      fire(s, Event{"leave_region", r.id, h ? h->owner : uint8_t{0}, e});
    }
  }
  fire(s, Event{"tick", {}, 0, kNoEntity, 0});
  // párbeszéd: a válasz nélküli csomópont az ideje után tovább
  if (!talk_.dialogue.empty()) {
    const MissionDialogue* d = def_->dialogue(talk_.dialogue);
    const DialogueNode* n = d ? d->node(talk_.node) : nullptr;
    if (!n) talk_ = DialogueView{};
    else if (n->choices.empty() && s.tick() - talk_.started >= n->ticks) advance_dialogue(s, n->next);
  }
  step_cutscene(s);
  while (!messages_.empty() && messages_.front().until < s.tick()) messages_.erase(messages_.begin());
}

void MissionHost::on_death(Sim& s, EntityId id, uint8_t owner) {
  budget_ = std::max(budget_, kTickBudget / 4);
  fire(s, Event{"unit_dies", {}, owner, id});
}

void MissionHost::on_command(Sim& s, const Command& c) {
  budget_ = std::max(budget_, kTickBudget / 4);
  if (c.def == "__skip") {   // átvezető kihagyása: a hátralévő lépések azonnal (a játékszabályt érintők sem maradnak ki)
    if (cut_.id.empty()) return;
    const auto it = def_->cutscenes.find(cut_.id);
    const Value* steps = it != def_->cutscenes.end() ? it->second->find("steps") : nullptr;
    if (steps && steps->is_array()) {
      const std::string id = cut_.id;
      while (!cut_.id.empty() && cut_.id == id && cut_step_ < steps->items().size()) {
        run_action(s, steps->items()[cut_step_], kNoEntity);
        ++cut_step_;
      }
      if (cut_.id == id) {
        const std::string cam = cut_.camera;
        cut_ = CutsceneView{};
        cut_.camera = cam;
        fire(s, Event{"cutscene_end", id});
      }
    }
    return;
  }
  if (talk_.dialogue.empty() || c.def != talk_.dialogue) return;
  const MissionDialogue* d = def_->dialogue(talk_.dialogue);
  const DialogueNode* n = d ? d->node(talk_.node) : nullptr;
  if (!n || n->choices.empty() || c.rx < 0 || c.rx >= static_cast<int32_t>(n->choices.size())) return;
  const bool human = std::any_of(def_->players.begin(), def_->players.end(),
                                 [&](const MissionPlayer& p) { return p.human && (c.owner == 0 || p.slot == c.owner); });
  if (!human) return;
  const DialogueChoice& ch = n->choices[static_cast<size_t>(c.rx)];
  if (!ch.var.empty()) vars_[ch.var] = ch.value;
  const std::string did = talk_.dialogue;
  fire(s, Event{"choice", did, c.owner, kNoEntity, c.rx});
  if (talk_.dialogue == did) advance_dialogue(s, ch.next);
}

uint64_t MissionHost::hash() const {
  core::StateHash h;
  h.add_u64(static_cast<uint64_t>(result_));
  h.add_u64(started_ ? 1 : 0);
  for (const auto& t : triggers_) {
    h.add_u64((t.enabled ? 1u : 0u) | (t.done ? 2u : 0u));
    h.add_i64(t.last);
  }
  for (const auto& [k, v] : vars_) { h.add_bytes(k); h.add_i64(v); }
  for (const auto& [k, v] : objectives_) { h.add_bytes(k); h.add_u64(v); }
  for (const auto& [k, v] : names_) { h.add_bytes(k); for (const EntityId e : v) h.add_u64(e); }
  for (const auto& [k, v] : timers_) { h.add_bytes(k); h.add_i64(v); }
  for (const auto& [k, v] : inside_) { h.add_bytes(k); for (const EntityId e : v) h.add_u64(e); }
  h.add_bytes(talk_.dialogue);
  h.add_bytes(talk_.node);
  h.add_i64(talk_.started);
  for (const auto& q : dialogue_queue_) h.add_bytes(q);
  h.add_bytes(cut_.id);
  h.add_i64(cut_.started);
  h.add_u64(cut_step_);
  h.add_u64(messages_.size());
  return h.value();
}

void MissionHost::save(core::ByteWriter& w) const {
  w.u8(static_cast<uint8_t>(result_));
  w.u8(started_ ? 1 : 0);
  w.u32(static_cast<uint32_t>(triggers_.size()));
  for (const auto& t : triggers_) { w.u8((t.enabled ? 1 : 0) | (t.done ? 2 : 0)); w.i64(t.last); }
  w.u32(static_cast<uint32_t>(vars_.size()));
  for (const auto& [k, v] : vars_) { w.str(k); w.i64(v); }
  w.u32(static_cast<uint32_t>(objectives_.size()));
  for (const auto& [k, v] : objectives_) { w.str(k); w.u8(v); }
  auto ids = [&](const std::map<std::string, std::vector<EntityId>>& m) {
    w.u32(static_cast<uint32_t>(m.size()));
    for (const auto& [k, v] : m) {
      w.str(k);
      w.u32(static_cast<uint32_t>(v.size()));
      for (const EntityId e : v) w.u32(e);
    }
  };
  ids(names_);
  ids(inside_);
  w.u32(static_cast<uint32_t>(timers_.size()));
  for (const auto& [k, v] : timers_) { w.str(k); w.i64(v); }
  w.u32(static_cast<uint32_t>(dialogue_queue_.size()));
  for (const auto& q : dialogue_queue_) w.str(q);
  w.str(talk_.dialogue); w.str(talk_.node); w.i64(talk_.started);
  w.str(cut_.id); w.i64(cut_.started); w.u8(cut_.letterbox ? 1 : 0); w.str(cut_.camera); w.i32(cut_.camera_ticks);
  w.str(cut_.subtitle); w.str(cut_.speaker); w.i64(cut_.subtitle_until); w.u32(static_cast<uint32_t>(cut_step_));
  w.u32(static_cast<uint32_t>(messages_.size()));
  for (const auto& m : messages_) { w.i64(m.tick); w.i64(m.until); w.str(m.text); }
}

bool MissionHost::load(core::ByteReader& r) {
  constexpr uint32_t kMax = 1u << 20;
  result_ = static_cast<Result>(std::min<uint8_t>(r.u8(), 2));
  started_ = r.u8() != 0;
  if (r.u32() != triggers_.size()) return false;   // más küldetés mentése
  for (auto& t : triggers_) {
    const uint8_t f = r.u8();
    t.enabled = (f & 1) != 0;
    t.done = (f & 2) != 0;
    t.last = r.i64();
  }
  vars_.clear();
  for (uint32_t i = 0, n = r.u32(); i < n && i < kMax && r.ok(); ++i) { const std::string k = r.str(); vars_[k] = r.i64(); }
  objectives_.clear();
  for (uint32_t i = 0, n = r.u32(); i < n && i < kMax && r.ok(); ++i) { const std::string k = r.str(); objectives_[k] = std::min<uint8_t>(r.u8(), 3); }
  auto ids = [&](std::map<std::string, std::vector<EntityId>>& m) {
    m.clear();
    for (uint32_t i = 0, n = r.u32(); i < n && i < kMax && r.ok(); ++i) {
      const std::string k = r.str();
      auto& v = m[k];
      for (uint32_t j = 0, c = r.u32(); j < c && j < kMax && r.ok(); ++j) v.push_back(r.u32());
    }
  };
  ids(names_);
  ids(inside_);
  timers_.clear();
  for (uint32_t i = 0, n = r.u32(); i < n && i < kMax && r.ok(); ++i) { const std::string k = r.str(); timers_[k] = r.i64(); }
  dialogue_queue_.clear();
  for (uint32_t i = 0, n = r.u32(); i < n && i < 64 && r.ok(); ++i) dialogue_queue_.push_back(r.str());
  talk_.dialogue = r.str(); talk_.node = r.str(); talk_.started = r.i64();
  cut_.id = r.str(); cut_.started = r.i64(); cut_.letterbox = r.u8() != 0; cut_.camera = r.str(); cut_.camera_ticks = r.i32();
  cut_.subtitle = r.str(); cut_.speaker = r.str(); cut_.subtitle_until = r.i64(); cut_step_ = r.u32();
  messages_.clear();
  for (uint32_t i = 0, n = r.u32(); i < n && i < kMaxMessages && r.ok(); ++i) {
    Message m;
    m.tick = r.i64();
    m.until = r.i64();
    m.text = r.str();
    messages_.push_back(std::move(m));
  }
  return r.ok();
}

std::optional<Replay> load_mission_package(const std::string& mission_dir, const std::string& maps_dir, uint64_t seed,
                                           std::string* error) {
  namespace fs = std::filesystem;
  auto slurp = [](const fs::path& p, std::string& out) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
  };
  Replay r;
  r.seed = seed;
  if (!slurp(fs::path(mission_dir) / "mission.json", r.mission)) {
    if (error) *error = "nincs mission.json: " + mission_dir;
    return std::nullopt;
  }
  const auto doc = content::parse(r.mission);
  if (!doc || !doc->is_object()) {
    if (error) *error = "hibás mission.json";
    return std::nullopt;
  }
  const std::string map = str(*doc, "map");
  if (map.empty() || map.find("..") != std::string::npos) {
    if (error) *error = "a küldetés pályája hiányzik vagy érvénytelen";
    return std::nullopt;
  }
  if (!slurp(fs::path(mission_dir) / map, r.map) && !slurp(fs::path(maps_dir) / map, r.map)) {
    if (error) *error = "a küldetés pályája nem található: " + map;
    return std::nullopt;
  }
  if (const std::string script = str(*doc, "script"); !script.empty()) {
    std::string src;
    if (script.find("..") != std::string::npos || !slurp(fs::path(mission_dir) / script, src)) {
      if (error) *error = "a küldetés szkriptje nem található: " + script;
      return std::nullopt;
    }
    r.scripts.push_back(ScriptFile{"mission", script, std::move(src)});
  }
  return r;
}

}  // namespace totu::sim
