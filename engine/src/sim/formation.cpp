// SPDX-License-Identifier: GPL-3.0-or-later
#include "sim/formation.hpp"

#include <algorithm>

namespace totu::sim {

namespace {

const char* const kNames[kFormationCount] = {"none", "auto", "line", "box", "spread", "flank"};

int isqrt_ceil(int n) {
  int r = 0;
  while (r * r < n) ++r;
  return r;
}

// sorokra osztás: a sor szélessége `width`, az utolsó sor középre igazítva
void rows_of(std::vector<Slot>& out, int n, int width, Fixed spacing, Fixed lateral_shift, int first_row) {
  width = std::max(1, width);
  for (int i = 0, row = 0; i < n; ++row) {
    const int k = std::min(width, n - i);
    for (int j = 0; j < k; ++j, ++i) {
      Slot s;
      // (2j - (k-1)) / 2 × távolság: középre igazítva
      s.lateral = spacing * Fixed::from_int(2 * j - (k - 1)) / Fixed::from_int(2) + lateral_shift;
      s.depth = spacing * Fixed::from_int(row);
      s.row = first_row + row;
      out.push_back(s);
    }
  }
}

}  // namespace

FormationSet::FormationSet() {
  spec[static_cast<size_t>(Formation::None)].together = false;
  spec[static_cast<size_t>(Formation::Spread)].spacing = Fixed::from_int(2);
}

Formation formation_from_name(const std::string& name, bool* ok) {
  for (int i = 0; i < kFormationCount; ++i) {
    if (name == kNames[i]) {
      if (ok) *ok = true;
      return static_cast<Formation>(i);
    }
  }
  if (ok) *ok = false;
  return Formation::Auto;
}

FormationSet FormationSet::from_json(const content::Value* doc, std::vector<std::string>& errors) {
  FormationSet set;
  if (!doc || !doc->is_object()) return set;
  for (const auto& [name, v] : doc->members()) {
    if (name.empty() || name[0] == '_') continue;
    bool ok = false;
    const Formation f = formation_from_name(name, &ok);
    if (!ok || !v.is_object()) {
      errors.push_back("formations." + name + ": ismeretlen alakzat");
      continue;
    }
    FormationSpec& s = set.spec[static_cast<size_t>(f)];
    if (const content::Value* x = v.find("spacing")) {
      const auto fx = x->as_fixed();
      if (!fx || fx->raw() <= 0) errors.push_back("formations." + name + ".spacing: pozitív szám");
      else s.spacing = *fx;
    }
    if (const content::Value* x = v.find("max_row")) {
      const long long n = x->as_int().value_or(0);
      if (n < 1 || n > 200) errors.push_back("formations." + name + ".max_row: 1 és 200 közötti egész");
      else s.max_row = static_cast<int>(n);
    }
    if (const content::Value* x = v.find("gap")) {
      const auto fx = x->as_fixed();
      if (!fx || fx->raw() < 0) errors.push_back("formations." + name + ".gap: nem negatív szám");
      else s.gap = *fx;
    }
    if (const content::Value* x = v.find("together"); x && x->is_bool()) s.together = x->as_bool();
  }
  return set;
}

std::vector<Slot> formation_slots(Formation f, int n, const FormationSpec& spec, Fixed min_spacing) {
  std::vector<Slot> out;
  if (n <= 0) return out;
  const Fixed sp = std::max(spec.spacing, min_spacing);
  switch (f) {
    case Formation::Line:   // egy (vagy több) hosszú sor
      rows_of(out, n, std::min(n, spec.max_row), sp, Fixed{}, 0);
      break;
    case Formation::Box:    // négyzet
    case Formation::Spread:
      rows_of(out, n, std::min(isqrt_ceil(n), spec.max_row), sp, Fixed{}, 0);
      break;
    case Formation::Flank: {   // két szárny, köztük rés
      const int left = (n + 1) / 2, right = n - left;
      const int w = std::min(isqrt_ceil(left), spec.max_row);
      const Fixed half_w = sp * Fixed::from_int(w - 1) / Fixed::from_int(2);
      const Fixed shift = half_w + spec.gap / Fixed::from_int(2) + sp / Fixed::from_int(2);
      rows_of(out, left, w, sp, Fixed{} - shift, 0);
      rows_of(out, right, w, sp, shift, 0);
      // sorrend: elölről hátra, balról jobbra
      std::stable_sort(out.begin(), out.end(), [](const Slot& a, const Slot& b) {
        return a.row != b.row ? a.row < b.row : a.lateral < b.lateral;
      });
      break;
    }
    case Formation::None:
    case Formation::Auto:
    default:   // széles, néhány soros (WC3-szerű): kb. 2 : 1 arány
      rows_of(out, n, std::min(isqrt_ceil(2 * n), spec.max_row), sp, Fixed{}, 0);
      break;
  }
  return out;
}

}  // namespace totu::sim
