add_library(PhysicsPlugin SHARED
    PhysicsPlugin.cpp
)

target_link_libraries(PhysicsPlugin
    PRIVATE
        FaluEngine::Engine
)

set_target_properties(PhysicsPlugin PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY
        "${CMAKE_BINARY_DIR}/bin/plugins"
)