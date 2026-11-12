// SPDX-License-Identifier: GPL-3.0-or-later
// A futtató rendszer és fordító azonosítása (naplóba, hibajelentésbe, replay-fejlécbe).
#pragma once

#include <string>

namespace totu::core {

const char* version();       // pl. "0.1.0"
const char* os_name();       // windows, linux, macos, freebsd, openbsd, ismeretlen
const char* cpu_arch();      // x86_64, arm64, ...
std::string compiler();      // fordító neve és verziója

}  // namespace totu::core
