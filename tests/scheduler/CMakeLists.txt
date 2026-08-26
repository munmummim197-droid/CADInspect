add_executable(stepcompare_scheduler_tests
    scheduler_tests.cpp)

target_link_libraries(stepcompare_scheduler_tests
    PRIVATE
        StepCompare::scheduler)

add_test(NAME stepcompare.scheduler COMMAND stepcompare_scheduler_tests)
set_tests_properties(stepcompare.scheduler PROPERTIES TIMEOUT 10)
