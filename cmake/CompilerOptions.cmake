add_library(falu_compiler_options INTERFACE)

target_compile_features(falu_compiler_options
    INTERFACE cxx_std_20
)

if(MSVC)
    target_compile_options(falu_compiler_options INTERFACE
        /W4
        /permissive-
        /utf-8
        /MP
        /wd4100
        /wd4505
    )
else()
    target_compile_options(falu_compiler_options INTERFACE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

target_compile_definitions(falu_compiler_options INTERFACE
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    $<$<CONFIG:Debug>:ENGINE_DEBUG>
)