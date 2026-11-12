// SPDX-License-Identifier: GPL-3.0-or-later
#include "content/json.hpp"

#include "core/decimal.hpp"

namespace totu::content {

std::optional<core::Fixed> Value::as_fixed() const {
  if (type_ != Type::Number) return std::nullopt;
  return core::parse_fixed(str_);
}

std::optional<long long> Value::as_int() const {
  if (type_ != Type::Number || str_.empty()) return std::nullopt;
  size_t i = 0;
  bool neg = false;
  if (str_[0] == '-') { neg = true; i = 1; }
  if (i >= str_.size()) return std::nullopt;
  long long v = 0;
  for (; i < str_.size(); ++i) {
    const char c = str_[i];
    if (c < '0' || c > '9') return std::nullopt;            // tört vagy kitevő: nem egész
    if (v > (9223372036854775807LL - (c - '0')) / 10) return std::nullopt;
    v = v * 10 + (c - '0');
  }
  return neg ? -v : v;
}

const Value* Value::find(std::string_view key) const {
  for (const auto& m : members_) if (m.first == key) return &m.second;
  return nullptr;
}

Value* Value::find(std::string_view key) {
  for (auto& m : members_) if (m.first == key) return &m.second;
  return nullptr;
}

void Value::set(std::string key, Value v) {
  if (Value* old = find(key)) { *old = std::move(v); return; }
  members_.emplace_back(std::move(key), std::move(v));
}

bool Value::erase(std::string_view key) {
  for (size_t i = 0; i < members_.size(); ++i) {
    if (members_[i].first == key) { members_.erase(members_.begin() + static_cast<std::ptrdiff_t>(i)); return true; }
  }
  return false;
}

bool operator==(const Value& a, const Value& b) {
  if (a.type_ != b.type_) return false;
  switch (a.type_) {
    case Value::Type::Null: return true;
    case Value::Type::Bool: return a.bool_ == b.bool_;
    case Value::Type::Number:
    case Value::Type::String: return a.str_ == b.str_;
    case Value::Type::Array: return a.items_ == b.items_;
    case Value::Type::Object: return a.members_ == b.members_;
  }
  return false;
}

namespace {

class Parser {
 public:
  explicit Parser(std::string_view s) : s_(s) {}

  std::optional<Value> run(ParseError* err) {
    skip_ws();
    auto v = value(0);
    if (v) {
      skip_ws();
      if (pos_ != s_.size()) { fail("a dokumentum vége után további szöveg áll"); v.reset(); }
    }
    if (!v && err) *err = error_;
    return v;
  }

 private:
  static constexpr int kMaxDepth = 200;

  void fail(const char* msg) {
    if (!error_.message.empty()) return;
    int line = 1, col = 1;
    for (size_t i = 0; i < pos_ && i < s_.size(); ++i) {
      if (s_[i] == '\n') { ++line; col = 1; } else { ++col; }
    }
    error_ = {msg, line, col};
  }

  void skip_ws() {
    while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r')) ++pos_;
  }

  bool literal(std::string_view word) {
    if (s_.substr(pos_, word.size()) == word) { pos_ += word.size(); return true; }
    return false;
  }

  std::optional<Value> value(int depth) {
    if (depth > kMaxDepth) { fail("túl mély egymásba ágyazás"); return std::nullopt; }
    if (pos_ >= s_.size()) { fail("váratlan fájlvég"); return std::nullopt; }
    const char c = s_[pos_];
    if (c == '{') return object(depth);
    if (c == '[') return array(depth);
    if (c == '"') { auto s = string(); if (!s) return std::nullopt; return Value::string(std::move(*s)); }
    if (c == '-' || (c >= '0' && c <= '9')) return number();
    if (literal("true")) return Value::boolean(true);
    if (literal("false")) return Value::boolean(false);
    if (literal("null")) return Value::null();
    fail("érvénytelen érték");
    return std::nullopt;
  }

  std::optional<Value> number() {
    const size_t start = pos_;
    if (s_[pos_] == '-') ++pos_;
    auto digits = [&] {
      const size_t d = pos_;
      while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
      return pos_ > d;
    };
    if (!digits()) { fail("hibás szám"); return std::nullopt; }
    if (pos_ < s_.size() && s_[pos_] == '.') { ++pos_; if (!digits()) { fail("hibás szám"); return std::nullopt; } }
    if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
      ++pos_;
      if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) ++pos_;
      if (!digits()) { fail("hibás kitevő"); return std::nullopt; }
    }
    return Value::number(std::string(s_.substr(start, pos_ - start)));
  }

