include_guard(GLOBAL)

install(TARGETS FaluEngine
    EXPORT FaluEngineTargets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION lib
)

if(TARGET FaluEngineRuntime)
    install(TARGETS FaluEngineRuntime
        RUNTIME_DEPENDENCY_SET falu_runtime_dependencies
        RUNTIME DESTINATION bin
    )
endif()

if(TARGET FaluEngineEditor)
    install(TARGETS FaluEngineEditor
        RUNTIME_DEPENDENCY_SET falu_runtime_dependencies
        RUNTIME DESTINATION bin
    )
endif()

if(TARGET GameCode)
    install(TARGETS GameCode
        RUNTIME_DEPENDENCY_SET falu_runtime_dependencies
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION bin
    )
endif()

if(TARGET PhysicsPlugin)
    install(TARGETS PhysicsPlugin
        RUNTIME DESTINATION bin/plugins
        LIBRARY DESTINATION bin/plugins
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