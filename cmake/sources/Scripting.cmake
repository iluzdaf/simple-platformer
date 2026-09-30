# Keep this file source-only so clang-tidy can scope manifest changes safely.
target_sources(
    simple_platformer_scripting
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src/scripting/lua_activity_values.cpp
    ${PROJECT_SOURCE_DIR}/src/scripting/lua_npc_scripts.cpp
    ${PROJECT_SOURCE_DIR}/src/scripting/lua_sandbox.cpp
    ${PROJECT_SOURCE_DIR}/src/scripting/lua_vec2.cpp
)
