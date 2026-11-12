// SPDX-License-Identifier: GPL-3.0-or-later
// JSON-séma ellenőrző (ADR-005) – a JSON Schema egy részhalmaza, ami a tartalomhoz kell:
//   type (szöveg vagy lista: object, array, string, number, integer, boolean, null), properties, required,
//   additionalProperties (true/false vagy séma), items, enum, minimum, maximum, $ref ("#/$defs/név").
// A számhatárok egész aritmetikával (fixpontos összevetés) ellenőrzöttek, determinisztikusan.
// A `_` kezdetű kulcsok (megjegyzések) minden objektumban megengedettek.
#pragma once

#include <string>
#include <vector>

#include "content/json.hpp"

namespace totu::content {

class Schema {
 public:
  explicit Schema(Value root) : root_(std::move(root)) {}
  // Hibák listája „útvonal: üzenet” formában; üres = érvényes.
  std::vector<std::string> validate(const Value& doc) const;

 private:
  void check(const Value& schema, const Value& v, const std::string& path, std::vector<std::string>& errs,
             int depth) const;
  Value root_;
};

}  // namespace totu::content
