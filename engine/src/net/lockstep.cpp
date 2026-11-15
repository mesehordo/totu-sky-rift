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
  if (static_cast<int>(desc_.factions.size()) < seats_) error_ = "kevesebb nép, mint emberi hely";
  lobby_.resize(static_cast<size_t>(seats_));
  for (int i = 0; i < seats_ && error_.empty(); ++i) lobby_[static_cast<size_t>(i)].faction = desc_.factions[static_cast<size_t>(i)];
}

bool Relay::can_start() const {
  if (started_ || !error_.empty() || lobby_.empty()) return false;
  for (const auto& l : lobby_) {
    if (!l.taken || !l.ready) return false;
  }
  return true;
}

void Relay::send_lobby() {
  core::ByteWriter w;
  w.u8(static_cast<uint8_t>(Msg::Lobby));
  w.u8(static_cast<uint8_t>(lobby_.size()));
  for (const auto& l : lobby_) {
    w.u8(l.taken ? 1 : 0);
    w.str(l.name);
    w.str(l.faction);
    w.u8(l.ready ? 1 : 0);
  }
  broadcast(bytes_of(w));
}

void Relay::send_welcome(PeerId p, uint8_t slot, bool state_follows) {
  core::ByteWriter w;
  w.u8(static_cast<uint8_t>(Msg::Welcome));
  w.u8(slot);
  w.u8(state_follows ? 1 : 0);
  w.str(as_str(sim::encode(desc_)));
  send(p, bytes_of(w));
  if (!state_follows) return;
  core::ByteWriter s;
  s.u8(static_cast<uint8_t>(Msg::State));
  s.i64(closed_);
  s.str(as_str(match_->sim->save()));
  send(p, bytes_of(s));
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

void Relay::on_disconnect(PeerId peer) {
  const auto it = peers_.find(peer);
  if (it == peers_.end()) return;
  const uint8_t slot = it->second.slot;
  peers_.erase(it);
  if (slot > 0 && !started_) {   // a lobbiban a hely felszabadul
    lobby_[slot - 1u] = LobbySeat{false, {}, lobby_[slot - 1u].faction, false};
    send_lobby();
  }
  if (slot > 0) last_hash_.erase(slot);
}

void Relay::on_message(PeerId peer, const Bytes& data) {
  if (!error_.empty() || data.empty() || data.size() > kMaxMessage) return;
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
    if (started_) {   // visszatérés vagy megfigyelő: a meccs leírása és a mostani állapot
      send_welcome(peer, slot, true);
      if (slot > 0) last_hash_[slot] = closed_ - 1;
      return;
    }
    core::ByteWriter w;
    w.u8(static_cast<uint8_t>(Msg::Joined));
    w.u8(slot);
    send(peer, bytes_of(w));
    if (slot > 0) {
      lobby_[slot - 1u].taken = true;
      lobby_[slot - 1u].name = name.substr(0, 32);
      lobby_[slot - 1u].ready = false;
    }
    send_lobby();
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
  } else if (kind == Msg::Pick || kind == Msg::Ready) {
    if (started_ || p.slot == 0) return;
    LobbySeat& l = lobby_[p.slot - 1u];
    if (kind == Msg::Pick) {
      const std::string f = r.str();
      const sim::FactionDef* fd = rules_ ? rules_->faction(f) : nullptr;
      if (!r.ok() || !fd || fd->neutral || fd->ally) return;   // csak játszható nép
      l.faction = f;
      l.ready = false;
    } else {
      const uint8_t v = r.u8();
      if (!r.ok()) return;
      l.ready = v != 0;
    }
    send_lobby();
  } else if (kind == Msg::Chat) {
    const std::string text = r.str();
    if (!r.ok() || text.empty()) return;
    core::ByteWriter w;
    w.u8(static_cast<uint8_t>(Msg::ChatOut));
    w.u8(p.slot);
    w.str(text.substr(0, 400));
    broadcast(bytes_of(w));
  } else if (kind == Msg::Hash) {
    const int64_t turn = r.i64();
    const uint64_t h = r.u64();
    if (!r.ok() || p.slot == 0) return;
    if (turn > last_hash_[p.slot] && turn < closed_) last_hash_[p.slot] = turn;
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

bool Relay::start(int64_t now_ms) {
  if (started_ || !error_.empty()) return false;
  for (int i = 0; i < seats_; ++i) desc_.factions[static_cast<size_t>(i)] = lobby_[static_cast<size_t>(i)].faction;
  match_ = sim::start_match(desc_, terrain_, rules_, error_);
  if (!match_) return false;
  started_ = true;
  next_close_ms_ = now_ms;
  for (const auto& [id, p] : peers_) {   // a játékosok a leírásból maguk építik fel; a megfigyelő állapotot kap
    if (!p.joined) continue;
    send_welcome(id, p.slot, p.slot == 0);
    if (p.slot > 0) last_hash_[p.slot] = -1;
  }
  return true;
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
    // a lassú kliens bevárása: ha valamelyik túl sok fordulóval lemaradt, a relé nem zár újat (0 A.D., OpenRA)
    int64_t slowest = closed_;
    for (const auto& [slot, t] : last_hash_) slowest = std::min(slowest, t + 1);
    stalled_ = closed_ - slowest > kMaxLagTurns;
    if (stalled_) {
      next_close_ms_ = now_ms + kTurnMs;
      return;
    }
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
    case Msg::Joined:
      slot_ = r.u8();
      joined_ = r.ok();
      break;
    case Msg::Lobby: {
      const uint8_t n = r.u8();
      std::vector<LobbySeat> ls(n);
      for (auto& l : ls) {
        l.taken = r.u8() != 0;
        l.name = r.str();
        l.faction = r.str();
        l.ready = r.u8() != 0;
      }
      if (r.ok()) lobby_ = std::move(ls);
      break;
    }
    case Msg::ChatOut: {
      ChatLine c;
      c.from = r.u8();
      c.text = r.str();
      if (r.ok() && chats_.size() < 1000) chats_.push_back(std::move(c));
      break;
    }
    case Msg::Welcome: {
      const uint8_t slot = r.u8();
      const bool follows = r.u8() != 0;
      const auto desc = sim::decode(as_bytes(r.str()));
      if (!r.ok() || !desc) {
        error_ = "hibás meccsleírás";
        return;
      }
      slot_ = slot;
      joined_ = true;
      desc_ = *desc;
      wait_state_ = follows;
      if (!follows) match_ = sim::start_match(*desc_, terrain_, rules_, error_);   // új meccs: azonnal felépül
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
      in_turn_ = 0;
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

void Client::pick(const std::string& faction) {
  core::ByteWriter w;
  w.u8(static_cast<uint8_t>(Msg::Pick));
  w.str(faction);
  out_.push_back(bytes_of(w));
}

void Client::set_ready(bool ready) {
  core::ByteWriter w;
  w.u8(static_cast<uint8_t>(Msg::Ready));
  w.u8(ready ? 1 : 0);
  out_.push_back(bytes_of(w));
}

void Client::chat(const std::string& text) {
  core::ByteWriter w;
  w.u8(static_cast<uint8_t>(Msg::Chat));
  w.str(text);
  out_.push_back(bytes_of(w));
}

void Client::submit(sim::Command c) {
  if (slot_ == 0) return;   // a megfigyelő nem parancsol
  c.owner = slot_;
  local_.push_back(std::move(c));
}

std::vector<sim::Command> Client::begin_tick() {
  if (!tick_ready()) return {};
  if (in_turn_ > 0) return {};
  auto [t, cs] = std::move(turns_.front());
  turns_.pop_front();
  if (t != done_) {   // kimaradt forduló: ez nem fordulhat elő megbízható átvitelen
    error_ = "kimaradt forduló";
    return {};
  }
  return std::move(cs);
}

void Client::end_tick(uint64_t hash) {
  if (++in_turn_ < kTicksPerTurn) return;
  in_turn_ = 0;
  if (slot_ > 0) {
    core::ByteWriter w;
    w.u8(static_cast<uint8_t>(Msg::Hash));
    w.i64(done_);
    w.u64(hash);
    out_.push_back(bytes_of(w));
  }
  ++done_;
  flush_local();
}

void Client::flush_local() {
  if (local_.empty()) return;
  core::ByteWriter w;
  w.u8(static_cast<uint8_t>(Msg::Cmds));
  w.i64(done_ + kDelayTurns);
  for (auto& c : local_) c.tick = (done_ + kDelayTurns) * kTicksPerTurn;
  write_cmds(w, local_);
  out_.push_back(bytes_of(w));
  local_.clear();
}

int Client::update(int max_turns) {
  if (!match_) return 0;
  sim::Sim& s = *match_->sim;
  int ran = 0;
  while (ran < max_turns && tick_ready() && error_.empty()) {
    for (int i = 0; i < kTicksPerTurn && error_.empty(); ++i) {
      for (const auto& c : begin_tick()) s.queue(c);
      s.step();
      end_tick(s.hash());
    }
    ++ran;
  }
  flush_local();
  return ran;
}

std::vector<Bytes> Client::take_outbox() { return std::exchange(out_, {}); }

}  // namespace totu::net
