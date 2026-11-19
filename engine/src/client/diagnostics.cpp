// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/diagnostics.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <random>

#include "content/json.hpp"
#include "core/platform_info.hpp"

namespace totu::client {

namespace {

using content::Value;

Value num_i(int64_t v) { return Value::number(std::to_string(v)); }

Value num_f(double v) {   // két tizedes, a nem mért (−1) `null`
  if (v < 0) return Value::null();
  char b[32];
  std::snprintf(b, sizeof b, "%.2f", v);
  return Value::number(b);
}

Value str_or_null(const std::string& s) { return s.empty() ? Value::null() : Value::string(s); }

// az eseményszótár (DIAGNOSTICS_DESIGN 4.): hiba-jellegű kódok és a felület elutasításai
const char* const kErrorCodes[] = {"ASSET_FALLBACK", "LOCALE_MISSING", "LOAD_FAILED", "SAVE_FAILED", "NET_DESYNC"};
const char* const kUxCodes[] = {"CMD_LOCAL_NO_RESOURCE", "CMD_LOCAL_NO_SUPPLY", "CMD_LOCAL_BAD_PLACE"};

}  // namespace

Diagnostics::Diagnostics() {
  std::random_device rd;   // munkamenetenként véletlen; semmihez nem kötődik (nem gép- és nem felhasználó-azonosító)
  char b[17];
  std::snprintf(b, sizeof b, "%08x%08x", rd(), rd());
  session_id_ = b;
}

Diagnostics& Diagnostics::get() {
  static Diagnostics d;
  return d;
}

const std::vector<std::string>& Diagnostics::codes() {
  static const std::vector<std::string> c = [] {
    std::vector<std::string> v;
    for (const char* x : kErrorCodes) v.emplace_back(x);
    for (const char* x : kUxCodes) v.emplace_back(x);
    return v;
  }();
  return c;
}

void Diagnostics::count(const std::string& code) {
  const auto& c = codes();
  if (std::find(c.begin(), c.end(), code) == c.end()) return;   // engedélyezőlista
  std::lock_guard<std::mutex> lock(mu_);
  ++counts_[code];
}

void Diagnostics::count_once(const std::string& code, const std::string& key) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (seen_keys_.size() > 100000 || !seen_keys_.insert(code + '\n' + key).second) return;
  }
  count(code);
}

int64_t Diagnostics::counter(const std::string& code) const {
  std::lock_guard<std::mutex> lock(mu_);
  const auto it = counts_.find(code);
  return it == counts_.end() ? 0 : it->second;
}

void Diagnostics::frame_ms(double ms) {
  std::lock_guard<std::mutex> lock(mu_);
  ++frame_hist_[static_cast<size_t>(std::clamp(ms / kBinMs, 0.0, static_cast<double>(kBins)))];
  ++frames_;
}

void Diagnostics::tick_ms(double ms) {
  std::lock_guard<std::mutex> lock(mu_);
  ++tick_hist_[static_cast<size_t>(std::clamp(ms / kBinMs, 0.0, static_cast<double>(kBins)))];
  ++ticks_;
  if (ms > kTickBudgetMs) ++ticks_over_;
}

void Diagnostics::note_units(int n) {
  std::lock_guard<std::mutex> lock(mu_);
  peak_units_ = std::max(peak_units_, n);
}

double Diagnostics::percentile(const std::array<uint32_t, kBins + 1>& h, uint64_t n, double q) {
  if (n == 0) return -1;
  const auto need = static_cast<uint64_t>(std::ceil(q * static_cast<double>(n)));
  uint64_t acc = 0;
  for (size_t i = 0; i < h.size(); ++i) {
    acc += h[i];
    if (acc >= std::max<uint64_t>(1, need)) return static_cast<double>(i + 1) * kBinMs;   // a rekesz felső széle
  }
  return static_cast<double>(kBins) * kBinMs;
}

double Diagnostics::frame_percentile(double q) const {
  std::lock_guard<std::mutex> lock(mu_);
  return percentile(frame_hist_, frames_, q);
}

double Diagnostics::tick_percentile(double q) const {
  std::lock_guard<std::mutex> lock(mu_);
  return percentile(tick_hist_, ticks_, q);
}

void Diagnostics::reset() {
  std::lock_guard<std::mutex> lock(mu_);
  counts_.clear();
  seen_keys_.clear();
  frame_hist_ = {};
  tick_hist_ = {};
  frames_ = ticks_ = ticks_over_ = 0;
  peak_units_ = -1;
}

