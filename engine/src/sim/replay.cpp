// SPDX-License-Identifier: GPL-3.0-or-later
#include "sim/replay.hpp"

namespace totu::sim {

namespace {

Checkpoint make_checkpoint(const Sim& s) {
  Checkpoint c;
  c.tick = s.tick();
  c.hash = s.hash();
  c.components = s.component_hashes();
  return c;
}

void run(Sim& s, const Replay& r, std::vector<Checkpoint>& out) {
  for (const auto& c : r.commands) s.queue(c);
  for (int64_t t = 0; t < r.ticks; ++t) {
    s.step();
    if (s.tick() % r.checkpoint_every == 0 || s.tick() == r.ticks) out.push_back(make_checkpoint(s));
  }
}

}  // namespace

Replay record(uint64_t seed, int64_t ticks, std::vector<Command> commands, int64_t checkpoint_every) {
  Replay r;
  r.seed = seed;
  r.ticks = ticks;
  r.checkpoint_every = checkpoint_every > 0 ? checkpoint_every : 30;
  r.commands = std::move(commands);
  Sim s(seed);
  run(s, r, r.checkpoints);
  return r;
}

VerifyResult verify(const Replay& r) {
  VerifyResult v;
  Sim s(r.seed);
  std::vector<Checkpoint> got;
  run(s, r, got);
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
  w.str("totu-replay");
  w.u32(1);
  w.u64(r.seed);
  w.i64(r.ticks);
  w.i64(r.checkpoint_every);
  w.u32(static_cast<uint32_t>(r.commands.size()));
  for (const auto& c : r.commands) save_command(w, c);
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
  if (rd.str() != "totu-replay" || rd.u32() != 1) return std::nullopt;
  Replay r;
  r.seed = rd.u64();
  r.ticks = rd.i64();
  r.checkpoint_every = rd.i64();
  if (r.ticks < 0 || r.checkpoint_every <= 0) return std::nullopt;
  const uint32_t n = rd.u32();
  for (uint32_t i = 0; i < n && rd.ok(); ++i) {
    auto c = load_command(rd);
    if (!c) return std::nullopt;
    r.commands.push_back(std::move(*c));
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
