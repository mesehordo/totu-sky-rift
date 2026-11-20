// SPDX-License-Identifier: GPL-3.0-or-later
// Profilok (a régi rts/profiles.py `Profile` és a `ProfileMenu`): név, címer (a kedvenc nép), statisztika (csaták,
// győzelmek, vereségek, megölt ellenség, játékidő, népenként). Profilonként egy JSON a felhasználói mappa `profilok`
// alatt. A beállítások közösek (a régi profilonkénti beállítás helyett), a kiválasztott profil neve a beállításokban.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace totu::client {

struct Profile {
  std::string name;
  int64_t created = 0;          // Unix-idő (másodperc)
  std::string emblem = "ursan";
  int games = 0, wins = 0, losses = 0, kills = 0;
  int64_t play_seconds = 0;
  struct ByFaction {
    std::string faction;
    int games = 0, wins = 0;
  };
  std::vector<ByFaction> by_faction;
};

class ProfileStore {
 public:
  explicit ProfileStore(std::filesystem::path dir) : dir_(std::move(dir)) {}
  std::vector<Profile> list() const;   // név szerint
  std::optional<Profile> load(const std::string& name) const;
  bool save(const Profile& p) const;
  // új profil (üres vagy foglalt névnél false, a hiba kulcsa az `error`-ban)
  bool create(const std::string& name, const std::string& emblem, int64_t now, std::string* error);
  bool remove(const std::string& name) const;
  // a meccs vége (a régi `record_game`): csata +1, győzelem vagy vereség, a nép, a játékidő, a megölt ellenség
  void record_game(const std::string& name, const std::string& faction, bool won, int64_t seconds, int kills) const;
  const std::filesystem::path& dir() const { return dir_; }

 private:
  std::filesystem::path file_of(const std::string& name) const;
  std::filesystem::path dir_;
};

}  // namespace totu::client
