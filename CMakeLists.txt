cmake_minimum_required(VERSION 3.20)
project(shaped_cli VERSION 1.0.1 LANGUAGES C)

add_executable(shaped src/cli.c)

# Keep Windows Explorer metadata and the application icon without making the
# CLI depend on any Win32 APIs at runtime.
if(WIN32)
    enable_language(RC)
    target_sources(shaped PRIVATE src/shaped.rc)
endif()

target_compile_features(shaped PRIVATE c_std_11)
target_compile_options(shaped PRIVATE -Wall -Wextra -Wpedantic)
if(NOT MSVC)
    target_link_libraries(shaped PRIVATE m)
endif()

set_target_properties(shaped PROPERTIES
    OUTPUT_NAME "Shaped"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
)

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME shaped_cli_regression
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/regression.py"
                    "$<TARGET_FILE:shaped>" "${CMAKE_SOURCE_DIR}"
        )
        set_tests_properties(shaped_cli_regression PROPERTIES TIMEOUT 60)
    endif()
endif()
