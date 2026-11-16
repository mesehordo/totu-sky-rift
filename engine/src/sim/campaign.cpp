// SPDX-License-Identifier: GPL-3.0-or-later
#include "sim/campaign.hpp"

#include <algorithm>
#include <filesystem>
#include <set>

#include "sim/mission.hpp"

namespace totu::sim {

using content::Value;

namespace {

std::string str(const Value& v, std::string_view k) {
  const Value* x = v.find(k);
  return x && x->is_string() ? x->text() : std::string();
}

std::vector<std::string> strings(const Value* v) {
  std::vector<std::string> out;
  if (v && v->is_array()) {
    for (const auto& x : v->items()) {
      if (x.is_string()) out.push_back(x.text());
    }
  }
  return out;
}

bool holds(const Value& cond, const std::map<std::string, int64_t>& flags) {
  const auto it = flags.find(str(cond, "var"));
  const int64_t a = it == flags.end() ? 0 : it->second;
  const Value* bv = cond.find("value");
  const int64_t b = bv ? bv->as_int().value_or(0) : 0;
  const std::string op = str(cond, "op").empty() ? "eq" : str(cond, "op");
  if (op == "ne") return a != b;
  if (op == "lt") return a < b;
  if (op == "le") return a <= b;
  if (op == "gt") return a > b;
  if (op == "ge") return a >= b;
  return a == b;
}

}  // namespace

const CampaignMission* CampaignDef::mission(const std::string& key) const {
  for (const auto& m : missions) {
    if (m.id == key) return &m;
  }
  return nullptr;
}

std::optional<CampaignDef> CampaignDef::parse(const std::string& text, std::vector<std::string>& errors) {
  content::ParseError pe;
  auto v = content::parse(text, &pe);
  if (!v || !v->is_object()) {
    errors.push_back("campaign.json: " + pe.message);
    return std::nullopt;
  }
  CampaignDef d;
  d.doc = std::make_shared<const Value>(std::move(*v));
  const Value& doc = *d.doc;
  const size_t before = errors.size();
  d.id = str(doc, "id");
  d.name = str(doc, "name");
  d.start = str(doc, "start");
  if (const Value* ms = doc.find("missions"); ms && ms->is_array()) {
    for (const auto& m : ms->items()) {
      CampaignMission cm;
      cm.id = str(m, "id");
      cm.dir = str(m, "dir");
      cm.carry = strings(m.find("carry"));
      cm.keep = strings(m.find("keep"));
      if (const Value* ns = m.find("next"); ns && ns->is_array()) {
        for (const auto& n : ns->items()) {
          const Value* c = n.find("if");
          cm.next.emplace_back(str(n, "to"), c && c->is_object() ? c : nullptr);
        }
      }
      if (cm.dir.empty() || cm.dir.find("..") != std::string::npos) errors.push_back("missions." + cm.id + ": érvénytelen mappa");
      d.missions.push_back(std::move(cm));
    }
  }
  std::set<std::string> ids;
  for (const auto& m : d.missions) {
    if (m.id.empty() || !ids.insert(m.id).second) errors.push_back("missions: hiányzó vagy ismétlődő azonosító: " + m.id);
  }
  if (!d.mission(d.start)) errors.push_back("a kezdő küldetés nem létezik: " + d.start);
  for (const auto& m : d.missions) {
    for (const auto& [to, c] : m.next) {
      if (!d.mission(to)) errors.push_back("missions." + m.id + ": ismeretlen következő küldetés: " + to);
    }
  }
  if (errors.size() > before) return std::nullopt;
  return d;
}

Value CampaignState::to_json() const {
  Value v = Value::object();
  v.set("campaign", Value::string(campaign));
  v.set("current", Value::string(current));
  Value dn = Value::array();
  for (const auto& x : done) dn.push(Value::string(x));
  v.set("done", dn);
  Value fl = Value::object();
  for (const auto& [k, x] : flags) fl.set(k, Value::number(std::to_string(x)));
  v.set("flags", fl);
  Value hs = Value::object();
  for (const auto& [k, x] : heroes) hs.set(k, x);
  v.set("heroes", hs);
  return v;
}

std::optional<CampaignState> CampaignState::from_json(const Value& v) {
  if (!v.is_object()) return std::nullopt;
  CampaignState s;
  s.campaign = str(v, "campaign");
  s.current = str(v, "current");
  s.done = strings(v.find("done"));
  if (const Value* fl = v.find("flags"); fl && fl->is_object()) {
    for (const auto& [k, x] : fl->members()) s.flags[k] = x.as_int().value_or(0);
  }
  if (const Value* hs = v.find("heroes"); hs && hs->is_object()) {
    for (const auto& [k, x] : hs->members()) s.heroes[k] = x;
  }
  return s;
}

CampaignState CampaignState::fresh(const CampaignDef& d) {
  CampaignState s;
  s.campaign = d.id;
  s.current = d.start;
  return s;
}

std::optional<Replay> prepare_mission(const CampaignDef& def, const CampaignState& st, const std::string& campaign_dir,
                                      const std::string& maps_dir, uint64_t seed, std::string* error) {
  const CampaignMission* cm = def.mission(st.current);
  if (!cm) {
    if (error) *error = st.current.empty() ? "a hadjárat véget ért" : "ismeretlen küldetés: " + st.current;
    return std::nullopt;
  }
  auto r = load_mission_package((std::filesystem::path(campaign_dir) / cm->dir).string(), maps_dir, seed, error);
  if (!r) return std::nullopt;
  auto m = content::parse(r->mission);
  if (!m || !m->is_object()) {
    if (error) *error = "hibás küldetés: " + cm->id;
    return std::nullopt;
  }
  // a megtartott változók a küldetés változóinak kezdőértékébe (csak a küldetésben is meglévő nevek)
  if (Value* vars = m->find("variables"); vars && vars->is_object()) {
    for (auto& [k, v] : vars->members()) {
      if (const auto it = st.flags.find(k); it != st.flags.end()) v = Value::number(std::to_string(it->second));
    }
  }
  // az átvitt hősök a névvel adott egységekbe
  if (Value* us = m->find("units"); us && us->is_array()) {
    for (auto& u : us->items()) {
      const std::string name = str(u, "name");
      if (const auto it = st.heroes.find(name); it != st.heroes.end() &&
          std::find(cm->carry.begin(), cm->carry.end(), name) != cm->carry.end()) {
        u.set("hero", it->second);
      }
    }
  }
  r->mission = content::dump(*m);
  return r;
}

bool finish_mission(const CampaignDef& def, CampaignState& st, MatchInstance& m) {
  const CampaignMission* cm = def.mission(st.current);
  if (!cm || !m.mission || m.mission->result() != MissionHost::Result::Won) return false;
  for (const auto& name : cm->carry) {
    if (const auto* ids = m.mission->named(name)) {
      for (const EntityId e : *ids) {
        const Health* h = m.sim->world().alive(e) ? m.sim->world().get<Health>(e) : nullptr;
        if (h && h->hp > 0 && m.sim->world().get<HeroState>(e)) {
          st.heroes[name] = export_hero(*m.sim, e);
          break;
        }
      }
    }
  }
  for (const auto& k : cm->keep) st.flags[k] = m.mission->var(k);
  if (std::find(st.done.begin(), st.done.end(), cm->id) == st.done.end()) st.done.push_back(cm->id);
  std::string next;
  for (const auto& [to, cond] : cm->next) {
    if (!cond || holds(*cond, st.flags)) {
      next = to;
      break;
    }
  }
  st.current = next;
  return true;
}

}  // namespace totu::sim
