// SPDX-License-Identifier: GPL-3.0-or-later
// UDP-átvitel az ENet felett (ADR-009): megbízható, sorrendtartó csatorna. A relé `listen`-nel vár, a kliens
// `connect`-tel kapcsolódik (a relé nála az 1-es partner). A hálózati logika (net/lockstep) bájtüzeneteket kap.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "net/lockstep.hpp"

typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;

namespace totu::net {

struct NetEvent {
  enum class Kind : uint8_t { Connect, Message, Disconnect } kind = Kind::Message;
  PeerId peer = 0;
  Bytes data;
};

class EnetEndpoint {
 public:
  EnetEndpoint();
  ~EnetEndpoint();
  EnetEndpoint(const EnetEndpoint&) = delete;
  EnetEndpoint& operator=(const EnetEndpoint&) = delete;

  bool listen(uint16_t port, size_t max_peers, std::string* error = nullptr);
  bool connect(const std::string& host, uint16_t port, std::string* error = nullptr);
  void send(PeerId peer, const Bytes& data);
  void disconnect(PeerId peer);
  // a beérkezett események (legfeljebb `timeout_ms` várakozással); a küldés is itt megy ki
  std::vector<NetEvent> poll(int timeout_ms = 0);
  void flush();
  bool connected(PeerId peer) const { return peers_.count(peer) > 0; }
  uint32_t rtt_ms(PeerId peer) const;

 private:
  ENetHost* host_ = nullptr;
  std::map<PeerId, ENetPeer*> peers_;
  PeerId next_ = 1;
};

}  // namespace totu::net
