// SPDX-License-Identifier: GPL-3.0-or-later
// Hálózat (M10, ADR-009): determinisztikus lockstep relével. A relé sorba rendezi és fordulónként továbbítja a
// játékosok parancsait, maga is futtatja a szimulációt (fej nélkül): összeveti a kliensek állapothash-ét, és
// újracsatlakozáskor mentést küld. Az átvitel külön réteg (net/transport.hpp): ez a modul csak bájtüzeneteket fogad és
// ad ki, így memóriabeli átvitellel is próbálható.
#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "sim/replay.hpp"

namespace totu::map {
class TerrainTable;
}

namespace totu::net {

constexpr uint32_t kProtocol = 1;
constexpr int kTicksPerTurn = 3;     // 1 forduló = 100 ms 30 Hz mellett
constexpr int kDelayTurns = 2;       // a parancs az N+2. fordulóban hajtódik végre
constexpr int64_t kTurnMs = 100;
constexpr size_t kMaxMessage = 64u << 20;   // mentéssel együtt

using PeerId = uint32_t;
using Bytes = std::vector<uint8_t>;

enum class Msg : uint8_t {
  Hello = 1,     // kliens → relé: protokoll, tartalomhash, név, megfigyelő, visszatérő hely
  Welcome = 2,   // relé → kliens: a kapott hely (0 = megfigyelő) és a meccs leírása (kódolt Replay)
  Reject = 3,    // relé → kliens: elutasítás oka
  Cmds = 4,      // kliens → relé: parancsok a kért fordulóra
  Turn = 5,      // relé → kliens: egy lezárt forduló minden parancsa
  Hash = 6,      // kliens → relé: az állapothash egy forduló végén
  Desync = 7,    // relé → kliens: szinkronhiba (forduló, játékos)
  State = 8,     // relé → kliens: mentés (újracsatlakozás, megfigyelő); utána a következő fordulóktól
};

struct DesyncInfo {
  int64_t turn = 0;
  uint8_t player = 0;
  friend bool operator==(const DesyncInfo&, const DesyncInfo&) = default;
};

// --- relé -------------------------------------------------------------------------------------------------------
class Relay {
 public:
  // `match`: a meccs leírása (mag, pálya, népek, gépek, szkriptek); `seats`: az emberi helyek (1..seats); a
  // `content_hash` a modlock (a tartalom és a szabályok ujjlenyomata)
  Relay(sim::Replay match, const map::TerrainTable* terrain, const sim::Rules* rules, uint64_t content_hash, int seats);
  ~Relay();
  bool ok() const { return match_ != nullptr; }
  const std::string& error() const { return error_; }

  void on_message(PeerId peer, const Bytes& data);
  void on_disconnect(PeerId peer);
  void start(int64_t now_ms);           // a meccs indul (a gazda dönt: mindenki bent van)
  void advance(int64_t now_ms);         // az esedékes fordulók lezárása és továbbítása
  std::vector<std::pair<PeerId, Bytes>> take_outbox();

  bool started() const { return started_; }
  int64_t turns() const { return closed_; }   // lezárt fordulók száma
  sim::Sim& sim() { return *match_->sim; }
  const std::vector<DesyncInfo>& desyncs() const { return desyncs_; }
  int connected_players() const;

 private:
  struct Peer {
    uint8_t slot = 0;        // 0 = megfigyelő
    bool joined = false;
  };
  void send(PeerId p, Bytes b);
  void broadcast(const Bytes& b);
  void close_turn();
  void reject(PeerId p, const std::string& why);

  sim::Replay desc_;
  const map::TerrainTable* terrain_;
  const sim::Rules* rules_;
  uint64_t content_hash_;
  int seats_;
  std::unique_ptr<sim::MatchInstance> match_;
  std::string error_;
  std::map<PeerId, Peer> peers_;
  std::vector<std::pair<PeerId, Bytes>> out_;
  bool started_ = false;
  int64_t next_close_ms_ = 0;
  int64_t closed_ = 0;
  std::map<int64_t, std::vector<sim::Command>> pending_;   // forduló → parancsok
  std::map<int64_t, uint64_t> hashes_;                     // a relé saját hash-e fordulónként (az utolsó 256)
  std::vector<DesyncInfo> desyncs_;
};

// --- kliens -----------------------------------------------------------------------------------------------------
class Client {
 public:
  Client(const map::TerrainTable* terrain, const sim::Rules* rules, uint64_t content_hash, std::string name,
         bool observer = false, uint8_t rejoin_slot = 0);
  ~Client();

  Bytes hello() const;
  void on_message(const Bytes& data);
  // a helyi játékos parancsa (a tulajdonos és a tick a küldéskor kerül bele)
  void submit(sim::Command c);
  // a megérkezett fordulók lefuttatása (legfeljebb `max_turns`), a parancsok és a hash-ek elküldése
  int update(int max_turns = 1 << 30);
  std::vector<Bytes> take_outbox();

  bool ready() const { return match_ != nullptr; }
  bool rejected() const { return !reject_.empty(); }
  const std::string& reject_reason() const { return reject_; }
  const std::string& error() const { return error_; }
  uint8_t slot() const { return slot_; }
  int64_t turns() const { return done_; }
  int64_t buffered() const { return static_cast<int64_t>(turns_.size()); }
  sim::Sim* sim() { return match_ ? match_->sim.get() : nullptr; }
  sim::MatchInstance* match() { return match_.get(); }
  const std::vector<DesyncInfo>& desyncs() const { return desyncs_; }

 private:
  const map::TerrainTable* terrain_;
  const sim::Rules* rules_;
  uint64_t content_hash_;
  std::string name_;
  bool observer_;
  uint8_t rejoin_;
  uint8_t slot_ = 0;
  std::optional<sim::Replay> desc_;
  std::unique_ptr<sim::MatchInstance> match_;
  int64_t done_ = 0;                                   // lefuttatott fordulók
  std::deque<std::pair<int64_t, std::vector<sim::Command>>> turns_;   // megérkezett, még le nem futott
  std::vector<sim::Command> local_;
  std::vector<Bytes> out_;
  std::vector<DesyncInfo> desyncs_;
  std::string reject_, error_;
};

}  // namespace totu::net
