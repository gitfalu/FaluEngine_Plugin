add_library(GameCode SHARED
    scripts/PlayerController.cpp
    GameModule.cpp
)

target_link_libraries(GameCode PUBLIC FaluEngine::Engine)
target_include_directories(GameCode PUBLIC game/)