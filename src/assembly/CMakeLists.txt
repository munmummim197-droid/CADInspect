add_library(stepcompare_assembly STATIC
    assembly_index.cpp
    component_matching.cpp)

add_library(StepCompare::assembly ALIAS stepcompare_assembly)

get_filename_component(STEPCOMPARE_ASSEMBLY_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

target_include_directories(stepcompare_assembly
    PUBLIC
        $<BUILD_INTERFACE:${STEPCOMPARE_ASSEMBLY_ROOT}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(stepcompare_assembly
    PUBLIC
        StepCompare::domain
        stepcompare_project_options)
