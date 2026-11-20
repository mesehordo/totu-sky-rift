// SPDX-License-Identifier: GPL-3.0-or-later
// Zene és hangok (a régi rts/audio.py `Audio`): SDL3 hangfolyamokkal – egy a zenének, 24 a hanghatásoknak; az SDL
// keveri őket. Zene (assets/music): a menükben a `menuzene.mp3` ismétlődve; csatában véletlen szám (a menüzene nélkül,
// küldetésnél a `kuldetesek/<id>` mappa számai előbb), ha véget ér, másik; a szünetmenü „Zene” pontjában bármelyik
// választható. Hangok (assets/sounds): lásd SoundBank. Hangeszköz nélkül néma (a játék fut).
#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "client/audio_decode.hpp"
#include "client/settings.hpp"
#include "client/sound_bank.hpp"

struct SDL_AudioStream;

namespace totu::client {

class Audio {
 public:
  Audio() = default;
  ~Audio();
  Audio(const Audio&) = delete;
  Audio& operator=(const Audio&) = delete;

  bool init(const std::string& assets);   // false: nincs hangeszköz (néma)
  void shutdown();
  bool ok() const { return ok_; }
  // képkockánként: a hangerők, a zene folytatása (a következő szám, a menüzene ismétlése), a kész dekódolások
  void update(const Settings& st, double now);

  void menu_music();
  void game_music(const std::string& mission_id);
  // a választható számok: (név, a music mappán belüli útvonal) – előbb a küldetés saját zenéi
  std::vector<std::pair<std::string, std::string>> tracks() const;
  void choose(const std::string& rel);
  const std::string& current() const { return current_; }

  // egy esemény hangja (a régi `play`): a legspecifikusabb fájl, a kategória hangerejével × `vol`; `throttle_ms`
  // a `key` szerinti legkisebb időköz
  void play(const std::string& ev, const std::string& unit_id, const std::string& faction, const std::string& utype, float vol,
            int throttle_ms, const std::string& key, bool building = false);

 private:
  struct Job {   // háttérben dekódolt szám
    std::string rel;
    std::optional<Pcm> pcm;
    std::atomic<bool> done{false};
  };
  void start_track(const std::string& rel, bool loop);
  void next_game_track();
  std::vector<std::string> list_tracks(const std::string& sub) const;
  const Pcm* sfx(const std::string& path);

  bool ok_ = false;
  std::string music_dir_, sound_dir_;
  std::unique_ptr<SoundBank> bank_;
  uint32_t dev_ = 0;
  SDL_AudioStream* music_ = nullptr;
  std::vector<SDL_AudioStream*> sfx_streams_;
  std::map<std::string, std::optional<Pcm>> sfx_cache_;
  std::map<std::string, double> cooldown_;
  std::string mode_, mission_, current_, pending_rel_;
  bool loop_ = false, exhausted_ = false;
  std::vector<std::string> bad_;
  std::shared_ptr<Job> job_;
  std::thread worker_;
  std::shared_ptr<Pcm> playing_;   // a szóló szám (a menüzene ismétléséhez)
  float music_gain_ = 0, fade_in_ = 0;
  double now_ = 0, last_ = -1;
  uint32_t rng_ = 0x2545F491u;
  const Settings* st_ = nullptr;
};

}  // namespace totu::client
