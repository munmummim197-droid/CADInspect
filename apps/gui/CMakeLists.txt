add_executable(stepcompare_gui WIN32
    comparison_runner.cpp
    comparison_runner.hpp
    main.cpp
    component_tree_panel.cpp
    component_tree_panel.hpp
    preview_status_widget.cpp
    preview_status_widget.hpp
    step_preview_loader.cpp
    step_preview_loader.hpp
    step_preview_model.cpp
    step_preview_model.hpp
    step_preview_scene_adapter.cpp
    step_preview_scene_adapter.hpp
    main_window.cpp
    main_window.hpp
    viewer_actions.cpp
    viewer_actions.hpp)

set_target_properties(stepcompare_gui PROPERTIES OUTPUT_NAME StepCompare)

target_link_libraries(stepcompare_gui
    PRIVATE
        StepCompare::viewer
        StepCompare::application
        StepCompare::cache
        StepCompare::reporting
        StepCompare::deep_occt
        StepCompare::surface_deviation_occt
        StepCompare::step_import_occt
        TKernel
        TKMath
        TKBRep
        Qt6::Core
        Qt6::Concurrent
        Qt6::Gui
        Qt6::Widgets)

target_include_directories(stepcompare_gui PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../src")

if(TARGET stepcompare_project_options)
    target_link_libraries(stepcompare_gui PRIVATE stepcompare_project_options)
else()
    target_compile_features(stepcompare_gui PRIVATE cxx_std_20)
endif()
