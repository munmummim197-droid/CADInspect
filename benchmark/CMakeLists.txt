cmake_minimum_required(VERSION 3.28)

project(StepCompareScaleFixtureGenerator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCASCADE CONFIG REQUIRED)

add_executable(stepcompare_fixture_generator
    fixture_generator.cpp)

add_executable(stepcompare_viewer_quality_fixture_generator
    viewer_quality_fixture_generator.cpp)

target_include_directories(stepcompare_fixture_generator
    PRIVATE
        ${OpenCASCADE_INCLUDE_DIR})

target_include_directories(stepcompare_viewer_quality_fixture_generator
    PRIVATE
        ${OpenCASCADE_INCLUDE_DIR})

target_link_libraries(stepcompare_fixture_generator
    PRIVATE
        TKDESTEP
        TKXCAF
        TKLCAF
        TKCAF
        TKVCAF
        TKPrim
        TKBRep
        TKG3d
        TKG2d
        TKGeomBase
        TKMath)

target_link_libraries(stepcompare_viewer_quality_fixture_generator
    PRIVATE
        TKDESTEP
        TKXCAF
        TKLCAF
        TKCAF
        TKVCAF
        TKBO
        TKBool
        TKFillet
        TKPrim
        TKTopAlgo
        TKBRep
        TKG3d
        TKG2d
        TKGeomBase
        TKMath)

if(MSVC)
    target_compile_options(stepcompare_fixture_generator PRIVATE
        /W4 /permissive- /Zc:__cplusplus /utf-8)
    target_compile_options(stepcompare_viewer_quality_fixture_generator PRIVATE
        /W4 /permissive- /Zc:__cplusplus /utf-8)
endif()
