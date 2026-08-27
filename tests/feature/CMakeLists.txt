add_executable(stepcompare_feature_tests
    occt_feature_recognizer_tests.cpp)

add_executable(stepcompare_feature_physical_matrix_tests
    physical_feature_matrix_tests.cpp)

target_include_directories(stepcompare_feature_tests
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../src"
        ${OpenCASCADE_INCLUDE_DIR})

target_include_directories(stepcompare_feature_physical_matrix_tests
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../src"
        ${OpenCASCADE_INCLUDE_DIR})

target_link_libraries(stepcompare_feature_tests
    PRIVATE
        StepCompare::feature_occt
        StepCompare::step_import_occt
        TKBO
        TKBool
        TKBRep
        TKFillet
        TKG3d
        TKGeomBase
        TKMath
        TKPrim
        TKTopAlgo)

target_link_libraries(stepcompare_feature_physical_matrix_tests
    PRIVATE
        StepCompare::application
        StepCompare::deep_occt
        StepCompare::surface_deviation_occt
        StepCompare::feature_occt
        StepCompare::step_import_occt
        TKBO
        TKBool
        TKBRep
        TKFillet
        TKG3d
        TKGeomBase
        TKMath
        TKPrim
        TKTopAlgo
        TKDESTEP)

add_test(NAME stepcompare.feature_recognition COMMAND stepcompare_feature_tests)
add_test(NAME stepcompare.feature_physical_matrix
    COMMAND stepcompare_feature_physical_matrix_tests)

if(DEFINED VCPKG_TARGET_TRIPLET AND DEFINED VCPKG_INSTALLED_DIR)
    set(_vcpkg_triplet_root
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    set_tests_properties(stepcompare.feature_recognition PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_vcpkg_triplet_root}/debug/bin;PATH=path_list_prepend:${_vcpkg_triplet_root}/bin")
    set_tests_properties(stepcompare.feature_physical_matrix PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_vcpkg_triplet_root}/debug/bin;PATH=path_list_prepend:${_vcpkg_triplet_root}/bin")
endif()
