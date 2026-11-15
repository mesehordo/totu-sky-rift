// SPDX-License-Identifier: GPL-3.0-or-later
// Modlock (ADR-009): a tartalom ujjlenyomata. A tartalommappa (és a modok) minden fájlja relatív útvonal szerint
// rendezve, útvonallal és tartalommal együtt; sorvég-független (a CRLF LF-ként számít), így a Windows- és a
// Linux-kicsomagolás ugyanazt adja.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace totu::net {

uint64_t content_hash(const std::vector<std::string>& dirs);

}  // namespace totu::net
