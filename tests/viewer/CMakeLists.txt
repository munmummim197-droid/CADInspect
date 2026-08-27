if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.28)
    project(StepCompareViewerStandalone LANGUAGES CXX)

    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    include(CTest)

    find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Gui Widgets)
    find_package(OpenCASCADE CONFIG REQUIRED)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../src/viewer" viewer)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../apps/gui" gui)
endif()

add_executable(stepcompare_viewer_state_tests
    viewer_state_tests.cpp)

target_link_libraries(stepcompare_viewer_state_tests
    PRIVATE
        StepCompare::viewer)

add_test(NAME stepcompare.viewer.state COMMAND stepcompare_viewer_state_tests)

add_executable(stepcompare_viewer_deviation_coloring_tests
    deviation_coloring_tests.cpp)

target_link_libraries(stepcompare_viewer_deviation_coloring_tests
    PRIVATE
        StepCompare::viewer)

add_test(NAME stepcompare.viewer.deviation_coloring
    COMMAND stepcompare_viewer_deviation_coloring_tests)

add_executable(stepcompare_viewer_selection_presenter_tests
    selection_presenter_tests.cpp)

target_link_libraries(stepcompare_viewer_selection_presenter_tests
    PRIVATE
        StepCompare::viewer)

add_test(NAME stepcompare.viewer.selection_presenter
    COMMAND stepcompare_viewer_selection_presenter_tests)

add_executable(stepcompare_viewer_preview_load_state_tests
    preview_load_state_tests.cpp)

target_link_libraries(stepcompare_viewer_preview_load_state_tests
    PRIVATE
        StepCompare::viewer)

add_test(NAME stepcompare.viewer.preview_load_state
    COMMAND stepcompare_viewer_preview_load_state_tests)

add_executable(stepcompare_viewer_step_preview_model_tests
    step_preview_model_tests.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../apps/gui/step_preview_model.cpp")

target_include_directories(stepcompare_viewer_step_preview_model_tests
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../apps/gui")

target_link_libraries(stepcompare_viewer_step_preview_model_tests
    PRIVATE
        StepCompare::viewer)

add_test(NAME stepcompare.viewer.step_preview_model
    COMMAND stepcompare_viewer_step_preview_model_tests)

add_executable(stepcompare_viewer_comparison_readability_tests
    comparison_readability_tests.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../apps/gui/comparison_readability_model.cpp")

target_include_directories(stepcompare_viewer_comparison_readability_tests
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../apps/gui")

target_link_libraries(stepcompare_viewer_comparison_readability_tests
    PRIVATE
        StepCompare::viewer
        Qt6::Core
        Qt6::Gui)

add_test(NAME stepcompare.viewer.comparison_readability
    COMMAND stepcompare_viewer_comparison_readability_tests)

add_executable(stepcompare_viewer_preview_quality_tests
    preview_quality_tests.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../apps/gui/preview_quality.cpp")

target_include_directories(stepcompare_viewer_preview_quality_tests
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../apps/gui"
        "${CMAKE_CURRENT_LIST_DIR}/../../src")

target_link_libraries(stepcompare_viewer_preview_quality_tests
    PRIVATE
        StepCompare::viewer
        StepCompare::step_import_occt
        TKMesh
        TKTopAlgo
        TKPrim
        TKBRep)

add_test(NAME stepcompare.viewer.preview_quality
    COMMAND stepcompare_viewer_preview_quality_tests)
