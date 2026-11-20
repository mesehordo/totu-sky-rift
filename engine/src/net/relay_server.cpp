// SPDX-License-Identifier: GPL-3.0-or-later
#include "net/relay_server.hpp"

#include "content/json.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>

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
  if (!ep_.listen(port, 16, error)) return false;
  lan_ = LanGame{};   // a LAN-keresés válasza (ha a keresési port foglalt, csak a közvetlen csatlakozás megy)
  lan_.port = port;
  lan_.seats = seats;
  lan_.content = content_hash;
  if (const auto mv = content::parse(relay_->replay().map)) {
    if (const auto* nm = mv->find("name")) {   // szöveg vagy nyelvenként ({"hu": …, "en": …})
      if (nm->is_string()) lan_.map = nm->text();
      else if (const auto* hu = nm->is_object() ? nm->find("hu") : nullptr; hu && hu->is_string()) lan_.map = hu->text();
    }
  }
  beacon_.open();
  return true;
}

void RelayServer::loop_once() {
  for (auto& e : ep_.poll(5)) {
    if (e.kind == NetEvent::Kind::Message) relay_->on_message(e.peer, e.data);
    else if (e.kind == NetEvent::Kind::Disconnect) relay_->on_disconnect(e.peer);
  }
  if (beacon_.is_open() && !relay_->started()) {   // a helyi hálózati keresés: a foglalt helyek száma frissen
    lan_.name = lan_name_;
    lan_.taken = 0;
    for (const auto& s : relay_->lobby()) lan_.taken += s.taken ? 1 : 0;
    beacon_.set_info(lan_);
    beacon_.poll();
  }
  if (relay_->can_start() && relay_->start(now_ms())) started_.store(true);
  if (!hold_.load()) relay_->advance(now_ms());
  for (auto& [p, b] : relay_->take_outbox()) ep_.send(p, b);
  ep_.flush();
  turns_.store(relay_->turns());
  {
    std::lock_guard<std::mutex> lock(mu_);
    desyncs_ = relay_->desyncs();
  }
  if (!dump_dir_.empty() && relay_->desyncs().size() > dumped_) {   // egyszer, az első szinkronhibánál
    dumped_ = relay_->desyncs().size();
    if (dumped_ == 1) dump_desync(dump_dir_, "rele", relay_->desyncs().front().turn, relay_->sim().save(), relay_->replay());
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

bool dump_desync(const std::string& dir, const std::string& who, int64_t turn, const std::vector<uint8_t>& state,
                 const sim::Replay& replay) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  const std::string base = (std::filesystem::path(dir) / ("szinkronhiba_" + std::to_string(turn) + "_" + who)).string();
  std::ofstream s(base + ".tsav", std::ios::binary);
  s.write(reinterpret_cast<const char*>(state.data()), static_cast<std::streamsize>(state.size()));
  bool ok = static_cast<bool>(s);
  if (!replay.map.empty() || !replay.commands.empty()) {
    const auto rb = sim::encode(replay);
    std::ofstream r(base + ".trep", std::ios::binary);
    r.write(reinterpret_cast<const char*>(rb.data()), static_cast<std::streamsize>(rb.size()));
    ok = ok && static_cast<bool>(r);
  }
  return ok;
}

std::vector<DesyncInfo> RelayServer::desyncs() {
  std::lock_guard<std::mutex> lock(mu_);
  return desyncs_;
}

}  // namespace totu::net
