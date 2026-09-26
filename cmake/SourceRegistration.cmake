function(verify_project_source_registration)
    set(project_targets
        simple_platformer_core
        simple_platformer_scripting
        simple_platformer
    )
    set(project_source_patterns
        ${PROJECT_SOURCE_DIR}/app/*.cpp
        ${PROJECT_SOURCE_DIR}/src/*.cpp
    )

    if(BUILD_TESTING)
        list(APPEND project_targets simple_platformer_tests)
        list(APPEND project_source_patterns ${PROJECT_SOURCE_DIR}/tests/*.cpp)
    endif()

    set(registered_sources)
    foreach(target IN LISTS project_targets)
        get_target_property(target_sources ${target} SOURCES)
        get_target_property(target_source_directory ${target} SOURCE_DIR)
        foreach(source IN LISTS target_sources)
            if(IS_ABSOLUTE "${source}")
                cmake_path(NORMAL_PATH source OUTPUT_VARIABLE absolute_source)
            else()
                cmake_path(
                    ABSOLUTE_PATH source
                    BASE_DIRECTORY "${target_source_directory}"
                    NORMALIZE
                    OUTPUT_VARIABLE absolute_source
                )
            endif()
            list(APPEND registered_sources "${absolute_source}")
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES registered_sources)

    file(
        GLOB_RECURSE discovered_sources
        CONFIGURE_DEPENDS
        ${project_source_patterns}
    )

    set(unregistered_sources)
    foreach(source IN LISTS discovered_sources)
        cmake_path(NORMAL_PATH source OUTPUT_VARIABLE absolute_source)
        if(NOT absolute_source IN_LIST registered_sources)
            cmake_path(
                RELATIVE_PATH absolute_source
                BASE_DIRECTORY "${PROJECT_SOURCE_DIR}"
                OUTPUT_VARIABLE relative_source
            )
            list(APPEND unregistered_sources "${relative_source}")
        endif()
    endforeach()

    if(unregistered_sources)
        list(JOIN unregistered_sources "\n  " missing_sources)
        message(
            FATAL_ERROR
            "First-party C++ sources are not registered with a target:\n"
            "  ${missing_sources}\n"
            "Add each source to the matching file under cmake/sources/."
        )
    endif()
endfunction()
