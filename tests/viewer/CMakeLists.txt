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
