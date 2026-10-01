include(EngineHelpers)

add_executable(FaluEngineEditor
    main.cpp
    panels/HierarchyPanel.cpp
    panels/InspectorPanel.cpp
    panels/SceneViewPanel.cpp
    panels/GameViewPanel.cpp
    panels/ContentBrowserPanel.cpp
    panels/LLMDebugPanel.cpp
    EditorSettings.cpp
)

target_link_libraries(FaluEngineEditor 
    PRIVATE
        FaluEngine::Internal
)

if(TARGET GameCode)
    add_dependencies(FaluEngineEditor GameCode)
endif()

engine_copy_assets(FaluEngineEditor assets)