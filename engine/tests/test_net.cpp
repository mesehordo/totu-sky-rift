// SPDX-License-Identifier: GPL-3.0-or-later
// M10 hálózat: lockstep relével, memóriabeli átvitellel (késleltetés, kilépés, visszatérés, szinkronhiba, modlock,
// megfigyelő).
#include <chrono>
#include <cstdio>
#include <thread>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "match_util.hpp"
#include "net/content_hash.hpp"
#include "net/enet_transport.hpp"
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
  // mindenki kész, a meccs indul
  bool begin() {
    for (auto& [id, c] : clients) c->set_ready(true);
    tick(latency * 3 + 100);
    if (!relay.can_start() || !relay.start(now)) return false;
    tick(latency * 2 + 100);
    return true;
  }
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
  CHECK(a.joined() && b.joined() && !a.ready());
  CHECK(n.begin());
  if (!a.ready()) std::printf("  relé: %s; kliens: %s / %s\n", n.relay.error().c_str(), a.error().c_str(), a.reject_reason().c_str());
  CHECK(a.ready() && b.ready() && a.slot() == 1 && b.slot() == 2);
  if (!a.ready() || !b.ready()) return;
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
  CHECK(n.begin());
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
  CHECK(n.begin());
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

TEST_CASE("halozat: valodi UDP (ENet, helyi gep) - rele es ket kliens, modlock-ujjlenyomat") {
  const uint64_t ch = net::content_hash({TOTU_CONTENT_DIR});
  CHECK(ch != 0 && ch == net::content_hash({TOTU_CONTENT_DIR}));
  net::Relay relay(desc(), &content_once().terrain, &*content_once().rules, ch, 2);
  net::EnetEndpoint server;
  std::string err;
  const uint16_t port = 47913;
  CHECK(server.listen(port, 8, &err));
  struct Side {
    net::EnetEndpoint ep;
    std::unique_ptr<net::Client> c;
    bool said = false;
  };
  std::vector<std::unique_ptr<Side>> sides;
  for (int i = 0; i < 2; ++i) {
    auto s = std::make_unique<Side>();
    CHECK(s->ep.connect("127.0.0.1", port, &err));
    s->c = std::make_unique<net::Client>(&content_once().terrain, &*content_once().rules, ch, "udp");
    sides.push_back(std::move(s));
  }
  int64_t t = 0;
  auto pump = [&](bool relay_runs) {
    for (auto& e : server.poll(0)) {
      if (e.kind == net::NetEvent::Kind::Message) relay.on_message(e.peer, e.data);
      else if (e.kind == net::NetEvent::Kind::Disconnect) relay.on_disconnect(e.peer);
    }
    if (relay_runs) relay.advance(t);
    for (auto& [p, b] : relay.take_outbox()) server.send(p, b);
    server.flush();
    for (auto& s : sides) {
      for (auto& e : s->ep.poll(0)) {
        if (e.kind == net::NetEvent::Kind::Connect && !s->said) {
          s->ep.send(e.peer, s->c->hello());
          s->c->set_ready(true);
          s->said = true;
        } else if (e.kind == net::NetEvent::Kind::Message) {
          s->c->on_message(e.data);
        }
      }
      s->c->update();
      for (auto& b : s->c->take_outbox()) s->ep.send(1, b);
      s->ep.flush();
    }
  };
  for (int i = 0; i < 400 && !relay.can_start(); ++i) {
    pump(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  CHECK(relay.connected_players() == 2 && relay.can_start());
  CHECK(relay.start(0));
  for (t = 0; t <= 3000; t += 10) pump(true);   // 30 forduló, a valódi idő helyett lépésenként
  for (int i = 0; i < 400 && (sides[0]->c->turns() < relay.turns() || sides[1]->c->turns() < relay.turns()); ++i) {
    pump(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  std::printf("  UDP: relé %lld forduló, kliensek %lld / %lld\n", static_cast<long long>(relay.turns()),
              static_cast<long long>(sides[0]->c->turns()), static_cast<long long>(sides[1]->c->turns()));
  CHECK(sides[0]->c->ready() && sides[1]->c->ready());
  CHECK(sides[0]->c->turns() == relay.turns() && sides[1]->c->turns() == relay.turns());
  if (sides[0]->c->sim() && sides[1]->c->sim()) {
    CHECK(sides[0]->c->sim()->hash() == relay.sim().hash() && sides[1]->c->sim()->hash() == relay.sim().hash());
  }
}

TEST_CASE("halozat: lobbi - nepvalasztas, kesz-jelzes, cseveges, a kilepo helye felszabadul") {
  Net n(40);
  auto& a = n.join(1);
  auto& b = n.join(2);
  n.tick(200);
  CHECK(a.lobby().size() == 2 && a.lobby()[0].taken && a.lobby()[1].taken);
  CHECK(a.lobby()[0].faction == "ursan" && a.lobby()[1].faction == "wullof");
  b.pick("croftian");
  b.pick("vadon");          // a semleges nem választható
  b.pick("nincs_ilyen");
  a.chat("szia");
  n.tick(200);
  CHECK(a.lobby()[1].faction == "croftian" && b.lobby()[1].faction == "croftian");
  CHECK(!b.chats().empty() && b.chats().back().from == 1 && b.chats().back().text == "szia");
  a.set_ready(true);
  n.tick(200);
  CHECK(a.lobby()[0].ready && !a.lobby()[1].ready && !n.relay.can_start());
  n.leave(2);
  n.tick(200);
  CHECK(!a.lobby()[1].taken);
  auto& c = n.join(3);
  n.tick(200);
  CHECK(c.slot() == 2);
  c.set_ready(true);
  n.tick(200);
  CHECK(n.relay.can_start() && n.relay.start(n.now));
  n.tick(300);
  CHECK(a.ready() && c.ready());
  CHECK(n.relay.sim().player(2).faction == "croftian");   // a választott nép a meccsben
}

TEST_CASE("halozat: a rele bevarja a lemarado klienst") {
  Net n(30);
  auto& a = n.join(1);
  n.join(2);
  n.tick(150);
  CHECK(n.begin());
  // az 1. kliens „lefagy”: nem futtat fordulót, így nem küld hash-t
  for (int i = 0; i < 300; ++i) {
    n.now += 10;
    n.relay.advance(n.now);
    for (auto& [p, bb] : n.relay.take_outbox()) n.wire.push_back({n.now + n.latency, p, false, std::move(bb)});
    for (auto& [id, c] : n.clients) {
      if (id != 1) c->update();
      for (auto& bb : c->take_outbox()) n.wire.push_back({n.now + n.latency, id, true, std::move(bb)});
    }
    std::vector<Net::Packet> due;
    std::erase_if(n.wire, [&](Net::Packet& p) {
      if (p.at > n.now) return false;
      due.push_back(std::move(p));
      return true;
    });
    for (auto& p : due) {
      if (p.to_relay) n.relay.on_message(p.peer, p.data);
      else if (auto it = n.clients.find(p.peer); it != n.clients.end()) it->second->on_message(p.data);
    }
  }
  CHECK(n.relay.stalled());
  CHECK(n.relay.turns() <= a.turns() + net::kMaxLagTurns + 2);
  n.tick(1500);   // újra fut: utoléri, a relé folytatja
  CHECK(!n.relay.stalled());
  n.drain();
  CHECK(a.turns() == n.relay.turns() && a.sim()->hash() == n.relay.sim().hash());
}
