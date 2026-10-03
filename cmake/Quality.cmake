find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format)
find_program(PRETTIER_EXECUTABLE NAMES prettier)
find_program(RUFF_EXECUTABLE NAMES ruff)
find_program(
    CLANG_TIDY_EXECUTABLE
    NAMES clang-tidy
    HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin
)

file(
    GLOB_RECURSE PROJECT_CPP_FILES
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/app/*.cpp
    ${PROJECT_SOURCE_DIR}/src/*.cpp
    ${PROJECT_SOURCE_DIR}/tests/*.cpp
)

file(
    GLOB_RECURSE PROJECT_HEADERS
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/app/*.hpp
    ${PROJECT_SOURCE_DIR}/include/*.hpp
    ${PROJECT_SOURCE_DIR}/tests/*.hpp
)

file(
    GLOB_RECURSE PROJECT_PUBLIC_HEADERS
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/include/*.hpp
)
# File selection lives in tools/format.py for both standalone and CMake use.
find_package(Python3 COMPONENTS Interpreter QUIET)

if(Python3_Interpreter_FOUND)
    function(add_format_targets kind tool option target)
        if(${tool})
            add_custom_target(
                ${target}
                COMMAND
                    ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/format.py
                    --only ${kind} ${option} "${${tool}}"
                COMMENT "Formatting first-party ${kind} files"
                VERBATIM
            )
            add_custom_target(
                ${target}-check
                COMMAND
                    ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/format.py
                    --check --only ${kind} ${option} "${${tool}}"
                COMMENT "Checking first-party ${kind} files"
                VERBATIM
            )
        else()
            message(STATUS "${tool} not found; ${target} targets are unavailable")
        endif()
    endfunction()

    add_format_targets(cpp CLANG_FORMAT_EXECUTABLE --clang-format format)
    add_format_targets(json PRETTIER_EXECUTABLE --prettier format-json)
    add_format_targets(yaml PRETTIER_EXECUTABLE --prettier format-yaml)
    add_format_targets(markdown PRETTIER_EXECUTABLE --prettier format-markdown)
    add_format_targets(python RUFF_EXECUTABLE --ruff format-python)

    if(RUFF_EXECUTABLE)
        # Keep the existing target name; the Python check also runs lint.
        add_custom_target(lint-python DEPENDS format-python-check)
    endif()
else()
    message(STATUS "Python3 not found; formatting and Python lint targets are unavailable")
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
