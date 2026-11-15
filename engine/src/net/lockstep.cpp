// SPDX-License-Identifier: GPL-3.0-or-later
#include "net/lockstep.hpp"

#include <algorithm>

#include "core/bytes.hpp"
#include "map/map.hpp"
#include "map/pathfinder.hpp"

namespace totu::net {

namespace {

constexpr int kCmdFormat = 4;
constexpr size_t kMaxCmdsPerMsg = 4096;
constexpr int64_t kHashKeep = 256;

Bytes bytes_of(const core::ByteWriter& w) { return w.data(); }
std::string as_str(const Bytes& b) { return std::string(b.begin(), b.end()); }
Bytes as_bytes(const std::string& s) { return Bytes(s.begin(), s.end()); }

void write_cmds(core::ByteWriter& w, const std::vector<sim::Command>& cs) {
  w.u32(static_cast<uint32_t>(cs.size()));
  for (const auto& c : cs) sim::save_command(w, c, kCmdFormat);
}

bool read_cmds(core::ByteReader& r, std::vector<sim::Command>& out) {
  const uint32_t n = r.u32();
  if (n > kMaxCmdsPerMsg) return false;
  for (uint32_t i = 0; i < n && r.ok(); ++i) {
    auto c = sim::load_command(r, kCmdFormat);
    if (!c) return false;
    out.push_back(std::move(*c));
  }
  return r.ok();
}

// a játékos parancsa: csak a saját nevében, és csak játékos-parancs (a rendszerparancsok – lerakás, akadály, kapu –
// nem jöhetnek a hálózatról)
bool player_command(const sim::Command& c) {
  using K = sim::Command::Kind;
  return c.kind != K::Spawn && c.kind != K::Block && c.kind != K::Gate;
}

}  // namespace

// --- relé -------------------------------------------------------------------------------------------------------
Relay::Relay(sim::Replay match, const map::TerrainTable* terrain, const sim::Rules* rules, uint64_t content_hash,
             int seats)
    : desc_(std::move(match)), terrain_(terrain), rules_(rules), content_hash_(content_hash),
      seats_(std::clamp(seats, 0, 8)) {
  desc_.commands.clear();
  desc_.checkpoints.clear();
  desc_.ticks = 0;
  desc_.checkpoint_every = 1;   // a dekódoló ellenőrzi (H-88): a hossz 0
  match_ = sim::start_match(desc_, terrain_, rules_, error_);
}

Relay::~Relay() = default;

int Relay::connected_players() const {
  int n = 0;
  for (const auto& [id, p] : peers_) n += p.joined && p.slot > 0 ? 1 : 0;
  return n;
}

void Relay::send(PeerId p, Bytes b) { out_.emplace_back(p, std::move(b)); }

void Relay::broadcast(const Bytes& b) {
  for (const auto& [id, p] : peers_) {
    if (p.joined) out_.emplace_back(id, b);
  }
}

void Relay::reject(PeerId p, const std::string& why) {
  core::ByteWriter w;
  w.u8(static_cast<uint8_t>(Msg::Reject));
  w.str(why);
  send(p, bytes_of(w));
}

std::vector<std::pair<PeerId, Bytes>> Relay::take_outbox() { return std::exchange(out_, {}); }

void Relay::on_disconnect(PeerId peer) { peers_.erase(peer); }

void Relay::on_message(PeerId peer, const Bytes& data) {
  if (!match_ || data.empty() || data.size() > kMaxMessage) return;
  core::ByteReader r(data);
  const auto kind = static_cast<Msg>(r.u8());
  if (kind == Msg::Hello) {
    const uint32_t proto = r.u32();
    const uint64_t ch = r.u64();
    const std::string name = r.str();
    const bool observer = r.u8() != 0;
    const uint8_t rejoin = r.u8();
    if (!r.ok()) return;
    if (peers_.count(peer) && peers_[peer].joined) return;
    if (proto != kProtocol) return reject(peer, "eltérő hálózati protokoll");
    if (ch != content_hash_) return reject(peer, "eltérő tartalom (modlock)");
    uint8_t slot = 0;
    if (!observer) {
      auto taken = [&](uint8_t s) {
        for (const auto& [id, p] : peers_) {
          if (p.joined && p.slot == s) return true;
        }
        return false;
      };
      if (rejoin > 0) {
        if (rejoin > seats_ || taken(rejoin)) return reject(peer, "a hely foglalt vagy nem létezik");
        slot = rejoin;
      } else {
        if (started_) return reject(peer, "a meccs már elindult (csak visszatérés vagy megfigyelés)");
        for (uint8_t s = 1; s <= seats_ && slot == 0; ++s) {
          if (!taken(s)) slot = s;
        }
        if (slot == 0) return reject(peer, "nincs szabad hely");
      }
    }
    peers_[peer] = Peer{slot, true};
    core::ByteWriter w;
    w.u8(static_cast<uint8_t>(Msg::Welcome));
    w.u8(slot);
    w.str(as_str(sim::encode(desc_)));
    send(peer, bytes_of(w));
    if (started_) {   // visszatérés vagy megfigyelő: a mostani állapot, utána a következő fordulók
      core::ByteWriter s;
      s.u8(static_cast<uint8_t>(Msg::State));
      s.i64(closed_);
      s.str(as_str(match_->sim->save()));
      send(peer, bytes_of(s));
    }
    return;
  }
  const auto it = peers_.find(peer);
  if (it == peers_.end() || !it->second.joined) return;
  const Peer& p = it->second;
  if (kind == Msg::Cmds) {
    const int64_t turn = r.i64();
    std::vector<sim::Command> cs;
    if (!read_cmds(r, cs) || p.slot == 0 || !started_) return;
    const int64_t at = std::max(turn, closed_);   // a késve érkező a következő nyitott fordulóba kerül
    auto& dst = pending_[at];
    for (auto& c : cs) {
      if (!player_command(c)) continue;
      c.owner = p.slot;   // csak a saját nevében (H-01)
      dst.push_back(std::move(c));
    }
  } else if (kind == Msg::Hash) {
    const int64_t turn = r.i64();
    const uint64_t h = r.u64();
    if (!r.ok() || p.slot == 0) return;
    const auto hit = hashes_.find(turn);
    if (hit != hashes_.end() && hit->second != h) {
      const DesyncInfo d{turn, p.slot};
      if (std::find(desyncs_.begin(), desyncs_.end(), d) == desyncs_.end()) {
        desyncs_.push_back(d);
        core::ByteWriter w;
        w.u8(static_cast<uint8_t>(Msg::Desync));
        w.i64(turn);
        w.u8(p.slot);
        broadcast(bytes_of(w));
      }
    }
  }
}

void Relay::start(int64_t now_ms) {
  if (started_ || !match_) return;
  started_ = true;
  next_close_ms_ = now_ms;
  for (const auto& [id, p] : peers_) {   // az indulás előtt bejött megfigyelő a kezdőállapotot kapja
    if (!p.joined || p.slot != 0) continue;
    core::ByteWriter s;
    s.u8(static_cast<uint8_t>(Msg::State));
    s.i64(closed_);
    s.str(as_str(match_->sim->save()));
    send(id, bytes_of(s));
  }
}

void Relay::close_turn() {
  const int64_t t = closed_;
  std::vector<sim::Command> cs;
  if (auto it = pending_.find(t); it != pending_.end()) {
    cs = std::move(it->second);
    pending_.erase(it);
  }
  for (auto& c : cs) c.tick = t * kTicksPerTurn;
  core::ByteWriter w;
  w.u8(static_cast<uint8_t>(Msg::Turn));
  w.i64(t);
  write_cmds(w, cs);
  broadcast(bytes_of(w));
  // a relé saját szimulációja ugyanazt futtatja
  sim::Sim& s = *match_->sim;
  for (const auto& c : cs) s.queue(c);
  for (int i = 0; i < kTicksPerTurn; ++i) s.step();
  hashes_[t] = s.hash();
  while (!hashes_.empty() && hashes_.begin()->first < t - kHashKeep) hashes_.erase(hashes_.begin());
  ++closed_;
}

void Relay::advance(int64_t now_ms) {
  if (!started_) return;
  int guard = 0;
  while (now_ms >= next_close_ms_ && guard++ < 50) {   // lemaradásnál egyszerre legfeljebb 5 s
    close_turn();
    next_close_ms_ += kTurnMs;
  }
}

// --- kliens -----------------------------------------------------------------------------------------------------
Client::Client(const map::TerrainTable* terrain, const sim::Rules* rules, uint64_t content_hash, std::string name,
               bool observer, uint8_t rejoin_slot)
    : terrain_(terrain), rules_(rules), content_hash_(content_hash), name_(std::move(name)), observer_(observer),
      rejoin_(rejoin_slot) {}

Client::~Client() = default;

Bytes Client::hello() const {
  core::ByteWriter w;
  w.u8(static_cast<uint8_t>(Msg::Hello));
  w.u32(kProtocol);
  w.u64(content_hash_);
  w.str(name_);
  w.u8(observer_ ? 1 : 0);
  w.u8(rejoin_);
  return bytes_of(w);
}

void Client::on_message(const Bytes& data) {
  if (data.empty() || data.size() > kMaxMessage) return;
  core::ByteReader r(data);
  const auto kind = static_cast<Msg>(r.u8());
  switch (kind) {
    case Msg::Welcome: {
      const uint8_t slot = r.u8();
      const auto desc = sim::decode(as_bytes(r.str()));
      if (!r.ok() || !desc) {
        error_ = "hibás meccsleírás";
        return;
      }
      slot_ = slot;
      desc_ = *desc;
      if (!observer_ && rejoin_ == 0) {   // új meccs: azonnal felépül (a State-et nem várja)
        match_ = sim::start_match(*desc_, terrain_, rules_, error_);
      }
      break;
    }
    case Msg::State: {
      const int64_t next = r.i64();
      const std::string save = r.str();
      if (!r.ok() || !desc_) return;
      auto m = sim::start_match(*desc_, terrain_, rules_, error_, true);
      if (!m || !m->sim->load(as_bytes(save))) {
        if (error_.empty()) error_ = "a kapott állapot nem tölthető be";
        return;
      }
      if (m->map) m->sim->restore_obstacles(*m->map);
      match_ = std::move(m);
      done_ = next;
      turns_.clear();
      break;
    }
    case Msg::Reject:
      reject_ = r.str();
      break;
    case Msg::Turn: {
      const int64_t t = r.i64();
      std::vector<sim::Command> cs;
      if (!read_cmds(r, cs)) return;
      if (t < done_ + static_cast<int64_t>(turns_.size())) return;   // már megvan (a mentés utánit várjuk)
      turns_.emplace_back(t, std::move(cs));
      break;
    }
    case Msg::Desync: {
      const int64_t t = r.i64();
      const uint8_t pl = r.u8();
      if (r.ok()) desyncs_.push_back({t, pl});
      break;
    }
    default:
      break;
  }
}

void Client::submit(sim::Command c) {
  if (slot_ == 0) return;   // a megfigyelő nem parancsol
  c.owner = slot_;
  local_.push_back(std::move(c));
}

int Client::update(int max_turns) {
  if (!match_) return 0;
  sim::Sim& s = *match_->sim;
  int ran = 0;
  while (ran < max_turns && !turns_.empty()) {
    auto [t, cs] = std::move(turns_.front());
    turns_.pop_front();
    if (t != done_) {   // kimaradt forduló: ez nem fordulhat elő megbízható átvitelen
      error_ = "kimaradt forduló";
      return ran;
    }
    for (const auto& c : cs) s.queue(c);
    for (int i = 0; i < kTicksPerTurn; ++i) s.step();
    if (slot_ > 0) {
      core::ByteWriter w;
      w.u8(static_cast<uint8_t>(Msg::Hash));
      w.i64(t);
      w.u64(s.hash());
      out_.push_back(bytes_of(w));
    }
    ++done_;
    ++ran;
  }
  if (!local_.empty()) {
    core::ByteWriter w;
    w.u8(static_cast<uint8_t>(Msg::Cmds));
    w.i64(done_ + kDelayTurns);
    for (auto& c : local_) c.tick = (done_ + kDelayTurns) * kTicksPerTurn;
    write_cmds(w, local_);
    out_.push_back(bytes_of(w));
    local_.clear();
  }
  return ran;
}

std::vector<Bytes> Client::take_outbox() { return std::exchange(out_, {}); }

}  // namespace totu::net
