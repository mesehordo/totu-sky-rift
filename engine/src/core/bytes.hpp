// SPDX-License-Identifier: GPL-3.0-or-later
// Bájtfolyam a mentéshez és a visszajátszáshoz: mindig little-endian, rögzített szélességű egészekkel, így a
// mentés minden rendszeren ugyanaz a bájtsorozat (ADR-004, M2).
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace totu::core {

class ByteWriter {
 public:
  void u8(uint8_t v) { buf_.push_back(v); }
  void u32(uint32_t v) { for (int i = 0; i < 4; ++i) buf_.push_back(static_cast<uint8_t>(v >> (8 * i))); }
  void u64(uint64_t v) { for (int i = 0; i < 8; ++i) buf_.push_back(static_cast<uint8_t>(v >> (8 * i))); }
  void i32(int32_t v) { u32(static_cast<uint32_t>(v)); }
  void i64(int64_t v) { u64(static_cast<uint64_t>(v)); }
  void str(std::string_view s) {
    u32(static_cast<uint32_t>(s.size()));
    for (char c : s) buf_.push_back(static_cast<uint8_t>(c));
  }
  const std::vector<uint8_t>& data() const { return buf_; }

 private:
  std::vector<uint8_t> buf_;
};

class ByteReader {
 public:
  explicit ByteReader(const std::vector<uint8_t>& d) : d_(d) {}
  bool ok() const { return ok_; }
  bool at_end() const { return pos_ == d_.size(); }
  uint8_t u8() { return need(1) ? d_[pos_++] : 0; }
  uint32_t u32() {
    if (!need(4)) return 0;
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= uint32_t{d_[pos_++]} << (8 * i);
    return v;
  }
  uint64_t u64() {
    if (!need(8)) return 0;
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= uint64_t{d_[pos_++]} << (8 * i);
    return v;
  }
  int32_t i32() { return static_cast<int32_t>(u32()); }
  int64_t i64() { return static_cast<int64_t>(u64()); }
  std::string str() {
    const uint32_t n = u32();
    if (!need(n)) return {};
    std::string s(reinterpret_cast<const char*>(d_.data() + pos_), n);
    pos_ += n;
    return s;
  }

 private:
  bool need(size_t n) {
    if (!ok_ || d_.size() - pos_ < n) { ok_ = false; return false; }
    return true;
  }
  const std::vector<uint8_t>& d_;
  size_t pos_ = 0;
  bool ok_ = true;
};

}  // namespace totu::core
