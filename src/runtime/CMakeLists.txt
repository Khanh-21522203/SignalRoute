add_library(sr_runtime STATIC
    runtime_application.cpp
)
target_include_directories(sr_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(sr_runtime PUBLIC
    sr_common
    sr_gateway
    sr_processor
    sr_query
    sr_geofence
    sr_matching
)
