include_guard(GLOBAL)

include(FetchContent)

# 全DLLで同じ動的MSVC Runtimeを使う。
# プラグインとエンジンのメモリ管理ABIを合わせるため重要。
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)

find_package(spdlog CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(Lua REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(DirectX REQUIRED)

FetchContent_Declare(
    joltphysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.0.0
    SOURCE_SUBDIR Build
)

FetchContent_MakeAvailable(joltphysics)