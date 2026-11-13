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

# --- Lua 5.4.8 (MIT, változatlan forrás) egy C++ fordítási egységként; a determinizmus-makrók a lua_build.cpp-ben ---
add_library(totu_lua STATIC src/script/lua_build.cpp)
target_include_directories(totu_lua PUBLIC third_party/lua)
if(MSVC)
  target_compile_options(totu_lua PRIVATE /W0 /utf-8 /fp:precise /EHsc)
else()
  target_compile_options(totu_lua PRIVATE -w -ffp-contract=off)
endif()

# --- mag: grafika nélküli, minden rendszeren tesztelt könyvtár --------------------------------------------------
add_library(totu_core
  src/core/platform_info.cpp
  src/core/hash.cpp
  src/core/fixed.cpp
  src/core/angle.cpp
  src/core/decimal.cpp
  src/content/json.cpp
  src/content/vfs.cpp
  src/content/templates.cpp
  src/content/schema.cpp
  src/content/loader.cpp
  src/sim/sim.cpp src/sim/rules.cpp src/sim/economy.cpp src/sim/build.cpp src/sim/production.cpp src/sim/combat.cpp src/sim/vision.cpp src/sim/environment.cpp src/sim/abilities.cpp src/sim/garrison.cpp src/sim/heroes.cpp
  src/sim/replay.cpp
  src/sim/navmove.cpp
  src/sim/formation.cpp
  src/script/host.cpp
  src/map/terrain.cpp
  src/map/map.cpp
  src/map/navgrid.cpp
  src/map/pathfinder.cpp)
target_include_directories(totu_core PUBLIC src)
target_link_libraries(totu_core PRIVATE totu_options PUBLIC totu_lua)
target_compile_definitions(totu_core PUBLIC TOTU_VERSION="${PROJECT_VERSION}")

# --- képek (PNG, stb_image – MIT / közkincs, változatlan): grafikus kártya nélkül is (tesztek, atlaszkészítő) ---
add_library(totu_stb STATIC src/render/stb_impl.cpp)
target_include_directories(totu_stb SYSTEM PUBLIC third_party/stb)
if(MSVC)
  target_compile_options(totu_stb PRIVATE /W0 /utf-8)
else()
  target_compile_options(totu_stb PRIVATE -w)
endif()
add_library(totu_image STATIC src/render/image.cpp src/render/font.cpp)
target_include_directories(totu_image PUBLIC src)
target_link_libraries(totu_image PUBLIC totu_stb PRIVATE totu_options)
# atlaszkészítő (grafikus kártya nélkül) és eszköze
add_library(totu_assets STATIC src/render/atlas_build.cpp src/render/calm.cpp src/render/daylight.cpp src/render/particles.cpp
            src/render/weather_look.cpp)
target_link_libraries(totu_assets PUBLIC totu_core totu_image PRIVATE totu_options)
add_executable(totu_atlas src/tools/totu_atlas.cpp)
target_link_libraries(totu_atlas PRIVATE totu_assets totu_options)
add_executable(totu_imgdiff src/tools/totu_imgdiff.cpp)
target_link_libraries(totu_imgdiff PRIVATE totu_image totu_options)

# --- tesztek -----------------------------------------------------------------------------------------------------
enable_testing()
add_executable(totu_tests tests/main.cpp tests/test_core.cpp tests/test_math.cpp tests/test_content.cpp tests/test_loader.cpp tests/test_sim.cpp
  tests/test_script.cpp tests/test_map.cpp tests/test_path.cpp tests/test_nav.cpp
  tests/test_form.cpp tests/test_image.cpp tests/test_atlas.cpp tests/test_daylight.cpp
  tests/test_weather.cpp tests/test_session.cpp tests/test_economy.cpp tests/test_build.cpp tests/test_production.cpp tests/test_combat.cpp tests/test_vision.cpp tests/test_environment.cpp tests/test_abilities.cpp tests/test_garrison.cpp tests/test_ursan.cpp tests/test_match.cpp tests/test_elements.cpp tests/test_heroes.cpp src/client/session.cpp)
target_compile_definitions(totu_tests PRIVATE TOTU_CONTENT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/content"
                                              TOTU_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
                                              TOTU_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets")
find_package(Threads REQUIRED)   # a szimulációs szál (M6) – a kliens és a tesztek
target_link_libraries(totu_tests PRIVATE totu_core totu_image totu_assets totu_options Threads::Threads)
add_test(NAME core COMMAND totu_tests)

# --- ablak nélküli szimulációs futtató és az M2 kapu: a golden visszajátszás minden rendszeren egyezik -----------
add_executable(totu_sim src/tools/totu_sim.cpp src/tools/nav_tools.cpp)
target_link_libraries(totu_sim PRIVATE totu_core totu_options)
target_compile_definitions(totu_sim PRIVATE TOTU_CONTENT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/content")
add_test(NAME golden_replay COMMAND totu_sim verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/golden_m2.trep)
# M3 kapu: a szkriptes golden visszajátszás (Lua-küldetés + csata) minden rendszeren egyezik
add_test(NAME golden_replay_m3 COMMAND totu_sim verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/golden_m3.trep)
# M4 kapu: a pályás golden visszajátszás (alakzatok, csata, kapu, építés a NAV-BRIDGE pályán) minden rendszeren egyezik
add_test(NAME golden_replay_m4 COMMAND totu_sim verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/golden_m4.trep
         --content ${CMAKE_CURRENT_SOURCE_DIR}/content)
# M6 kapu: két szkriptelt ursan gép 12 perces meccse (gazdaság, építés, termelés, kutatás, harc, időjárás) mindenhol egyezik
add_test(NAME golden_replay_m6 COMMAND totu_sim verify ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/golden_m6.trep
         --content ${CMAKE_CURRENT_SOURCE_DIR}/content)

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
  # megjelenítő (ADR-003): saját vékony réteg, OpenGL 3.3 core
  add_library(totu_render STATIC src/render/gl.cpp src/render/rhi.cpp src/render/sprite_batch.cpp src/render/sprite_sheet.cpp
              src/render/atlas.cpp src/render/terrain_view.cpp src/render/lighting.cpp src/render/effects.cpp)
  target_link_libraries(totu_render PUBLIC totu_core totu_image totu_assets PRIVATE totu_options)
  add_executable(totu src/client/main.cpp src/client/world.cpp src/client/session.cpp src/client/hud.cpp)
  target_link_libraries(totu PRIVATE totu_core totu_render totu_options SDL3::SDL3 Threads::Threads)
  target_compile_definitions(totu PRIVATE TOTU_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets"
                                          TOTU_CONTENT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/content")
  add_test(NAME client_version COMMAND totu --version)
endif()
