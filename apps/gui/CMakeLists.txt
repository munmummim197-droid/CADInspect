add_executable(stepcompare_gui WIN32
    main.cpp
    component_tree_panel.cpp
    component_tree_panel.hpp
    main_window.cpp
    main_window.hpp
    viewer_actions.cpp
    viewer_actions.hpp)

set_target_properties(stepcompare_gui PROPERTIES OUTPUT_NAME StepCompare)

target_link_libraries(stepcompare_gui
    PRIVATE
        StepCompare::viewer
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets)

if(TARGET stepcompare_project_options)
    target_link_libraries(stepcompare_gui PRIVATE stepcompare_project_options)
else()
    target_compile_features(stepcompare_gui PRIVATE cxx_std_20)
endif()
