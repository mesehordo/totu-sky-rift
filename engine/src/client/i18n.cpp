// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/i18n.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace totu::client {

namespace {

// egy idézőjeles .po-szöveg tartalma, a kódolás feloldásával
std::string unquote(std::string_view line) {
  const size_t a = line.find('"'), b = line.rfind('"');
  if (a == std::string_view::npos || b <= a) return {};
  std::string out;
  for (size_t i = a + 1; i < b; ++i) {
    char c = line[i];
    if (c == '\\' && i + 1 < b) {
      c = line[++i];
      if (c == 'n') c = '\n';
      else if (c == 't') c = '\t';
    }
    out += c;
  }
  return out;
}

std::string read_file(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// a fejléc egy mezője ("Language-Name: Magyar\n…" → "Magyar")
std::string header_field(const std::string& header, std::string_view field) {
  size_t pos = 0;
  while (pos < header.size()) {
    size_t end = header.find('\n', pos);
    if (end == std::string::npos) end = header.size();
    std::string_view line(header.data() + pos, end - pos);
    if (line.size() > field.size() && line.substr(0, field.size()) == field && line[field.size()] == ':') {
      line.remove_prefix(field.size() + 1);
      while (!line.empty() && line.front() == ' ') line.remove_prefix(1);
      while (!line.empty() && (line.back() == ' ' || line.back() == '\r')) line.remove_suffix(1);
      return std::string(line);
    }
    pos = end + 1;
  }
  return {};
}

}  // namespace

std::vector<PoEntry> parse_po(std::string_view text, std::string* header) {
  std::vector<PoEntry> out;
  PoEntry cur;
  std::string* field = nullptr;   // a folytatósor ide kerül
  bool has_id = false;
  auto flush = [&] {
    if (has_id && cur.id.empty() && cur.ctx.empty()) {
      if (header) *header = cur.str;
    } else if (has_id) {
      out.push_back(cur);
    }
    cur = PoEntry{};
    has_id = false;
    field = nullptr;
  };
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = text.substr(pos, end - pos);
    pos = end + 1;
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.remove_suffix(1);
    while (!line.empty() && line.front() == ' ') line.remove_prefix(1);
    if (line.empty() || line.front() == '#') continue;
    if (line.rfind("msgctxt ", 0) == 0) {
      flush();
      cur.ctx = unquote(line);
      field = &cur.ctx;
    } else if (line.rfind("msgid ", 0) == 0) {
      if (has_id) flush();
      cur.id = unquote(line);
      has_id = true;
      field = &cur.id;
    } else if (line.rfind("msgstr ", 0) == 0) {
      cur.str = unquote(line);
      field = &cur.str;
    } else if (line.front() == '"' && field) {
      *field += unquote(line);
    }
  }
  flush();
  return out;
}

bool Locale::load(const std::string& content_dir) {
  const std::filesystem::path root(content_dir);
  add_dir((root / "base" / "locale").string());
  std::error_code ec;
  std::vector<std::filesystem::path> mods;
  for (const auto& e : std::filesystem::directory_iterator(root / "mods", ec)) {
    if (e.is_directory()) mods.push_back(e.path() / "locale");
  }
  std::sort(mods.begin(), mods.end());
  for (const auto& m : mods) add_dir(m.string());
  return !langs_.empty();
}

void Locale::add_dir(const std::string& locale_dir) {
  std::error_code ec;
  std::vector<std::filesystem::path> files;
  for (const auto& e : std::filesystem::directory_iterator(locale_dir, ec)) {
    if (e.path().extension() == ".po") files.push_back(e.path());
  }
  std::sort(files.begin(), files.end());
  for (const auto& f : files) add_po(f.stem().string(), read_file(f));
}

void Locale::add_po(const std::string& code, std::string_view text) {
  std::string header;
  auto& t = tables_[code];
  const bool source = code == kSource;
  for (PoEntry& e : parse_po(text, &header)) {
    std::string key = e.ctx.empty() ? e.id : std::move(e.ctx);
    if (!e.str.empty()) t[key] = std::move(e.str);
    else if (source && !e.id.empty()) t[key] = std::move(e.id);
  }
  std::string name = header_field(header, "Language-Name");
  auto it = std::find_if(langs_.begin(), langs_.end(), [&](const Language& l) { return l.code == code; });
  if (it == langs_.end()) {
    langs_.push_back({code, name.empty() ? code : name});
    std::sort(langs_.begin(), langs_.end(), [](const Language& a, const Language& b) { return a.code < b.code; });
  } else if (!name.empty()) {
    it->name = name;
  }
}

bool Locale::has(std::string_view code) const {
  return std::any_of(langs_.begin(), langs_.end(), [&](const Language& l) { return l.code == code; });
}

std::string Locale::language_name(std::string_view code) const {
  for (const Language& l : langs_) {
    if (l.code == code) return l.name;
  }
  return std::string(code);
}

void Locale::set_lang(const std::string& code) { lang_ = has(code) ? code : std::string(kSource); }

const std::string* Locale::find(std::string_view code, std::string_view key) const {
  const auto t = tables_.find(code);
  if (t == tables_.end()) return nullptr;
  const auto it = t->second.find(key);
  return it == t->second.end() ? nullptr : &it->second;
}

std::string Locale::tr(std::string_view key) const {
  if (const std::string* s = find(lang_, key)) return *s;
  if (const std::string* s = find(kFallback, key)) return *s;
  return std::string(key);
}

std::string Locale::tr(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string>> fields) const {
  std::string s = tr(key);
  for (const auto& [name, value] : fields) {
    const std::string mark = "{" + std::string(name) + "}";
    for (size_t p = s.find(mark); p != std::string::npos; p = s.find(mark, p + value.size())) s.replace(p, mark.size(), value);
  }
  return s;
}

}  // namespace totu::client
