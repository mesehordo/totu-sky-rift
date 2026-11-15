// SPDX-License-Identifier: GPL-3.0-or-later
// Hálózati mérés (M10, NET-LOSS): relé és N kliens valódi UDP-n (ENet) a helyi gépen, közöttük egy veszteséges,
// késleltető proxy. Mér: a relé fordulóit az ideálishoz képest, a parancs késését (beküldéstől a saját parancs
// visszaérkezéséig), a kliensek lemaradását; ellenőrzi, hogy nincs szinkronhiba.
//
//   totu_netbench [--clients 2] [--loss 0] [--rtt 50] [--seconds 10] [--map <pálya.json>] [--port 47950]
//
// Kilépési kód: 0 rendben; 3 szinkronhiba vagy eltérő hash; 1 indítási hiba.
#include <enet/enet.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "core/rng.hpp"
#include "map/map.hpp"
#include "net/content_hash.hpp"
#include "net/enet_transport.hpp"
#include "net/lockstep.hpp"
#include "net/relay_server.hpp"
#include "sim/rules.hpp"
#include "tools/nav_tools.hpp"

using namespace totu;
using clock_type = std::chrono::steady_clock;

namespace {

int64_t now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(clock_type::now().time_since_epoch()).count();
}

// UDP-proxy: a kliensek a proxyhoz kapcsolódnak; kliensenként saját felmenő foglalat a reléhez. Mindkét irányban
// `loss` ezrelék valószínűséggel eldob, és `delay` ms-mal késleltet.
class LossyProxy {
 public:
  bool open(uint16_t listen_port, uint16_t relay_port, int loss_permille, int one_way_ms) {
    loss_ = loss_permille;
    delay_ = one_way_ms;
    enet_address_set_host(&relay_, "127.0.0.1");
    relay_.port = relay_port;
    down_ = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (down_ == ENET_SOCKET_NULL) return false;
    ENetAddress a{};
    a.host = ENET_HOST_ANY;
    a.port = listen_port;
    if (enet_socket_bind(down_, &a) != 0) return false;
    enet_socket_set_option(down_, ENET_SOCKOPT_NONBLOCK, 1);
    return true;
  }
  ~LossyProxy() {
    for (auto& [k, c] : conns_) enet_socket_destroy(c.up);
    if (down_ != ENET_SOCKET_NULL) enet_socket_destroy(down_);
  }
  void pump() {
    uint8_t buf[4096];
    ENetBuffer b{};
    b.data = buf;
    b.dataLength = sizeof buf;
    ENetAddress from{};
    for (int n; (n = enet_socket_receive(down_, &from, &b, 1)) > 0;) {   // kliens → relé
      const uint64_t key = (uint64_t{from.host} << 16) | from.port;
      auto it = conns_.find(key);
      if (it == conns_.end()) {
        Conn c;
        c.client = from;
        c.up = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
        ENetAddress any{};
        any.host = ENET_HOST_ANY;
        any.port = 0;
        enet_socket_bind(c.up, &any);
        enet_socket_set_option(c.up, ENET_SOCKOPT_NONBLOCK, 1);
        it = conns_.emplace(key, c).first;
      }
      queue(it->first, true, buf, static_cast<size_t>(n));
    }
    for (auto& [key, c] : conns_) {   // relé → kliens
      for (int n; (n = enet_socket_receive(c.up, &from, &b, 1)) > 0;) queue(key, false, buf, static_cast<size_t>(n));
    }
    const int64_t t = now_ms();
    while (!wire_.empty() && wire_.front().at <= t) {
      Packet& p = wire_.front();
      Conn& c = conns_[p.key];
      ENetBuffer o{};
      o.data = p.data.data();
      o.dataLength = p.data.size();
      if (p.up) enet_socket_send(c.up, &relay_, &o, 1);
      else enet_socket_send(down_, &c.client, &o, 1);
      wire_.pop_front();
    }
  }
  int64_t dropped = 0, passed = 0;

