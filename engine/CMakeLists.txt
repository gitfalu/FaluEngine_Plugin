# 公開API
add_library(FaluEngineInternal INTERFACE)
add_library(FaluEngine::Internal ALIAS FaluEngineInternal)
include(EngineHelpers)

add_library(falu_module_base INTERFACE)
target_link_libraries(falu_module_base INTERFACE
    falu_compiler_options
    glm::glm
    EnTT::EnTT
    spdlog::spdlog
    sol2::sol2
)

target_include_directories(falu_module_base INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${joltphysics_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

function(falu_add_module_headers  target)
    cmake_parse_arguments(H "" "" "DIRS;FILES;EXCLUDE" ${ARGN})
    set(_hdrs "")
    foreach(_d ${H_DIRS})
        file(GLOB _g CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/${_d}/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/${_d}/*.hpp")
        list(APPEND _hdrs ${_g})
    endforeach()
    foreach(_f ${H_FILES})
        list(APPEND _hdrs "${CMAKE_CURRENT_SOURCE_DIR}/${_f}")
    endforeach()
    foreach(_e ${H_EXCLUDE})
        list(REMOVE_ITEM _hdrs "${CMAKE_CURRENT_SOURCE_DIR}/${_e}")
    endforeach()
    target_sources(${target} PRIVATE ${_hdrs})
    get_target_property(_all ${target} SOURCES)
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${_all})
endfunction()

# DLL モジュールを一つ作る
function(falu_add_engine_module NAME)
    cmake_parse_arguments(M "" "EXPORT" "SOURCES;PUBLIC_DEPS;PRIVATE_DEPS" ${ARGN})
    add_library(${NAME} SHARED ${M_SOURCES})
    target_link_libraries(${NAME}
        PUBLIC falu_module_base ${M_PUBLIC_DEPS}
        PRIVATE ${M_PRIVATE_DEPS}
    )
    target_compile_definitions(${NAME} PRIVATE ${M_EXPORT})
    set_target_properties(${NAME} PROPERTIES
        OUTPUT_NAME "${NAME}"
        WINDOWS_EXPORT_ALL_SYMBOLS OFF
        FOLDER "Engine"
    )
endfunction()

# FaluCore
falu_add_engine_module(FaluCore
    EXPORT FALU_CORE_BUILD_DLL
    SOURCES 
        asset/AssetManager.cpp
        
        core/EventBus.cpp
        core/FileWatcher.cpp
        core/InputManager.cpp
        core/Localization.cpp
        core/Logger.cpp

        core/Memory/PoolAllocator.cpp

        core/PathResolver.cpp

        plugin/HotReloadManager.cpp
        plugin/PluginManager.cpp

        script/NativeScriptRegistry.cpp
)
falu_add_module_headers(FaluCore
    DIRS    core core/Memory plugin
    FILES   asset/AssetManager.h
            include/FaluEngine/EventBus.h
            include/FaluEngine/Input.h
            include/FaluEngine/InputManager.h
            include/FaluEngine/Localization.h
            include/FaluEngine/EngineExport.h
            include/FaluEngine/IPlugin.h
            include/FaluEngine/NativeScript.h
            include/FaluEngine/NativeScriptRegistry.h
    EXCLUDE core/EditorStateManager.h core/GameLoop.h
)

# FaluAudio
falu_add_engine_module(FaluAudio
    EXPORT FALU_AUDIO_BUILD_DLL
    SOURCES
        audio/Audio.cpp
        audio/AudioClip.cpp
        audio/AudioEngine.cpp
    PUBLIC_DEPS FaluCore
    PRIVATE_DEPS xaudio2.lib
)
falu_add_module_headers(FaluAudio
    DIRS    audio
    FILES   include/FaluEngine/Audio.h
            include/FaluEngine/AudioTypes.h
)

# FaluGraphics
falu_add_engine_module(FaluGraphics
    EXPORT FALU_GRAPHICS_BUILD_DLL
    SOURCES
        asset/loaders/AnimationCache.cpp
        asset/loaders/AnimationClip.cpp
        asset/loaders/MaterialLoader.cpp
        asset/loaders/MeshLoader.cpp
        asset/loaders/ShaderLoader.cpp
        asset/loaders/TextureLoader.cpp

        platform/Window.cpp

        platform/win32/Win32Platform.cpp

        renderer/RenderGraph.cpp

        renderer/dx11/DX11Device.cpp
        renderer/dx11/DX11Renderer.cpp
        renderer/dx11/EnvironmentMap.cpp
        renderer/dx11/ImGuiLayer.cpp
        renderer/dx11/RenderTexture.cpp
        renderer/dx11/ScreenshotCapture.cpp
        renderer/dx11/ShadowMap.cpp
        renderer/dx11/UI.cpp
    PUBLIC_DEPS FaluCore DirectX::D3D11
    PRIVATE_DEPS
        assimp::assimp
        imgui::imgui
        Microsoft::DirectXTex
        nlohmann_json::nlohmann_json
        d3dcompiler.lib
        dxgi.lib
)
falu_add_module_headers(FaluGraphics
    DIRS    renderer renderer/dx11 asset/loaders platform platform/win32
    FILES   include/FaluEngine/ImGuiLayer.h
            include/FaluEngine/LightTypes.h
            include/FaluEngine/UI.h
            include/FaluEngine/UITypes.h
            include/FaluEngine/Camera.h
    EXCLUDE renderer/dx11/FocusMapPass.h
)

# FaluScene
falu_add_engine_module(FaluScene
    EXPORT FALU_SCENE_BUILD_DLL
    SOURCES
        animation/ActionEventLibrary.cpp
        animation/ActionEventSystem.cpp
        animation/ActionEventTrack.cpp

        physics/PhysicsSystem.cpp

        replay/ReplayRecorder.cpp

        scene/CameraController.cpp
        scene/Component.cpp
        scene/Entity.cpp
        scene/Scene.cpp
        scene/SceneManager.cpp
        scene/SceneSerializer.cpp

        script/ScriptEngine.cpp
        script/ScriptInstance.cpp

        terrain/TerrainFactory.cpp
        terrain/TerrainGenerator.cpp

        tools/TerrainSculptTool.cpp

    PUBLIC_DEPS FaluCore FaluAudio FaluGraphics
    PRIVATE_DEPS
        Jolt
        ${LUA_LIBRARIES}
        nlohmann_json::nlohmann_json
)
falu_add_module_headers(FaluScene
    DIRS    scene physics script terrain animation replay tools
    FILES   include/FaluEngine/Scene.h
            include/FaluEngine/Entity.h
            include/FaluEngine/Component.h
            include/FaluEngine/ReplayRecorder.h
            include/FaluEngine/ActionEventLibrary.h
            include/FaluEngine/ActionEventSystem.h
            include/FaluEngine/ActionEventTrack.h
)

# FaluEngine
falu_add_engine_module(FaluEngine
    EXPORT FALU_ENGINE_BUILD_DLL
    SOURCES
        core/Application.cpp
        core/EditorStateManager.cpp
        core/GameLoop.cpp

        llm/Claudeapiclient.cpp
        llm/FrameContextBuilder.cpp

        renderer/dx11/FocusMapPass.cpp
    PUBLIC_DEPS FaluCore FaluAudio FaluGraphics FaluScene
    PRIVATE_DEPS
        Jolt
        nlohmann_json::nlohmann_json
        d3dcompiler.lib
        dxgi.lib
        winhttp.lib
)
falu_add_module_headers(FaluEngine
    DIRS    llm
    FILES   core/EditorStateManager.h
            core/GameLoop.h
            renderer/dx11/FocusMapPass.h
            include/FaluEngine/Application.h
            include/FaluEngine/Llmclient.h
            include/FaluEngine/FocusTypes.h
)

# 従来のターゲット
add_library(FaluEngine::Engine ALIAS FaluEngine)
add_library(FaluEngine::Core        ALIAS FaluCore)
add_library(FaluEngine::Audio       ALIAS FaluAudio)
add_library(FaluEngine::Graphics    ALIAS FaluGraphics)
add_library(FaluEngine::Scene       ALIAS FaluScene)

# 公開API
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