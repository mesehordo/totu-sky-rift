// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/audio.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace totu::client {

namespace {

constexpr const char* kMenuTrack = "menuzene.mp3";
constexpr int kSfxStreams = 24;

std::string u8(const std::filesystem::path& p) {
  const auto s = p.u8string();
  return std::string(s.begin(), s.end());
}
std::filesystem::path from_u8(const std::string& s) { return std::filesystem::path(std::u8string(s.begin(), s.end())); }
std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}
SDL_AudioSpec spec_of(const Pcm& p) {
  SDL_AudioSpec s{};
  s.format = SDL_AUDIO_S16;
  s.channels = p.channels;
  s.freq = p.rate;
  return s;
}

}  // namespace

Audio::~Audio() { shutdown(); }

bool Audio::init(const std::string& assets) {
  music_dir_ = u8(from_u8(assets) / "music");
  sound_dir_ = u8(from_u8(assets) / "sounds");
  bank_ = std::make_unique<SoundBank>(from_u8(sound_dir_));
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
  dev_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
  if (!dev_) return false;
  SDL_AudioSpec dst{};
  int frames = 0;
  if (!SDL_GetAudioDeviceFormat(dev_, &dst, &frames)) return false;
  SDL_AudioSpec src{};
  src.format = SDL_AUDIO_S16;
  src.channels = 2;
  src.freq = 44100;
  std::vector<SDL_AudioStream*> all;
  music_ = SDL_CreateAudioStream(&src, &dst);
  if (!music_) return false;
  all.push_back(music_);
  for (int i = 0; i < kSfxStreams; ++i) {
    if (SDL_AudioStream* s = SDL_CreateAudioStream(&src, &dst)) {
      sfx_streams_.push_back(s);
      all.push_back(s);
    }
  }
  if (!SDL_BindAudioStreams(dev_, all.data(), static_cast<int>(all.size()))) return false;
  SDL_SetAudioStreamGain(music_, 0.0f);
  ok_ = true;
  return true;
}

void Audio::shutdown() {
  if (worker_.joinable()) worker_.join();
  job_.reset();
  if (music_) SDL_DestroyAudioStream(music_);
  music_ = nullptr;
  for (SDL_AudioStream* s : sfx_streams_) SDL_DestroyAudioStream(s);
  sfx_streams_.clear();
  if (dev_) SDL_CloseAudioDevice(dev_);
  dev_ = 0;
  ok_ = false;
}

std::vector<std::string> Audio::list_tracks(const std::string& sub) const {
  std::vector<std::string> out;
  std::error_code ec;
  const auto dir = sub.empty() ? from_u8(music_dir_) : from_u8(music_dir_) / from_u8(sub);
  for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
    if (!e.is_regular_file(ec) || lower(u8(e.path().extension())) != ".mp3") continue;
    out.push_back(sub.empty() ? u8(e.path().filename()) : sub + "/" + u8(e.path().filename()));
  }
  std::sort(out.begin(), out.end());
  return out;
}

std::vector<std::pair<std::string, std::string>> Audio::tracks() const {
  std::vector<std::pair<std::string, std::string>> out;
  auto stem = [](const std::string& rel) { return u8(from_u8(rel).stem()); };
  if (!mission_.empty()) {
    for (const auto& t : list_tracks("kuldetesek/" + mission_)) out.emplace_back(stem(t), t);
  }
  for (const auto& t : list_tracks("")) {
    if (lower(t) != kMenuTrack) out.emplace_back(stem(t), t);
  }
  return out;
}

void Audio::start_track(const std::string& rel, bool loop) {
  if (!ok_) return;
  if (worker_.joinable()) worker_.join();   // az előző dekódolás vége (ritka: gyors egymásutáni váltás)
  loop_ = loop;
  pending_rel_ = rel;
  auto job = std::make_shared<Job>();
  job->rel = rel;
  job_ = job;
  const std::string path = u8(from_u8(music_dir_) / from_u8(rel));
  worker_ = std::thread([job, path] {
    job->pcm = decode_audio(path);
    job->done = true;
  });
}

void Audio::menu_music() {
  if (mode_ == "menu" && (current_ == kMenuTrack || pending_rel_ == kMenuTrack)) return;   // már ez szól
  mode_ = "menu";
  start_track(kMenuTrack, true);
}

void Audio::game_music(const std::string& mission_id) {
  mode_ = "game";
  mission_ = mission_id;
  exhausted_ = false;
  bad_.clear();
  next_game_track();
}

