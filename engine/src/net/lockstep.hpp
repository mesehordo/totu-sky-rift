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
  Joined = 9,    // relé → kliens: a lobbiban kapott hely (0 = megfigyelő)
  Lobby = 10,    // relé → mindenki: a helyek (név, nép, kész)
  Pick = 11,     // kliens → relé: népválasztás a lobbiban
  Ready = 12,    // kliens → relé: kész / nem kész
  Chat = 13,     // kliens → relé: üzenet
  ChatOut = 14,  // relé → mindenki: üzenet (küldő helye, szöveg)
};

constexpr int kMaxLagTurns = 25;   // ennél több forduló lemaradásnál a relé bevárja a lassú klienst (2,5 s)

struct LobbySeat {
  bool taken = false;
  std::string name, faction;
  bool ready = false;
  friend bool operator==(const LobbySeat&, const LobbySeat&) = default;
};
// a lobbi kiegészítő adatai (a tanulmány: csapat, gépi hely, néző, verzió, modlista – indulás előtt látható)
struct LobbyAi {
  uint8_t player = 0;
  std::string faction, difficulty;
  friend bool operator==(const LobbyAi&, const LobbyAi&) = default;
};
struct LobbyInfo {
  std::vector<uint8_t> teams;        // játékosonként (0: mindenki ellen); az emberi helyek az elején
  std::vector<LobbyAi> ai;           // a gépi helyek
  uint8_t observers = 0;             // a csatlakozott nézők
  std::string version;               // a gazda (relé) játékverziója
  std::vector<std::string> mods;     // a betöltött modok (azonosító és verzió); üres: csak az alapjáték
  friend bool operator==(const LobbyInfo&, const LobbyInfo&) = default;
};
struct ChatLine {
  uint8_t from = 0;   // 0 = megfigyelő
  std::string text;
};

struct DesyncInfo {
  int64_t turn = 0;
  uint8_t player = 0;
  friend bool operator==(const DesyncInfo&, const DesyncInfo&) = default;
};

// --- relé -------------------------------------------------------------------------------------------------------
class Relay {
 public:
  // `match`: a meccs leírása (mag, pálya, alapértelmezett népek helyenként, gépek, szkriptek); `seats`: az emberi
  // helyek (1..seats, a lobbiban választanak népet); a `content_hash` a modlock (a tartalom ujjlenyomata)
  Relay(sim::Replay match, const map::TerrainTable* terrain, const sim::Rules* rules, uint64_t content_hash, int seats);
  ~Relay();
  bool ok() const { return error_.empty(); }
  const std::string& error() const { return error_; }

  void on_message(PeerId peer, const Bytes& data);
  void on_disconnect(PeerId peer);
  bool can_start() const;               // minden hely betelt, és mindenki kész
  bool start(int64_t now_ms);           // a meccs indul (a gazda dönt); hibánál false és error()
  void advance(int64_t now_ms);         // az esedékes fordulók lezárása és továbbítása
  std::vector<std::pair<PeerId, Bytes>> take_outbox();

  bool started() const { return started_; }
  int64_t turns() const { return closed_; }   // lezárt fordulók száma
  sim::Sim& sim() { return *match_->sim; }
  const std::vector<LobbySeat>& lobby() const { return lobby_; }
  // a gazda verziója és modlistája a lobbihoz (a modlock a tartalomhash; ez csak a kiírás)
  void set_info(std::string version, std::vector<std::string> mods);
  bool stalled() const { return stalled_; }
  // a meccs eddigi visszajátszása (a leírás és minden lezárt forduló parancsa) – szinkronhiba esetén a hibakereséshez
  sim::Replay replay() const;
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
  void send_lobby();
  void send_welcome(PeerId p, uint8_t slot, bool state_follows);

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
  std::vector<LobbySeat> lobby_;
  std::string version_;
  std::vector<std::string> mods_;
  std::map<uint8_t, int64_t> last_hash_;   // hely → az utolsó beküldött hash fordulója
  std::vector<sim::Command> history_;      // minden lezárt forduló parancsa (visszajátszáshoz)
  bool stalled_ = false;
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
  // lobbi
  void pick(const std::string& faction);
  void set_ready(bool ready);
  void chat(const std::string& text);
  const std::vector<LobbySeat>& lobby() const { return lobby_; }
  const LobbyInfo& lobby_info() const { return info_; }
  const std::vector<ChatLine>& chats() const { return chats_; }
  bool joined() const { return joined_; }
  // a megérkezett fordulók lefuttatása (legfeljebb `max_turns`), a parancsok és a hash-ek elküldése
  int update(int max_turns = 1 << 30);
  // tickenkénti léptetés (a játékkliens szimulációs szálához): van-e megerősített következő tick; a tick parancsai
  // (a forduló első tickjén); a lépés után a hash (a forduló végén megy el)
  bool tick_ready() const { return match_ && (in_turn_ > 0 || !turns_.empty()); }
  std::vector<sim::Command> begin_tick();
  void end_tick(uint64_t hash);
  void flush_local();
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
  const std::vector<int64_t>& own_turns() const { return own_turns_; }   // a saját parancsot hozó fordulók (méréshez)
  // a kliens visszajátszása (az elejétől futott meccsnél; mentésből folytatottnál üres) – szinkronhiba esetén
  sim::Replay replay() const;

 private:
  std::vector<int64_t> own_turns_;
  std::vector<sim::Command> history_;
  bool from_state_ = false;
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
  int in_turn_ = 0;                                    // a folyamatban lévő forduló lefutott tickjei
  std::deque<std::pair<int64_t, std::vector<sim::Command>>> turns_;   // megérkezett, még le nem futott
  std::vector<sim::Command> local_;
  std::vector<Bytes> out_;
  std::vector<DesyncInfo> desyncs_;
  std::string reject_, error_;
  LobbyInfo info_;
  std::vector<LobbySeat> lobby_;
  std::vector<ChatLine> chats_;
  bool joined_ = false;
  bool wait_state_ = false;
};

}  // namespace totu::net
