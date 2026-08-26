add_library(stepcompare_scheduler STATIC
    bounded_task_scheduler.cpp)

add_library(StepCompare::scheduler ALIAS stepcompare_scheduler)

target_include_directories(stepcompare_scheduler
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(stepcompare_scheduler
    PUBLIC
        stepcompare_project_options)

