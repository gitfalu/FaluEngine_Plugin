cmake_minimum_required(VERSION 3.25)

include(EngineHelpers)

add_library(FaluRuntimeModule SHARED
    main.cpp
)

falu_add_headers(FaluRuntimeModule ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(FaluRuntimeModule
    PRIVATE 
        FaluEngine::Internal
)

set_target_properties(FaluRuntimeModule PROPERTIES
    OUTPUT_NAME "FaluEngine"
    FOLDER "Runtime"
)

if(TARGET GameCode)
    add_dependencies(FaluRuntimeModule GameCode)
endif()

add_executable(FaluEngineRuntime ${CMAKE_SOURCE_DIR}/launcher/main.cpp)
add_executable(FaluEngine::Runtime ALIAS FaluEngineRuntime)
target_compile_definitions(FaluEngineRuntime PRIVATE FALU_MODULE_NAME=FaluRuntime)
add_dependencies(FaluEngineRuntime FaluRuntimeModule)
set_target_properties(FaluEngineRuntime PROPERTIES
    OUTPUT_NAME "FaluEngine"
    RUNTIME_OUTPUT_DIRECTORY "${FALU_OUTPUT_DIR}"
    FOLDER "Runtime"
)

engine_request_high_performance_gpu(FaluEngineRuntime)
engine_copy_assets(FaluEngineRuntime assets)