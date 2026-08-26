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

add_library(stepcompare_surface_deviation_occt STATIC
    occt_surface_deviation_engine.cpp)

add_library(StepCompare::surface_deviation_occt ALIAS
    stepcompare_surface_deviation_occt)

target_include_directories(stepcompare_surface_deviation_occt
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/..
        ${OpenCASCADE_INCLUDE_DIR})

target_link_libraries(stepcompare_surface_deviation_occt
    PUBLIC
        stepcompare_project_options
    PRIVATE
        TKMesh
        TKBRep
        TKTopAlgo
        TKGeomBase
        TKG3d
        TKG2d
        TKMath)
