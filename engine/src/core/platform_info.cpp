// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/platform_info.hpp"

namespace totu::core {

const char* version() { return TOTU_VERSION; }

const char* os_name() {
#if defined(_WIN32)
  return "windows";
#elif defined(__APPLE__)
  return "macos";
#elif defined(__linux__)
  return "linux";
#elif defined(__FreeBSD__)
  return "freebsd";
#elif defined(__OpenBSD__)
  return "openbsd";
#elif defined(__NetBSD__)
  return "netbsd";
#else
  return "ismeretlen";
#endif
}

const char* cpu_arch() {
#if defined(__x86_64__) || defined(_M_X64)
  return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
  return "arm64";
#elif defined(__i386__) || defined(_M_IX86)
  return "x86";
#else
  return "ismeretlen";
#endif
}

std::string compiler() {
#if defined(__clang__)
  return "clang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#elif defined(__GNUC__)
  return "gcc " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#elif defined(_MSC_VER)
  return "msvc " + std::to_string(_MSC_VER);
#else
  return "ismeretlen";
#endif
}

}  // namespace totu::core
