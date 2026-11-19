// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/savegame.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

#include "core/bytes.hpp"

namespace totu::client {

namespace {
constexpr const char* kMagic = "totu-mentes";
constexpr uint32_t kVersion = 1;
}  // namespace

std::vector<uint8_t> encode_save(const SaveFile& f) {
  core::ByteWriter w;
  w.str(kMagic);
  w.u32(kVersion);
  w.str(f.meta);
  const auto d = sim::encode(f.desc);
  w.str(std::string_view(reinterpret_cast<const char*>(d.data()), d.size()));
  w.str(std::string_view(reinterpret_cast<const char*>(f.state.data()), f.state.size()));
  return w.data();
}

std::optional<SaveFile> decode_save(const std::vector<uint8_t>& data) {
  core::ByteReader r(data);
  if (r.str() != kMagic || r.u32() != kVersion || !r.ok()) return std::nullopt;
  SaveFile f;
  f.meta = r.str();
  const std::string d = r.str();
  const std::string st = r.str();
  if (!r.ok()) return std::nullopt;
  auto desc = sim::decode(std::vector<uint8_t>(d.begin(), d.end()));
  if (!desc) return std::nullopt;
  f.desc = std::move(*desc);
  f.state.assign(st.begin(), st.end());
  return f;
}

std::optional<SaveFile> read_save(const std::filesystem::path& p, bool meta_only) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return std::nullopt;
  const std::vector<uint8_t> data{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  if (!meta_only) return decode_save(data);
  core::ByteReader r(data);
  if (r.str() != kMagic || r.u32() != kVersion) return std::nullopt;
  SaveFile f;
  f.meta = r.str();
  if (!r.ok()) return std::nullopt;
  return f;
}

bool write_save(const std::filesystem::path& p, const SaveFile& f, std::string* error) {
  std::error_code ec;
  std::filesystem::create_directories(p.parent_path(), ec);
  const auto bytes = encode_save(f);
  const auto tmp = p.string() + ".tmp";   // előbb ideiglenesbe, aztán csere (a régi `_replace_file`)
  {
    std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
    o.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!o) {
      if (error) *error = "nem írható: " + tmp;
      return false;
    }
  }
  std::filesystem::rename(tmp, p, ec);
  if (ec) {
    if (error) *error = "nem nevezhető át: " + p.string();
    return false;
  }
  return true;
}

std::vector<SaveEntry> list_saves(const std::filesystem::path& dir) {
  std::vector<SaveEntry> out;
  std::error_code ec;
  for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
    if (!e.is_regular_file(ec) || e.path().extension() != ".totumentes") continue;
    auto f = read_save(e.path(), true);
    if (!f) continue;
    out.push_back({e.path(), f->meta, static_cast<int64_t>(e.last_write_time(ec).time_since_epoch().count())});
  }
  std::sort(out.begin(), out.end(), [](const SaveEntry& a, const SaveEntry& b) { return a.mtime > b.mtime; });
  return out;
}

}  // namespace totu::client
