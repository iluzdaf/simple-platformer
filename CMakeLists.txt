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

        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(
                ${target}
                PRIVATE
                -Wshadow
                -Wfloat-conversion
                -Wimplicit-int-conversion
                -Wshorten-64-to-32
            )
        endif()
    endif()
endfunction()

add_library(simple_platformer_glm INTERFACE)
target_include_directories(
    simple_platformer_glm
    SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw EXCLUDE_FROM_ALL)

add_library(simple_platformer_glad external/glad/src/glad.c)
target_include_directories(
    simple_platformer_glad
    SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
)

add_library(simple_platformer_stb external/stb/stb_image.cpp)
target_include_directories(
    simple_platformer_stb
    SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/stb
)

add_library(
    simple_platformer_imgui
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)
target_compile_features(simple_platformer_imgui PUBLIC cxx_std_17)
target_include_directories(
    simple_platformer_imgui
    SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
)
target_link_libraries(simple_platformer_imgui PUBLIC glfw)

add_library(
    simple_platformer_core
    src/actor/actor.cpp
    src/actor/actor_system.cpp
    src/actor/lifecycle.cpp
    src/combat/attack_system.cpp
    src/combat/combat.cpp
    src/combat/projectile_system.cpp
    src/input/input_state.cpp
    src/inventory/inventory.cpp
    src/inventory/item.cpp
    src/inventory/item_use.cpp
    src/math/aabb.cpp
    src/math/coordinates.cpp
    src/math/validation.cpp
    src/movement/flying_movement.cpp
    src/movement/platformer_movement.cpp
    src/navigation/path_search.cpp
    src/navigation/flying_navigation.cpp
    src/navigation/input_program.cpp
    src/navigation/path_follower.cpp
    src/navigation/platformer_navigation.cpp
    src/npc/npc_senses.cpp
    src/npc/npc_system.cpp
    src/physics/collision.cpp
    src/physics/segment_cast.cpp
    src/render/animation.cpp
    src/render/animation_system.cpp
    src/render/camera.cpp
    src/render/render_scene.cpp
    src/render/sprite.cpp
    src/timing/fixed_step.cpp
    src/world/tile_map.cpp
    src/world/world.cpp
    src/world/level_exit.cpp
    src/world/level_validation.cpp
    src/world/pickup.cpp
    src/world/world_requests.cpp
    src/world/world_simulation.cpp
)
target_include_directories(
    simple_platformer_core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(simple_platformer_core PUBLIC cxx_std_17)
target_link_libraries(simple_platformer_core PUBLIC simple_platformer_glm)
enable_project_warnings(simple_platformer_core)

add_executable(
    simple_platformer
    app/application.cpp
    app/debug/debug_overlay.cpp
    app/debug/debug_overlay_ui.cpp
    app/game/example_animations.cpp
    app/game/example_content.cpp
    app/game/example_game.cpp
    app/game/example_items.cpp
    app/graphics/display_viewport.cpp
    app/graphics/sprite_renderer.cpp
    app/main.cpp
    app/ui/completion_ui.cpp
    app/ui/health_hud_ui.cpp
    app/ui/inventory_layout.cpp
    app/ui/inventory_ui.cpp
)
target_compile_features(simple_platformer PRIVATE cxx_std_17)
target_include_directories(simple_platformer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app)
target_link_libraries(
    simple_platformer
    PRIVATE
    simple_platformer_core
    simple_platformer_glad
    simple_platformer_imgui
    simple_platformer_stb
    glfw
)
enable_project_warnings(simple_platformer)

add_custom_command(
    TARGET simple_platformer
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:simple_platformer>/assets
    COMMENT "Copying game assets beside the executable"
    VERBATIM
)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT simple_platformer)
set_target_properties(
    simple_platformer
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:simple_platformer>"
)

find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format)
find_program(
    CLANG_TIDY_EXECUTABLE
    NAMES clang-tidy
    HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin
)

file(
    GLOB_RECURSE PROJECT_CPP_FILES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
)

file(
    GLOB_RECURSE PROJECT_HEADERS
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/app/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
)

file(
    GLOB_RECURSE PROJECT_PUBLIC_HEADERS
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
)