  static void put_utf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) { out.push_back(static_cast<char>(cp)); }
    else if (cp < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }

  std::optional<uint32_t> hex4() {
    if (pos_ + 4 > s_.size()) return std::nullopt;
    uint32_t v = 0;
    for (int k = 0; k < 4; ++k) {
      const char h = s_[pos_++];
      v <<= 4;
      if (h >= '0' && h <= '9') v |= static_cast<uint32_t>(h - '0');
      else if (h >= 'a' && h <= 'f') v |= static_cast<uint32_t>(h - 'a' + 10);
      else if (h >= 'A' && h <= 'F') v |= static_cast<uint32_t>(h - 'A' + 10);
      else return std::nullopt;
    }
    return v;
  }

  std::optional<std::string> string() {
    ++pos_;   // nyitó idézőjel
    std::string out;
    while (pos_ < s_.size()) {
      const char c = s_[pos_++];
      if (c == '"') return out;
      if (static_cast<unsigned char>(c) < 0x20) { --pos_; fail("vezérlőkarakter a szövegben"); return std::nullopt; }
      if (c != '\\') { out.push_back(c); continue; }
      if (pos_ >= s_.size()) break;
      const char e = s_[pos_++];
      switch (e) {
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'u': {
          auto cp = hex4();
          if (!cp) { fail("hibás \\u kód"); return std::nullopt; }
          uint32_t code = *cp;
          if (code >= 0xD800 && code <= 0xDBFF) {              // UTF-16 pár
            if (!literal("\\u")) { fail("hiányzó UTF-16 pár"); return std::nullopt; }
            auto lo = hex4();
            if (!lo || *lo < 0xDC00 || *lo > 0xDFFF) { fail("hibás UTF-16 pár"); return std::nullopt; }
            code = 0x10000 + ((code - 0xD800) << 10) + (*lo - 0xDC00);
          }
          put_utf8(out, code);
          break;
        }
        default: --pos_; fail("ismeretlen escape-jel"); return std::nullopt;
      }
    }
    fail("lezáratlan szöveg");
    return std::nullopt;
  }

  std::optional<Value> array(int depth) {
    ++pos_;
    Value arr = Value::array();
    skip_ws();
    if (pos_ < s_.size() && s_[pos_] == ']') { ++pos_; return arr; }
    for (;;) {
      skip_ws();
      auto v = value(depth + 1);
      if (!v) return std::nullopt;
      arr.push(std::move(*v));
      skip_ws();
      if (pos_ < s_.size() && s_[pos_] == ',') { ++pos_; continue; }
      if (pos_ < s_.size() && s_[pos_] == ']') { ++pos_; return arr; }
      fail("vessző vagy ] hiányzik");
      return std::nullopt;
    }
  }

  std::optional<Value> object(int depth) {
    ++pos_;
    Value obj = Value::object();
    skip_ws();
    if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; return obj; }
    for (;;) {
      skip_ws();
      if (pos_ >= s_.size() || s_[pos_] != '"') { fail("kulcs (idézőjeles szöveg) hiányzik"); return std::nullopt; }
      const size_t key_pos = pos_;
      auto key = string();
      if (!key) return std::nullopt;
      if (obj.find(*key)) { pos_ = key_pos; fail("ismétlődő kulcs"); return std::nullopt; }
      skip_ws();
      if (pos_ >= s_.size() || s_[pos_] != ':') { fail("kettőspont hiányzik"); return std::nullopt; }
      ++pos_;
      skip_ws();
      auto v = value(depth + 1);
      if (!v) return std::nullopt;
      obj.members().emplace_back(std::move(*key), std::move(*v));
      skip_ws();
      if (pos_ < s_.size() && s_[pos_] == ',') { ++pos_; continue; }
      if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; return obj; }
      fail("vessző vagy } hiányzik");
      return std::nullopt;
    }
  }

  std::string_view s_;
  size_t pos_ = 0;
  ParseError error_;
};

void dump_string(std::string& out, const std::string& s) {
  out.push_back('"');
  for (const char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          const char* hex = "0123456789abcdef";
          out += "\\u00";
          out.push_back(hex[(c >> 4) & 0xF]);
          out.push_back(hex[c & 0xF]);
        } else {
          out.push_back(c);
        }
    }
  }
  out.push_back('"');
}

void dump_into(std::string& out, const Value& v) {
  switch (v.type()) {
    case Value::Type::Null: out += "null"; break;
    case Value::Type::Bool: out += v.as_bool() ? "true" : "false"; break;
    case Value::Type::Number: out += v.text(); break;
    case Value::Type::String: dump_string(out, v.text()); break;
    case Value::Type::Array: {
      out.push_back('[');
      bool first = true;
      for (const auto& it : v.items()) { if (!first) out.push_back(','); first = false; dump_into(out, it); }
      out.push_back(']');
      break;
    }
    case Value::Type::Object: {
      out.push_back('{');
      bool first = true;
      for (const auto& m : v.members()) {
        if (!first) out.push_back(',');
        first = false;
        dump_string(out, m.first);
        out.push_back(':');
        dump_into(out, m.second);
      }
      out.push_back('}');
      break;
    }
  }
}

}  // namespace

std::optional<Value> parse(std::string_view text, ParseError* error) {
  if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB &&
      static_cast<unsigned char>(text[2]) == 0xBF) {
    text.remove_prefix(3);   // UTF-8 BOM
  }
  return Parser(text).run(error);
}

std::string dump(const Value& v) {
  std::string out;
  dump_into(out, v);
  return out;
}

}  // namespace totu::content
