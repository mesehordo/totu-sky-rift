// SPDX-License-Identifier: GPL-3.0-or-later
// Mentett játék (a régi `savegame.py` és `SaveMenu`/`LoadMenu` szerint): a meccs leírása (sim::Replay – pálya, beállítás,
// a gépek és a szkriptek forrása) és a szimuláció teljes állapota (Sim::save), elöl egy rövid JSON a betöltő listájának.
// Betöltéskor a leírásból a meccs a kezdés nélkül épül fel (start_match, load_state), majd a szimuláció betölti az
// állapotot – ugyanazok a csatlakozók ugyanabban a sorrendben, ahogy mentéskor.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "sim/replay.hpp"

namespace totu::client {

struct SaveFile {
  std::string meta;             // JSON: {"title", "map", "factions", "tick", "time"}
  sim::Replay desc;
  std::vector<uint8_t> state;   // Sim::save
};

std::vector<uint8_t> encode_save(const SaveFile& f);
std::optional<SaveFile> decode_save(const std::vector<uint8_t>& data);
std::optional<SaveFile> read_save(const std::filesystem::path& p, bool meta_only = false);
bool write_save(const std::filesystem::path& p, const SaveFile& f, std::string* error);

struct SaveEntry {
  std::filesystem::path path;
  std::string meta;
  int64_t mtime = 0;   // a rendezéshez (legújabb elöl)
};
// a mentések mappája (a felhasználói adatmappa `mentesek` alatt; client/menu_view.cpp – SDL) és a mentések, legújabb elöl
std::filesystem::path saves_dir();
std::vector<SaveEntry> list_saves(const std::filesystem::path& dir);

}  // namespace totu::client
