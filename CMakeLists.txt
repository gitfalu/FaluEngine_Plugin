cmake_minimum_required(VERSION 3.25)
project(FaluEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(EngineHelpers)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE
        "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain")
endif()

option(ENGINE_BUILD_EDITOR  "Build the editor"      ON)
option(ENGINE_BUILD_TESTS   "Build unit tests"      ON)
option(ENGINE_BUILD_PLUGINS "Build example plugins" ON)


# ── コンパイルオプション ────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /MP /utf-8 /wd4100 /wd4505)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# WIN32_LEAN_AND_MEAN / NOMINMAX はヘッダー側で #ifndef ガードして定義するため
# ここではグローバル定義しない（再定義警告の原因になる）
# ENGINE_DEBUG だけ Debug ビルド時に定義する
add_compile_definitions($<$<CONFIG:Debug>:ENGINE_DEBUG>)

add_subdirectory(engine)
add_subdirectory(runtime)

if(ENGINE_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

if(ENGINE_BUILD_PLUGINS)
    add_subdirectory(plugins/PhysicsPlugin)
endif()

if(ENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()