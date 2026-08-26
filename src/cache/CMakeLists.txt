add_library(stepcompare_cache STATIC cache_key.cpp)
add_library(StepCompare::cache ALIAS stepcompare_cache)
target_include_directories(stepcompare_cache PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(stepcompare_cache PUBLIC stepcompare_project_options)
