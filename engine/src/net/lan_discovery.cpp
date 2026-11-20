// SPDX-License-Identifier: GPL-3.0-or-later
#include "net/lan_discovery.hpp"

#include <enet/enet.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <sstream>

namespace totu::net {

namespace {

constexpr const char* kAsk = "TOTU-LAN?";
constexpr const char* kTell = "TOTU-LAN!";

bool init_enet() {
  static std::once_flag flag;
  static bool ok = false;
  std::call_once(flag, [] { ok = enet_initialize() == 0; });
  return ok;
}

}  // namespace

std::string lan_encode(const LanGame& g) {
  std::ostringstream o;
  o << kTell << "\nport=" << g.port << "\nname=" << g.name << "\nmap=" << g.map << "\nseats=" << g.seats << "\ntaken=" << g.taken
    << "\ncontent=" << g.content << "\n";
  return o.str();
}

bool lan_decode(const std::string& text, LanGame* out) {
  std::istringstream in(text);
  std::string line;
  if (!std::getline(in, line) || line != kTell) return false;
  LanGame g;
  while (std::getline(in, line)) {
    const auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
    if (k == "port") g.port = static_cast<uint16_t>(std::strtoul(v.c_str(), nullptr, 10));
    else if (k == "name") g.name = v.substr(0, 64);
    else if (k == "map") g.map = v.substr(0, 64);
    else if (k == "seats") g.seats = std::clamp(static_cast<int>(std::strtol(v.c_str(), nullptr, 10)), 0, 8);
    else if (k == "taken") g.taken = std::clamp(static_cast<int>(std::strtol(v.c_str(), nullptr, 10)), 0, 8);
    else if (k == "content") g.content = std::strtoull(v.c_str(), nullptr, 10);
  }
  if (g.port == 0) return false;
  *out = g;
  return true;
}

struct LanBeacon::Impl {
  ENetSocket sock = ENET_SOCKET_NULL;
};

LanBeacon::LanBeacon() : impl_(std::make_unique<Impl>()) {}
LanBeacon::~LanBeacon() {
  if (impl_->sock != ENET_SOCKET_NULL) enet_socket_destroy(impl_->sock);
}
bool LanBeacon::is_open() const { return impl_->sock != ENET_SOCKET_NULL; }

bool LanBeacon::open(uint16_t port, std::string* error) {
  if (!init_enet()) {
    if (error) *error = "az ENet nem indult";
    return false;
  }
  ENetSocket s = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
  if (s == ENET_SOCKET_NULL) {
    if (error) *error = "a keresési socket nem jött létre";
    return false;
  }
  enet_socket_set_option(s, ENET_SOCKOPT_NONBLOCK, 1);
  enet_socket_set_option(s, ENET_SOCKOPT_REUSEADDR, 1);
  ENetAddress a{};
  a.host = ENET_HOST_ANY;
  a.port = port;
  if (enet_socket_bind(s, &a) < 0) {
    enet_socket_destroy(s);
    if (error) *error = "a keresési port foglalt: " + std::to_string(port);
    return false;
  }
  impl_->sock = s;
  return true;
}

void LanBeacon::poll() {
  if (impl_->sock == ENET_SOCKET_NULL) return;
  for (int n = 0; n < 16; ++n) {   // tickenként legfeljebb 16 kérdés
    char buf[64];
    ENetBuffer b{};
    b.data = buf;
    b.dataLength = sizeof buf;
    ENetAddress from{};
    const int got = enet_socket_receive(impl_->sock, &from, &b, 1);
    if (got <= 0) return;
    if (std::string(buf, static_cast<size_t>(got)) != kAsk) continue;
    const std::string reply = lan_encode(info_);
    ENetBuffer r{};
    r.data = const_cast<char*>(reply.data());
    r.dataLength = reply.size();
    enet_socket_send(impl_->sock, &from, &r, 1);
  }
}

std::vector<LanGame> lan_scan(int timeout_ms, uint16_t port, const std::string& target) {
  std::vector<LanGame> out;
  if (!init_enet()) return out;
  ENetSocket s = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
  if (s == ENET_SOCKET_NULL) return out;
  enet_socket_set_option(s, ENET_SOCKOPT_BROADCAST, 1);
  enet_socket_set_option(s, ENET_SOCKOPT_NONBLOCK, 1);
  ENetAddress to{};
  if (enet_address_set_host_ip(&to, target.c_str()) != 0) to.host = ENET_HOST_BROADCAST;
  to.port = port;
  ENetBuffer q{};
  q.data = const_cast<char*>(kAsk);
  q.dataLength = std::char_traits<char>::length(kAsk);
  enet_socket_send(s, &to, &q, 1);
  const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < end) {
    enet_uint32 cond = ENET_SOCKET_WAIT_RECEIVE;
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now()).count();
    if (enet_socket_wait(s, &cond, static_cast<enet_uint32>(std::max<long long>(1, left))) < 0) break;
    if (!(cond & ENET_SOCKET_WAIT_RECEIVE)) continue;
    char buf[512];
    ENetBuffer b{};
    b.data = buf;
    b.dataLength = sizeof buf;
    ENetAddress from{};
    const int got = enet_socket_receive(s, &from, &b, 1);
    if (got <= 0) continue;
    LanGame g;
    if (!lan_decode(std::string(buf, static_cast<size_t>(got)), &g)) continue;
    char ip[64] = {};
    enet_address_get_host_ip(&from, ip, sizeof ip);
    g.address = ip;
    const bool dup = std::any_of(out.begin(), out.end(), [&](const LanGame& x) { return x.address == g.address && x.port == g.port; });
    if (!dup) out.push_back(std::move(g));
  }
  enet_socket_destroy(s);
  return out;
}

}  // namespace totu::net
