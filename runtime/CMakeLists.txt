cmake_minimum_required(VERSION 3.25)

include(EngineHelpers)

add_executable(FaluEngineRuntime 
    main.cpp
)
add_executable(FaluEngine::Runtime ALIAS FaluEngineRuntime)

target_link_libraries(FaluEngineRuntime 
    PRIVATE 
        FaluEngine::Internal
)

set_target_properties(FaluEngineRuntime PROPERTIES
    OUTPUT_NAME "FaluEngine"
    FOLDER "Runtime"
)

if(TARGET GameCode)
    add_dependencies(FaluEngineRuntime GameCode)
endif()

engine_request_high_performance_gpu(FaluEngineRuntime)
engine_copy_assets(FaluEngineRuntime assets)