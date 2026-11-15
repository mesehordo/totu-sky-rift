// SPDX-License-Identifier: GPL-3.0-or-later
#include "net/relay_server.hpp"

#include <chrono>

namespace totu::net {

namespace {
int64_t now_ms() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
}  // namespace

bool RelayServer::start(sim::Replay match, const map::TerrainTable* terrain, const sim::Rules* rules,
                        uint64_t content_hash, int seats, uint16_t port, std::string* error) {
  relay_ = std::make_unique<Relay>(std::move(match), terrain, rules, content_hash, seats);
  if (!relay_->ok()) {
    if (error) *error = relay_->error();
    return false;
  }
  return ep_.listen(port, 16, error);
}

void RelayServer::loop_once() {
  for (auto& e : ep_.poll(5)) {
    if (e.kind == NetEvent::Kind::Message) relay_->on_message(e.peer, e.data);
    else if (e.kind == NetEvent::Kind::Disconnect) relay_->on_disconnect(e.peer);
  }
  if (relay_->can_start() && relay_->start(now_ms())) started_.store(true);
  relay_->advance(now_ms());
  for (auto& [p, b] : relay_->take_outbox()) ep_.send(p, b);
  ep_.flush();
  turns_.store(relay_->turns());
  {
    std::lock_guard<std::mutex> lock(mu_);
    desyncs_ = relay_->desyncs();
  }
  if (relay_->started() && relay_->connected_players() == 0) finished_.store(true);
}

void RelayServer::run_blocking() {
  if (!relay_) return;
  while (!stop_.load() && !finished_.load()) loop_once();
}

void RelayServer::spawn() {
  if (!relay_ || thread_.joinable()) return;
  thread_ = std::thread([this] { run_blocking(); });
}

void RelayServer::stop() {
  stop_.store(true);
  if (thread_.joinable()) thread_.join();
}

std::vector<DesyncInfo> RelayServer::desyncs() {
  std::lock_guard<std::mutex> lock(mu_);
  return desyncs_;
}

}  // namespace totu::net
