// SPDX-License-Identifier: GPL-3.0-or-later
// Ablak nélküli szimulációs futtató (M2).
//   totu_sim scenario <ki.trep> [--seed S] [--units N] [--ticks T]   próbacsata rögzítése visszajátszásként
//   totu_sim scripted <ki.trep> <szkript.lua> [--seed S] [--ticks T]   Lua-küldetés rögzítése (M3)
//   totu_sim navgolden <ki.trep> <pálya.json> [--seed S] [--ticks T]  golden M4: pálya, alakzatok, kapu, építés
//   totu_sim bench [--players 8] [--units 200] [--size 256] [--ticks 1800]   mozgásmérés (M4 kapu: p99 <= 40 ms)
//   totu_sim verify <be.trep>                                         visszajátszás ellenőrzése (kilépési kód 0 = egyezik)
// A pályás parancsok a tartalmat (terepfajták) a --content <mappa> helyről töltik (alapérték: a forrásfa tartalma).
// A forgatókönyv két sereg csatája: mindkét fél N egysége a pálya két szélén jelenik meg, és rendszeres
// mozgásparancsokat kap; a parancsokat a visszajátszás rögzíti, így az ellenőrzés a forgatókönyv nélkül is fut.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "core/platform_info.hpp"
#include "core/rng.hpp"
#include "map/terrain.hpp"
#include "sim/replay.hpp"
#include "tools/nav_tools.hpp"

using namespace totu;
using core::Fixed;

namespace {

std::vector<sim::Command> scenario(uint64_t seed, int units, int64_t ticks) {
  core::Pcg32 rng(seed, 99);   // a forgatókönyv saját folyama (a szimulációétól független)
  std::vector<sim::Command> cmds;
  auto rnd = [&](int lo, int hi) { return rng.range(lo, hi); };
  for (int side = 0; side < 2; ++side) {
    for (int i = 0; i < units; ++i) {
      sim::Command c;
      c.kind = sim::Command::Kind::Spawn;
      c.tick = 0;
      c.owner = static_cast<uint8_t>(side + 1);
      c.x = Fixed::from_ratio(side == 0 ? rnd(200, 1600) : rnd(6400, 7800), 100);
      c.y = Fixed::from_ratio(rnd(200, 7800), 100);
      c.hp = rnd(80, 160);
      c.speed = Fixed::from_ratio(rnd(4, 9), 100);          // 0,04–0,09 csempe / tick
      c.damage = rnd(6, 14);
      c.spread = rnd(1, 6);
      c.cooldown = rnd(20, 45);
      c.range = Fixed::from_ratio(rnd(80, 600), 100);        // 0,8–6 csempe
      cmds.push_back(c);
    }
  }
  // mozgásparancsok: 60 tickenként mindkét fél egységeinek egy véletlen csoportja a közép felé indul
  for (int64_t t = 30; t < ticks; t += 60) {
    for (int side = 0; side < 2; ++side) {
      sim::Command c;
      c.kind = sim::Command::Kind::Move;
      c.tick = t;
      c.x = Fixed::from_ratio(rnd(3000, 5000), 100);
      c.y = Fixed::from_ratio(rnd(1000, 7000), 100);
      const int n = rnd(5, units);
      for (int k = 0; k < n; ++k) {
        // az azonosítók 1..2N; az első N az 1. félé
        const uint32_t base = side == 0 ? 1u : static_cast<uint32_t>(units) + 1u;
        c.units.push_back(base + rng.below(static_cast<uint32_t>(units)));
      }
      cmds.push_back(c);
    }
  }
  return cmds;
}

bool write_file(const char* path, const std::vector<uint8_t>& d) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
  return static_cast<bool>(out);
}

std::vector<uint8_t> read_file(const char* path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

const char* arg_str(int argc, char** argv, const char* name, const char* def) {
  for (int i = 0; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], name) == 0) return argv[i + 1];
  }
  return def;
}

long long arg_ll(int argc, char** argv, const char* name, long long def) {
  for (int i = 0; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], name) == 0) return std::strtoll(argv[i + 1], nullptr, 10);
  }
  return def;
}

}  // namespace

