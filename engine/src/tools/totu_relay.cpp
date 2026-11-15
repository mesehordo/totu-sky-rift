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

int64_t now_ms() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
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
  net::Relay relay(desc, &terrain, &*rules, net::content_hash({content}), seats);
  if (!relay.ok()) {
    std::fprintf(stderr, "a meccs nem indítható: %s\n", relay.error().c_str());
    return 1;
  }
  net::EnetEndpoint ep;
  std::string err;
  if (!ep.listen(static_cast<uint16_t>(port), 16, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  std::printf("relé: %d. port, %d hely, %zu nép\n", port, seats, factions.size());
  size_t reported = 0;
  for (;;) {
    for (auto& e : ep.poll(5)) {
      if (e.kind == net::NetEvent::Kind::Message) relay.on_message(e.peer, e.data);
      else if (e.kind == net::NetEvent::Kind::Disconnect) relay.on_disconnect(e.peer);
    }
    if (relay.can_start()) {   // minden hely betelt, és mindenki kész
      if (!relay.start(now_ms())) {
        std::fprintf(stderr, "a meccs nem indítható: %s\n", relay.error().c_str());
        return 1;
      }
      std::printf("a meccs indul\n");
    }
    relay.advance(now_ms());
    for (auto& [p, b] : relay.take_outbox()) ep.send(p, b);
    ep.flush();
    for (; reported < relay.desyncs().size(); ++reported) {
      const auto& d = relay.desyncs()[reported];
      std::printf("SZINKRONHIBA: %lld. forduló, %d. játékos\n", static_cast<long long>(d.turn), d.player);
    }
    if (relay.started() && relay.connected_players() == 0) {
      std::printf("mindenki kilépett: %lld forduló\n", static_cast<long long>(relay.turns()));
      return relay.desyncs().empty() ? 0 : 3;
    }
  }
}
