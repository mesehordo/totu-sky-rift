// SPDX-License-Identifier: GPL-3.0-or-later
// Tömeges gép–gép meccsek (az M9 kapuja: „100 gép–gép meccs elakadás nélkül”): minden meccs saját szimulációval,
// párhuzamos szálakon; játékosonként mérőszámok és elakadás-észlelés. Elakadás: egy még játszó (nem vesztes) játékos
// haladása (gyűjtött nyersanyag, egységek és épületek száma, kutatások) 3 percig nem változik, és a meccsnek nincs győztese.
#include "tools/aibatch.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <mutex>
#include <thread>

#include "content/json.hpp"
#include "map/map.hpp"
#include "map/pathfinder.hpp"
#include "script/host.hpp"
#include "sim/sim.hpp"

namespace totu::tools {

namespace {

std::string slurp(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

struct PlayerTrack {
  int64_t last_sig = -1, last_change = 0;
};

int64_t progress_sig(sim::Sim& s, uint8_t p) {
  const auto& pl = s.player(p);
  int64_t sig = 0;
  for (int r = 0; r < sim::kResources; ++r) sig += pl.gathered[static_cast<size_t>(r)] / 1000;
  int64_t things = 0;
  s.world().pool<sim::Health>().each([&](sim::EntityId, const sim::Health& h) { things += h.owner == p && h.hp > 0 ? 1 : 0; });
  return sig * 1000003 + things * 131 + static_cast<int64_t>(pl.researched.size());
}

}  // namespace

AiBatchResult run_ai_match(const AiBatchMatch& m, const map::TerrainTable& terrain, const sim::Rules& rules) {
  AiBatchResult res;
  res.match = m;
  std::vector<std::string> errors;
  const auto doc = content::parse(slurp(m.map_path));
  if (!doc) {
    res.error = "a pálya nem olvasható";
    return res;
  }
  auto mp = map::Map::from_json(*doc, terrain, errors);
  if (!mp) {
    res.error = "hibás pálya";
    return res;
  }
  map::NavGrid nav(*mp, terrain);
  map::Pathfinder pf(nav);
  sim::Sim s(m.seed);
  s.attach_navigation(&pf);
  s.attach_rules(&rules);
  sim::MatchSetup setup;
  setup.factions = m.factions;
  setup.camps = m.camps;
  std::string err;
  if (!s.setup_match(*mp, setup, &err)) {
    res.error = "a meccs nem indult: " + err;
    return res;
  }
  script::ScriptHost ai("ai", m.seed);
  std::string players = "AI_PLAYERS = {";
  for (size_t i = 0; i < m.factions.size(); ++i) players += (i ? ", " : "") + std::to_string(i + 1);
  players += "}";
  if (!ai.run("ai_players", players) || !ai.run(m.script_name, m.script_source)) {
    res.error = "a gép szkriptje hibás";
    return res;
  }
  s.add_hooks(&ai);
  ai.start(s);
  std::vector<PlayerTrack> tr(m.factions.size() + 1);
  constexpr int64_t kStall = 3 * 60 * 30;   // 3 perc haladás nélkül
  for (int64_t t = 0; t < m.ticks; ++t) {
    s.step();
    if (t % 30 != 0) continue;
    if (s.winner() != 0) {
      res.winner = static_cast<uint8_t>(s.winner());
      res.end_tick = s.tick();
      break;
    }
    for (uint8_t p = 1; p <= m.factions.size(); ++p) {
      if (s.player(p).defeated) continue;
      const int64_t sig = progress_sig(s, p);
      auto& k = tr[p];
      if (sig != k.last_sig) {
        k.last_sig = sig;
        k.last_change = s.tick();
      } else if (s.tick() - k.last_change >= kStall && res.stalled_player == 0) {   // elakadt
        res.stalled_player = p;
        res.stall_tick = s.tick();
      }
    }
  }
  if (res.end_tick == 0) res.end_tick = s.tick();
  for (uint8_t p = 1; p <= m.factions.size(); ++p) {
    AiBatchResult::Player pr;
    const auto& pl = s.player(p);
    for (int r = 0; r < sim::kResources; ++r) pr.gathered += pl.gathered[static_cast<size_t>(r)] / 1000;
    s.world().pool<sim::Health>().each([&](sim::EntityId e, const sim::Health& h) {
      if (h.owner != p || h.hp <= 0) return;
      if (s.world().get<sim::Building>(e)) ++pr.buildings;
      else ++pr.units;
    });
    pr.researched = static_cast<int>(pl.researched.size());
    pr.defeated = pl.defeated;
    res.players.push_back(pr);
  }
  res.script_errors = ai.errors();
  res.ok = res.error.empty() && res.stalled_player == 0 && res.script_errors == 0;
  return res;
}

std::vector<AiBatchResult> run_ai_batch(const std::vector<AiBatchMatch>& matches, const map::TerrainTable& terrain,
                                        const sim::Rules& rules, int threads) {
  std::vector<AiBatchResult> out(matches.size());
  std::atomic<size_t> next{0};
  std::mutex print_mu;
  auto worker = [&] {
    for (size_t i = next++; i < matches.size(); i = next++) {
      out[i] = run_ai_match(matches[i], terrain, rules);
      std::lock_guard<std::mutex> lock(print_mu);
      const auto& r = out[i];
      std::printf("%3zu. %-24s mag %-6llu %s – %s%s\n", i + 1, (r.match.factions.empty() ? "" : r.match.factions[0] + "–" +
                  (r.match.factions.size() > 1 ? r.match.factions[1] : "")).c_str(), static_cast<unsigned long long>(r.match.seed),
                  r.ok ? "rendben" : "HIBA", r.error.empty() ? (r.stalled_player ? "elakadt" : r.winner ? "győztes van" : "lejárt")
                                                             : r.error.c_str(),
                  r.stalled_player ? (" (" + std::to_string(r.stalled_player) + ". játékos, " + std::to_string(r.stall_tick / 30) + " s)").c_str() : "");
      std::fflush(stdout);
    }
  };
  std::vector<std::thread> pool;
  const int n = std::max(1, threads);
  for (int k = 0; k < n; ++k) pool.emplace_back(worker);
  for (auto& t : pool) t.join();
  return out;
}

}  // namespace totu::tools
