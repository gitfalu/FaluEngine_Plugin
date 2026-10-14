include_guard(GLOBAL)

# エンジンのDLLを機能ごとに
install(TARGETS FaluCore FaluAudio FaluGraphics FaluScene FaluEngine
    EXPORT FaluEngineTargets
    RUNTIME DESTINATION bin/modules
    LIBRARY DESTINATION bin/modules
    ARCHIVE DESTINATION lib
)

if(TARGET FaluEngineRuntime)
    install(TARGETS FaluEngineRuntime RUNTIME DESTINATION bin)
    install(TARGETS FaluRuntimeModule
        RUNTIME_DEPENDENCY_SET falu_runtime_dependencies
        RUNTIME DESTINATION bin/modules
    )
endif()

if(TARGET FaluEngineEditor)
    install(TARGETS FaluEngineEditor RUNTIME DESTINATION bin)
    install(TARGETS FaluEditorModule
        RUNTIME_DEPENDENCY_SET falu_runtime_dependencies
        RUNTIME DESTINATION bin/modules
    )
endif()

if(TARGET GameCode)
    install(TARGETS GameCode
        RUNTIME_DEPENDENCY_SET falu_runtime_dependencies
        RUNTIME DESTINATION bin/modules
        LIBRARY DESTINATION bin/modules
    )
endif()

if(TARGET PhysicsPlugin)
    install(TARGETS PhysicsPlugin
        RUNTIME DESTINATION bin/modules/plugins
        LIBRARY DESTINATION bin/modules/plugins
    )
endif()

install(DIRECTORY
    "${PROJECT_SOURCE_DIR}/assets/"
    DESTINATION bin/assets
)

install(DIRECTORY
    "${PROJECT_SOURCE_DIR}/engine/include/FaluEngine/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/FaluEngine"
)

install(RUNTIME_DEPENDENCY_SET falu_runtime_dependencies
    DESTINATION bin
    PRE_EXCLUDE_REGEXES
        "api-ms-win-.*"
        "ext-ms-win-.*"
    POST_EXCLUDE_REGEXES
        ".*/[Ww]indows/[Ss]ystem32/.*"
)