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

if(TARGET GameCode)
    add_dependencies(FaluEngineEditor GameCode)
endif()


engine_request_high_performance_gpu(FaluEngineEditor)
engine_copy_assets(FaluEngineEditor assets)