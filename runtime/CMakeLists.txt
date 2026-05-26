cmake_minimum_required(VERSION 3.25)

add_executable(FaluEngineRuntime main.cpp)
add_executable(FaluEngine::Runtime ALIAS FaluEngineRuntime)

target_link_libraries(FaluEngineRuntime PRIVATE FaluEngine::Engine)

set_target_properties(FaluEngineRuntime PROPERTIES
    OUTPUT_NAME "FaluEngine"
    FOLDER "Runtime"
)

# アセットをビルドディレクトリにコピー
engine_copy_assets(FaluEngineRuntime assets)
