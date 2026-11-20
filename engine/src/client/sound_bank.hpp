// SPDX-License-Identifier: GPL-3.0-or-later
// A hangfájlok keresése (a régi rts/audio.py `_find`, `unit_sound`, `play` kategóriái): assets/sounds alatt egy
// eseményhez az első létező a sorrendben – units/<egység>/<esemény>, factions/<nép>/<esemény>,
// common/<egységosztály>/<esemény>, common/<esemény>; változatok: <esemény>_1, _2, … (véletlenül váltakoznak).
// Csak fájlrendszer; a lejátszás a client/audio dolga.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "client/settings.hpp"

namespace totu::client {

class SoundBank {
 public:
  explicit SoundBank(std::filesystem::path dir) : dir_(std::move(dir)) {}
  // a `rel` (kiterjesztés nélkül, pl. "common/death") fájljai: <név>.ext és <név>_N.ext (.ogg, .wav, .mp3)
  const std::vector<std::string>& find(const std::string& rel);
  // a legspecifikusabb létező hang egy eseményhez (üres: nincs); `rng` a változat választásához
  std::string pick(const std::string& ev, const std::string& unit_id, const std::string& faction, const std::string& utype,
                   uint32_t& rng);

 private:
  std::filesystem::path dir_;
  std::map<std::string, std::vector<std::string>> cache_;
};

// az esemény hangkategóriája (a régi `play`): környezet, kezelőfelület (riasztás), épület, egység, különben harc
Sound sound_category(const std::string& ev, bool building);
// a kategória tényleges hangereje (0..1): főhangerő × kategória, a kikapcsolt 0
float sound_volume(const Settings& st, Sound cat);

}  // namespace totu::client
