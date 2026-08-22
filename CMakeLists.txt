cmake_minimum_required(VERSION 3.20)
project(shaped_native VERSION 1.0.0 LANGUAGES C RC)

add_executable(shaped WIN32
    src/main.c
    src/shaped.rc
)

target_compile_features(shaped PRIVATE c_std_11)
target_compile_definitions(shaped PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_compile_options(shaped PRIVATE -Wall -Wextra -Wpedantic)
target_link_options(shaped PRIVATE -static -static-libgcc -municode)
target_link_libraries(shaped PRIVATE gdi32 user32)

set_target_properties(shaped PROPERTIES
    OUTPUT_NAME "Shaped"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
)

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME shaped_headless_regression
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/regression.py"
                    "$<TARGET_FILE:shaped>" "${CMAKE_SOURCE_DIR}"
        )
        set_tests_properties(shaped_headless_regression PROPERTIES TIMEOUT 60)
    endif()
endif()
