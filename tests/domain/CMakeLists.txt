add_executable(stepcompare_domain_tests
    domain_tests.cpp)

target_link_libraries(stepcompare_domain_tests
    PRIVATE
        StepCompare::domain)

add_test(NAME stepcompare.domain COMMAND stepcompare_domain_tests)

