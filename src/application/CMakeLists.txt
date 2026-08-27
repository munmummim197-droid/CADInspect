if(NOT TARGET stepcompare_project_options)
    add_library(stepcompare_project_options INTERFACE)
    target_compile_features(stepcompare_project_options INTERFACE cxx_std_20)
    if(MSVC)
        target_compile_options(stepcompare_project_options INTERFACE
            /W4 /permissive- /Zc:__cplusplus /utf-8)
    else()
        target_compile_options(stepcompare_project_options INTERFACE
            -Wall -Wextra -Wpedantic)
    endif()
endif()

add_library(stepcompare_application STATIC
    comparison_coordinator.cpp
    feature_evidence.cpp
    feature_evidence.hpp)

add_library(StepCompare::application ALIAS stepcompare_application)

target_include_directories(stepcompare_application
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(stepcompare_application
    PUBLIC
        StepCompare::assembly
        StepCompare::cache
        StepCompare::domain
        stepcompare_project_options)
