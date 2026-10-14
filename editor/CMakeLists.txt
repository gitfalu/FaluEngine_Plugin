include(EngineHelpers)

add_library(FaluEditorModule SHARED
    main.cpp
    panels/HierarchyPanel.cpp
    panels/InspectorPanel.cpp
    panels/SceneViewPanel.cpp
    panels/GameViewPanel.cpp
    panels/ContentBrowserPanel.cpp
    panels/LLMDebugPanel.cpp
    panels/AnimationEventPanel.cpp
    panels/FocusAnalysisPanel.cpp
    EditorSettings.cpp
    EditorLocalization.cpp
)

falu_add_headers(FaluEditorModule ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(FaluEditorModule
    PRIVATE
        FaluEngine::Internal
)

set_target_properties(FaluEditorModule PROPERTIES
    OUTPUT_NAME "FaluEditor"
    FOLDER "Editor"
)

target_compile_definitions(FaluEditorModule PRIVATE
    FALU_CMAKE_EXE="${CMAKE_COMMAND}"
    FALU_BUILD_DIR="${CMAKE_BINARY_DIR}"
    FALU_BUILD_CONFIG="$<CONFIG>"
    FALU_GAME_SRC_DIR="${PROJECT_SOURCE_DIR}/game"
    FALU_SHADER_SRC_DIR="${PROJECT_SOURCE_DIR}/assets/shaders"
)

if(CMAKE_GENERATOR MATCHES "Visual Studio")
    target_compile_definitions(FaluEditorModule PRIVATE FALU_VS_GENERATOR=1)
else()
    target_compile_definitions(FaluEditorModule PRIVATE FALU_VS_GENERATOR=0)
endif()

if(TARGET GameCode)
    add_dependencies(FaluEditorModule GameCode)
endif()

add_executable(FaluEngineEditor ${CMAKE_SOURCE_DIR}/launcher/main.cpp)
target_compile_definitions(FaluEngineEditor PRIVATE FALU_MODULE_NAME=FaluEditor)
add_dependencies(FaluEngineEditor FaluEditorModule)
set_target_properties(FaluEngineEditor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${FALU_OUTPUT_DIR}"
    FOLDER "Editor"
)

engine_request_high_performance_gpu(FaluEngineEditor)
engine_copy_assets(FaluEditorModule assets "${FALU_OUTPUT_DIR}")