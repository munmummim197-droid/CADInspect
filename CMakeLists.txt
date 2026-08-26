cmake_minimum_required(VERSION 3.28)

project(
    StepCompare
    VERSION 0.1.0
    DESCRIPTION "Native Windows STEP geometry and assembly comparison"
    LANGUAGES CXX)

option(STEPCOMPARE_BUILD_TESTS "Build StepCompare automated tests" ON)
option(STEPCOMPARE_ENABLE_QT "Build the Qt desktop application" OFF)
option(STEPCOMPARE_ENABLE_OCCT "Build OpenCASCADE adapters" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(stepcompare_project_options INTERFACE)
target_compile_features(stepcompare_project_options INTERFACE cxx_std_20)

if(MSVC)
    target_compile_options(stepcompare_project_options INTERFACE
        /W4
        /permissive-
        /Zc:__cplusplus
        /utf-8)
else()
    target_compile_options(stepcompare_project_options INTERFACE
        -Wall
        -Wextra
        -Wpedantic)
endif()

add_subdirectory(src/domain)
add_subdirectory(src/adapters/scheduler)
add_subdirectory(src/adapters/reporting)

if(STEPCOMPARE_ENABLE_OCCT)
    find_package(OpenCASCADE CONFIG REQUIRED)
endif()

if(STEPCOMPARE_ENABLE_QT)
    find_package(Qt6 CONFIG REQUIRED COMPONENTS
        Core
        Concurrent
        Gui
        OpenGL
        OpenGLWidgets
        Widgets)
endif()

if(STEPCOMPARE_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests/domain)
    add_subdirectory(tests/scheduler)
    add_subdirectory(tests/reporting)
    if(STEPCOMPARE_ENABLE_OCCT AND STEPCOMPARE_ENABLE_QT)
        add_subdirectory(tests/integration)
    endif()
endif()
