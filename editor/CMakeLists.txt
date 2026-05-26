cmake_minimum_required(VERSION 3.25)

find_package(imgui CONFIG REQUIRED)

add_executable(FaluEngineEditor WIN32 main.cpp)
add_executable(FaluEngine::Editor ALIAS FaluEngineEditor)

target_link_libraries(FaluEngineEditor PRIVATE
    FaluEngine::Engine
    imgui::imgui
)

set_target_properties(FaluEngineEditor PROPERTIES
    OUTPUT_NAME "FaluEngineEditor"
    FOLDER "Editor"
)

engine_copy_assets(FaluEngineEditor assets)
