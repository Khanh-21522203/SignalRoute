# Geofence engine
add_library(sr_geofence STATIC
    geofence_engine.cpp
    fence_registry.cpp
    evaluator.cpp
    point_in_polygon.cpp
    dwell_checker.cpp
)
target_include_directories(sr_geofence PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(sr_geofence PUBLIC sr_common)