std::string Diagnostics::to_json(const DiagSession& s, const std::string& renderer) const {
  std::lock_guard<std::mutex> lock(mu_);
  Value o = Value::object();
  o.set("schema_version", num_i(1));
  Value app = Value::object();
  app.set("version", Value::string(core::version()));
  app.set("platform", Value::string(core::os_name()));
  app.set("arch", Value::string(core::cpu_arch()));
  app.set("renderer", str_or_null(renderer));
#ifdef NDEBUG
  app.set("build_type", Value::string("release"));
#else
  app.set("build_type", Value::string("debug"));
#endif
  o.set("app", std::move(app));
  Value se = Value::object();
  se.set("id", Value::string(session_id_));
  se.set("mode", str_or_null(s.mode));
  se.set("map_id", str_or_null(s.map_id));
  se.set("seed", s.seed < 0 ? Value::null() : num_i(s.seed));
  se.set("players", s.players < 0 ? Value::null() : num_i(s.players));
  Value fa = Value::array();
  for (const auto& f : s.factions) fa.push(Value::string(f));
  se.set("factions", std::move(fa));
  se.set("victory", str_or_null(s.victory));
  se.set("tempo", str_or_null(s.tempo));
  se.set("duration_ticks", s.duration_ticks < 0 ? Value::null() : num_i(s.duration_ticks));
  se.set("result", str_or_null(s.result));
  o.set("session", std::move(se));
  Value pe = Value::object();
  pe.set("frame_ms_p50", num_f(percentile(frame_hist_, frames_, 0.50)));
  pe.set("frame_ms_p95", num_f(percentile(frame_hist_, frames_, 0.95)));
  pe.set("frame_ms_p99", num_f(percentile(frame_hist_, frames_, 0.99)));
  pe.set("tick_ms_p95", num_f(percentile(tick_hist_, ticks_, 0.95)));
  pe.set("tick_ms_p99", num_f(percentile(tick_hist_, ticks_, 0.99)));
  if (ticks_ == 0) pe.set("over_budget_tick_ratio", Value::null());
  else pe.set("over_budget_tick_ratio", num_f(static_cast<double>(ticks_over_) / static_cast<double>(ticks_)));
  const int peak = std::max(peak_units_, s.peak_units);
  pe.set("peak_units", peak < 0 ? Value::null() : num_i(peak));
  o.set("performance", std::move(pe));
  auto cnt = [&](const char* c) {
    const auto it = counts_.find(c);
    return it == counts_.end() ? int64_t{0} : it->second;
  };
  Value er = Value::object();
  int64_t total = 0;
  Value by = Value::object();
  for (const char* c : kErrorCodes) {
    if (cnt(c) == 0) continue;
    total += cnt(c);
    by.set(c, num_i(cnt(c)));
  }
  er.set("total", num_i(total));
  er.set("by_code", std::move(by));
  o.set("errors", std::move(er));
  Value ux = Value::object();
  Value rej = Value::object();
  rej.set("no_resource", num_i(cnt("CMD_LOCAL_NO_RESOURCE")));
  rej.set("no_supply", num_i(cnt("CMD_LOCAL_NO_SUPPLY")));
  rej.set("bad_place", num_i(cnt("CMD_LOCAL_BAD_PLACE")));
  ux.set("command_rejected_local", std::move(rej));
  o.set("ux", std::move(ux));
  Value fb = Value::object();   // a véleménymező a tulajdonos döntéséig üres (DIAGNOSTICS_DESIGN 7.)
  fb.set("liked", Value::null());
  fb.set("disliked", Value::null());
  fb.set("confusing", Value::null());
  o.set("feedback", std::move(fb));
  Value pr = Value::object();
  pr.set("sent_automatically", Value::boolean(false));
  o.set("privacy", std::move(pr));
  return content::dump_pretty(o) + "\n";
}

bool save_diagnostics(const std::string& dir, const std::string& json, std::string* file_name, std::string* error) {
  namespace fs = std::filesystem;
  std::error_code ec;
  if (dir.empty()) {
    if (error) *error = "nincs felhasználói mappa";
    return false;
  }
  fs::create_directories(dir, ec);
  const std::time_t now = std::time(nullptr);
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &now);
#else
  localtime_r(&now, &tm);
#endif
  char name[64];
  std::strftime(name, sizeof name, "diagnosztika_%Y%m%d_%H%M%S.json", &tm);
  const fs::path p = fs::path(dir) / name;
  const fs::path tmp = p.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    out << json;
    if (!out) {
      if (error) *error = "a fájl nem írható";
      fs::remove(tmp, ec);
      return false;
    }
  }
  fs::rename(tmp, p, ec);
  if (ec) {
    if (error) *error = "a fájl nem nevezhető át";
    fs::remove(tmp, ec);
    return false;
  }
  if (file_name) *file_name = name;
  return true;
}

}  // namespace totu::client
