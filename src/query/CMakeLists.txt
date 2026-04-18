# Query service
add_library(sr_query STATIC
    query_service.cpp
    latest_handler.cpp
    nearby_handler.cpp
    trip_handler.cpp
)
target_include_directories(sr_query PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(sr_query PUBLIC sr_common)
