# Background workers
add_library(sr_workers STATIC
    h3_cleanup_worker.cpp
    dlq_replay_worker.cpp
    metrics_reporter.cpp
)
target_include_directories(sr_workers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(sr_workers PUBLIC sr_common)
