// SPDX-License-Identifier: GPL-3.0-or-later
// Munkamenet és szimulációs szál (M6, 1. lépés): a külön szálon futó szimuláció ugyanazt az állapotot adja, mint a
// szinkron; a pillanatkép rendezett, és a látványesemények (találat, halál) megjelennek.
#include <chrono>
#include <string>
#include <thread>

#include "check.hpp"
#include "client/session.hpp"

using namespace totu;

namespace {

const std::string kMap = std::string(TOTU_TEST_DATA) + "/maps/DEMO-VILAG.json";

bool open(client::Session& s) {
  std::string err;
  if (!s.load(TOTU_CONTENT_DIR, kMap, 20261112, &err)) return false;
  s.spawn_demo(2, 30);
  s.order_demo_attack();
  return true;
}

sim::Command move_cmd(const client::Session& s) {
  sim::Command c;
  c.kind = sim::Command::Kind::GroupMove;
  c.units = {1, 2, 3, 4, 5};
  c.x = core::Fixed::from_int(s.map().width() / 2);
  c.y = core::Fixed::from_int(s.map().height() / 2 + 6);
  c.formation = static_cast<uint8_t>(sim::Formation::Line);
  return c;
}

}  // namespace

TEST_CASE("szimulációs szál: ugyanaz az állapot, mint szinkron futtatva (parancssal együtt)") {
  client::Session a, b;
  CHECK(open(a) && open(b));
  client::SimRunner ra(a), rb(b);
  // szálon: a parancs a következő tickben érvényesül
  ra.post(move_cmd(a));
  ra.start();
  CHECK(ra.threaded());
  for (int i = 0; i < 400; ++i) {
    std::shared_ptr<const client::Snapshot> p, c;
    ra.latest(&p, &c);
    if (c->tick >= 25) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ra.stop();
  const int64_t ticks = a.sim().tick();
  CHECK(ticks >= 25);
  // szinkron: ugyanaz a parancs, ugyanannyi tick
  rb.post(move_cmd(b));
  for (int64_t i = 0; i < ticks; ++i) rb.step_sync();
  CHECK(b.sim().tick() == ticks);
  CHECK(a.sim().hash() == b.sim().hash());
}

TEST_CASE("pillanatkép: rendezett, a két legutóbbi a simításhoz, találat- és halálesemények") {
  client::Session s;
  CHECK(open(s));
  client::SimRunner r(s);
  std::shared_ptr<const client::Snapshot> p, c;
  r.latest(&p, &c);
  // a saját 30 egység mind benne van; a ködben álló ellenség nem (D41)
  size_t own = 0;
  for (const auto& u : c->units) own += u.owner == 1 ? 1u : 0u;
  CHECK(c->tick == 0 && p == c && own == 30 && c->units.size() < 60);
  CHECK(c->fog && !c->fog->empty());
  for (size_t i = 1; i < c->units.size(); ++i) CHECK(c->units[i - 1].id < c->units[i].id);
  CHECK(c->find(c->units[7].id) == &c->units[7]);
  CHECK(c->find(999999) == nullptr);
  r.step_sync();
  r.latest(&p, &c);
  CHECK(p->tick == 0 && c->tick == 1);
  // a seregek összecsapnak: lesz találat és halál is
  std::vector<client::FxEvent> fx;
  bool hit = false, death = false;
  for (int i = 0; i < 900 && !(hit && death); ++i) {
    r.step_sync();
    r.take_fx(fx);
    for (const auto& e : fx) (e.death ? death : hit) = true;
    fx.clear();
  }
  CHECK(hit && death);
  std::vector<double> ms;
  r.take_tick_ms(ms);
  CHECK(!ms.empty());
}
