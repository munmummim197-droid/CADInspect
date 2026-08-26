add_library(stepcompare_cli_contract STATIC
    arguments.cpp)

add_library(StepCompare::cli_contract ALIAS stepcompare_cli_contract)

get_filename_component(STEPCOMPARE_CLI_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

target_include_directories(stepcompare_cli_contract
    PUBLIC
        $<BUILD_INTERFACE:${STEPCOMPARE_CLI_ROOT}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(stepcompare_cli_contract
    PUBLIC
        stepcompare_project_options)

add_executable(stepcompare_cli
    main.cpp)

set_target_properties(stepcompare_cli PROPERTIES
    OUTPUT_NAME "stepcompare-cli")

target_link_libraries(stepcompare_cli
    PRIVATE
        StepCompare::cli_contract)
