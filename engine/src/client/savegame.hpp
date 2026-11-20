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

// a tartalom ujjlenyomata a mentésben (a tanulmány): a meta „content” (a modlock hash-e, hexában) és „mods” (azonosító és
// verzió) kulcsa. Betöltéskor eltérő modkészletnél figyelmeztetés, de a betöltés nem tiltott (mint a 0 A.D.-ban).
std::string content_tag(uint64_t hash);
// a mentett és a mostani modlista különbsége („+új 1.0, −régi”); üres: azonos
std::string mod_diff(const std::vector<std::string>& saved, const std::vector<std::string>& now);
// a meta szerint eltér-e a tartalom (régebbi mentésnél, ujjlenyomat nélkül: nem); `diff`: a modlista különbsége
bool content_differs(const std::string& meta, uint64_t now_hash, const std::vector<std::string>& now_mods, std::string* diff);
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
