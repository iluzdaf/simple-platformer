add_library(simple_platformer_glm INTERFACE)
target_include_directories(
    simple_platformer_glm
    SYSTEM INTERFACE
    ${PROJECT_SOURCE_DIR}/external/glm
)

add_library(simple_platformer_json INTERFACE)
target_include_directories(
    simple_platformer_json
    SYSTEM INTERFACE
    ${PROJECT_SOURCE_DIR}/external/nlohmann/single_include
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${PROJECT_SOURCE_DIR}/external/glfw external/glfw EXCLUDE_FROM_ALL)

add_library(simple_platformer_glad ${PROJECT_SOURCE_DIR}/external/glad/src/glad.c)
target_include_directories(
    simple_platformer_glad
    SYSTEM PUBLIC
    ${PROJECT_SOURCE_DIR}/external/glad/include
)

add_library(simple_platformer_stb ${PROJECT_SOURCE_DIR}/external/stb/stb_image.cpp)
target_include_directories(
    simple_platformer_stb
    SYSTEM PUBLIC
    ${PROJECT_SOURCE_DIR}/external/stb
)

add_library(
    simple_platformer_imgui
    ${PROJECT_SOURCE_DIR}/external/imgui/imgui.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/imgui_draw.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/imgui_tables.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.cpp
)
target_compile_features(simple_platformer_imgui PUBLIC cxx_std_17)
target_include_directories(
    simple_platformer_imgui
    SYSTEM PUBLIC
    ${PROJECT_SOURCE_DIR}/external/imgui
    ${PROJECT_SOURCE_DIR}/external/imgui/backends
)
target_link_libraries(simple_platformer_imgui PUBLIC glfw)
