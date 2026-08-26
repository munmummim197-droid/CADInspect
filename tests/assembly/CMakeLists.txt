if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.28)
    project(StepCompareAssemblyTests LANGUAGES CXX)

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

    get_filename_component(STEPCOMPARE_ASSEMBLY_TEST_ROOT
        "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
    add_subdirectory(
        "${STEPCOMPARE_ASSEMBLY_TEST_ROOT}/src/domain"
        "${CMAKE_CURRENT_BINARY_DIR}/domain")
    add_subdirectory(
        "${STEPCOMPARE_ASSEMBLY_TEST_ROOT}/src/assembly"
        "${CMAKE_CURRENT_BINARY_DIR}/assembly")
    include(CTest)
endif()

add_executable(stepcompare_assembly_tests
    assembly_tests.cpp)

target_link_libraries(stepcompare_assembly_tests
    PRIVATE
        StepCompare::assembly)

add_test(NAME stepcompare.assembly COMMAND stepcompare_assembly_tests)
