// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/savegame.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>

#include "content/json.hpp"
#include "core/bytes.hpp"

namespace totu::client {

namespace {
constexpr const char* kMagic = "totu-mentes";
constexpr uint32_t kVersion = 1;
}  // namespace

std::string content_tag(uint64_t hash) {
  char b[24] = {};
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(hash));
  return b;
}

std::string mod_diff(const std::vector<std::string>& saved, const std::vector<std::string>& now) {
  std::string out;
  auto add = [&](const std::string& s) { out += (out.empty() ? "" : ", ") + s; };
  for (const auto& m : now) {
    if (std::find(saved.begin(), saved.end(), m) == saved.end()) add("+" + m);
  }
  for (const auto& m : saved) {
    if (std::find(now.begin(), now.end(), m) == now.end()) add("\u2212" + m);
  }
  return out;
}

bool content_differs(const std::string& meta, uint64_t now_hash, const std::vector<std::string>& now_mods, std::string* diff) {
  const auto v = content::parse(meta);
  const content::Value* c = v ? v->find("content") : nullptr;
  if (!c || !c->is_string()) return false;   // régebbi mentés: nincs mihez mérni
  std::vector<std::string> saved;
  if (const auto* ms = v->find("mods"); ms && ms->is_array()) {
    for (const auto& m : ms->items()) {
      if (m.is_string()) saved.push_back(m.text());
    }
  }
  if (diff) *diff = mod_diff(saved, now_mods);
  return c->text() != content_tag(now_hash);
}

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

namespace {
constexpr const char* kReplayMagic = "totu-visszajatszas";
}

bool write_replay(const std::filesystem::path& p, const ReplayFile& f, std::string* error) {
  core::ByteWriter w;
  w.str(kReplayMagic);
  w.u32(kVersion);
  w.str(f.meta);
  const auto d = sim::encode(f.rep);
  w.str(std::string_view(reinterpret_cast<const char*>(d.data()), d.size()));
  std::error_code ec;
  std::filesystem::create_directories(p.parent_path(), ec);
  const auto tmp = p.string() + ".tmp";
  {
    std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
    o.write(reinterpret_cast<const char*>(w.data().data()), static_cast<std::streamsize>(w.data().size()));
    if (!o) {
      if (error) *error = "nem írható: " + tmp;
      return false;
    }
  }
  std::filesystem::rename(tmp, p, ec);
  if (ec && error) *error = "nem nevezhető át: " + p.string();
  return !ec;
}

std::optional<ReplayFile> read_replay(const std::filesystem::path& p, bool meta_only) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return std::nullopt;
  const std::vector<uint8_t> data{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  ReplayFile f;
  if (p.extension() == ".trep") {   // nyers visszajátszás (a tesztek és a totu_sim formátuma)
    if (meta_only) return f;
    auto r = sim::decode(data);
    if (!r) return std::nullopt;
    f.rep = std::move(*r);
    return f;
  }
  core::ByteReader r(data);
  if (r.str() != kReplayMagic || r.u32() != kVersion || !r.ok()) return std::nullopt;
  f.meta = r.str();
  if (!r.ok()) return std::nullopt;
  if (meta_only) return f;
  const std::string d = r.str();
  if (!r.ok()) return std::nullopt;
  auto rp = sim::decode(std::vector<uint8_t>(d.begin(), d.end()));
  if (!rp) return std::nullopt;
  f.rep = std::move(*rp);
  return f;
}

std::vector<SaveEntry> list_replays(const std::filesystem::path& dir) {
  std::vector<SaveEntry> out;
  std::error_code ec;
  for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
    if (!e.is_regular_file(ec) || e.path().extension() != kReplayExt) continue;
    auto f = read_replay(e.path(), true);
    if (!f) continue;
    out.push_back({e.path(), f->meta, static_cast<int64_t>(e.last_write_time(ec).time_since_epoch().count())});
  }
  std::sort(out.begin(), out.end(), [](const SaveEntry& a, const SaveEntry& b) { return a.mtime > b.mtime; });
  return out;
}

void prune_replays(const std::filesystem::path& dir, size_t keep) {
  const auto all = list_replays(dir);
  std::error_code ec;
  for (size_t i = keep; i < all.size(); ++i) std::filesystem::remove(all[i].path, ec);
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