int main(int argc, char** argv) {
  std::printf("totu_sim %s – %s/%s, %s\n", core::version(), core::os_name(), core::cpu_arch(), core::compiler().c_str());
  if (argc >= 3 && std::strcmp(argv[1], "scenario") == 0) {
    const auto seed = static_cast<uint64_t>(arg_ll(argc, argv, "--seed", 20261112));
    const int units = static_cast<int>(arg_ll(argc, argv, "--units", 100));
    const int64_t ticks = arg_ll(argc, argv, "--ticks", 1800);
    const auto t0 = std::chrono::steady_clock::now();
    sim::Replay r = sim::record(seed, ticks, scenario(seed, units, ticks));
    sim::Sim check(seed);   // a csata valóban lezajlott-e: az élő egységek száma a végén
    for (const auto& c : r.commands) check.queue(c);
    for (int64_t t = 0; t < ticks; ++t) check.step();
    std::printf("élő egység a végén: %zu / %d\n", check.world().entity_count(), 2 * units);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (!write_file(argv[2], sim::encode(r))) { std::fprintf(stderr, "nem írható: %s\n", argv[2]); return 2; }
    std::printf("rögzítve: %s – %lld tick, %zu parancs, %zu ellenőrzőpont, végső hash 0x%016llx (%.1f ms)\n", argv[2],
                static_cast<long long>(r.ticks), r.commands.size(), r.checkpoints.size(),
                static_cast<unsigned long long>(r.checkpoints.back().hash), ms);
    return 0;
  }
  if (argc >= 4 && std::strcmp(argv[1], "scripted") == 0) {
    const auto seed = static_cast<uint64_t>(arg_ll(argc, argv, "--seed", 20261112));
    const int64_t ticks = arg_ll(argc, argv, "--ticks", 3600);
    const auto src = read_file(argv[3]);
    if (src.empty()) { std::fprintf(stderr, "nem olvasható: %s\n", argv[3]); return 2; }
    std::string name = argv[3];
    const auto slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    const auto t0 = std::chrono::steady_clock::now();
    sim::Replay r = sim::record(seed, ticks, {}, 30, {{"kuldetes", name, std::string(src.begin(), src.end())}});
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    const auto v = sim::verify(r);   // a napló kiírásához (és mert a felvételnek önmagával egyeznie kell)
    for (const auto& m : v.script_messages) std::printf("%s\n", m.c_str());
    if (!v.ok) { std::fprintf(stderr, "a felvétel önmagával sem egyezik: %s\n", v.error.c_str()); return 1; }
    if (!write_file(argv[2], sim::encode(r))) { std::fprintf(stderr, "nem írható: %s\n", argv[2]); return 2; }
    std::printf("rögzítve: %s – %lld tick, %zu ellenőrzőpont, végső hash 0x%016llx (%.1f ms)\n", argv[2],
                static_cast<long long>(r.ticks), r.checkpoints.size(),
                static_cast<unsigned long long>(r.checkpoints.back().hash), ms);
    return 0;
  }
  const char* content = arg_str(argc, argv, "--content", TOTU_CONTENT_DIR);
  if (argc >= 4 && std::strcmp(argv[1], "navgolden") == 0) {
    map::TerrainTable terrain;
    if (!tools::load_terrain(content, terrain)) return 2;
    const auto seed = static_cast<uint64_t>(arg_ll(argc, argv, "--seed", 20261112));
    const int64_t ticks = arg_ll(argc, argv, "--ticks", 2400);
    const auto mapbytes = read_file(argv[3]);
    if (mapbytes.empty()) { std::fprintf(stderr, "nem olvasható: %s\n", argv[3]); return 2; }
    const auto t0 = std::chrono::steady_clock::now();
    sim::Replay r = sim::record(seed, ticks, tools::golden_m4_commands(seed), 30, {},
                                std::string(mapbytes.begin(), mapbytes.end()), &terrain);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (r.checkpoints.empty()) { std::fprintf(stderr, "a pálya nem tölthető be\n"); return 2; }
    const auto v = sim::verify(r, &terrain);
    if (!v.ok) { std::fprintf(stderr, "a felvétel önmagával sem egyezik: %s\n", v.error.c_str()); return 1; }
    if (!write_file(argv[2], sim::encode(r))) { std::fprintf(stderr, "nem írható: %s\n", argv[2]); return 2; }
    std::printf("rögzítve: %s – %lld tick, %zu parancs, %zu ellenőrzőpont, végső hash 0x%016llx (%.1f ms)\n", argv[2],
                static_cast<long long>(r.ticks), r.commands.size(), r.checkpoints.size(),
                static_cast<unsigned long long>(r.checkpoints.back().hash), ms);
    return 0;
  }
  if (argc >= 2 && std::strcmp(argv[1], "bench") == 0) {
    map::TerrainTable terrain;
    if (!tools::load_terrain(content, terrain)) return 2;
    const int players = static_cast<int>(arg_ll(argc, argv, "--players", 8));
    const int units = static_cast<int>(arg_ll(argc, argv, "--units", 200));
    const int size = static_cast<int>(arg_ll(argc, argv, "--size", 256));
    const int ticks = static_cast<int>(arg_ll(argc, argv, "--ticks", 1800));
    const auto b = tools::bench(terrain, players, units, size, ticks, 7);
    std::printf("mérés: %d × %d egység, %d × %d pálya, %d tick – tick p50 %.2f ms, p99 %.2f ms, max %.2f ms, "
                "átlag %.2f ms; %lld útkeresés; kapu (p99 <= 40 ms): %s\n", players, units, size, size, ticks, b.p50,
                b.p99, b.max, b.avg, static_cast<long long>(b.paths), b.p99 <= 40.0 ? "TELJESÜL" : "NEM TELJESÜL");
    std::printf("  útkeresés: %lld (közvetlen %lld, közös %lld), kibontva %lld; idő: útkeresés %.0f ms, mozgás %.0f ms\n",
                static_cast<long long>(b.paths), static_cast<long long>(b.direct), static_cast<long long>(b.shared),
                static_cast<long long>(b.expanded), b.path_ms, b.move_ms);
    return b.p99 <= 40.0 ? 0 : 1;
  }
  if (argc >= 4 && std::strcmp(argv[1], "m6golden") == 0) {   // golden M6: két szkriptelt ursan gép egymás ellen
    map::TerrainTable terrain;
    std::optional<sim::Rules> rules;
    if (!tools::load_match_content(content, terrain, rules)) return 2;
    const auto mapbytes = read_file(argv[3]);
    const auto ai = read_file((std::string(content) + "/base/scripts/ai_ursan.lua").c_str());
    if (mapbytes.empty() || ai.empty()) { std::fprintf(stderr, "nem olvasható: %s vagy a gép szkriptje\n", argv[3]); return 2; }
    sim::Replay base;
    base.seed = static_cast<uint64_t>(arg_ll(argc, argv, "--seed", 20261113));
    base.ticks = arg_ll(argc, argv, "--ticks", 12 * 60 * 30);
    base.checkpoint_every = 150;
    base.map = std::string(mapbytes.begin(), mapbytes.end());
    base.factions = {"ursan", "ursan"};
    base.weather = 3;
    base.scripts = {sim::ScriptFile{"ai", "ai_players", "AI_PLAYERS = {1, 2}"},
                    sim::ScriptFile{"ai", "ai_ursan.lua", std::string(ai.begin(), ai.end())}};
    const sim::Replay r = sim::record_match(base, terrain, *rules);
    const auto v = sim::verify(r, &terrain, &*rules);
    if (!v.ok) { std::fprintf(stderr, "a felvétel önmagával sem egyezik: %s\n", v.error.c_str()); return 1; }
    if (!write_file(argv[2], sim::encode(r))) { std::fprintf(stderr, "nem írható: %s\n", argv[2]); return 2; }
    std::printf("rögzítve: %s – %lld tick, %zu ellenőrzőpont, végső hash 0x%016llx\n", argv[2],
                static_cast<long long>(r.ticks), r.checkpoints.size(),
                static_cast<unsigned long long>(r.checkpoints.back().hash));
    return 0;
  }
  if (argc >= 3 && std::strcmp(argv[1], "verify") == 0) {
    auto r = sim::decode(read_file(argv[2]));
    if (!r) { std::fprintf(stderr, "hibás vagy hiányzó visszajátszás: %s\n", argv[2]); return 2; }
    map::TerrainTable terrain;
    std::optional<sim::Rules> rules;
    if (!r->factions.empty()) {
      if (!tools::load_match_content(content, terrain, rules)) return 2;
    } else if (!r->map.empty() && !tools::load_terrain(content, terrain)) {
      return 2;
    }
    const auto v = sim::verify(*r, r->map.empty() ? nullptr : &terrain, rules ? &*rules : nullptr);
    if (!v.script_messages.empty()) {
      std::printf("szkriptnapló: %zu sor, az utolsó: %s\n", v.script_messages.size(), v.script_messages.back().c_str());
    }
    if (v.ok) {
      std::printf("EGYEZIK: %zu ellenőrzőpont, végső hash 0x%016llx\n", v.checked,
                  static_cast<unsigned long long>(r->checkpoints.back().hash));
      return 0;
    }
    std::printf("ELTÉR: %s", v.error.c_str());
    for (const auto& c : v.differing_components) std::printf(" [%s]", c.c_str());
    std::printf("\n");
    return 1;
  }
  std::fprintf(stderr, "használat: totu_sim scenario <ki.trep> [--seed S] [--units N] [--ticks T] | "
                       "scripted <ki.trep> <szkript.lua> [--seed S] [--ticks T] | navgolden <ki.trep> <pálya.json> | "
                       "bench [--players P] [--units N] [--size S] [--ticks T] | verify <be.trep> [--content mappa]\n");
  return 2;
}
