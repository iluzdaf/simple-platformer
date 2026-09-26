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

set(LUA_ENABLE_SHARED OFF CACHE BOOL "" FORCE)
set(LUA_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(LUA_BUILD_BINARY OFF CACHE BOOL "" FORCE)
set(LUA_BUILD_COMPILER OFF CACHE BOOL "" FORCE)
add_subdirectory(${PROJECT_SOURCE_DIR}/external/lua external/lua EXCLUDE_FROM_ALL)

add_library(simple_platformer_sol2 INTERFACE)
target_include_directories(
    simple_platformer_sol2
    SYSTEM INTERFACE
    ${PROJECT_SOURCE_DIR}/external/sol2/include
)
target_link_libraries(simple_platformer_sol2 INTERFACE Lua::Library)

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
    ${PROJECT_SOURCE_DIR}/external/implot/implot.cpp
    ${PROJECT_SOURCE_DIR}/external/implot/implot_items.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui-node-editor/crude_json.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui-node-editor/imgui_canvas.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui-node-editor/imgui_node_editor.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui-node-editor/imgui_node_editor_api.cpp
)
target_compile_features(simple_platformer_imgui PUBLIC cxx_std_17)
target_include_directories(
    simple_platformer_imgui
    SYSTEM PUBLIC
    ${PROJECT_SOURCE_DIR}/external/imgui
    ${PROJECT_SOURCE_DIR}/external/imgui/backends
    ${PROJECT_SOURCE_DIR}/external/implot
    ${PROJECT_SOURCE_DIR}/external/imgui-node-editor
)
target_link_libraries(simple_platformer_imgui PUBLIC glfw)