void Audio::next_game_track() {
  if (!ok_) return;
  // előbb a küldetés saját zenéi, ha mind rossz vagy nincs: az általánosak
  for (const std::string& sub : {mission_.empty() ? std::string() : "kuldetesek/" + mission_, std::string()}) {
    std::vector<std::string> pool;
    for (const auto& t : list_tracks(sub)) {
      if (lower(t) != kMenuTrack && std::find(bad_.begin(), bad_.end(), t) == bad_.end()) pool.push_back(t);
    }
    for (size_t i = pool.size(); i > 1; --i) {   // keverés (a régi `shuffle`)
      rng_ ^= rng_ << 13;
      rng_ ^= rng_ >> 17;
      rng_ ^= rng_ << 5;
      std::swap(pool[i - 1], pool[rng_ % i]);
    }
    if (pool.size() > 1) {   // ne ugyanaz jöjjön újra
      const auto it = std::find(pool.begin(), pool.end(), current_);
      if (it != pool.end()) std::rotate(it, it + 1, pool.end());
    }
    if (!pool.empty()) {
      start_track(pool.front(), false);
      return;
    }
    if (sub.empty()) break;
  }
  exhausted_ = true;
  SDL_ClearAudioStream(music_);
  playing_.reset();
  current_.clear();
}

void Audio::choose(const std::string& rel) {
  mode_ = "game";
  exhausted_ = false;
  bad_.erase(std::remove(bad_.begin(), bad_.end(), rel), bad_.end());
  start_track(rel, false);
}

void Audio::update(const Settings& st, double now) {
  st_ = &st;
  const float dt = last_ < 0 ? 0.0f : static_cast<float>(std::clamp(now - last_, 0.0, 0.5));
  last_ = now;
  now_ = now;
  if (!ok_) return;
  if (job_ && job_->done) {   // kész a dekódolás: a szám indul (halkan felúszva)
    if (worker_.joinable()) worker_.join();
    std::shared_ptr<Job> job = std::move(job_);
    if (job->pcm && job->pcm->channels > 0) {
      playing_ = std::make_shared<Pcm>(std::move(*job->pcm));
      SDL_ClearAudioStream(music_);
      const SDL_AudioSpec s = spec_of(*playing_);
      SDL_SetAudioStreamFormat(music_, &s, nullptr);
      SDL_PutAudioStreamData(music_, playing_->samples.data(), static_cast<int>(playing_->samples.size() * 2));
      current_ = job->rel;
      fade_in_ = 0;
    } else {
      bad_.push_back(job->rel);
      if (mode_ == "game") next_game_track();
    }
    pending_rel_.clear();
  }
  if (playing_ && !job_) {
    const int queued = SDL_GetAudioStreamQueued(music_);
    if (loop_ && queued < playing_->rate * playing_->channels * 2 * 2) {   // a menüzene ismétlődik (2 mp előre)
      SDL_PutAudioStreamData(music_, playing_->samples.data(), static_cast<int>(playing_->samples.size() * 2));
    } else if (!loop_ && queued <= 0 && mode_ == "game" && !exhausted_) {
      next_game_track();   // véget ért: a következő szám
    }
  }
  fade_in_ = std::min(1.0f, fade_in_ + dt / 1.2f);
  SDL_SetAudioStreamGain(music_, sound_volume(st, Sound::Music) * fade_in_);
}

const Pcm* Audio::sfx(const std::string& path) {
  auto it = sfx_cache_.find(path);
  if (it == sfx_cache_.end()) {
    if (sfx_cache_.size() > 512) sfx_cache_.clear();
    it = sfx_cache_.emplace(path, decode_audio(path)).first;
  }
  return it->second ? &*it->second : nullptr;
}

void Audio::play(const std::string& ev, const std::string& unit_id, const std::string& faction, const std::string& utype, float vol,
                 int throttle_ms, const std::string& key, bool building) {
  if (!ok_ || !st_ || !bank_) return;
  const float volume = sound_volume(*st_, sound_category(ev, building)) * vol;
  if (volume <= 0.0f) return;
  const std::string k = key.empty() ? ev + "|" + unit_id : key;
  if (throttle_ms > 0) {
    const auto c = cooldown_.find(k);
    if (c != cooldown_.end() && c->second > now_) return;
  }
  const std::string path = bank_->pick(ev, unit_id, faction, utype, rng_);
  if (path.empty()) return;
  const Pcm* p = sfx(path);
  if (!p || p->samples.empty()) return;
  for (SDL_AudioStream* s : sfx_streams_) {
    if (SDL_GetAudioStreamQueued(s) > 0) continue;
    const SDL_AudioSpec sp = spec_of(*p);
    SDL_SetAudioStreamFormat(s, &sp, nullptr);
    SDL_SetAudioStreamGain(s, volume);
    SDL_PutAudioStreamData(s, p->samples.data(), static_cast<int>(p->samples.size() * 2));
    break;
  }
  if (throttle_ms > 0) cooldown_[k] = now_ + throttle_ms / 1000.0;
}

}  // namespace totu::client