 private:
  struct Conn {
    ENetAddress client{};
    ENetSocket up = ENET_SOCKET_NULL;
  };
  struct Packet {
    int64_t at;
    uint64_t key;
    bool up;
    std::vector<uint8_t> data;
  };
  void queue(uint64_t key, bool up, const uint8_t* d, size_t n) {
    if (static_cast<int>(rng_.below(1000)) < loss_) {
      ++dropped;
      return;
    }
    ++passed;
    wire_.push_back({now_ms() + delay_, key, up, std::vector<uint8_t>(d, d + n)});   // azonos késés: sorrendtartó
  }
  ENetSocket down_ = ENET_SOCKET_NULL;
  ENetAddress relay_{};
  std::map<uint64_t, Conn> conns_;
  std::deque<Packet> wire_;
  int loss_ = 0, delay_ = 0;
  core::Pcg32 rng_{20261115, 7};
};

std::string slurp(const std::string& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}  // namespace

int main(int argc, char** argv) {
  int clients = 2, loss = 0, rtt = 50, seconds = 10, port = 47950;
  std::string content = TOTU_CONTENT_DIR;
  std::string map_path = content + "/base/maps/nyolc_tuz.json";
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string k = argv[i], v = argv[i + 1];
    if (k == "--clients") clients = std::atoi(v.c_str());
    else if (k == "--loss") loss = std::atoi(v.c_str());
    else if (k == "--rtt") rtt = std::atoi(v.c_str());
    else if (k == "--seconds") seconds = std::atoi(v.c_str());
    else if (k == "--map") map_path = v;
    else if (k == "--port") port = std::atoi(v.c_str());
    else if (k == "--content") content = v;
  }
  clients = std::clamp(clients, 1, 8);
  map::TerrainTable terrain;
  std::optional<sim::Rules> rules;
  if (!tools::load_match_content(content, terrain, rules)) return 1;
  static const char* kFactions[8] = {"ursan", "wullof", "croftian", "elves", "imperium", "hukucha", "ooni", "ursan"};
  sim::Replay desc;
  desc.seed = 77;
  desc.map = slurp(map_path);
  for (int i = 0; i < clients; ++i) desc.factions.push_back(kFactions[i]);
  if (clients == 1) desc.factions.push_back("wullof");
  const uint64_t ch = net::content_hash({content});
  net::RelayServer server;
  std::string err;
  if (!server.start(desc, &terrain, &*rules, ch, clients, static_cast<uint16_t>(port), &err)) {
    std::fprintf(stderr, "relé: %s\n", err.c_str());
    return 1;
  }
  server.spawn();
  LossyProxy proxy;
  if (!proxy.open(static_cast<uint16_t>(port + 1), static_cast<uint16_t>(port), loss * 10, rtt / 2)) {
    std::fprintf(stderr, "a proxy nem nyitható meg\n");
    return 1;
  }
  // kliensenként saját szál (a valóságban külön gép): a nyolc szimuláció ne egy szálon versengjen
  struct Side {
    net::EnetEndpoint ep;
    std::unique_ptr<net::Client> c;
    net::PeerId relay = 0;
    bool said = false;
    std::deque<int64_t> sent;   // a beküldött parancsok ideje
    size_t seen = 0;
    std::vector<int64_t> lat;
    std::atomic<int64_t> turns{0};
    std::thread th;
  };
  std::atomic<int> phase{0};   // 0 csatlakozás, 1 játék (parancsok), 2 utolérés, 3 vége
  std::vector<std::unique_ptr<Side>> sides;
  for (int i = 0; i < clients; ++i) {
    auto s = std::make_unique<Side>();
    if (!s->ep.connect("127.0.0.1", static_cast<uint16_t>(port + 1), &err)) {
      std::fprintf(stderr, "kliens: %s\n", err.c_str());
      return 1;
    }
    s->c = std::make_unique<net::Client>(&terrain, &*rules, ch, "m" + std::to_string(i + 1));
    sides.push_back(std::move(s));
  }
  for (auto& sp : sides) {
    Side* s = sp.get();
    s->th = std::thread([s, &phase] {
      int64_t next_cmd = 0;
      while (phase.load() != 3) {
        for (auto& e : s->ep.poll(0)) {
          if (e.kind == net::NetEvent::Kind::Connect) {
            s->relay = e.peer;
            if (!s->said) {
              s->ep.send(s->relay, s->c->hello());
              s->c->set_ready(true);
              s->said = true;
            }
          } else if (e.kind == net::NetEvent::Kind::Message) {
            s->c->on_message(e.data);
          }
        }
        s->c->update();
        s->turns.store(s->c->turns());
        if (phase.load() == 1 && s->c->sim() && now_ms() >= next_cmd) {   // félmásodpercenként egy saját egység megáll
          if (next_cmd == 0) next_cmd = now_ms();
          next_cmd += 500;
          sim::Sim* sm = s->c->sim();
          sim::EntityId u = sim::kNoEntity;
          sm->world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit&) {
            if (u == sim::kNoEntity && sm->world().get<sim::Health>(e)->owner == s->c->slot()) u = e;
          });
          if (u != sim::kNoEntity) {
            sim::Command c;
            c.kind = sim::Command::Kind::Stop;
            c.units = {u};
            s->c->submit(c);
            s->sent.push_back(now_ms());
          }
        }
        while (s->seen < s->c->own_turns().size() && !s->sent.empty()) {
          s->lat.push_back(now_ms() - s->sent.front());
          s->sent.pop_front();
          ++s->seen;
        }
        if (s->relay) {
          for (auto& b : s->c->take_outbox()) s->ep.send(s->relay, b);
        }
        s->ep.flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    });
  }
  auto finish = [&] {
    phase.store(3);
    for (auto& sp : sides) {
      if (sp->th.joinable()) sp->th.join();
    }
  };
  // csatlakozás és indulás (a fő szál a proxyt pumpálja)
  const int64_t t_join = now_ms();
  while (!server.started() && now_ms() - t_join < 30000) {
    proxy.pump();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if (!server.started()) {
    finish();
    std::fprintf(stderr, "a meccs nem indult el (csatlakozás)\n");
    return 1;
  }
  phase.store(1);
  const int64_t t0 = now_ms();
  int64_t max_lag = 0;
  while (now_ms() - t0 < seconds * 1000LL) {
    proxy.pump();
    for (auto& sp : sides) max_lag = std::max(max_lag, server.turns() - sp->turns.load());
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  const int64_t elapsed = now_ms() - t0;
  phase.store(2);
  server.hold(true);   // a relé nem zár több fordulót (de az átvitel fut tovább); a kliensek utolérik
  const int64_t final_turns = server.turns();
  const int64_t t_drain = now_ms();
  auto caught_up = [&] {
    for (auto& sp : sides) {
      if (sp->turns.load() < final_turns) return false;
    }
    return true;
  };
  while (!caught_up() && now_ms() - t_drain < 20000) {
    proxy.pump();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  bool same = caught_up();
  finish();
  std::vector<int64_t> lat;
  for (auto& sp : sides) {
    same = same && sp->c->sim() && sp->c->sim()->hash() == sides.front()->c->sim()->hash();
    lat.insert(lat.end(), sp->lat.begin(), sp->lat.end());
  }
  std::sort(lat.begin(), lat.end());
  auto pct = [&](double q) { return lat.empty() ? 0 : lat[std::min(lat.size() - 1, static_cast<size_t>(q * static_cast<double>(lat.size())))]; };
  int64_t sum = 0;
  for (const int64_t v : lat) sum += v;
  const auto ds = server.desyncs();
  std::printf("netbench: %d kliens, veszteség %d%%, RTT %d ms, %d s\n", clients, loss, rtt, seconds);
  std::printf("  fordulók: %lld / ideális %lld (%.1f%%), legnagyobb kliens-lemaradás %lld forduló\n",
              static_cast<long long>(final_turns), static_cast<long long>(elapsed / net::kTurnMs),
              100.0 * static_cast<double>(final_turns) / static_cast<double>(std::max<int64_t>(1, elapsed / net::kTurnMs)),
              static_cast<long long>(max_lag));
  std::printf("  parancskésés: átlag %lld ms, p50 %lld, p95 %lld, max %lld (%zu minta)\n",
              static_cast<long long>(lat.empty() ? 0 : sum / static_cast<int64_t>(lat.size())), static_cast<long long>(pct(0.5)),
              static_cast<long long>(pct(0.95)), static_cast<long long>(lat.empty() ? 0 : lat.back()), lat.size());
  std::printf("  proxy: %lld továbbítva, %lld eldobva; szinkronhiba: %zu, egyező hash: %s\n",
              static_cast<long long>(proxy.passed), static_cast<long long>(proxy.dropped), ds.size(), same ? "igen" : "NEM");
  server.stop();
  return ds.empty() && same ? 0 : 3;
}
