if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.28)
    project(StepCompareCliTests LANGUAGES CXX)

    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    add_library(stepcompare_project_options INTERFACE)
    target_compile_features(stepcompare_project_options INTERFACE cxx_std_20)
    if(MSVC)
        target_compile_options(stepcompare_project_options INTERFACE
            /W4 /permissive- /Zc:__cplusplus /utf-8)
    else()
        target_compile_options(stepcompare_project_options INTERFACE
            -Wall -Wextra -Wpedantic)
    endif()

    get_filename_component(STEPCOMPARE_CLI_TEST_ROOT
        "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
    add_subdirectory(
        "${STEPCOMPARE_CLI_TEST_ROOT}/apps/cli"
        "${CMAKE_CURRENT_BINARY_DIR}/cli")
    include(CTest)
endif()

add_executable(stepcompare_cli_tests
    cli_tests.cpp)

target_link_libraries(stepcompare_cli_tests
    PRIVATE
        StepCompare::cli_contract)

add_test(NAME stepcompare.cli COMMAND stepcompare_cli_tests)
