cmake_minimum_required(VERSION 3.25)

project(FaluEngine 
    VERSION 0.1.0 
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FALU_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
set(FALU_MODULE_DIR ${FALU_OUTPUT_DIR}/modules)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${FALU_MODULE_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${FALU_MODULE_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_PDB_OUTPUT_DIRECTORY     ${CMAKE_BINARY_DIR}/pdb/$<CONFIG>)

option(FALU_BUILD_EDITOR  "Build editor" ON)
option(FALU_BUILD_RUNTIME "Build runtime" ON)
option(FALU_BUILD_GAME    "Build game module" ON)
option(FALU_BUILD_PLUGINS "Build plugins" ON)
option(FALU_BUILD_TESTS   "Build tests" ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include(CompilerOptions)
include(Dependencies)
include(GNUInstallDirs)

add_subdirectory(engine)

if(FALU_BUILD_GAME)
    add_subdirectory(game)
endif()

if(FALU_BUILD_RUNTIME)
    add_subdirectory(runtime)
endif()

if(FALU_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

if(FALU_BUILD_PLUGINS)
    add_subdirectory(plugins/PhysicsPlugin)
endif()

if(FALU_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(InstallRules)