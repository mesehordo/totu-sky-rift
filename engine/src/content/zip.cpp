// SPDX-License-Identifier: GPL-3.0-or-later
#include "content/zip.hpp"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <vector>

// a deflate-kibontás az stb_image beépített (nyers) zlib-dekóderével (MIT / közkincs, változatlan)
extern "C" int stbi_zlib_decode_noheader_buffer(char* obuffer, int olen, const char* ibuffer, int ilen);

namespace totu::content {

namespace fs = std::filesystem;

namespace {

uint32_t u16(const std::vector<uint8_t>& b, size_t o) { return o + 2 <= b.size() ? static_cast<uint32_t>(b[o] | (b[o + 1] << 8)) : 0; }
uint32_t u32(const std::vector<uint8_t>& b, size_t o) {
  return o + 4 <= b.size() ? static_cast<uint32_t>(b[o]) | (static_cast<uint32_t>(b[o + 1]) << 8) | (static_cast<uint32_t>(b[o + 2]) << 16) |
                                 (static_cast<uint32_t>(b[o + 3]) << 24)
                           : 0;
}

bool safe_name(const std::string& n) {
  if (n.empty() || n[0] == '/' || n[0] == '\\' || n.find(':') != std::string::npos) return false;
  size_t p = 0;
  while (p <= n.size()) {
    const size_t q = n.find_first_of("/\\", p);
    const std::string part = n.substr(p, q == std::string::npos ? std::string::npos : q - p);
    if (part == "..") return false;
    if (q == std::string::npos) break;
    p = q + 1;
  }
  return true;
}

struct Entry {
  std::string name;
  uint32_t method = 0, csize = 0, usize = 0, local = 0, flags = 0;
};

}  // namespace

bool extract_zip(const fs::path& zip, const fs::path& dest, std::string* error) {
  auto fail = [&](const std::string& m) {
    if (error) *error = zip.filename().string() + ": " + m;
    return false;
  };
  std::ifstream in(zip, std::ios::binary);
  if (!in) return fail("nem olvasható");
  const std::vector<uint8_t> b{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  if (b.size() < 22 || b.size() > 0x7fffffffu) return fail("nem ZIP vagy túl nagy");
  // a központi könyvtár vége (EOCD) – a fájl végéről visszafelé, legfeljebb 64 KiB megjegyzéssel
  size_t eocd = std::string::npos;
  for (size_t i = b.size() - 22 + 1; i-- > 0 && b.size() - i <= 22 + 65535;) {
    if (u32(b, i) == 0x06054b50u) { eocd = i; break; }
  }
  if (eocd == std::string::npos) return fail("nem ZIP (nincs központi könyvtár)");
  const uint32_t count = u16(b, eocd + 10), cd_off = u32(b, eocd + 16);
  if (count == 0xffff || cd_off == 0xffffffffu) return fail("a ZIP64 nem támogatott");
  std::vector<Entry> es;
  size_t p = cd_off;
  for (uint32_t i = 0; i < count; ++i) {
    if (u32(b, p) != 0x02014b50u) return fail("sérült központi könyvtár");
    Entry e;
    e.flags = u16(b, p + 8);
    e.method = u16(b, p + 10);
    e.csize = u32(b, p + 20);
    e.usize = u32(b, p + 24);
    const uint32_t nl = u16(b, p + 28), xl = u16(b, p + 30), cl = u16(b, p + 32);
    e.local = u32(b, p + 42);
    if (p + 46 + nl > b.size()) return fail("sérült központi könyvtár");
    e.name.assign(reinterpret_cast<const char*>(&b[p + 46]), nl);
    p += 46 + nl + xl + cl;
    if (e.flags & 1u) return fail("titkosított bejegyzés: " + e.name);
    if (!safe_name(e.name)) return fail("tiltott útvonal: " + e.name);
    es.push_back(std::move(e));
  }
  // egyetlen gyökérmappa: az a mod gyökere (ha ott van a mod.json)
  std::string strip;
  if (!es.empty()) {
    const auto slash = es[0].name.find('/');
    const std::string top = slash == std::string::npos ? std::string() : es[0].name.substr(0, slash + 1);
    bool common = !top.empty();
    for (const auto& e : es) common = common && e.name.rfind(top, 0) == 0;
    bool has_mod = false;
    for (const auto& e : es) has_mod = has_mod || e.name == top + "mod.json";
    if (common && has_mod) strip = top;
  }
  std::error_code ec;
  fs::remove_all(dest, ec);
  fs::create_directories(dest, ec);
  for (const auto& e : es) {
    std::string rel = e.name.substr(strip.size());
    if (rel.empty()) continue;
    const fs::path out = dest / fs::path(std::u8string(rel.begin(), rel.end()));
    if (rel.back() == '/') {   // mappa
      fs::create_directories(out, ec);
      continue;
    }
    if (u32(b, e.local) != 0x04034b50u) return fail("sérült helyi fejléc: " + e.name);
    const size_t data = e.local + 30 + u16(b, e.local + 26) + u16(b, e.local + 28);
    if (data + e.csize > b.size() || e.usize > 0x10000000u) return fail("sérült vagy túl nagy bejegyzés: " + e.name);
    std::string bytes;
    if (e.method == 0) {
      bytes.assign(reinterpret_cast<const char*>(&b[data]), e.csize);
    } else if (e.method == 8) {
      bytes.resize(e.usize);
      const int n = e.usize == 0 ? 0
                                 : stbi_zlib_decode_noheader_buffer(bytes.data(), static_cast<int>(e.usize), reinterpret_cast<const char*>(&b[data]),
                                                                    static_cast<int>(e.csize));
      if (n != static_cast<int>(e.usize)) return fail("hibás tömörített adat: " + e.name);
    } else {
      return fail("nem támogatott tömörítés (" + std::to_string(e.method) + "): " + e.name);
    }
    fs::create_directories(out.parent_path(), ec);
    std::ofstream o(out, std::ios::binary | std::ios::trunc);
    o.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!o) return fail("nem írható: " + rel);
  }
  return true;
}

fs::path unpack_zip_mod(const fs::path& zip, const fs::path& cache_dir, std::string* error) {
  std::error_code ec;
  const auto size = fs::file_size(zip, ec);
  const auto mt = fs::last_write_time(zip, ec).time_since_epoch().count();
  const std::string stamp = std::to_string(size) + "|" + std::to_string(mt);
  const fs::path dest = cache_dir / zip.stem();
  {
    std::ifstream s(cache_dir / (zip.stem().string() + ".ujjlenyomat"), std::ios::binary);   // a mod mappáján kívül (a modlock miatt)
    const std::string old{std::istreambuf_iterator<char>(s), std::istreambuf_iterator<char>()};
    if (old == stamp && fs::is_regular_file(dest / "mod.json", ec)) return dest;
  }
  if (!extract_zip(zip, dest, error)) {
    fs::remove_all(dest, ec);
    return {};
  }
  std::ofstream(cache_dir / (zip.stem().string() + ".ujjlenyomat"), std::ios::binary) << stamp;
  return dest;
}

}  // namespace totu::content
