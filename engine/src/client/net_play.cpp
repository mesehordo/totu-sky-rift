// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/net_play.hpp"

namespace totu::client {

bool NetPlay::connect(const std::string& host, uint16_t port, const map::TerrainTable* terrain, const sim::Rules* rules,
                      uint64_t content_hash, const std::string& name, bool observer, uint8_t rejoin,
                      std::string* error) {
  std::lock_guard<std::mutex> lock(mu_);
  client_ = std::make_unique<net::Client>(terrain, rules, content_hash, name, observer, rejoin);
  return ep_.connect(host, port, error);
}

void NetPlay::pump_locked() {
  if (!client_) return;
  for (auto& e : ep_.poll(0)) {
    if (e.kind == net::NetEvent::Kind::Connect) {
      relay_ = e.peer;
      if (!said_) {
        ep_.send(relay_, client_->hello());
        said_ = true;
      }
    } else if (e.kind == net::NetEvent::Kind::Message) {
      client_->on_message(e.data);
    } else if (e.kind == net::NetEvent::Kind::Disconnect) {
      lost_ = true;
    }
  }
  if (relay_ != 0) {
    for (auto& b : client_->take_outbox()) ep_.send(relay_, b);
  }
  ep_.flush();
}

void NetPlay::pump() {
  std::lock_guard<std::mutex> lock(mu_);
  pump_locked();
}

void NetPlay::pick(const std::string& faction) {
  std::lock_guard<std::mutex> lock(mu_);
  if (client_) client_->pick(faction);
}

void NetPlay::set_ready(bool ready) {
  std::lock_guard<std::mutex> lock(mu_);
  if (client_) client_->set_ready(ready);
}

void NetPlay::chat(const std::string& text) {
  std::lock_guard<std::mutex> lock(mu_);
  if (client_) client_->chat(text);
}

bool NetPlay::begin(std::vector<sim::Command>& cmds) {
  std::lock_guard<std::mutex> lock(mu_);
  pump_locked();
  if (!client_ || !client_->tick_ready()) return false;
  cmds = client_->begin_tick();
  return true;
}

void NetPlay::end(uint64_t hash) {
  std::lock_guard<std::mutex> lock(mu_);
  client_->end_tick(hash);
  pump_locked();
}

void NetPlay::submit(sim::Command c) {
  std::lock_guard<std::mutex> lock(mu_);
  if (client_) client_->submit(std::move(c));
}

int NetPlay::backlog() const {
  std::lock_guard<std::mutex> lock(mu_);
  return client_ ? static_cast<int>(client_->buffered()) : 0;
}

}  // namespace totu::client
