# Content loading, game flow, and debug snapshots can be tested without a window.
file(
    GLOB_RECURSE testable_application_sources
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/app/content/*.cpp"
    "${PROJECT_SOURCE_DIR}/app/game/*.cpp"
)
file(
    GLOB testable_debug_sources
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/app/debug/*.cpp"
)

# Other application code is included only when a headless test needs it.
set(test_application_sources
    ${PROJECT_SOURCE_DIR}/app/graphics/display_viewport.cpp
    ${PROJECT_SOURCE_DIR}/app/ui/inventory_layout.cpp
)

# New test files and headers are discovered by folder.
file(
    GLOB_RECURSE test_sources
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/tests/*.cpp"
    "${PROJECT_SOURCE_DIR}/tests/*.hpp"
)
target_sources(
    simple_platformer_tests
    PRIVATE
    ${testable_application_sources}
    ${testable_debug_sources}
    ${test_application_sources}
    ${test_sources}
)
