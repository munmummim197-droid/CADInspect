add_executable(stepcompare_gui WIN32
    comparison_runner.cpp
    comparison_runner.hpp
    comparison_readability_model.cpp
    comparison_readability_model.hpp
    comparison_results_panel.cpp
    comparison_results_panel.hpp
    dropped_step_files.cpp
    dropped_step_files.hpp
    main.cpp
    ../../resources/stepcompare.qrc
    pair_isolation_model.cpp
    pair_isolation_model.hpp
    component_tree_panel.cpp
    component_tree_panel.hpp
    preview_status_widget.cpp
    preview_status_widget.hpp
    preview_quality.cpp
    preview_quality.hpp
    section_view_controls.cpp
    section_view_controls.hpp
    step_preview_loader.cpp
    step_preview_loader.hpp
    step_preview_model.cpp
    step_preview_model.hpp
    step_preview_scene_adapter.cpp
    step_preview_scene_adapter.hpp
    main_window.cpp
    main_window.hpp
    viewer_actions.cpp
    viewer_actions.hpp
    viewer_control_bar.cpp
    viewer_control_bar.hpp)

if(WIN32)
    target_sources(stepcompare_gui PRIVATE
        ../../resources/windows/stepcompare.rc)
    include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/CADInspectWindowsVersion.cmake")
    cadinspect_add_windows_version_resource(
        stepcompare_gui "CADInspect" "CADInspect.exe" "CADInspect")
endif()

set_target_properties(stepcompare_gui PROPERTIES
    AUTORCC ON
    OUTPUT_NAME CADInspect)

if(WIN32)
    function(stepcompare_deploy_qt_plugin plugin_target plugin_directory)
        if(NOT TARGET ${plugin_target})
            message(FATAL_ERROR "Required Qt plugin target is unavailable: ${plugin_target}")
        endif()

        add_custom_command(TARGET stepcompare_gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:stepcompare_gui>/${plugin_directory}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${plugin_target}>"
                "$<TARGET_FILE_DIR:stepcompare_gui>/${plugin_directory}/$<TARGET_FILE_NAME:${plugin_target}>"
            VERBATIM)
    endfunction()

    stepcompare_deploy_qt_plugin(Qt6::QWindowsIntegrationPlugin platforms)
    stepcompare_deploy_qt_plugin(Qt6::QICOPlugin imageformats)
    stepcompare_deploy_qt_plugin(Qt6::QGifPlugin imageformats)
    stepcompare_deploy_qt_plugin(Qt6::QModernWindowsStylePlugin styles)
endif()

target_link_libraries(stepcompare_gui
    PRIVATE
        StepCompare::viewer
        StepCompare::application
        StepCompare::cache
        StepCompare::reporting
        StepCompare::deep_occt
        StepCompare::surface_deviation_occt
        StepCompare::feature_occt
        StepCompare::step_import_occt
        TKernel
        TKMath
        TKBRep
        TKMesh
        Qt6::Core
        Qt6::Concurrent
        Qt6::Gui
        Qt6::Widgets)

if(WIN32)
    target_link_libraries(stepcompare_gui PRIVATE Ole32 Propsys Shell32)
endif()

target_include_directories(stepcompare_gui PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../src")

if(TARGET stepcompare_project_options)
    target_link_libraries(stepcompare_gui PRIVATE stepcompare_project_options)
else()
    target_compile_features(stepcompare_gui PRIVATE cxx_std_20)
endif()
