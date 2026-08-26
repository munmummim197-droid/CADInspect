if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.28)
    project(StepCompareDeepTests LANGUAGES CXX)
    include(CTest)
    find_package(OpenCASCADE CONFIG REQUIRED)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../src/import"
                     "${CMAKE_CURRENT_BINARY_DIR}/step-import")
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../src/deep"
                     "${CMAKE_CURRENT_BINARY_DIR}/deep")
endif()

add_executable(stepcompare_deep_tests
    occt_deep_geometry_engine_tests.cpp)

target_include_directories(stepcompare_deep_tests
    PRIVATE
        ${OpenCASCADE_INCLUDE_DIR})

target_link_libraries(stepcompare_deep_tests
    PRIVATE
        StepCompare::deep_occt
        StepCompare::step_import_occt
        TKDESTEP
        TKPrim
        TKBRep
        TKG3d)

add_test(NAME stepcompare.deep_geometry COMMAND stepcompare_deep_tests)

if(DEFINED VCPKG_TARGET_TRIPLET AND DEFINED VCPKG_INSTALLED_DIR)
    set(_vcpkg_triplet_root
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    set_tests_properties(stepcompare.deep_geometry PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_vcpkg_triplet_root}/debug/bin;PATH=path_list_prepend:${_vcpkg_triplet_root}/bin")
endif()
