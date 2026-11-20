// SPDX-License-Identifier: GPL-3.0-or-later
// A helyi hálózaton futó játékok keresése (a régi `net.discover`): a gazda egy külön UDP-porton (`kLanPort`)
// válaszol a kérdésre a játék adataival; a kereső szórással (broadcast) kérdez, és a válaszokat gyűjti. Az ENet
// socket-API-ját használja (új külső könyvtár nélkül). A válasz rövid, kulcs=érték sorokból áll.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace totu::net {

constexpr uint16_t kLanPort = 47799;   // a keresés portja (a játék portja a válaszban)

struct LanGame {
  std::string address;   // a válaszoló gép címe (pl. 192.168.1.10)
  uint16_t port = 0;     // a relé portja
  std::string name;      // a gazda neve
  std::string map;       // a pálya neve
  int seats = 0, taken = 0;
  uint64_t content = 0;  // a tartalom ujjlenyomata (modlock)
};

// a gazda oldala: nem blokkoló; `poll` megválaszolja a beérkezett kérdéseket (a relé ciklusából hívva)
class LanBeacon {
 public:
  LanBeacon();
  ~LanBeacon();
  LanBeacon(const LanBeacon&) = delete;
  LanBeacon& operator=(const LanBeacon&) = delete;
  bool open(uint16_t port = kLanPort, std::string* error = nullptr);
  void set_info(const LanGame& info) { info_ = info; }
  void poll();
  bool is_open() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  LanGame info_;
};

// a kereső: szórás a helyi hálózatra, a válaszok `timeout_ms` ideig (címenként és portonként egyszer)
std::vector<LanGame> lan_scan(int timeout_ms = 800, uint16_t port = kLanPort, const std::string& target = "255.255.255.255");

// a válasz szövege és visszafejtése (a próbáknak is)
std::string lan_encode(const LanGame& g);
bool lan_decode(const std::string& text, LanGame* out);

}  // namespace totu::net
