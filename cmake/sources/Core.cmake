# Include public headers in the generated IDE project beside core implementations.
file(
    GLOB_RECURSE core_sources
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/include/*.hpp"
)
target_sources(simple_platformer_core PRIVATE ${core_sources})
