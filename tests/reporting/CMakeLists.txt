if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.28)
    project(StepCompareReportingTests LANGUAGES CXX)

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

    get_filename_component(STEPCOMPARE_REPORTING_TEST_ROOT
        "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
    add_subdirectory(
        "${STEPCOMPARE_REPORTING_TEST_ROOT}/src/adapters/reporting"
        "${CMAKE_CURRENT_BINARY_DIR}/reporting")
    include(CTest)
endif()

add_executable(stepcompare_reporting_tests
    reporting_tests.cpp)

target_link_libraries(stepcompare_reporting_tests
    PRIVATE
        StepCompare::reporting)

if(TARGET Qt6::Core)
    target_link_libraries(stepcompare_reporting_tests PRIVATE Qt6::Core)
    target_compile_definitions(stepcompare_reporting_tests
        PRIVATE STEPCOMPARE_TEST_QT_JSON=1)
endif()

add_test(NAME stepcompare.reporting COMMAND stepcompare_reporting_tests)
