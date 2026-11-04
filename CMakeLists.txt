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
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)
set(CMAKE_PDB_OUTPUT_DIRECTORY     ${CMAKE_BINARY_DIR}/pdb/$<CONFIG>)

option(FALU_BUILD_EDITOR  "Build editor" ON)
option(FALU_BUILD_RUNTIME "Build runtime" ON)
option(FALU_BUILD_PLUGINS "Build plugins" ON)
option(FALU_BUILD_TESTS   "Build tests" ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include(CompilerOptions)
include(Dependencies)
include(GNUInstallDirs)

add_subdirectory(engine)

# エディタ配布時にエンジンソースを配布しなくてもNativeCodeを作成、編集できるように
if(VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
    set(FALU_VCPKG_PREFIX "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
endif()
add_custom_target(FaluSdk ALL
    COMMAND ${CMAKE_COMMAND}
        -DENGINE_DIR=${CMAKE_SOURCE_DIR}/engine
        -DSDK_DIR=${FALU_OUTPUT_DIR}/sdk
        -DCONFIG=$<CONFIG>
        -DENGINE_LIB_DIR=${CMAKE_BINARY_DIR}/lib/$<CONFIG>
        -DVCPKG_DIR=${FALU_VCPKG_PREFIX}
        -P ${CMAKE_SOURCE_DIR}/cmake/StageSdk.cmake
    COMMENT "Staging FaluEngine SDK"
    VERBATIM
)
add_dependencies(FaluSdk FaluEngine)
set_target_properties(FaluSdk PROPERTIES FOLDER "Engine")

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