// SPDX-License-Identifier: GPL-3.0-or-later
// Saját, kicsi JSON-elemző a tartalomhoz (ADR-005). Két tulajdonsága miatt nem külső könyvtár:
//   1. a számokat SZÖVEGKÉNT tartja meg, így a `parse_fixed` bitre azonosan alakítja őket fixpontossá (ADR-004);
//   2. az objektumok kulcssorrendje megmarad (determinisztikus bejárás és változatlan visszamentés).
// Hibánál sor- és oszlopszámot ad, hogy a modder megtalálja a hibát.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/fixed.hpp"

namespace totu::content {

class Value {
 public:
  enum class Type { Null, Bool, Number, String, Array, Object };
  using Member = std::pair<std::string, Value>;

  Value() = default;
  static Value null() { return Value{}; }
  static Value boolean(bool b) { Value v; v.type_ = Type::Bool; v.bool_ = b; return v; }
  static Value number(std::string text) { Value v; v.type_ = Type::Number; v.str_ = std::move(text); return v; }
  static Value string(std::string s) { Value v; v.type_ = Type::String; v.str_ = std::move(s); return v; }
  static Value array() { Value v; v.type_ = Type::Array; return v; }
  static Value object() { Value v; v.type_ = Type::Object; return v; }

  Type type() const { return type_; }
  bool is_null() const { return type_ == Type::Null; }
  bool is_bool() const { return type_ == Type::Bool; }
  bool is_number() const { return type_ == Type::Number; }
  bool is_string() const { return type_ == Type::String; }
  bool is_array() const { return type_ == Type::Array; }
  bool is_object() const { return type_ == Type::Object; }

  bool as_bool() const { return bool_; }
  const std::string& text() const { return str_; }          // szám vagy szöveg
  std::optional<core::Fixed> as_fixed() const;              // számból, determinisztikusan
  std::optional<long long> as_int() const;                  // csak egész számból

  // tömb
  std::vector<Value>& items() { return items_; }
  const std::vector<Value>& items() const { return items_; }
  void push(Value v) { items_.push_back(std::move(v)); }

  // objektum (a kulcsok sorrendje a beolvasásé)
  std::vector<Member>& members() { return members_; }
  const std::vector<Member>& members() const { return members_; }
  const Value* find(std::string_view key) const;
  Value* find(std::string_view key);
  void set(std::string key, Value v);                       // felülír vagy a végére tesz
  bool erase(std::string_view key);

  friend bool operator==(const Value& a, const Value& b);

 private:
  Type type_ = Type::Null;
  bool bool_ = false;
  std::string str_;
  std::vector<Value> items_;
  std::vector<Member> members_;
};

struct ParseError {
  std::string message;
  int line = 0;
  int column = 0;
};

// Elemzés; hiba esetén üres eredmény és kitöltött `error`.
std::optional<Value> parse(std::string_view text, ParseError* error = nullptr);
// Tömör, determinisztikus kiírás (kulcssorrend megtartva).
std::string dump(const Value& v);

}  // namespace totu::content
