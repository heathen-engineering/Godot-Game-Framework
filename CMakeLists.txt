cmake_minimum_required(VERSION 3.16)
project(FoundationGameFramework)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Defaults to the godot-cpp git submodule vendored directly at the repo root
# (<repo>/godot-cpp — this CMakeLists.txt lives at the repo root too, unlike
# the nested addons/<Name>/CMakeLists.txt layout other gems use). Override
# with -DGODOT_CPP_PATH=... to build against a different checkout.
set(GODOT_CPP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/godot-cpp" CACHE PATH "Path to Godot-cpp bindings")

# Defaults to the Game-Framework git submodule vendored at <repo>/extern/Game-Framework —
# the engine-agnostic core this whole addon is a thin wrap around. Override with
# -DGAMEFRAMEWORK_CORE_PATH=... to build against a different checkout.
set(GAMEFRAMEWORK_CORE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/extern/Game-Framework/Core" CACHE PATH "Path to Game-Framework's Core (engine-agnostic C++)")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" TYPE_STRING)

if(ANDROID)
    set(PLATFORM "android")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(PLATFORM "ios")
elseif(WIN32)
    set(PLATFORM "windows")
elseif(APPLE)
    set(PLATFORM "macos")
else()
    set(PLATFORM "linux")
endif()

if(ANDROID)
    # Godot's own arch names: arm64-v8a -> arm64, armeabi-v7a -> arm32.
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(ARCH "arm64")
    elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(ARCH "arm32")
    else()
        set(ARCH "${ANDROID_ABI}")
    endif()
elseif(PLATFORM STREQUAL "ios")
    set(ARCH "arm64")
else()
    set(ARCH "x86_64")
endif()

message(STATUS "Building: ${CMAKE_BUILD_TYPE} on ${PLATFORM} ${ARCH}")
message(STATUS "GODOT_CPP_PATH: ${GODOT_CPP_PATH}")
message(STATUS "GAMEFRAMEWORK_CORE_PATH: ${GAMEFRAMEWORK_CORE_PATH}")

if(NOT EXISTS "${GODOT_CPP_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "godot-cpp not found at ${GODOT_CPP_PATH}. Did you run 'git submodule update --init --recursive'?")
endif()
if(NOT EXISTS "${GAMEFRAMEWORK_CORE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "Game-Framework Core not found at ${GAMEFRAMEWORK_CORE_PATH}. Did you run 'git submodule update --init --recursive'?")
endif()

# Build godot-cpp as part of this build (no separate SCons pre-build step needed).
add_subdirectory(${GODOT_CPP_PATH} ${CMAKE_BINARY_DIR}/godot-cpp)

# Build Game-Framework's Core. Every platform except iOS links the SHARED
# target and ships it as a real runtime artifact (libgameframework.*, right
# alongside FoundationGameFramework.* below) — NOT the static target. This
# matters: GameplayTags/Lexicon/Ogham/every future gem that defines real
# Subsystem subclasses is a SEPARATE GDExtension .so, and Subsystem/
# SubsystemManager instances must be shared process-wide (one
# SubsystemManager::instance() everyone registers with, so the dock sees
# every gem's subsystems and cross-gem depends_on() ordering works). Static
# linking would give each .so its own private, disconnected copy of that
# singleton state — silently wrong, not merely suboptimal. Every consumer
# gem links against its OWN build of gameframework::shared (for the
# compile-time import stub) but declares a RUNTIME dependency, via its own
# .gdextension [dependencies] section, on THIS repo's shipped copy at
# res://addons/FoundationGameFramework/bin/libgameframework.* — Godot's
# loader resolves that path once, so every gem's .so ends up linked against
# the same loaded module. See this repo's README for the full explanation.
#
# iOS is the one exception: GDExtensions must be statically linked there
# (Godot disallows dynamic loading on iOS), so cross-gem Subsystem sharing
# doesn't apply on iOS the same way — this addon links the STATIC target
# there instead, same as every other gem does for its own iOS build.
set(GAMEFRAMEWORK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${GAMEFRAMEWORK_CORE_PATH} ${CMAKE_BINARY_DIR}/gameframework-core)

file(GLOB_RECURSE SOURCES "src/private/*.cpp")

# iOS does not allow dynamically-loaded GDExtensions — Godot requires them
# statically linked into the exported binary there (unlike every other
# platform this addon targets). Everywhere else this is an ordinary shared
# library dropped straight into bin/ per the .gdextension library table.
if(PLATFORM STREQUAL "ios")
    add_library(FoundationGameFramework STATIC ${SOURCES})
else()
    add_library(FoundationGameFramework SHARED ${SOURCES})
endif()

target_include_directories(FoundationGameFramework PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/public
)

if(PLATFORM STREQUAL "ios")
    target_link_libraries(FoundationGameFramework PRIVATE godot::cpp gameframework::gameframework)
else()
    target_link_libraries(FoundationGameFramework PRIVATE godot::cpp gameframework::shared)
endif()

set_target_properties(FoundationGameFramework
    PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    # ARCHIVE_OUTPUT_DIRECTORY is what governs STATIC lib placement (iOS) —
    # LIBRARY_OUTPUT_DIRECTORY alone only covers SHARED/MODULE targets, so
    # without this the iOS .a would land wherever the Xcode generator
    # defaults to instead of bin/.
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    OUTPUT_NAME "libFoundationGameFramework.${PLATFORM}.${TYPE_STRING}.${ARCH}"
)

# Without this, CMake bakes an ABSOLUTE build-tree RUNPATH pointing at this
# machine's build directory (confirmed with readelf — it silently works
# here and would silently fail on every other machine, CI artifact, or
# install location, since that exact path only exists on the box that
# built it). $ORIGIN (Linux/Android) / @loader_path (macOS) makes the
# shared gameframework runtime resolve relative to wherever this .so
# itself ends up on disk — same bin/ folder, wherever that is. No RPATH
# concept on Windows (DLL search order already checks the loading DLL's
# own directory by default); nothing to set there. iOS is static, moot.
if(PLATFORM STREQUAL "linux" OR PLATFORM STREQUAL "android")
    set_target_properties(FoundationGameFramework PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN"
    )
elseif(PLATFORM STREQUAL "macos")
    set_target_properties(FoundationGameFramework PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "@loader_path"
    )
endif()

if(NOT WIN32 AND NOT PLATFORM STREQUAL "ios")
    target_link_options(FoundationGameFramework PRIVATE "-rdynamic")
endif()

# Ship the shared gameframework runtime itself as a real artifact in bin/,
# right alongside FoundationGameFramework.* — this is the canonical copy
# every other gem's .gdextension [dependencies] section points at (see the
# big comment above). Not shipped at all on iOS, where everything is static.
if(NOT PLATFORM STREQUAL "ios")
    set_target_properties(gameframework_shared
        PROPERTIES
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
        OUTPUT_NAME "libgameframework.${PLATFORM}.${TYPE_STRING}.${ARCH}"
    )
endif()
