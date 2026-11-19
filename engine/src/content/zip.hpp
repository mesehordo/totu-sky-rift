// SPDX-License-Identifier: GPL-3.0-or-later
// ZIP-be csomagolt modok (M13): a ZIP-et a játék egy gyorsítótár-mappába bontja ki, és onnan mappaként kezeli (így a
// betöltő, a fordítás és a modlock-ujjlenyomat változatlan). Csak a tárolt (0) és a deflate (8) tömörítés; a
// titkosított és a ZIP64-es csomag, valamint a `..`-t vagy abszolút utat tartalmazó bejegyzés hiba. Ha minden
// bejegyzés egyetlen gyökérmappában van (pl. `sajat_mod/mod.json`), az a mappa a mod gyökere.
#pragma once

#include <filesystem>
#include <string>

namespace totu::content {

// A ZIP kibontása a `dest` mappába (előbb törli). Hibánál false és rövid ok.
bool extract_zip(const std::filesystem::path& zip, const std::filesystem::path& dest, std::string* error);

// A ZIP-mod gyorsítótára: ha a `cache_dir/<a ZIP neve>` a ZIP mostani változatából készült (méret és módosítási idő),
// nem bont ki újra. A kibontott mod gyökerét adja (üres: hiba).
std::filesystem::path unpack_zip_mod(const std::filesystem::path& zip, const std::filesystem::path& cache_dir, std::string* error);

}  // namespace totu::content
