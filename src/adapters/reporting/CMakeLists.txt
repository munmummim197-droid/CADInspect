add_library(stepcompare_reporting STATIC
    writers.cpp)

add_library(StepCompare::reporting ALIAS stepcompare_reporting)

get_filename_component(STEPCOMPARE_REPORTING_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

target_include_directories(stepcompare_reporting
    PUBLIC
        $<BUILD_INTERFACE:${STEPCOMPARE_REPORTING_ROOT}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(stepcompare_reporting
    PUBLIC
        stepcompare_project_options)
