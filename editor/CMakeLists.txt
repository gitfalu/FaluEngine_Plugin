include(EngineHelpers)

add_executable(FaluEngineEditor
    main.cpp
    panels/HierarchyPanel.cpp
    panels/InspectorPanel.cpp
    panels/SceneViewPanel.cpp
    panels/GameViewPanel.cpp
    panels/ContentBrowserPanel.cpp
    panels/LLMDebugPanel.cpp
    panels/AnimationEventPanel.cpp
    EditorSettings.cpp
    EditorLocalization.cpp
)

target_link_libraries(FaluEngineEditor 
    PRIVATE
        FaluEngine::Internal
)

target_compile_definitions(FaluEngineEditor PRIVATE
    FALU_CMAKE_EXE="${CMAKE_COMMAND}"
    FALU_BUILD_DIR="${CMAKE_BINARY_DIR}"
    FALU_BUILD_CONFIG="$<CONFIG>"
    FALU_GAME_SRC_DIR="${PROJECT_SOURCE_DIR}/game"
    FALU_SHADER_SRC_DIR="${PROJECT_SOURCE_DIR}/assets/shaders"
)

if(CMAKE_GENERATOR MATCHES "Visual Studio")
    target_compile_definitions(FaluEngineEditor PRIVATE FALU_VS_GENERATOR=1)
else()
    target_compile_definitions(FaluEngineEditor PRIVATE FALU_VS_GENERATOR=0)
endif()

if(TARGET GameCode)
    add_dependencies(FaluEngineEditor GameCode)
endif()


engine_request_high_performance_gpu(FaluEngineEditor)
engine_copy_assets(FaluEngineEditor assets)