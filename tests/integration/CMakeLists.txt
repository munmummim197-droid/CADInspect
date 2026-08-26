add_executable(stepcompare_dependency_smoke
    dependency_smoke.cpp)

target_link_libraries(stepcompare_dependency_smoke
    PRIVATE
        stepcompare_project_options
        Qt6::Core
        TKDESTEP
        TKXCAF
        TKPrim
        TKMesh
        TKTopAlgo)

add_test(NAME stepcompare.dependencies COMMAND stepcompare_dependency_smoke)

set(_vcpkg_triplet_root
    "${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")
set_tests_properties(stepcompare.dependencies PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${_vcpkg_triplet_root}/debug/bin;PATH=path_list_prepend:${_vcpkg_triplet_root}/bin")

