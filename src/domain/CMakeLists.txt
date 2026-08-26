add_library(stepcompare_domain STATIC
    result_reducer.cpp)

add_library(StepCompare::domain ALIAS stepcompare_domain)

target_include_directories(stepcompare_domain
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(stepcompare_domain
    PUBLIC
        stepcompare_project_options)

