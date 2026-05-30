cmake_minimum_required(VERSION 3.25)

find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)

add_executable(FaluEngineEditor
    main.cpp
    panels/HierarchyPanel.cpp
    panels/InspectorPanel.cpp
    panels/SceneViewPanel.cpp
)
add_executable(FaluEngine::Editor ALIAS FaluEngineEditor)

target_include_directories(FaluEngineEditor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${joltphysics_SOURCE_DIR}
)

target_link_libraries(FaluEngineEditor PRIVATE
    FaluEngine::Engine
    imgui::imgui
    imguizmo::imguizmo
)

set_target_properties(FaluEngineEditor PROPERTIES
    OUTPUT_NAME "FaluEngineEditor"
    FOLDER "Editor"
)

engine_copy_assets(FaluEngineEditor assets)
