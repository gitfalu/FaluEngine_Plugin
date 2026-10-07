add_library(FaluEngine SHARED)

# 公開API
add_library(FaluEngineInternal INTERFACE)
add_library(FaluEngine::Internal ALIAS FaluEngineInternal)

target_include_directories(FaluEngineInternal INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(FaluEngineInternal INTERFACE 
    FaluEngine::Engine
    Jolt
    sol2::sol2
    ${LUA_LIBRARIES}
    assimp::assimp
    imgui::imgui
    imguizmo::imguizmo
    Microsoft::DirectXTex
    DirectX::D3D11
    spdlog::spdlog
)

add_library(FaluEngine::Engine ALIAS FaluEngine)

target_sources(FaluEngine
    PRIVATE
        core/Application.cpp
        core/EventBus.cpp
        core/Logger.cpp
        core/InputManager.cpp
        core/EditorStateManager.cpp
        core/FileWatcher.cpp
        core/GameLoop.cpp
        core/Memory/PoolAllocator.cpp
        core/PathResolver.cpp
        core/Localization.cpp

        scene/Scene.cpp
        scene/Entity.cpp
        scene/Component.cpp
        scene/CameraController.cpp
        scene/SceneSerializer.cpp
        scene/SceneManager.cpp

        renderer/RenderGraph.cpp
        renderer/dx11/DX11Renderer.cpp
        renderer/dx11/DX11Device.cpp
        renderer/dx11/ImGuiLayer.cpp
        renderer/dx11/RenderTexture.cpp
        renderer/dx11/ShadowMap.cpp
        renderer/dx11/EnvironmentMap.cpp
        renderer/dx11/UI.cpp
        renderer/dx11/ScreenshotCapture.cpp

        asset/AssetManager.cpp
        asset/loaders/MeshLoader.cpp
        asset/loaders/TextureLoader.cpp
        asset/loaders/ShaderLoader.cpp
        asset/loaders/MaterialLoader.cpp
        asset/loaders/AnimationClip.cpp
        asset/loaders/AnimationCache.cpp

        platform/Window.cpp
        platform/win32/Win32Platform.cpp

        physics/PhysicsSystem.cpp
        
        audio/AudioClip.cpp
        audio/AudioEngine.cpp
        audio/Audio.cpp

        script/ScriptEngine.cpp
        script/ScriptInstance.cpp
        script/NativeScriptRegistry.cpp

        plugin/PluginManager.cpp
        plugin/HotReloadManager.cpp

        replay/ReplayRecorder.cpp

        animation/ActionEventTrack.cpp
        animation/ActionEventLibrary.cpp
        animation/ActionEventSystem.cpp

        terrain/TerrainGenerator.cpp
        terrain/TerrainFactory.cpp
        
        llm/Claudeapiclient.cpp
        llm/FrameContextBuilder.cpp

        tools/TerrainSculptTool.cpp
)

get_target_property(FALU_ENGINE_SOURCES FaluEngine SOURCES)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${FALU_ENGINE_SOURCES})

target_include_directories(FaluEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${joltphysics_SOURCE_DIR}
)

target_link_libraries(FaluEngine
    PUBLIC
        falu_compiler_options
        glm::glm
        EnTT::EnTT
    PRIVATE
        spdlog::spdlog
        sol2::sol2
        ${LUA_LIBRARIES}
        assimp::assimp
        imgui::imgui
        imguizmo::imguizmo
        Microsoft::DirectXTex
        DirectX::D3D11
        Jolt
        nlohmann_json::nlohmann_json
        d3dcompiler.lib
        dxgi.lib
        xaudio2.lib
        winhttp.lib
)

target_compile_definitions(FaluEngine
    PRIVATE
        FALU_ENGINE_BUILD_DLL
    INTERFACE
        FALU_ENGINE_USE_DLL
)

set_target_properties(FaluEngine PROPERTIES
    OUTPUT_NAME "FaluEngine"
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
    FOLDER "Engine"
)