if(CLANG_FORMAT_EXECUTABLE)
    add_custom_target(
        format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -i ${PROJECT_CPP_FILES} ${PROJECT_HEADERS}
        COMMENT "Formatting first-party C++ source"
        VERBATIM
    )

    add_custom_target(
        format-check
        COMMAND
            ${CLANG_FORMAT_EXECUTABLE} --dry-run --Werror ${PROJECT_CPP_FILES}
            ${PROJECT_HEADERS}
        COMMENT "Checking first-party C++ formatting"
        VERBATIM
    )
else()
    message(STATUS "clang-format not found; format targets are unavailable")
endif()

set(CLANG_TIDY_EXTRA_ARGUMENTS)

if(CLANG_TIDY_EXECUTABLE AND CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    execute_process(
        COMMAND xcrun --show-sdk-path
        OUTPUT_VARIABLE MACOS_SDK_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
    )
    list(
        APPEND CLANG_TIDY_EXTRA_ARGUMENTS
        --extra-arg=-isysroot
        --extra-arg=${MACOS_SDK_PATH}
        --extra-arg=-isystem
        --extra-arg=${MACOS_SDK_PATH}/usr/include/c++/v1
    )
endif()

if(CLANG_TIDY_EXECUTABLE)
    add_custom_target(
        tidy
        COMMAND
            ${CLANG_TIDY_EXECUTABLE} -p ${CMAKE_BINARY_DIR} --warnings-as-errors=*
            --quiet ${CLANG_TIDY_EXTRA_ARGUMENTS} ${PROJECT_CPP_FILES} ${PROJECT_HEADERS}
        COMMENT "Checking first-party C++ source with clang-tidy"
        VERBATIM
    )
else()
    message(STATUS "clang-tidy not found; tidy target is unavailable")
endif()

set_source_files_properties(${PROJECT_PUBLIC_HEADERS} PROPERTIES LANGUAGE CXX)
add_library(header_self_containment OBJECT EXCLUDE_FROM_ALL ${PROJECT_PUBLIC_HEADERS})
target_compile_features(header_self_containment PRIVATE cxx_std_17)
target_link_libraries(header_self_containment PRIVATE simple_platformer_core)
enable_project_warnings(header_self_containment)

if(NOT MSVC)
    target_compile_options(
        header_self_containment
        PRIVATE
        -Wno-pragma-once-outside-header
        -Wno-unused-const-variable
    )
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
        app/debug/debug_overlay.cpp
        app/game/example_animations.cpp
        app/game/example_content.cpp
        app/game/example_game.cpp
        app/game/example_items.cpp
        app/graphics/display_viewport.cpp
        app/ui/inventory_layout.cpp
        tests/app/test_debug_overlay.cpp
        tests/app/test_display_viewport.cpp
        tests/app/test_example_content.cpp
        tests/app/test_inventory_layout.cpp
        tests/app/test_level_transition.cpp
        tests/actor/test_actor_system.cpp
        tests/actor/test_lifecycle.cpp
        tests/combat/test_attack_system.cpp
        tests/combat/test_projectile_system.cpp
        tests/input/test_input_state.cpp
        tests/inventory/test_inventory.cpp
        tests/math/test_coordinates.cpp
        tests/math/test_validation.cpp
        tests/movement/test_flying_movement.cpp
        tests/movement/test_platformer_movement.cpp
        tests/navigation/test_path_search.cpp
        tests/navigation/test_flying_navigation.cpp
        tests/navigation/test_input_program.cpp
        tests/navigation/test_path_follower.cpp
        tests/navigation/test_platformer_navigation.cpp
        tests/npc/test_npc_system.cpp
        tests/physics/test_collision.cpp
        tests/physics/test_segment_cast.cpp
        tests/render/test_animation.cpp
        tests/render/test_animation_system.cpp
        tests/render/test_camera.cpp
        tests/render/test_render_scene.cpp
        tests/timing/test_fixed_step.cpp
        tests/world/test_level_objects.cpp
        tests/world/test_level_validation.cpp
        tests/world/test_tile_map.cpp
        tests/world/test_world.cpp
        tests/world/test_world_simulation.cpp
    )
    target_compile_features(simple_platformer_tests PRIVATE cxx_std_17)
    target_include_directories(simple_platformer_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app)
    target_link_libraries(
        simple_platformer_tests
        PRIVATE
        simple_platformer_core
        Catch2::Catch2WithMain
    )
    enable_project_warnings(simple_platformer_tests)

    if(
        CMAKE_CXX_COMPILER_ID MATCHES "Clang"
        AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 22
    )
        target_compile_options(simple_platformer_tests PRIVATE -Wno-c2y-extensions)
    endif()

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
