# SPDX-License-Identifier: GPL-3.0-or-later
# TOTU: Sky Rift – az új motor (docs/alap/UJ_MOTOR_TERV.md, M1 platform-váz).
cmake_minimum_required(VERSION 3.24)
project(totu VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(TOTU_CLIENT "A grafikus kliens fordítása (SDL3)" ON)
option(TOTU_SYSTEM_SDL "A rendszer SDL3-ját használja, ha van" ON)
option(TOTU_WERROR "Fordítói figyelmeztetés = hiba" ON)

# Közös fordítói beállítások: szigorú figyelmeztetések, és a determinizmus miatt nincs lebegőpontos összevonás (FMA).
add_library(totu_options INTERFACE)
if(MSVC)
  target_compile_options(totu_options INTERFACE /W4 /permissive- /utf-8 /fp:precise)
  if(TOTU_WERROR)
    target_compile_options(totu_options INTERFACE /WX)
  endif()
else()
  target_compile_options(totu_options INTERFACE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -ffp-contract=off)
  if(TOTU_WERROR)
    target_compile_options(totu_options INTERFACE -Werror)
  endif()
endif()

# --- mag: grafika nélküli, minden rendszeren tesztelt könyvtár --------------------------------------------------
add_library(totu_core
  src/core/platform_info.cpp
  src/core/hash.cpp
  src/core/fixed.cpp
  src/core/angle.cpp
  src/core/decimal.cpp)
target_include_directories(totu_core PUBLIC src)
target_link_libraries(totu_core PRIVATE totu_options)
target_compile_definitions(totu_core PUBLIC TOTU_VERSION="${PROJECT_VERSION}")

# --- tesztek -----------------------------------------------------------------------------------------------------
enable_testing()
add_executable(totu_tests tests/main.cpp tests/test_core.cpp tests/test_math.cpp)
target_link_libraries(totu_tests PRIVATE totu_core totu_options)
add_test(NAME core COMMAND totu_tests)

# --- kliens (SDL3 + OpenGL 3.3 core) ------------------------------------------------------------------------------
if(TOTU_CLIENT)
  if(TOTU_SYSTEM_SDL)
    find_package(SDL3 CONFIG QUIET)
  endif()
  if(NOT SDL3_FOUND)
    # Rögzített SDL3-kiadás forrásból (zlib-licenc). A kiadási csomag a forrást a repóba vendorolja majd (ADR-003).
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-3.4.20
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
  endif()
  add_executable(totu src/client/main.cpp)
  target_link_libraries(totu PRIVATE totu_core totu_options SDL3::SDL3)
  add_test(NAME client_version COMMAND totu --version)
endif()
