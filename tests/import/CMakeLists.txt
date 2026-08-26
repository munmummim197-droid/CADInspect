if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.28)
    project(StepCompareImportTests LANGUAGES CXX)
    include(CTest)
    find_package(OpenCASCADE CONFIG REQUIRED)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../src/import"
                     "${CMAKE_CURRENT_BINARY_DIR}/step-import")
endif()

add_executable(stepcompare_step_import_tests
    occt_step_importer_tests.cpp)

target_include_directories(stepcompare_step_import_tests
    PRIVATE
        ${OpenCASCADE_INCLUDE_DIR})

target_link_libraries(stepcompare_step_import_tests
    PRIVATE
        StepCompare::step_import_occt
        TKDESTEP
        TKXCAF
        TKLCAF
        TKCAF
        TKPrim
        TKBRep)

add_test(NAME stepcompare.step_import COMMAND stepcompare_step_import_tests)

if(DEFINED VCPKG_TARGET_TRIPLET AND DEFINED VCPKG_INSTALLED_DIR)
    set(_vcpkg_triplet_root
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    set_tests_properties(stepcompare.step_import PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_vcpkg_triplet_root}/debug/bin;PATH=path_list_prepend:${_vcpkg_triplet_root}/bin")
endif()
