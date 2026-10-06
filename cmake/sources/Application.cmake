# Discover application files, including headers for generated IDE projects.
file(
    GLOB_RECURSE application_sources
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/app/*.cpp"
    "${PROJECT_SOURCE_DIR}/app/*.hpp"
)
target_sources(simple_platformer PRIVATE ${application_sources})
