# cmake/EngineHelpers.cmake

macro(engine_add_module NAME)
    cmake_parse_arguments(MOD "" "" "SOURCES;DEPS;INCLUDE_DIRS" ${ARGN})

    file(GLOB_RECURSE MOD_HEADERS
        "${CMAKE_SOURCE_DIR}/engine/${NAME}/*.h"
        "${CMAKE_SOURCE_DIR}/engine/${NAME}/*.hpp"
    )

    add_library(engine_${NAME} STATIC ${MOD_SOURCES} ${MOD_HEADERS})
    add_library(FaluEngine::${NAME} ALIAS engine_${NAME})

    source_group(TREE "${CMAKE_SOURCE_DIR}/engine"
        PREFIX "Header Files" FILES ${MOD_HEADERS})
    source_group(TREE "${CMAKE_SOURCE_DIR}/engine"
        PREFIX "Source Files" FILES ${MOD_SOURCES})

    target_include_directories(engine_${NAME}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/engine>
            $<INSTALL_INTERFACE:include>
            ${MOD_INCLUDE_DIRS}
    )

    target_link_libraries(engine_${NAME} PUBLIC ${MOD_DEPS})

    set_target_properties(engine_${NAME} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        FOLDER "Engine"
    )
endmacro()

macro(engine_add_plugin NAME)
    cmake_parse_arguments(PLG "" "" "SOURCES;DEPS" ${ARGN})

    get_filename_component(_PLG_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
    file(GLOB_RECURSE PLG_HEADERS "${_PLG_DIR}/*.h" "${_PLG_DIR}/*.hpp")

    add_library(${NAME} SHARED ${PLG_SOURCES} ${PLG_HEADERS})

    source_group(TREE "${_PLG_DIR}" PREFIX "Header Files" FILES ${PLG_HEADERS})
    source_group(TREE "${_PLG_DIR}" PREFIX "Source Files" FILES ${PLG_SOURCES})

    target_include_directories(${NAME} PRIVATE ${CMAKE_SOURCE_DIR}/engine)
    target_link_libraries(${NAME} PRIVATE engine_core ${PLG_DEPS})

    set_target_properties(${NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/plugins
        FOLDER "Plugins"
    )
endmacro()

# ── engine_copy_assets ───────────────────────────────────────────────────────
# ソースディレクトリが存在しない場合はスキップする
function(engine_copy_assets TARGET ASSET_DIR)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${ASSET_DIR}")
        message(STATUS "engine_copy_assets: '${ASSET_DIR}' not found, skipping")
        return()
    endif()
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/${ASSET_DIR}"
            "$<TARGET_FILE_DIR:${TARGET}>/${ASSET_DIR}"
        COMMENT "Copying assets: ${ASSET_DIR}"
    )
endfunction()
