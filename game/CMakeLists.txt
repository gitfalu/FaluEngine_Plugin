add_library(GameCode STATIC
    
)

target_link_libraries(GameCode PUBLIC FaluEngine::Engine)
target_include_directories(GameCode PUBLIC game/)