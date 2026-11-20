// SPDX-License-Identifier: GPL-3.0-or-later
// Hangfájlok dekódolása (a régi pygame.mixer helyett): Ogg Vorbis (stb_vorbis), MP3 (dr_mp3) és 16 bites PCM WAV,
// 16 bites, váltott csatornás mintákká. A fájlt bájtként olvassa (a Windows-os ékezetes útvonal is működik).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace totu::client {

struct Pcm {
  int channels = 0, rate = 0;
  std::vector<int16_t> samples;   // csatornánként váltakozva
  double seconds() const { return channels > 0 && rate > 0 ? static_cast<double>(samples.size()) / channels / rate : 0.0; }
};

std::optional<Pcm> decode_audio(const std::string& path);                                // kiterjesztés szerint
std::optional<Pcm> decode_audio_memory(const std::vector<uint8_t>& data, const std::string& ext);   // ".ogg", ".mp3", ".wav"

}  // namespace totu::client
