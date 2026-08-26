if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.28)
    project(StepCompareSurfaceDeviationTests LANGUAGES CXX)
    include(CTest)
    find_package(OpenCASCADE CONFIG REQUIRED)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../src/deviation"
                     "${CMAKE_CURRENT_BINARY_DIR}/surface-deviation")
endif()

add_executable(stepcompare_surface_deviation_tests
    occt_surface_deviation_tests.cpp)

target_include_directories(stepcompare_surface_deviation_tests
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../src
        ${OpenCASCADE_INCLUDE_DIR})

target_link_libraries(stepcompare_surface_deviation_tests
    PRIVATE
        StepCompare::surface_deviation_occt
        TKPrim
        TKBRep
        TKG3d
        TKMath)

add_test(NAME stepcompare.surface_deviation
         COMMAND stepcompare_surface_deviation_tests)

if(DEFINED VCPKG_TARGET_TRIPLET AND DEFINED VCPKG_INSTALLED_DIR)
    set(_vcpkg_triplet_root
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    set_tests_properties(stepcompare.surface_deviation PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_vcpkg_triplet_root}/debug/bin;PATH=path_list_prepend:${_vcpkg_triplet_root}/bin")
endif()
