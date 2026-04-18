add_executable(sr_tests
    unit/test_haversine.cpp
    unit/test_point_in_polygon.cpp
    unit/test_validator.cpp
)

target_link_libraries(sr_tests
    PRIVATE
        sr_common
        sr_gateway
        sr_processor
        sr_query
        sr_geofence
        sr_matching
)

# TODO: Add GTest/Catch2 include directories and link libraries here once integrated

add_test(NAME sr_tests COMMAND sr_tests)
