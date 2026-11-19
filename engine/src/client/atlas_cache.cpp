// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/atlas_cache.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include "render/atlas_build.hpp"
#include "render/image.hpp"

namespace totu::client {

namespace fs = std::filesystem;

namespace {

// az atlaszkészítő változata: ha a készítés módja változik (szűrő, elrendezés), növelni kell – a régi atlasz újraépül
constexpr const char* kBuilderVersion = "atlasz-2";   // 2: kulcsszínes csapatmaszk (D52)

struct Cancelled {};

std::string read_all(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

std::string atlas_fingerprint(const std::string& assets_dir) {
  uint64_t h = 1469598103934665603ULL;   // FNV-1a
  auto add = [&](const std::string& s) {
    for (const unsigned char c : s) {
      h ^= c;
      h *= 1099511628211ULL;
    }
    h ^= 0xff;
    h *= 1099511628211ULL;
  };
  add(kBuilderVersion);
  const fs::path root = fs::path(assets_dir) / "sprites";
  std::vector<std::string> rows;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
    if (!it->is_regular_file(ec)) continue;
    const auto rel = fs::relative(it->path(), root, ec).generic_u8string();
    const auto size = it->file_size(ec);
    const auto mt = it->last_write_time(ec).time_since_epoch().count();
    rows.push_back(std::string(rel.begin(), rel.end()) + "|" + std::to_string(size) + "|" + std::to_string(mt));
  }
  std::sort(rows.begin(), rows.end());
  for (const auto& r : rows) add(r);
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(h));
  return b;
}

AtlasCache::~AtlasCache() {
  cancel_.store(true);   // a kilépés nem vár a teljes építésre: a következő lap után megszakad
  if (thread_.joinable()) thread_.join();
}

std::string AtlasCache::error() const {
  std::lock_guard<std::mutex> lock(mu_);
  return error_;
}

void AtlasCache::wait() {
  if (thread_.joinable()) thread_.join();
}

bool AtlasCache::start(const std::string& assets_dir, const std::string& cache_root, bool build) {
  if (cache_root.empty() || building_.load()) return ready_.load();
  dir_ = (fs::path(cache_root) / "atlasz").string();
  tmp_ = (fs::path(cache_root) / "atlasz.uj").string();
  const std::string fp = atlas_fingerprint(assets_dir);
  std::error_code ec;
  if (fs::is_regular_file(fs::path(dir_) / "atlas.json", ec) && read_all(fs::path(dir_) / "ujjlenyomat.txt") == fp) {
    ready_.store(true);
    return true;
  }
  if (!build) return false;
  if (render::list_sprite_sheets(assets_dir).empty()) {
    std::lock_guard<std::mutex> lock(mu_);
    error_ = "nincs grafika: " + assets_dir;
    return false;
  }
  building_.store(true);
  thread_ = std::thread(&AtlasCache::run, this, assets_dir, fp);
  return false;
}

void AtlasCache::run(std::string assets, std::string fingerprint) {
  std::error_code ec;
  fs::remove_all(tmp_, ec);   // egy megszakadt korábbi építés maradéka
  fs::create_directories(tmp_, ec);
  bool ok = true;
  std::string err;
  try {
    render::AtlasOptions opt;
    std::vector<std::string> warnings;
    const auto atlas = render::build_atlas(
        render::list_sprite_sheets(assets), opt,
        [&](int page, const render::Image& base, const render::Image& mask) {
          if (cancel_.load()) throw Cancelled{};
          ok = ok && render::save_png((fs::path(tmp_) / ("page_" + std::to_string(page) + ".png")).string(), base);
          ok = ok && render::save_png((fs::path(tmp_) / ("mask_" + std::to_string(page) + ".png")).string(), mask);
          pages_.store(page + 1);
        },
        &warnings);
    if (cancel_.load()) throw Cancelled{};
    {
      std::ofstream js(fs::path(tmp_) / "atlas.json", std::ios::binary);
      js << atlas.to_json();
      ok = ok && static_cast<bool>(js);
      std::ofstream fpf(fs::path(tmp_) / "ujjlenyomat.txt", std::ios::binary);
      fpf << fingerprint;
      ok = ok && static_cast<bool>(fpf);
    }
    if (!ok) err = "az atlasz nem írható";
    if (ok) {   // csere: a régi törlése, az új átnevezése
      fs::remove_all(dir_, ec);
      fs::rename(tmp_, dir_, ec);
      if (ec) {
        ok = false;
        err = "az atlasz nem nevezhető át";
      }
    }
  } catch (const Cancelled&) {
    ok = false;
  } catch (const std::exception& e) {
    ok = false;
    err = e.what();
  }
  if (!ok) fs::remove_all(tmp_, ec);
  {
    std::lock_guard<std::mutex> lock(mu_);
    error_ = err;
  }
  ready_.store(ok);
  building_.store(false);
}

}  // namespace totu::client
