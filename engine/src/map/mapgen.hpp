// SPDX-License-Identifier: GPL-3.0-or-later
// A régi csatapálya-generátor (rts/mapgen.py `generate`) az új motorban: a pályafájl `generator` leírásából
// (méret, elrendezés, domborzati elemek, díszletek, háttérszereplők, terepjelek, lelőhely-mennyiségek) új maggal
// új, szimmetrikus pályát készít – a régi „minden indításkor új pálya”. A kezdőhelyek helye az elrendezésből adódik,
// így a maggal nem változik. Csak a meccs előtt fut (a kész pálya a visszajátszásba kerül), ezért a lebegőpontos
// számítás gépenkénti eltérése nem gond.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "content/json.hpp"

namespace totu::map {

// `doc`: egy kész pályafájl `generator` mezővel; az eredmény ugyanaz a dokumentum új terepppel, fákkal, lelőhelyekkel,
// díszletekkel és háttérszereplőkkel (a név, a leírás, a játékosszám, az éghajlat és a táborok maradnak).
// Üres, ha nincs `generator`, vagy hibás.
std::optional<content::Value> generate_map(const content::Value& doc, uint64_t seed, std::string* error = nullptr);

}  // namespace totu::map
