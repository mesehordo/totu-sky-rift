// SPDX-License-Identifier: GPL-3.0-or-later
// M10 hálózat: lockstep relével, memóriabeli átvitellel (késleltetés, kilépés, visszatérés, szinkronhiba, modlock,
// megfigyelő).
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "match_util.hpp"
#include "net/lockstep.hpp"

using namespace totu;
using namespace totu::testing;
using core::Fixed;

namespace {

constexpr uint64_t kContent = 0x5eed;

sim::Replay desc() {
  sim::Replay r;
  r.seed = 4242;
  r.map = read_file(std::filesystem::path(TOTU_TEST_DATA) / "maps" / "ECO-ALAP.json");
  r.factions = {"ursan", "wullof"};
  return r;
}

// memóriabeli hálózat: a relé és a kliensek; minden üzenet `latency` ms múlva érkezik (megbízható, sorrendtartó)
struct Net {
  net::Relay relay;
  std::map<net::PeerId, std::unique_ptr<net::Client>> clients;
  struct Packet { int64_t at; net::PeerId peer; bool to_relay; net::Bytes data; };
  std::vector<Packet> wire;
  int64_t now = 0, latency;
  explicit Net(int64_t lat, int seats = 2)
      : relay(desc(), &content_once().terrain, &*content_once().rules, kContent, seats), latency(lat) {}

  net::Client& join(net::PeerId id, uint64_t content = kContent, bool observer = false, uint8_t rejoin = 0) {
    clients[id] = std::make_unique<net::Client>(&content_once().terrain, &*content_once().rules, content,
                                                "j" + std::to_string(id), observer, rejoin);
    wire.push_back({now + latency, id, true, clients[id]->hello()});
    return *clients[id];
  }
  void leave(net::PeerId id) {
    relay.on_disconnect(id);
    clients.erase(id);
    std::erase_if(wire, [&](const Packet& p) { return p.peer == id; });
  }
  void tick(int64_t ms, bool relay_runs = true) {
    for (int64_t k = 0; k < ms; k += 10) {
      now += 10;
      if (relay_runs) relay.advance(now);
      for (auto& [p, b] : relay.take_outbox()) wire.push_back({now + latency, p, false, std::move(b)});
      for (auto& [id, c] : clients) {
        c->update();
        for (auto& b : c->take_outbox()) wire.push_back({now + latency, id, true, std::move(b)});
      }
      std::vector<Packet> due;
      std::erase_if(wire, [&](Packet& p) {
        if (p.at > now) return false;
        due.push_back(std::move(p));
        return true;
      });
      for (auto& p : due) {
        if (p.to_relay) relay.on_message(p.peer, p.data);
        else if (auto it = clients.find(p.peer); it != clients.end()) it->second->on_message(p.data);
      }
    }
  }
  // a relé leáll, a kliensek utolérik
  void drain() { tick(latency * 4 + 200, false); }
};

sim::EntityId unit_of(sim::Sim& s, uint8_t owner) {
  sim::EntityId out = sim::kNoEntity;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit&) {
    if (out == sim::kNoEntity && s.world().get<sim::Health>(e)->owner == owner) out = e;
  });
  return out;
}

}  // namespace

TEST_CASE("halozat: ket kliens es a rele 150 ms kesessel ugyanazt szamolja, a parancs mindenhol vegrehajtodik") {
  Net n(150);
  CHECK(n.relay.ok());
  auto& a = n.join(1);
  auto& b = n.join(2);
  n.tick(400);
  if (!a.ready()) std::printf("  relé: %s; kliens: %s / %s\n", n.relay.error().c_str(), a.error().c_str(), a.reject_reason().c_str());
  CHECK(a.ready() && b.ready() && a.slot() == 1 && b.slot() == 2);
  if (!a.ready() || !b.ready()) return;
  n.relay.start(n.now);
  n.tick(1000);
  const auto u = unit_of(*a.sim(), 1);
  const auto p0 = *a.sim()->world().get<sim::Position>(u);
  sim::Command c;
  c.kind = sim::Command::Kind::Move;
  c.units = {u};
  c.x = p0.x + Fixed::from_int(4);
  c.y = p0.y;
  a.submit(c);
  // a 2. játékos az 1. egységét akarná mozgatni: a relé a saját nevére írja, a szimuláció elutasítja (H-01)
  sim::Command bad = c;
  bad.x = p0.x - Fixed::from_int(4);
  b.submit(bad);
  n.tick(4000);
  n.drain();
  std::printf("  forduló: relé %lld, kliensek %lld / %lld\n", static_cast<long long>(n.relay.turns()),
              static_cast<long long>(a.turns()), static_cast<long long>(b.turns()));
  CHECK(a.turns() == n.relay.turns() && b.turns() == n.relay.turns());
  CHECK(a.sim()->hash() == n.relay.sim().hash() && b.sim()->hash() == n.relay.sim().hash());
  const auto p1 = *n.relay.sim().world().get<sim::Position>(u);
  CHECK(p1.x > p0.x + Fixed::from_int(2));   // az 1. játékos parancsa érvényesült, a 2.-é nem
  CHECK(n.relay.desyncs().empty() && a.desyncs().empty());
}

TEST_CASE("halozat: szinkronhiba-eszleles, modlock, betelt hely") {
  Net n(50);
  auto& a = n.join(1);
  auto& b = n.join(2);
  n.tick(200);
  n.relay.start(n.now);
  n.tick(500);
  b.sim()->kill(unit_of(*b.sim(), 2));   // a 2. kliens állapota eltér
  n.tick(800);
  CHECK(!n.relay.desyncs().empty() && n.relay.desyncs().front().player == 2);
  CHECK(!a.desyncs().empty());
  auto& wrong = n.join(3, 0xbad);
  n.tick(200);
  CHECK(wrong.rejected());
  auto& late = n.join(4);
  n.tick(200);
  CHECK(late.rejected());   // elindult meccsre új játékos nem jöhet be
}

TEST_CASE("halozat: kilepes es visszateres mentesbol, megfigyelo") {
  Net n(80);
  auto& a = n.join(1);
  n.join(2);
  n.tick(300);
  n.relay.start(n.now);
  n.tick(2000);
  n.leave(2);
  n.tick(1500);   // a meccs megy tovább nélküle
  auto& back = n.join(5, kContent, false, 2);
  auto& obs = n.join(6, kContent, true);
  n.tick(500);
  CHECK(back.ready() && back.slot() == 2 && obs.ready() && obs.slot() == 0);
  sim::Command c;
  c.kind = sim::Command::Kind::Stop;
  c.units = {unit_of(*back.sim(), 2)};
  back.submit(c);
  obs.submit(c);   // a megfigyelő nem parancsol
  n.tick(2000);
  n.drain();
  CHECK(back.turns() == n.relay.turns() && obs.turns() == n.relay.turns() && a.turns() == n.relay.turns());
  CHECK(back.sim()->hash() == n.relay.sim().hash() && obs.sim()->hash() == n.relay.sim().hash());
  CHECK(a.sim()->hash() == n.relay.sim().hash());
  CHECK(n.relay.desyncs().empty());
}
