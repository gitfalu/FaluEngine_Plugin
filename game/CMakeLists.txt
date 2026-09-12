add_library(GameCode SHARED
    scripts/PlayerController.cpp
    scripts/CollectiblesScript.cpp
    scripts/HitEffectScript.cpp
    scripts/BillboardScript.cpp
    scripts/GameManager.cpp
    GameModule.cpp
)

target_link_libraries(GameCode
    PRIVATE
        FaluEngine::Engine
)

target_include_directories(GameCode
    PRIVATE
        ${PROJECT_SOURCE_DIR}/engine/include
)

set_target_properties(GameCode PROPERTIES
    OUTPUT_NAME "GameCode"
    FOLDER "Game"
)
