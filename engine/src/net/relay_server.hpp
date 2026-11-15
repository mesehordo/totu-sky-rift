// SPDX-License-Identifier: GPL-3.0-or-later
// Relé-kiszolgáló (M10): a lockstep-relé UDP-n, saját szálon. A dedikált `totu_relay` és a játék gazdája
// („Meccs indítása” a többjátékos módban) ugyanezt futtatja.
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "net/enet_transport.hpp"
#include "net/lockstep.hpp"

namespace totu::net {

class RelayServer {
 public:
  RelayServer() = default;
  ~RelayServer() { stop(); }
  RelayServer(const RelayServer&) = delete;
  RelayServer& operator=(const RelayServer&) = delete;

  // a meccs akkor indul, amikor minden hely betelt és mindenki kész (`auto_start`), vagy a `start_now` hívására
  bool start(sim::Replay match, const map::TerrainTable* terrain, const sim::Rules* rules, uint64_t content_hash,
             int seats, uint16_t port, std::string* error = nullptr);
  void run_blocking();   // a hívó szálán (dedikált relé); minden játékos kilépése után tér vissza
  void spawn();          // saját szálon (a játék gazdája)
  void stop();
  void hold(bool on) { hold_.store(on); }
  // szinkronhibánál ide kerül a relé mentése és visszajátszása (üres: nem ment)
  void set_dump_dir(std::string dir) { dump_dir_ = std::move(dir); }   // a fordulózárás szünetel (mérés, hibakeresés); az átvitel fut tovább
  bool started() const { return started_.load(); }
  bool finished() const { return finished_.load(); }
  std::vector<DesyncInfo> desyncs();
  int64_t turns() const { return turns_.load(); }

 private:
  void loop_once();
  std::unique_ptr<Relay> relay_;
  EnetEndpoint ep_;
  std::thread thread_;
  std::atomic<bool> stop_{false}, started_{false}, finished_{false}, hold_{false};
  std::atomic<int64_t> turns_{0};
  std::mutex mu_;
  std::vector<DesyncInfo> desyncs_;
  std::string dump_dir_;
  size_t dumped_ = 0;
};

// szinkronhiba-mentés: `<dir>/szinkronhiba_<forduló>_<ki>.tsav` (állapot) és `.trep` (visszajátszás); az írt fájlok
bool dump_desync(const std::string& dir, const std::string& who, int64_t turn, const std::vector<uint8_t>& state,
                 const sim::Replay& replay);

}  // namespace totu::net
