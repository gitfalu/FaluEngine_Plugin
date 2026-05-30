cmake_minimum_required(VERSION 3.25)

# ── 外部ライブラリ ──────────────────────────────────────────────────────────
include(FetchContent)

set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    joltphysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.0.0
    SOURCE_SUBDIR Build
)

find_package(spdlog   CONFIG REQUIRED)
find_package(glm      CONFIG REQUIRED)
find_package(EnTT     CONFIG REQUIRED)  # ECS
find_package(sol2     CONFIG REQUIRED)  # Lua バインディング
find_package(Lua      CONFIG REQUIRED)
find_package(assimp   CONFIG REQUIRED)  # メッシュローダー
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)
find_package(DirectX  REQUIRED)         # DirectX 11 (カスタム Find モジュール)
find_package(nlohmann_json CONFIG REQUIRED)

FetchContent_MakeAvailable(joltphysics)

# ── Core モジュール ─────────────────────────────────────────────────────────
engine_add_module(core
    SOURCES
        core/Application.cpp
        core/GameLoop.cpp
        core/EventBus.cpp
        core/Logger.cpp
        core/Memory/PoolAllocator.cpp
        core/InputManager.cpp
    DEPS
        spdlog::spdlog
        glm::glm
        nlohmann_json::nlohmann_json
    INCLUDE_DIRS
        ${joltphysics_SOURCE_DIR}
)

# ── Scene モジュール ────────────────────────────────────────────────────────
engine_add_module(scene
    SOURCES
        scene/Scene.cpp
        scene/Entity.cpp
        scene/CameraController.cpp
        scene/SceneSerializer.cpp
    DEPS
        engine_core
        EnTT::EnTT
    INCLUDE_DIRS
        ${joltphysics_SOURCE_DIR}
)

# ── Renderer モジュール ─────────────────────────────────────────────────────
engine_add_module(renderer
    SOURCES
        renderer/RenderGraph.cpp
        renderer/dx11/DX11Renderer.cpp
        renderer/dx11/DX11Device.cpp
        renderer/dx11/ImGuiLayer.cpp
        renderer/dx11/RenderTexture.cpp
    DEPS
        engine_core
        engine_scene
        DirectX::D3D11
        glm::glm
        imgui::imgui
)

# ── Asset モジュール ────────────────────────────────────────────────────────
engine_add_module(asset
    SOURCES
        asset/AssetManager.cpp
        asset/loaders/MeshLoader.cpp
        asset/loaders/TextureLoader.cpp
       
    DEPS
        engine_core
        assimp::assimp
        Microsoft::DirectXTex
)

# ── Platform モジュール ─────────────────────────────────────────────────────
engine_add_module(platform
    SOURCES
        platform/Window.cpp
        platform/win32/Win32Platform.cpp
    DEPS
        engine_core
        DirectX::D3D11
)
# ── Physics モジュール ───────────────────────────────────────────────────────
engine_add_module(physics
    SOURCES
        physics/PhysicsSystem.cpp
    DEPS
        engine_core
        engine_scene
        Jolt
    INCLUDE_DIRS
        ${joltphysics_SOURCE_DIR}
)

target_include_directories(engine_physics PUBLIC
    ${joltphysics_SOURCE_DIR}
)

# ── Plugin モジュール ───────────────────────────────────────────────────────
engine_add_module(plugin
    SOURCES
        plugin/PluginManager.cpp
    DEPS
        engine_core
)

# ── Script モジュール ───────────────────────────────────────────────────────
engine_add_module(script
    SOURCES
        script/ScriptEngine.cpp
        script/ScriptInstance.cpp
    DEPS
        engine_core
        engine_scene
        sol2::sol2
        ${LUA_LIBRARIES}
)

target_include_directories(engine_script PUBLIC
    ${LUA_INCLUDE_DIR}
)

# ── 統合エイリアス FaluEngine::Engine ────────────────────────────────────────
# 各モジュールを個別にリンクしたくない場合に使う便利ターゲット
add_library(engine_all INTERFACE)
add_library(FaluEngine::Engine ALIAS engine_all)
target_link_libraries(engine_all INTERFACE
    engine_core
    engine_scene
    engine_renderer
    engine_asset
    engine_platform
    engine_plugin
    engine_script
    engine_physics
)
