cmake_minimum_required(VERSION 3.25)

find_package(GTest CONFIG REQUIRED)

# ── テスト実行ターゲット ────────────────────────────────────────────────────
add_executable(engine_tests
    core/test_EventBus.cpp
    core/test_Memory.cpp
    scene/test_Scene.cpp
)

target_link_libraries(engine_tests PRIVATE
    FaluEngine::Engine
    GTest::gtest_main
)

set_target_properties(engine_tests PROPERTIES FOLDER "Tests")

# CTest に登録
include(GoogleTest)
gtest_discover_tests(engine_tests)
