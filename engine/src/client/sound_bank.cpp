// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/sound_bank.hpp"

#include <algorithm>
#include <cctype>

namespace totu::client {

namespace {
std::string u8(const std::filesystem::path& p) {   // UTF-8 (a Windows-os ékezetes fájlnév is)
  const auto s = p.u8string();
  return std::string(s.begin(), s.end());
}
std::filesystem::path from_u8(const std::string& s) { return std::filesystem::path(std::u8string(s.begin(), s.end())); }
}  // namespace

const std::vector<std::string>& SoundBank::find(const std::string& rel) {
  if (const auto it = cache_.find(rel); it != cache_.end()) return it->second;
  std::vector<std::string> out;
  const std::filesystem::path full = dir_ / from_u8(rel);
  const std::filesystem::path d = full.parent_path();
  const std::string base = u8(full.filename());
  std::error_code ec;
  std::vector<std::filesystem::path> files;
  for (const auto& e : std::filesystem::directory_iterator(d, ec)) {
    if (e.is_regular_file(ec)) files.push_back(e.path());
  }
  std::sort(files.begin(), files.end());
  for (const auto& f : files) {
    std::string ext = u8(f.extension());
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext != ".ogg" && ext != ".wav" && ext != ".mp3") continue;
    const std::string stem = u8(f.stem());
    bool ok = stem == base;
    if (!ok && stem.size() > base.size() + 1 && stem.compare(0, base.size(), base) == 0 && stem[base.size()] == '_') {
      ok = std::all_of(stem.begin() + static_cast<std::ptrdiff_t>(base.size()) + 1, stem.end(), [](unsigned char c) { return std::isdigit(c); });
    }
    if (ok) out.push_back(u8(f));
  }
  return cache_.emplace(rel, std::move(out)).first->second;
}

std::string SoundBank::pick(const std::string& ev, const std::string& unit_id, const std::string& faction, const std::string& utype,
                            uint32_t& rng) {
  std::vector<std::string> cands;
  if (!unit_id.empty()) cands.push_back("units/" + unit_id + "/" + ev);
  if (!faction.empty()) cands.push_back("factions/" + faction + "/" + ev);
  if (!utype.empty()) cands.push_back("common/" + utype + "/" + ev);
  cands.push_back("common/" + ev);
  for (const auto& c : cands) {
    const auto& files = find(c);
    if (files.empty()) continue;
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return files[rng % files.size()];
  }
  return {};
}

Sound sound_category(const std::string& ev, bool building) {
  if (ev == "environment") return Sound::Environment;
  if (ev == "under_attack" || ev.rfind("ui_", 0) == 0) return Sound::Interface;
  if (ev == "building_done" || ev == "gate_arrival" || ev == "building_pack" || ev == "building_unpack" ||
      (building && (ev == "select" || ev == "death"))) {
    return Sound::Buildings;
  }
  if (ev == "select" || ev == "move" || ev == "attack_order" || ev == "trained" || ev == "death") return Sound::Units;
  return Sound::Combat;
}

float sound_volume(const Settings& st, Sound cat) {
  const auto& m = st.volume[static_cast<size_t>(Sound::Master)];
  const auto& c = st.volume[static_cast<size_t>(cat)];
  if (!m.enabled || !c.enabled) return 0.0f;
  return static_cast<float>(std::clamp(m.value, 0, 100) * std::clamp(c.value, 0, 100)) / 10000.0f;
}

}  // namespace totu::client
