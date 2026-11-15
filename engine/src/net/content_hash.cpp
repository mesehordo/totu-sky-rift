// SPDX-License-Identifier: GPL-3.0-or-later
#include "net/content_hash.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "core/hash.hpp"

namespace totu::net {

uint64_t content_hash(const std::vector<std::string>& dirs) {
  core::StateHash h;
  for (const auto& dir : dirs) {
    const std::filesystem::path root(dir);
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
      if (it->is_regular_file(ec)) files.push_back(it->path());
    }
    std::vector<std::pair<std::string, std::filesystem::path>> named;
    for (const auto& f : files) {
      const auto u8 = std::filesystem::relative(f, root, ec).generic_u8string();
      named.emplace_back(std::string(u8.begin(), u8.end()), f);
    }
    std::sort(named.begin(), named.end());
    for (const auto& [rel, path] : named) {
      std::ifstream in(path, std::ios::binary);
      std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
      data.erase(std::remove(data.begin(), data.end(), '\r'), data.end());
      h.add_bytes(rel);
      h.add_u64(data.size());
      h.add_bytes(data);
    }
  }
  return h.value();
}

}  // namespace totu::net
