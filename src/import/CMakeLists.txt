if(NOT TARGET stepcompare_project_options)
    add_library(stepcompare_project_options INTERFACE)
    target_compile_features(stepcompare_project_options INTERFACE cxx_std_20)
    if(MSVC)
        target_compile_options(stepcompare_project_options INTERFACE
            /W4 /permissive- /Zc:__cplusplus /utf-8)
    endif()
endif()

if(NOT OpenCASCADE_FOUND)
    find_package(OpenCASCADE CONFIG REQUIRED)
endif()

add_library(stepcompare_step_import_occt STATIC
    occt_step_importer.cpp)

add_library(StepCompare::step_import_occt ALIAS stepcompare_step_import_occt)

target_include_directories(stepcompare_step_import_occt
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(stepcompare_step_import_occt
    PUBLIC
        stepcompare_project_options
    PRIVATE
        TKDESTEP
        TKXCAF
        TKLCAF
        TKCAF
        TKVCAF
        TKPrim
        TKBO
        TKBool
        TKBRep
        TKG3d
        TKG2d
        TKGeomBase
        TKMath
        TKTopAlgo)
