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

if(WIN32)
    set(PLATFORM "windows")
elseif(APPLE)
    set(PLATFORM "macos")
else()
    set(PLATFORM "linux")
endif()

set(ARCH "x86_64")

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

# Build Game-Framework's Core the same way — a real, ordinary static-library
# dependency (see that repo's own README for why Subsystem/SubsystemManager
# are plain C++, not GDCLASS types: cross-extension GDExtension class
# inheritance is unsupported by Godot's ClassDB). This addon is the ONLY
# GDCLASS-registered bridge into it (SubsystemManagerBridge) — every other
# Godot gem that wants to define real Subsystem subclasses links against
# Game-Framework's Core directly too, the same way this one does, not
# through this addon's Godot-facing surface.
set(GAMEFRAMEWORK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${GAMEFRAMEWORK_CORE_PATH} ${CMAKE_BINARY_DIR}/gameframework-core)

file(GLOB_RECURSE SOURCES "src/private/*.cpp")

add_library(FoundationGameFramework SHARED ${SOURCES})

target_include_directories(FoundationGameFramework PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/public
)

target_link_libraries(FoundationGameFramework PRIVATE godot::cpp gameframework::gameframework)

set_target_properties(FoundationGameFramework
    PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    OUTPUT_NAME "libFoundationGameFramework.${PLATFORM}.${TYPE_STRING}.${ARCH}"
)

if(NOT WIN32)
    target_link_options(FoundationGameFramework PRIVATE "-rdynamic")
endif()
