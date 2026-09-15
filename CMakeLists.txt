cmake_minimum_required(VERSION 3.21)

project(
    SimplePlatformer
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

function(enable_project_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

add_library(simple_platformer_glm INTERFACE)
target_include_directories(
    simple_platformer_glm
    SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
)

add_library(
    simple_platformer_core
    src/math/aabb.cpp
    src/timing/fixed_step.cpp
)
target_include_directories(
    simple_platformer_core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(simple_platformer_core PUBLIC cxx_std_17)
target_link_libraries(simple_platformer_core PUBLIC simple_platformer_glm)
enable_project_warnings(simple_platformer_core)

add_executable(simple_platformer app/main.cpp)
target_compile_features(simple_platformer PRIVATE cxx_std_17)
target_link_libraries(simple_platformer PRIVATE simple_platformer_core)
enable_project_warnings(simple_platformer)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT simple_platformer)
set_target_properties(
    simple_platformer
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format)

if(CLANG_FORMAT_EXECUTABLE)
    file(
        GLOB_RECURSE PROJECT_FORMAT_FILES
        CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
    )

    add_custom_target(
        format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -i ${PROJECT_FORMAT_FILES}
        COMMENT "Formatting first-party C++ source"
        VERBATIM
    )

    add_custom_target(
        format-check
        COMMAND ${CLANG_FORMAT_EXECUTABLE} --dry-run --Werror ${PROJECT_FORMAT_FILES}
        COMMENT "Checking first-party C++ formatting"
        VERBATIM
    )
else()
    message(STATUS "clang-format not found; format targets are unavailable")
endif()

include(CTest)

if(BUILD_TESTING)
    set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(CATCH_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CATCH_BUILD_EXTRA_TESTS OFF CACHE BOOL "" FORCE)
    set(CATCH_ENABLE_REPRODUCIBLE_BUILD ON CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
    add_subdirectory(external/catch2 EXCLUDE_FROM_ALL)

    add_executable(
        simple_platformer_tests
        tests/math/test_coordinates.cpp
        tests/timing/test_fixed_step.cpp
    )
    target_compile_features(simple_platformer_tests PRIVATE cxx_std_17)
    target_link_libraries(
        simple_platformer_tests
        PRIVATE
        simple_platformer_core
        Catch2::Catch2WithMain
    )
    enable_project_warnings(simple_platformer_tests)

    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/catch2/extras)
    include(Catch)
    catch_discover_tests(simple_platformer_tests)

    add_custom_target(
        run_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>
        DEPENDS simple_platformer_tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Building and running all tests"
        USES_TERMINAL
    )
endif()
