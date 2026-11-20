// SPDX-License-Identifier: GPL-3.0-or-later
// A dekódolók egyetlen fordítási egységben (a harmadik féltől származó kód változatlan; figyelmeztetések nélkül fordul).
#include "client/audio_decode.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "stb_vorbis.c"

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include "dr_mp3.h"

namespace totu::client {

namespace {

std::optional<Pcm> decode_wav(const std::vector<uint8_t>& d) {
  auto u32 = [&](size_t i) { return static_cast<uint32_t>(d[i] | d[i + 1] << 8 | d[i + 2] << 16 | static_cast<uint32_t>(d[i + 3]) << 24); };
  auto u16 = [&](size_t i) { return static_cast<uint16_t>(d[i] | d[i + 1] << 8); };
  if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) != 0 || std::memcmp(d.data() + 8, "WAVE", 4) != 0) return std::nullopt;
  Pcm p;
  int bits = 0, format = 0;
  size_t i = 12;
  while (i + 8 <= d.size()) {
    const uint32_t len = u32(i + 4);
    const size_t body = i + 8;
    if (body + len > d.size()) break;
    if (std::memcmp(d.data() + i, "fmt ", 4) == 0 && len >= 16) {
      format = u16(body);
      p.channels = u16(body + 2);
      p.rate = static_cast<int>(u32(body + 4));
      bits = u16(body + 14);
    } else if (std::memcmp(d.data() + i, "data", 4) == 0) {
      if (format != 1 || bits != 16 || p.channels <= 0) return std::nullopt;   // csak 16 bites PCM
      p.samples.resize(len / 2);
      std::memcpy(p.samples.data(), d.data() + body, p.samples.size() * 2);
      return p;
    }
    i = body + len + (len & 1);
  }
  return std::nullopt;
}

}  // namespace

std::optional<Pcm> decode_audio_memory(const std::vector<uint8_t>& data, const std::string& ext) {
  if (data.empty() || data.size() > (256u << 20)) return std::nullopt;
  if (ext == ".ogg") {
    int ch = 0, rate = 0;
    short* out = nullptr;
    const int n = stb_vorbis_decode_memory(data.data(), static_cast<int>(data.size()), &ch, &rate, &out);
    if (n <= 0 || !out || ch <= 0) {
      std::free(out);
      return std::nullopt;
    }
    Pcm p;
    p.channels = ch;
    p.rate = rate;
    p.samples.assign(out, out + static_cast<size_t>(n) * static_cast<size_t>(ch));
    std::free(out);
    return p;
  }
  if (ext == ".mp3") {
    drmp3_config cfg{};
    drmp3_uint64 frames = 0;
    drmp3_int16* out = drmp3_open_memory_and_read_pcm_frames_s16(data.data(), data.size(), &cfg, &frames, nullptr);
    if (!out || cfg.channels == 0 || frames == 0) {
      drmp3_free(out, nullptr);
      return std::nullopt;
    }
    Pcm p;
    p.channels = static_cast<int>(cfg.channels);
    p.rate = static_cast<int>(cfg.sampleRate);
    p.samples.assign(out, out + frames * cfg.channels);
    drmp3_free(out, nullptr);
    return p;
  }
  if (ext == ".wav") return decode_wav(data);
  return std::nullopt;
}

std::optional<Pcm> decode_audio(const std::string& path) {
  const std::filesystem::path fp(std::u8string(path.begin(), path.end()));
  std::ifstream in(fp, std::ios::binary);
  if (!in) return std::nullopt;
  const std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  const auto e8 = fp.extension().u8string();
  std::string ext(e8.begin(), e8.end());
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return decode_audio_memory(data, ext);
}

}  // namespace totu::client
