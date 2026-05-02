add_library(sr_integration_harness INTERFACE)
target_include_directories(sr_integration_harness INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_integration_harness test_integration_harness.cpp)
target_link_libraries(test_integration_harness PRIVATE sr_integration_harness)
add_test(NAME test_integration_harness COMMAND test_integration_harness)
set_tests_properties(test_integration_harness PROPERTIES LABELS "integration;integration:harness")

function(sr_add_feature_integration_test target source feature_label)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE sr_integration_harness ${ARGN})
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES LABELS "integration;${feature_label}")
endfunction()
