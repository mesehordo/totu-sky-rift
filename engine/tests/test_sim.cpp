// SPDX-License-Identifier: GPL-3.0-or-later
// A szimulációs mag tesztjei (M2): determinizmus, mentés–betöltés–folytatás, visszajátszás, eltéréskereső.
#include "check.hpp"
#include "sim/replay.hpp"

using namespace totu;
using core::Fixed;

namespace {

std::vector<sim::Command> small_battle() {
  std::vector<sim::Command> cmds;
  for (int side = 0; side < 2; ++side) {
    for (int i = 0; i < 12; ++i) {
      sim::Command c;
      c.kind = sim::Command::Kind::Spawn;
      c.owner = static_cast<uint8_t>(side + 1);
      c.x = Fixed::from_int(side == 0 ? 2 + i % 3 : 30 + i % 3);
      c.y = Fixed::from_int(5 + i);
      c.speed = Fixed::from_ratio(6, 100);
      c.range = Fixed::from_ratio(3, 2);
      c.hp = 100;
      cmds.push_back(c);
    }
  }
  sim::Command mv;
  mv.kind = sim::Command::Kind::Move;
  mv.tick = 10;
  mv.x = Fixed::from_int(16);
  mv.y = Fixed::from_int(10);
  for (uint32_t id = 1; id <= 24; ++id) mv.units.push_back(id);
  cmds.push_back(mv);
  return cmds;
}

}  // namespace

TEST_CASE("szimuláció: két futás ugyanazzal a maggal és parancsokkal azonos") {
  sim::Sim a(7), b(7);
  for (const auto& c : small_battle()) { a.queue(c); b.queue(c); }
  bool same = true;
  for (int t = 0; t < 600; ++t) { a.step(); b.step(); same &= a.hash() == b.hash(); }
  CHECK(same);
  CHECK(a.world().entity_count() < 24);          // a harc valóban zajlott (volt halott)
  sim::Sim c(8);
  for (const auto& x : small_battle()) c.queue(x);
  for (int t = 0; t < 600; ++t) c.step();
  CHECK(c.hash() != a.hash());                   // más mag → más véletlen sebzés → más állapot
}

TEST_CASE("szimuláció: mentés, betöltés, folytatás = megszakítás nélküli futás") {
  sim::Sim full(11), first(11);
  for (const auto& c : small_battle()) { full.queue(c); first.queue(c); }
  for (int t = 0; t < 250; ++t) { full.step(); first.step(); }
  const auto data = first.save();
  sim::Sim resumed(999);                         // a mag a mentésből jön, nem a konstruktorból
  CHECK(resumed.load(data));
  CHECK(resumed.hash() == full.hash());
  for (int t = 0; t < 350; ++t) { full.step(); resumed.step(); }
  CHECK(resumed.hash() == full.hash());
  CHECK(resumed.save() == full.save());          // bájtra azonos mentés
  std::vector<uint8_t> broken = data;
  broken.resize(broken.size() / 2);
  sim::Sim bad(1);
  CHECK(!bad.load(broken));                      // csonka mentés elutasítva
}

TEST_CASE("visszajátszás: rögzítés, kódolás, ellenőrzés, eltéréskereső") {
  sim::Replay r = sim::record(5, 600, small_battle());
  CHECK(r.checkpoints.size() == 20);
  auto bytes = sim::encode(r);
  auto back = sim::decode(bytes);
  CHECK(back.has_value());
  CHECK(sim::verify(*back).ok);
  // szándékosan elrontott parancs: az eltéréskereső megmutatja a tickjét és a komponenst
  sim::Replay bad = *back;
  bad.commands.back().x = Fixed::from_int(17);
  const auto v = sim::verify(bad);
  CHECK(!v.ok);
  CHECK(v.first_bad_tick == 30);
  bool pos = false;
  for (const auto& n : v.differing_components) pos |= n == "Position";
  CHECK(pos);
  bytes[bytes.size() - 1] ^= 1;                  // sérült fájl: az utolsó hash bájtja
  auto damaged = sim::decode(bytes);
  CHECK(damaged.has_value() && !sim::verify(*damaged).ok);
}
