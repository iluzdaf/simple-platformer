find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format)
find_program(PRETTIER_EXECUTABLE NAMES prettier)
find_program(RUFF_EXECUTABLE NAMES ruff)
find_program(STYLUA_EXECUTABLE NAMES stylua)
find_program(LUACHECK_EXECUTABLE NAMES luacheck)
find_program(
    CLANG_TIDY_EXECUTABLE
    NAMES clang-tidy
    HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin
)

file(
    GLOB_RECURSE PROJECT_CPP_FILES
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/app/*.cpp
    ${PROJECT_SOURCE_DIR}/scripting/*.cpp
    ${PROJECT_SOURCE_DIR}/src/*.cpp
    ${PROJECT_SOURCE_DIR}/tests/*.cpp
)

file(
    GLOB_RECURSE PROJECT_HEADERS
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/app/*.hpp
    ${PROJECT_SOURCE_DIR}/include/*.hpp
    ${PROJECT_SOURCE_DIR}/scripting/*.hpp
    ${PROJECT_SOURCE_DIR}/tests/*.hpp
)

file(
    GLOB_RECURSE PROJECT_PUBLIC_HEADERS
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/include/*.hpp
)
# The scripting target's interface. Its other headers use sol2, which is private to it.
list(APPEND PROJECT_PUBLIC_HEADERS ${PROJECT_SOURCE_DIR}/scripting/lua_npc_scripts.hpp)

file(
    GLOB_RECURSE PROJECT_JSON_FILES
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/assets/*.json
    ${PROJECT_SOURCE_DIR}/tests/fixtures/*.json
)

file(
    GLOB_RECURSE PROJECT_YAML_FILES
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/.github/*.yml
    ${PROJECT_SOURCE_DIR}/.github/*.yaml
)

file(
    GLOB PROJECT_ROOT_MARKDOWN_FILES
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/*.md
)

file(
    GLOB_RECURSE PROJECT_DOC_MARKDOWN_FILES
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/docs/*.md
)

set(PROJECT_MARKDOWN_FILES ${PROJECT_ROOT_MARKDOWN_FILES} ${PROJECT_DOC_MARKDOWN_FILES})

file(
    GLOB_RECURSE PROJECT_PYTHON_FILES
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/tools/*.py
)

file(
    GLOB_RECURSE PROJECT_LUA_FILES
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/assets/*.lua
    ${PROJECT_SOURCE_DIR}/tests/fixtures/*.lua
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

if(PRETTIER_EXECUTABLE)
    add_custom_target(
        format-json
        COMMAND ${PRETTIER_EXECUTABLE} --write --log-level warn ${PROJECT_JSON_FILES}
        COMMENT "Formatting first-party JSON"
        VERBATIM
    )

    add_custom_target(
        format-json-check
        COMMAND ${PRETTIER_EXECUTABLE} --check --log-level warn ${PROJECT_JSON_FILES}
        COMMENT "Checking first-party JSON formatting"
        VERBATIM
    )

    add_custom_target(
        format-yaml
        COMMAND ${PRETTIER_EXECUTABLE} --write --log-level warn ${PROJECT_YAML_FILES}
        COMMENT "Formatting first-party YAML"
        VERBATIM
    )

    add_custom_target(
        format-yaml-check
        COMMAND ${PRETTIER_EXECUTABLE} --check --log-level warn ${PROJECT_YAML_FILES}
        COMMENT "Checking first-party YAML formatting"
        VERBATIM
    )

    add_custom_target(
        format-markdown
        COMMAND ${PRETTIER_EXECUTABLE} --write --log-level warn ${PROJECT_MARKDOWN_FILES}
        COMMENT "Formatting first-party Markdown"
        VERBATIM
    )

    add_custom_target(
        format-markdown-check
        COMMAND ${PRETTIER_EXECUTABLE} --check --log-level warn ${PROJECT_MARKDOWN_FILES}
        COMMENT "Checking first-party Markdown formatting"
        VERBATIM
    )
else()
    message(STATUS "prettier not found; JSON, YAML, and Markdown format targets are unavailable")
endif()

if(RUFF_EXECUTABLE)
    add_custom_target(
        format-python
        COMMAND ${RUFF_EXECUTABLE} format ${PROJECT_PYTHON_FILES}
        COMMENT "Formatting first-party Python source"
        VERBATIM
    )

    add_custom_target(
        format-python-check
        COMMAND ${RUFF_EXECUTABLE} format --check ${PROJECT_PYTHON_FILES}
        COMMENT "Checking first-party Python formatting"
        VERBATIM
    )

    add_custom_target(
        lint-python
        COMMAND ${RUFF_EXECUTABLE} check ${PROJECT_PYTHON_FILES}
        COMMENT "Checking first-party Python source with Ruff"
        VERBATIM
    )
else()
    message(STATUS "ruff not found; Python quality targets are unavailable")
endif()

if(STYLUA_EXECUTABLE)
    add_custom_target(
        format-lua
        COMMAND ${STYLUA_EXECUTABLE} ${PROJECT_LUA_FILES}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Formatting first-party Lua source"
        VERBATIM
    )

    add_custom_target(
        format-lua-check
        COMMAND ${STYLUA_EXECUTABLE} --check ${PROJECT_LUA_FILES}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Checking first-party Lua formatting"
        VERBATIM
    )
else()
    message(STATUS "stylua not found; Lua format targets are unavailable")
endif()

if(LUACHECK_EXECUTABLE)
    add_custom_target(
        lint-lua
        COMMAND ${LUACHECK_EXECUTABLE} ${PROJECT_LUA_FILES}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Checking first-party Lua source with Luacheck"
        VERBATIM
    )
else()
    message(STATUS "luacheck not found; Lua lint target is unavailable")
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
