if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.28)
    project(StepCompareApplicationTests LANGUAGES CXX)
    include(CTest)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../src/domain"
                     "${CMAKE_CURRENT_BINARY_DIR}/domain")
    target_include_directories(stepcompare_domain
        PUBLIC
            ${CMAKE_CURRENT_LIST_DIR}/../../include)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../src/assembly"
                     "${CMAKE_CURRENT_BINARY_DIR}/assembly")
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../src/adapters/reporting"
                     "${CMAKE_CURRENT_BINARY_DIR}/reporting")
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../src/application"
                     "${CMAKE_CURRENT_BINARY_DIR}/application")
endif()

add_executable(stepcompare_application_tests
    comparison_coordinator_tests.cpp)

target_link_libraries(stepcompare_application_tests
    PRIVATE
        StepCompare::application
        StepCompare::reporting)

add_test(NAME stepcompare.application COMMAND stepcompare_application_tests)
