// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/settings.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "content/json.hpp"

namespace totu::client {

// --- billentyűkiosztás ----------------------------------------------------------------------------------------------
Keymap::Keymap() {
  for (size_t i = 0; i < keys_.size(); ++i) keys_[i] = kDefault[i];
}

int Keymap::slot_of(char key) const {
  for (size_t i = 0; i < keys_.size(); ++i) {
    if (keys_[i] == key) return static_cast<int>(i);
  }
  return -1;
}

bool Keymap::valid_key(char key) { return key >= 'A' && key <= 'Z' && key != 'P'; }

int Keymap::assign(int slot, char key) {
  if (key >= 'a' && key <= 'z') key = static_cast<char>(key - 'a' + 'A');
  if (slot < 0 || slot >= kSlots || !valid_key(key)) return kInvalid;
  const int other = slot_of(key);
  if (other >= 0 && other != slot) return other;
  keys_[static_cast<size_t>(slot)] = key;
  return -1;
}

void Keymap::assign_swap(int slot, char key) {
  if (key >= 'a' && key <= 'z') key = static_cast<char>(key - 'a' + 'A');
  if (slot < 0 || slot >= kSlots || !valid_key(key)) return;
  const int other = slot_of(key);
  if (other >= 0) keys_[static_cast<size_t>(other)] = keys_[static_cast<size_t>(slot)];
  keys_[static_cast<size_t>(slot)] = key;
}

std::vector<std::pair<int, int>> Keymap::conflicts() const {
  std::vector<std::pair<int, int>> out;
  for (int a = 0; a < kSlots; ++a) {
    for (int b = a + 1; b < kSlots; ++b) {
      if (keys_[static_cast<size_t>(a)] == keys_[static_cast<size_t>(b)]) out.emplace_back(a, b);
    }
  }
  return out;
}

bool Keymap::parse(std::string_view s) {
  if (s.size() != static_cast<size_t>(kSlots)) return false;
  Keymap k;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if (!valid_key(c)) return false;
    k.keys_[i] = c;
  }
  if (!k.conflicts().empty()) return false;
  *this = k;
  return true;
}

// --- hang -----------------------------------------------------------------------------------------------------------
std::string_view sound_key(Sound s) {
  static constexpr std::string_view k[] = {"master", "music", "units", "combat", "buildings", "interface", "environment"};
  return k[static_cast<size_t>(s)];
}

std::string sound_label(Sound s) { return "ui.sound." + std::string(sound_key(s)); }

// --- beállítások ----------------------------------------------------------------------------------------------------
const std::vector<std::pair<int, int>>& Settings::resolutions() {
  static const std::vector<std::pair<int, int>> r = {{1024, 640}, {1280, 720}, {1280, 760}, {1366, 768},
                                                     {1600, 900}, {1920, 1080}, {2560, 1440}};
  return r;
}

const std::vector<int>& Settings::ui_scales() {
  static const std::vector<int> r = {100, 125, 150, 175, 200};
  return r;
}

namespace {

using content::Value;

Value num(long long v) { return Value::number(std::to_string(v)); }

}  // namespace

std::string settings_to_json(const Settings& s) {
  Value o = Value::object();
  o.set("language", Value::string(s.language));
  Value vol = Value::object();
  for (size_t i = 0; i < kSoundCount; ++i) {
    Value v = Value::object();
    v.set("volume", num(s.volume[i].value));
    v.set("enabled", Value::boolean(s.volume[i].enabled));
    vol.set(std::string(sound_key(static_cast<Sound>(i))), std::move(v));
  }
  o.set("sound", std::move(vol));
  o.set("resolution", Value::string(std::to_string(s.width) + "x" + std::to_string(s.height)));
  o.set("fullscreen", Value::boolean(s.fullscreen));
  o.set("ui_scale", num(s.ui_scale));
  o.set("colorblind", Value::boolean(s.colorblind));
  o.set("grid_keys", Value::string(s.keys.str()));
  o.set("scroll_speed", num(s.scroll_speed));
  o.set("edge_scroll", Value::boolean(s.edge_scroll));
  o.set("show_fps", Value::boolean(s.show_fps));
  o.set("fps_limit", num(s.fps_limit));
  return content::dump(o) + "\n";
}

