// SPDX-License-Identifier: GPL-3.0-or-later
#include "sim/replay.hpp"

#include <algorithm>
#include <memory>

#include "content/json.hpp"
#include "map/pathfinder.hpp"
#include "script/ai.hpp"
#include "script/host.hpp"
#include "sim/mission.hpp"

namespace totu::sim {

namespace {

Checkpoint make_checkpoint(const Sim& s) {
  Checkpoint c;
  c.tick = s.tick();
  c.hash = s.hash();
  c.components = s.component_hashes();
  return c;
}

// A szkriptek betöltése: modonként egy állapot, a megadás sorrendjében csatlakoztatva, majd az on_start.
std::vector<std::unique_ptr<script::ScriptHost>> attach_scripts(Sim& s, const Replay& r, bool started,
                                                                MissionHost* mission = nullptr) {
  std::vector<std::unique_ptr<script::ScriptHost>> hosts;
  for (const auto& f : r.scripts) {
    script::ScriptHost* h = nullptr;
    for (auto& x : hosts) {
      if (x->name() == f.mod) h = x.get();
    }
    if (!h) {
      hosts.push_back(std::make_unique<script::ScriptHost>(f.mod, r.seed));
      h = hosts.back().get();
      if (f.mod == "mission") h->set_mission(mission);   // M11: a küldetés szkriptje (`Mission` tábla)
      if (f.mod == "victory") h->enable_victory();       // M13: a győzelmi szkript (`Victory` tábla)
      s.add_hooks(h);
    }
    if (!h->run(f.name, f.source)) return {};   // H-08: a hibás szkript a visszajátszás hibája
  }
  if (started) return hosts;
  for (auto& h : hosts) {
    h->start(s);
    if (h->errors() > 0) return {};   // H-08: az on_start hibája is
  }
  return hosts;
}

bool run(Sim*& s, std::unique_ptr<MatchInstance>& m, const Replay& r, std::vector<Checkpoint>& out,
         const map::TerrainTable* terrain, std::string& error, std::vector<std::string>* messages = nullptr,
         const Rules* rules = nullptr) {
  m = start_match(r, terrain, rules, error);
  if (!m) return false;
  s = m->sim.get();
  for (const auto& c : r.commands) s->queue(c);
  for (int64_t t = 0; t < r.ticks; ++t) {
    s->step();
    if (s->tick() % r.checkpoint_every == 0 || s->tick() == r.ticks) out.push_back(make_checkpoint(*s));
  }
  if (messages) {
    const auto ms = m->messages();
    messages->insert(messages->end(), ms.begin(), ms.end());
  }
  return true;
}

}  // namespace

MatchInstance::MatchInstance() = default;
MatchInstance::~MatchInstance() {
  // a szkriptek és a gépek a szimulációra hivatkoznak: előbb ők, utána a szimuláció, végül a pálya
  ais.clear();
  hosts.clear();
  sim.reset();
  mission.reset();
  rules.reset();
}

std::vector<std::string> MatchInstance::messages() const {
  std::vector<std::string> out;
  for (const auto& h : hosts) out.insert(out.end(), h->messages().begin(), h->messages().end());
  for (const auto& h : ais) out.insert(out.end(), h->messages().begin(), h->messages().end());
  return out;
}

std::unique_ptr<MatchInstance> start_match(const Replay& r, const map::TerrainTable* terrain, const Rules* rules,
                                           std::string& error, bool load_state) {
  auto m = std::make_unique<MatchInstance>();
  m->sim = std::make_unique<Sim>(r.seed);
  Sim& s = *m->sim;
  if (!r.map.empty()) {
    if (!terrain) {
      error = "a pályás visszajátszáshoz terepadat kell";
      return nullptr;
    }
    std::vector<std::string> errors;
    const auto doc = content::parse(r.map);
    std::optional<map::Map> mp;
    if (doc) mp = map::Map::from_json(*doc, *terrain, errors);
    if (!mp) {
      error = "hibás pálya a visszajátszásban" + (errors.empty() ? std::string() : ": " + errors.front());
      return nullptr;
    }
    m->map = std::make_unique<map::Map>(std::move(*mp));
    map::NavConfig nc;   // a game.json navigációs beállítása (H-90) – ugyanaz, mint a munkamenet egyéni csatájában
    if (rules && rules->game_raw) {
      std::vector<std::string> nerr;
      nc = map::NavConfig::from_json(rules->game_raw->find("navigation"), nerr);
    }
    m->nav = std::make_unique<map::NavGrid>(*m->map, *terrain, nc);
    m->pf = std::make_unique<map::Pathfinder>(*m->nav);
    s.attach_navigation(m->pf.get());
  }
  std::shared_ptr<const MissionDef> mdef;
  std::vector<std::string> factions = r.factions;
  if (!r.mission.empty()) {   // M11: a küldetés leírása (a népek a küldetés játékosaiból)
    std::vector<std::string> errs;
    auto md = MissionDef::parse(r.mission, rules, errs);
    if (!md) {
      error = "hibás küldetés" + (errs.empty() ? std::string() : ": " + errs.front());
      return nullptr;
    }
    mdef = std::make_shared<const MissionDef>(std::move(*md));
    if (const content::Value* ob = mdef->doc->find("objects"); ob && rules) {   // az objektumszerkesztő felülírásai
      std::vector<std::string> oerrs;
      m->rules = apply_objects(*rules, *ob, oerrs);
      if (!m->rules) {
        error = "hibás objektum: " + (oerrs.empty() ? std::string() : oerrs.front());
        return nullptr;
      }
      rules = m->rules.get();
    }
    factions.clear();
    for (const auto& p : mdef->players) {
      if (factions.size() < p.slot) factions.resize(p.slot);
      factions[p.slot - 1u] = p.faction;
    }
  }
  if (!factions.empty()) {   // M6: meccs a szabályadatokkal
    if (!rules || !m->map) {
      error = "a meccs-visszajátszáshoz pálya és szabályadat kell";
      return nullptr;
    }
    s.attach_rules(rules);
    if (!load_state) {
      MatchSetup setup;
      setup.factions = factions;
      setup.bases = !mdef;
      setup.weather = static_cast<MatchSetup::WeatherMode>(std::min<uint8_t>(r.weather, 3));
      setup.ai = r.ai;
      setup.upkeep = r.upkeep;
      setup.camps = r.camps;
      setup.fog = static_cast<MatchSetup::Fog>(std::min<uint8_t>(r.fog, 2));
      setup.daytime = static_cast<MatchSetup::Daytime>(std::min<uint8_t>(r.daytime, 2));
      setup.victory = static_cast<MatchSetup::Victory>(std::min<uint8_t>(r.victory, 4));
      setup.tempo = static_cast<MatchSetup::Tempo>(std::min<uint8_t>(r.tempo, 3));
      setup.minutes = std::clamp(r.minutes, 0, 240);
      setup.teams = r.teams;
      if (!s.setup_match(*m->map, setup, &error)) return nullptr;
    }
  }
  if (mdef) {   // a küldetés a szkriptek előtt csatolva (a betöltésnél ugyanebben a sorrendben)
    m->mission = std::make_unique<MissionHost>(mdef);
    s.add_hooks(m->mission.get());
    if (!load_state) m->mission->start(s);
  }
  m->hosts = attach_scripts(s, r, load_state, m->mission.get());
  if (m->hosts.empty() && !r.scripts.empty()) {
    error = "a visszajátszás szkriptje hibás";
    return nullptr;
  }
  if (!r.ai.empty()) {   // M9: a gépi játékosok a beágyazott csomagokból
    std::vector<script::AiPackage> pkgs;
    for (const auto& f : r.ai_files) {
      auto it = std::find_if(pkgs.begin(), pkgs.end(), [&](const script::AiPackage& p) { return p.id == f.mod; });
      if (it == pkgs.end()) {
        pkgs.push_back(script::AiPackage{f.mod, {}});
        it = pkgs.end() - 1;
      }
      it->files[f.name] = f.source;
    }
    m->ais = script::attach_ais(s, r.ai, pkgs, r.seed, !load_state, &error);
    if (m->ais.empty()) return nullptr;
  }
  return m;
}

namespace {

int format_of(const Replay& r) {
  if (std::any_of(r.teams.begin(), r.teams.end(), [](uint8_t t) { return t != 0; })) return 9;
  if (r.victory != 0 || r.tempo != 0 || r.minutes != 0) return 8;
  if (!r.mission.empty()) return 7;
  if (!r.factions.empty() && (!r.upkeep || r.camps || r.fog != 0 || r.daytime != 2)) return 6;
  if (!r.ai.empty()) return 5;
  if (!r.factions.empty()) return 4;
  for (const auto& c : r.commands) {
    if (c.needs_v4()) return 4;
  }
  if (!r.map.empty()) return 3;
  for (const auto& c : r.commands) {
    if (c.needs_v3()) return 3;
  }
  return r.scripts.empty() ? 1 : 2;
}

}  // namespace

Replay record(uint64_t seed, int64_t ticks, std::vector<Command> commands, int64_t checkpoint_every,
              std::vector<ScriptFile> scripts, std::string map, const map::TerrainTable* terrain) {
  Replay r;
  r.seed = seed;
  r.ticks = ticks;
  r.checkpoint_every = checkpoint_every > 0 ? checkpoint_every : 30;
  r.commands = std::move(commands);
  r.scripts = std::move(scripts);
  r.map = std::move(map);
  Sim* s = nullptr;
  std::unique_ptr<MatchInstance> m;
  std::string error;
  run(s, m, r, r.checkpoints, terrain, error);
  return r;
}

Replay record_match(Replay r, const map::TerrainTable& terrain, const Rules& rules) {
  r.checkpoint_every = r.checkpoint_every > 0 ? r.checkpoint_every : 30;
  r.checkpoints.clear();
  Sim* s = nullptr;
  std::unique_ptr<MatchInstance> m;
  std::string error;
  run(s, m, r, r.checkpoints, &terrain, error, nullptr, &rules);
  return r;
}

VerifyResult verify(const Replay& r, const map::TerrainTable* terrain, const Rules* rules) {
  VerifyResult v;
  Sim* s = nullptr;
  std::unique_ptr<MatchInstance> m;
  std::vector<Checkpoint> got;
  if (!run(s, m, r, got, terrain, v.error, &v.script_messages, rules)) return v;
  if (got.size() != r.checkpoints.size()) {
    v.error = "eltérő számú ellenőrzőpont";
    return v;
  }
  for (size_t i = 0; i < got.size(); ++i) {
    ++v.checked;
    if (got[i].tick == r.checkpoints[i].tick && got[i].hash == r.checkpoints[i].hash &&
        got[i].components == r.checkpoints[i].components) {
      continue;
    }
    v.first_bad_tick = r.checkpoints[i].tick;
    const auto& want = r.checkpoints[i].components;
    for (size_t k = 0; k < got[i].components.size() && k < want.size(); ++k) {
      if (got[i].components[k].second != want[k].second) v.differing_components.push_back(want[k].first);
    }
    v.error = "eltérés a(z) " + std::to_string(v.first_bad_tick) + ". tickben";
    return v;
  }
  v.ok = true;
  return v;
}

std::vector<uint8_t> encode(const Replay& r) {
  core::ByteWriter w;
  const int format = format_of(r);
  w.str("totu-replay");
  w.u32(static_cast<uint32_t>(format));
  w.u64(r.seed);
  w.i64(r.ticks);
  w.i64(r.checkpoint_every);
  w.u32(static_cast<uint32_t>(r.commands.size()));
  for (const auto& c : r.commands) save_command(w, c, format >= 4 ? 4 : format >= 3 ? 3 : 1);
  if (format >= 2) {
    w.u32(static_cast<uint32_t>(r.scripts.size()));
    for (const auto& f : r.scripts) { w.str(f.mod); w.str(f.name); w.str(f.source); }
  }
  if (format >= 3) w.str(r.map);
  if (format >= 4) {
    w.u32(static_cast<uint32_t>(r.factions.size()));
    for (const auto& f : r.factions) w.str(f);
    w.u8(r.weather);
  }
  if (format >= 5) {
    w.u32(static_cast<uint32_t>(r.ai.size()));
    for (const auto& a : r.ai) { w.u8(a.player); w.str(a.ai); w.str(a.difficulty); w.str(a.personality); }
    w.u32(static_cast<uint32_t>(r.ai_files.size()));
    for (const auto& f : r.ai_files) { w.str(f.mod); w.str(f.name); w.str(f.source); }
  }
  if (format >= 6) {
    w.u8(r.upkeep ? 1 : 0);
    w.u8(r.camps ? 1 : 0);
    w.u8(r.fog);
    w.u8(r.daytime);
  }
  if (format >= 7) w.str(r.mission);
  if (format >= 8) {
    w.u8(r.victory);
    w.u8(r.tempo);
    w.i32(r.minutes);
  }
  if (format >= 9) {   // csapatok
    w.u32(static_cast<uint32_t>(r.teams.size()));
    for (const uint8_t t : r.teams) w.u8(t);
  }
  w.u32(static_cast<uint32_t>(r.checkpoints.size()));
  for (const auto& c : r.checkpoints) {
    w.i64(c.tick);
    w.u64(c.hash);
    w.u32(static_cast<uint32_t>(c.components.size()));
    for (const auto& [name, h] : c.components) { w.str(name); w.u64(h); }
  }
  return w.data();
}

std::optional<Replay> decode(const std::vector<uint8_t>& data) {
  core::ByteReader rd(data);
  if (rd.str() != "totu-replay") return std::nullopt;
  const uint32_t version = rd.u32();
  if (version < 1 || version > 9) return std::nullopt;
  Replay r;
  r.seed = rd.u64();
  r.ticks = rd.i64();
  r.checkpoint_every = rd.i64();
  // H-88: legfeljebb 24 óra (30 Hz); az ellenőrzőpontok sűrűsége ésszerű
  if (r.ticks < 0 || r.ticks > 24LL * 3600 * 30 || r.checkpoint_every <= 0 || r.checkpoint_every > r.ticks + 1) return std::nullopt;
  const uint32_t n = rd.u32();
  for (uint32_t i = 0; i < n && rd.ok(); ++i) {
    auto c = load_command(rd, version >= 4 ? 4 : version >= 3 ? 3 : 1);
    if (!c) return std::nullopt;
    r.commands.push_back(std::move(*c));
  }
  if (version >= 2) {
    const uint32_t k = rd.u32();
    if ((version == 2 && k == 0) || k > 10000) return std::nullopt;
    for (uint32_t i = 0; i < k && rd.ok(); ++i) {
      ScriptFile f;
      f.mod = rd.str();
      f.name = rd.str();
      f.source = rd.str();
      r.scripts.push_back(std::move(f));
    }
  }
  if (version >= 3) r.map = rd.str();
  if (version >= 4) {
    const uint32_t nf = rd.u32();
    for (uint32_t i = 0; i < nf && i < 8 && rd.ok(); ++i) r.factions.push_back(rd.str());
    r.weather = rd.u8();
  }
  if (version >= 5) {
    const uint32_t na = rd.u32();
    for (uint32_t i = 0; i < na && i < 8 && rd.ok(); ++i) {
      AiSlot a;
      a.player = rd.u8();
      a.ai = rd.str();
      a.difficulty = rd.str();
      a.personality = rd.str();
      r.ai.push_back(std::move(a));
    }
    const uint32_t nf = rd.u32();
    if (nf > 10000) return std::nullopt;
    for (uint32_t i = 0; i < nf && rd.ok(); ++i) {
      ScriptFile f;
      f.mod = rd.str();
      f.name = rd.str();
      f.source = rd.str();
      r.ai_files.push_back(std::move(f));
    }
  }
  if (version >= 6) {
    r.upkeep = rd.u8() != 0;
    r.camps = rd.u8() != 0;
    r.fog = rd.u8();
    r.daytime = rd.u8();
  }
  if (version >= 7) r.mission = rd.str();
  if (version >= 8) {
    r.victory = rd.u8();
    r.tempo = rd.u8();
    r.minutes = rd.i32();
  }
  if (version >= 9) {
    const uint32_t nt = rd.u32();
    for (uint32_t i = 0; i < nt && i < Sim::kMaxPlayers && rd.ok(); ++i) r.teams.push_back(std::min<uint8_t>(rd.u8(), 4));
  }
  const uint32_t m = rd.u32();
  for (uint32_t i = 0; i < m && rd.ok(); ++i) {
    Checkpoint c;
    c.tick = rd.i64();
    c.hash = rd.u64();
    const uint32_t k = rd.u32();
    for (uint32_t j = 0; j < k && rd.ok(); ++j) {
      std::string name = rd.str();
      const uint64_t h = rd.u64();
      c.components.emplace_back(std::move(name), h);
    }
    r.checkpoints.push_back(std::move(c));
  }
  if (!rd.ok() || !rd.at_end()) return std::nullopt;
  return r;
}

}  // namespace totu::sim
