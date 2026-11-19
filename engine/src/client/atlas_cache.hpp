// SPDX-License-Identifier: GPL-3.0-or-later
// A világ atlaszának gyorsítótára: a játék a grafikából (assets/sprites) maga készíti el az atlaszt a felhasználói
// mappába (`atlasz/`), háttérszálon, amíg a menü használható; utána minden indítás a késznél tölti be. Ha a grafika
// vagy az atlaszkészítő változik (ujjlenyomat), újraépíti. Az atomikus csere: `atlasz.uj/` → `atlasz/`.
#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace totu::client {

// a grafika ujjlenyomata (a sprite-mappák fájljainak neve, mérete, módosítási ideje és a készítő változata)
std::string atlas_fingerprint(const std::string& assets_dir);

class AtlasCache {
 public:
  AtlasCache() = default;
  ~AtlasCache();
  AtlasCache(const AtlasCache&) = delete;
  AtlasCache& operator=(const AtlasCache&) = delete;

  // `cache_root`: a felhasználói mappa; ha a kész atlasz érvényes, rögtön kész, különben (ha `build` igaz) háttérben
  // épül. A visszatérés: kész-e már.
  bool start(const std::string& assets_dir, const std::string& cache_root, bool build);
  bool ready() const { return ready_.load(); }
  bool building() const { return building_.load(); }
  int pages_done() const { return pages_.load(); }
  std::string error() const;
  const std::string& dir() const { return dir_; }   // a kész atlasz mappája
  void wait();                                       // megvárja az építést (próbákhoz)

 private:
  void run(std::string assets, std::string fingerprint);
  std::string dir_, tmp_;
  std::thread thread_;
  std::atomic<bool> ready_{false}, building_{false}, cancel_{false};
  std::atomic<int> pages_{0};
  mutable std::mutex mu_;
  std::string error_;
};

}  // namespace totu::client