bool settings_from_json(std::string_view text, Settings* out, std::vector<std::string>* warnings) {
  Settings s;
  const auto doc = content::parse(text);
  if (!doc || !doc->is_object()) return false;
  auto warn = [&](const std::string& k) {
    if (warnings) warnings->push_back("hibás beállítás, alapérték marad: " + k);
  };
  auto get_bool = [&](const Value& o, const char* k, bool* dst) {
    if (const Value* v = o.find(k)) {
      if (v->is_bool()) *dst = v->as_bool();
      else warn(k);
    }
  };
  auto get_int = [&](const Value& o, const char* k, int lo, int hi, int* dst) {
    if (const Value* v = o.find(k)) {
      const auto n = v->as_int();
      if (v->is_number() && n && *n >= lo && *n <= hi) *dst = static_cast<int>(*n);
      else warn(k);
    }
  };
  const Value& o = *doc;
  if (const Value* v = o.find("language")) {
    // a nyelv kódja (a locale mappa .po fájljának neve); hogy van-e ilyen nyelv, azt a Locale dönti el
    const bool ok = v->is_string() && v->text().size() >= 2 && v->text().size() <= 16 &&
                    std::all_of(v->text().begin(), v->text().end(), [](char c) {
                      return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-';
                    });
    if (ok) s.language = v->text();
    else warn("language");
  }
  if (const Value* snd = o.find("sound"); snd && snd->is_object()) {
    for (size_t i = 0; i < kSoundCount; ++i) {
      const std::string key(sound_key(static_cast<Sound>(i)));
      if (const Value* c = snd->find(key); c && c->is_object()) {
        get_int(*c, "volume", 0, 100, &s.volume[i].value);
        get_bool(*c, "enabled", &s.volume[i].enabled);
      }
    }
  }
  if (const Value* v = o.find("resolution")) {
    int w = 0, h = 0;
    if (v->is_string() && std::sscanf(v->text().c_str(), "%dx%d", &w, &h) == 2 && w >= 640 && h >= 400 && w <= 16384 &&
        h <= 16384) {
      s.width = w;
      s.height = h;
    } else {
      warn("resolution");
    }
  }
  get_bool(o, "fullscreen", &s.fullscreen);
  get_int(o, "ui_scale", 50, 300, &s.ui_scale);
  get_bool(o, "colorblind", &s.colorblind);
  if (const Value* v = o.find("grid_keys")) {
    if (!v->is_string() || !s.keys.parse(v->text())) warn("grid_keys");
  }
  get_int(o, "scroll_speed", 100, 5000, &s.scroll_speed);
  get_bool(o, "edge_scroll", &s.edge_scroll);
  get_bool(o, "show_fps", &s.show_fps);
  get_int(o, "fps_limit", 0, 1000, &s.fps_limit);
  *out = s;
  return true;
}

bool load_settings(const std::string& path, Settings* out, std::string* error) {
  *out = Settings{};
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) return true;
  std::ifstream in(path, std::ios::binary);
  const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  if (!settings_from_json(text, out)) {
    *out = Settings{};
    if (error) *error = "a beállításfájl hibás, alapértékek: " + path;
    return false;
  }
  return true;
}

bool save_settings(const std::string& path, const Settings& s, std::string* error) {
  const std::filesystem::path p(path);
  std::error_code ec;
  if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
  const std::filesystem::path tmp = p.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    out << settings_to_json(s);
    if (!out) {
      if (error) *error = "a beállításfájl nem írható: " + tmp.string();
      return false;
    }
  }
  std::filesystem::rename(tmp, p, ec);
  if (ec) {   // néhány rendszeren a meglévő fájlra nem lehet átnevezni
    std::filesystem::remove(p, ec);
    std::filesystem::rename(tmp, p, ec);
  }
  if (ec) {
    if (error) *error = "a beállításfájl nem írható: " + path;
    return false;
  }
  return true;
}

std::string settings_path(const std::string& pref_dir) {
  if (pref_dir.empty()) return "settings.json";
  return (std::filesystem::path(pref_dir) / "settings.json").string();
}

const std::array<TeamColor, 8>& team_colors(bool colorblind) {
  static const std::array<TeamColor, 8> normal = {TeamColor{220, 40, 40, 255},  TeamColor{40, 90, 230, 255},
                                                  TeamColor{40, 180, 60, 255},  TeamColor{230, 200, 40, 255},
                                                  TeamColor{160, 60, 200, 255}, TeamColor{240, 130, 30, 255},
                                                  TeamColor{40, 200, 200, 255}, TeamColor{235, 235, 235, 255}};
  static const std::array<TeamColor, 8> safe = {TeamColor{230, 159, 0, 255},   TeamColor{0, 114, 178, 255},
                                                TeamColor{240, 228, 66, 255},  TeamColor{86, 180, 233, 255},
                                                TeamColor{204, 121, 167, 255}, TeamColor{213, 94, 0, 255},
                                                TeamColor{0, 158, 115, 255},   TeamColor{235, 235, 235, 255}};
  return colorblind ? safe : normal;
}

}  // namespace totu::client
