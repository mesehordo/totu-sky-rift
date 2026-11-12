// SPDX-License-Identifier: GPL-3.0-or-later
// Tizedes szám szövegéből fixpontos érték, csak egész aritmetikával (ADR-004, ADR-005): a JSON-adatok számai
// így minden gépen bitre azonosan töltődnek be, a platform lebegőpontos átalakítójától függetlenül.
// Forma: [-]egész[.tört][e|E[+|-]kitevő]. Legfeljebb 12 tizedesjegy számít (a további jegyek csonkolódnak);
// kerekítés a legközelebbire, félnél a nullától el. Hiba: üres, hibás forma vagy a Q16.16 tartományon kívül.
#pragma once

#include <optional>
#include <string_view>

#include "core/fixed.hpp"

namespace totu::core {

std::optional<Fixed> parse_fixed(std::string_view text);

}  // namespace totu::core
