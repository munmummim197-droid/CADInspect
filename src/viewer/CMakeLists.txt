set(_stepcompare_root "${CMAKE_CURRENT_LIST_DIR}/../..")

add_library(stepcompare_viewer STATIC
    viewer_state.cpp
    preview_load_state.cpp
    selection_presenter.cpp
    occt_viewer_widget.cpp
    "${_stepcompare_root}/include/stepcompare/viewer/viewer_state.hpp"
    "${_stepcompare_root}/include/stepcompare/viewer/preview_load_state.hpp"
    "${_stepcompare_root}/include/stepcompare/viewer/selection_presenter.hpp"
    "${_stepcompare_root}/include/stepcompare/viewer/occt_viewer_widget.hpp")

add_library(StepCompare::viewer ALIAS stepcompare_viewer)

target_include_directories(stepcompare_viewer
    PUBLIC
        $<BUILD_INTERFACE:${_stepcompare_root}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(stepcompare_viewer
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
    PRIVATE
        TKernel
        TKMath
        TKG2d
        TKG3d
        TKBRep
        TKService
        TKV3d
        TKOpenGl)

if(TARGET stepcompare_project_options)
    target_link_libraries(stepcompare_viewer PUBLIC stepcompare_project_options)
else()
    target_compile_features(stepcompare_viewer PUBLIC cxx_std_20)
endif()
