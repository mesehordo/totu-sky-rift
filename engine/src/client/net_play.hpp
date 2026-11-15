// SPDX-License-Identifier: GPL-3.0-or-later
// Hálózati játék a kliensben (M10): a lockstep-kliens UDP-n, a szimulációs szál tickforrásaként. A lobbiban a fő
// szál pumpálja; a meccs alatt a SimRunner szála (a `begin`/`end` hívásokkal). A parancsok szálbiztosan jutnak át.
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "client/session.hpp"
#include "net/enet_transport.hpp"
#include "net/lockstep.hpp"

namespace totu::client {

class NetPlay : public TickSource {
 public:
  bool connect(const std::string& host, uint16_t port, const map::TerrainTable* terrain, const sim::Rules* rules,
               uint64_t content_hash, const std::string& name, bool observer, uint8_t rejoin, std::string* error);
  void pump();   // a beérkezett üzenetek feldolgozása és a kimenő üzenetek elküldése (szálbiztos)
  // a lobbi műveletei (szálbiztos)
  void pick(const std::string& faction);
  void set_ready(bool ready);
  void chat(const std::string& text);
  bool lost() const { return lost_; }   // a relé kapcsolata megszakadt
  void set_dump_dir(std::string dir) { dump_dir_ = std::move(dir); }   // szinkronhibánál ide ment

  // a kliens állapota (a lobbiban a fő szálról, a pump mellett olvasható)
  net::Client& client() { return *client_; }

  // TickSource (a szimulációs szálon)
  bool begin(std::vector<sim::Command>& cmds) override;
  void end(uint64_t hash) override;
  void submit(sim::Command c) override;
  int backlog() const override;

 private:
  void pump_locked();
  mutable std::mutex mu_;
  net::EnetEndpoint ep_;
  std::unique_ptr<net::Client> client_;
  net::PeerId relay_ = 0;
  bool said_ = false, lost_ = false;
  std::string dump_dir_;
  bool dumped_ = false;
};

}  // namespace totu::client
