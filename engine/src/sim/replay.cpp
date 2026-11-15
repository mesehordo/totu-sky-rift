// SPDX-License-Identifier: GPL-3.0-or-later
#include "sim/replay.hpp"

#include <algorithm>
#include <memory>

#include "content/json.hpp"
#include "map/pathfinder.hpp"
#include "script/ai.hpp"
#include "script/host.hpp"

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
std::vector<std::unique_ptr<script::ScriptHost>> attach_scripts(Sim& s, const Replay& r) {
  std::vector<std::unique_ptr<script::ScriptHost>> hosts;
  for (const auto& f : r.scripts) {
    script::ScriptHost* h = nullptr;
    for (auto& x : hosts) {
      if (x->name() == f.mod) h = x.get();
    }
    if (!h) {
      hosts.push_back(std::make_unique<script::ScriptHost>(f.mod, r.seed));
      h = hosts.back().get();
      s.add_hooks(h);
    }
    if (!h->run(f.name, f.source)) return {};   // H-08: a hibás szkript a visszajátszás hibája
  }
  for (auto& h : hosts) {
    h->start(s);
    if (h->errors() > 0) return {};   // H-08: az on_start hibája is
  }
  return hosts;
}

// A pálya és a navigáció a futás idejére (a szimuláció csak hivatkozik rájuk).
struct Navigation {
  std::optional<map::Map> map;
  std::unique_ptr<map::NavGrid> nav;
  std::unique_ptr<map::Pathfinder> pf;
};

bool attach_map(Sim& s, const Replay& r, const map::TerrainTable* terrain, Navigation& nav, std::string& error) {
  if (r.map.empty()) return true;
  if (!terrain) {
    error = "a pályás visszajátszáshoz terepadat kell";
    return false;
  }
  std::vector<std::string> errors;
  const auto doc = content::parse(r.map);
  if (doc) nav.map = map::Map::from_json(*doc, *terrain, errors);
  if (!nav.map) {
    error = "hibás pálya a visszajátszásban" + (errors.empty() ? std::string() : ": " + errors.front());
    return false;
  }
  nav.nav = std::make_unique<map::NavGrid>(*nav.map, *terrain);
  nav.pf = std::make_unique<map::Pathfinder>(*nav.nav);
  s.attach_navigation(nav.pf.get());
  return true;
}

bool run(Sim& s, const Replay& r, std::vector<Checkpoint>& out, const map::TerrainTable* terrain, std::string& error,
         std::vector<std::string>* messages = nullptr, const Rules* rules = nullptr) {
  Navigation navigation;
  if (!attach_map(s, r, terrain, navigation, error)) return false;
  if (!r.factions.empty()) {   // M6: meccs a szabályadatokkal
    if (!rules || !navigation.map) {
      error = "a meccs-visszajátszáshoz pálya és szabályadat kell";
      return false;
    }
    s.attach_rules(rules);
    MatchSetup setup;
    setup.factions = r.factions;
    setup.weather = static_cast<MatchSetup::WeatherMode>(std::min<uint8_t>(r.weather, 3));
    setup.ai = r.ai;
    setup.upkeep = r.upkeep;
    setup.camps = r.camps;
    setup.fog = static_cast<MatchSetup::Fog>(std::min<uint8_t>(r.fog, 2));
    setup.daytime = static_cast<MatchSetup::Daytime>(std::min<uint8_t>(r.daytime, 2));
    if (!s.setup_match(*navigation.map, setup, &error)) return false;
  }
  const auto hosts = attach_scripts(s, r);
  if (hosts.empty() && !r.scripts.empty()) {
    error = "a visszajátszás szkriptje hibás";
    return false;
  }
  std::vector<std::unique_ptr<script::ScriptHost>> ais;
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
    ais = script::attach_ais(s, r.ai, pkgs, r.seed, true, &error);
    if (ais.empty()) return false;
  }
  for (const auto& c : r.commands) s.queue(c);
  for (int64_t t = 0; t < r.ticks; ++t) {
    s.step();
    if (s.tick() % r.checkpoint_every == 0 || s.tick() == r.ticks) out.push_back(make_checkpoint(s));
  }
  if (messages) {
    for (const auto& h : hosts) messages->insert(messages->end(), h->messages().begin(), h->messages().end());
    for (const auto& h : ais) messages->insert(messages->end(), h->messages().begin(), h->messages().end());
  }
  return true;
}

int format_of(const Replay& r) {
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
  Sim s(seed);
  std::string error;
  run(s, r, r.checkpoints, terrain, error);
  return r;
}

Replay record_match(Replay r, const map::TerrainTable& terrain, const Rules& rules) {
  r.checkpoint_every = r.checkpoint_every > 0 ? r.checkpoint_every : 30;
  r.checkpoints.clear();
  Sim s(r.seed);
  std::string error;
  run(s, r, r.checkpoints, &terrain, error, nullptr, &rules);
  return r;
}

VerifyResult verify(const Replay& r, const map::TerrainTable* terrain, const Rules* rules) {
  VerifyResult v;
  Sim s(r.seed);
  std::vector<Checkpoint> got;
  if (!run(s, r, got, terrain, v.error, &v.script_messages, rules)) return v;
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
  if (version < 1 || version > 6) return std::nullopt;
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
