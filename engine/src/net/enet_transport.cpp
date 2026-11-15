// SPDX-License-Identifier: GPL-3.0-or-later
#include "net/enet_transport.hpp"

#include <enet/enet.h>

#include <mutex>

namespace totu::net {

namespace {

constexpr size_t kChannels = 1;

bool init_once() {
  static std::once_flag flag;
  static bool ok = false;
  std::call_once(flag, [] { ok = enet_initialize() == 0; });   // a folyamat végéig él (az ENet így javasolja)
  return ok;
}

PeerId id_of(ENetPeer* p) { return static_cast<PeerId>(reinterpret_cast<uintptr_t>(p->data)); }

}  // namespace

EnetEndpoint::EnetEndpoint() = default;

EnetEndpoint::~EnetEndpoint() {
  if (!host_) return;
  for (auto& [id, p] : peers_) enet_peer_disconnect_now(p, 0);
  enet_host_flush(host_);
  enet_host_destroy(host_);
}

bool EnetEndpoint::listen(uint16_t port, size_t max_peers, std::string* error) {
  if (!init_once() || host_) {
    if (error) *error = "az ENet nem indítható";
    return false;
  }
  ENetAddress addr{};
  addr.host = ENET_HOST_ANY;
  addr.port = port;
  host_ = enet_host_create(&addr, max_peers, kChannels, 0, 0);
  if (!host_) {
    if (error) *error = "a port nem nyitható meg: " + std::to_string(port);
    return false;
  }
  host_->maximumPacketSize = kMaxMessage;   // a mentés (visszatérés) nagyobb lehet az alapértéknél
  return true;
}

bool EnetEndpoint::connect(const std::string& host, uint16_t port, std::string* error) {
  if (!init_once() || host_) {
    if (error) *error = "az ENet nem indítható";
    return false;
  }
  host_ = enet_host_create(nullptr, 1, kChannels, 0, 0);
  if (!host_) {
    if (error) *error = "a kliens nem indítható";
    return false;
  }
  host_->maximumPacketSize = kMaxMessage;
  ENetAddress addr{};
  if (enet_address_set_host(&addr, host.c_str()) != 0) {
    if (error) *error = "ismeretlen cím: " + host;
    return false;
  }
  addr.port = port;
  ENetPeer* p = enet_host_connect(host_, &addr, kChannels, 0);
  if (!p) {
    if (error) *error = "a kapcsolódás nem indítható";
    return false;
  }
  return true;
}

void EnetEndpoint::send(PeerId peer, const Bytes& data) {
  const auto it = peers_.find(peer);
  if (it == peers_.end()) return;
  ENetPacket* pk = enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE);
  if (enet_peer_send(it->second, 0, pk) != 0) enet_packet_destroy(pk);
}

void EnetEndpoint::disconnect(PeerId peer) {
  const auto it = peers_.find(peer);
  if (it == peers_.end()) return;
  enet_peer_disconnect(it->second, 0);
}

void EnetEndpoint::flush() {
  if (host_) enet_host_flush(host_);
}

uint32_t EnetEndpoint::rtt_ms(PeerId peer) const {
  const auto it = peers_.find(peer);
  return it == peers_.end() ? 0 : it->second->roundTripTime;
}

std::vector<NetEvent> EnetEndpoint::poll(int timeout_ms) {
  std::vector<NetEvent> out;
  if (!host_) return out;
  ENetEvent ev;
  int wait = timeout_ms;
  while (enet_host_service(host_, &ev, static_cast<enet_uint32>(wait)) > 0) {
    wait = 0;
    switch (ev.type) {
      case ENET_EVENT_TYPE_CONNECT: {
        const PeerId id = next_++;
        ev.peer->data = reinterpret_cast<void*>(static_cast<uintptr_t>(id));
        peers_[id] = ev.peer;
        out.push_back({NetEvent::Kind::Connect, id, {}});
        break;
      }
      case ENET_EVENT_TYPE_RECEIVE: {
        if (ev.packet->dataLength <= kMaxMessage) {
          out.push_back({NetEvent::Kind::Message, id_of(ev.peer),
                         Bytes(ev.packet->data, ev.packet->data + ev.packet->dataLength)});
        }
        enet_packet_destroy(ev.packet);
        break;
      }
      case ENET_EVENT_TYPE_DISCONNECT: {
        const PeerId id = id_of(ev.peer);
        peers_.erase(id);
        out.push_back({NetEvent::Kind::Disconnect, id, {}});
        break;
      }
      default:
        break;
    }
  }
  return out;
}

}  // namespace totu::net
