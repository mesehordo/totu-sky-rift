// SPDX-License-Identifier: GPL-3.0-or-later
// Dedikált relé (M10, ADR-009): fej nélkül futtatja a meccset, sorba rendezi és továbbítja a játékosok parancsait,
// ellenőrzi a szinkront, és a visszatérő játékosnak mentést küld.
//
//   totu_relay --map <pálya.json> --factions ursan,wullof [--seats 2] [--port 47800] [--seed 1] [--content <mappa>]
//
// A meccs akkor indul, amikor minden emberi hely betelt; minden játékos kilépése után leáll.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "map/map.hpp"
#include "net/content_hash.hpp"
#include "net/enet_transport.hpp"
#include "net/lockstep.hpp"
#include "net/relay_server.hpp"
#include "sim/rules.hpp"
#include "tools/nav_tools.hpp"

using namespace totu;

namespace {

std::string slurp(const std::string& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> out;
  std::stringstream ss(s);
  for (std::string x; std::getline(ss, x, ',');) {
    if (!x.empty()) out.push_back(x);
  }
  return out;
}


}  // namespace

int main(int argc, char** argv) {
  std::string map_path, content = TOTU_CONTENT_DIR;
  std::vector<std::string> factions;
  int seats = 2, port = 47800;
  uint64_t seed = 1;
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string k = argv[i], v = argv[i + 1];
    if (k == "--map") map_path = v;
    else if (k == "--factions") factions = split(v);
    else if (k == "--seats") seats = std::atoi(v.c_str());
    else if (k == "--port") port = std::atoi(v.c_str());
    else if (k == "--seed") seed = std::strtoull(v.c_str(), nullptr, 10);
    else if (k == "--content") content = v;
  }
  if (map_path.empty() || factions.size() < 2 || seats < 1 || seats > 8 || port <= 0 || port > 65535) {
    std::fprintf(stderr, "használat: totu_relay --map <pálya.json> --factions a,b [--seats N] [--port P] [--seed S]\n");
    return 2;
  }
  map::TerrainTable terrain;
  std::optional<sim::Rules> rules;
  if (!tools::load_match_content(content, terrain, rules)) return 2;
  sim::Replay desc;
  desc.seed = seed;
  desc.map = slurp(map_path);
  desc.factions = factions;
  net::RelayServer server;
  std::string err;
  if (!server.start(desc, &terrain, &*rules, net::content_hash({content}), seats, static_cast<uint16_t>(port), &err)) {
    std::fprintf(stderr, "a relé nem indítható: %s\n", err.c_str());
    return 1;
  }
  std::printf("relé: %d. port, %d hely, %zu nép (a meccs akkor indul, amikor mindenki kész)\n", port, seats, factions.size());
  server.run_blocking();
  const auto ds = server.desyncs();
  for (const auto& d : ds) std::printf("SZINKRONHIBA: %lld. forduló, %d. játékos\n", static_cast<long long>(d.turn), d.player);
  std::printf("vége: %lld forduló\n", static_cast<long long>(server.turns()));
  return ds.empty() ? 0 : 3;
}
