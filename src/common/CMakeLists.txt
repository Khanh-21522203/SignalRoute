# Common shared library.
# Production dependencies are exposed through sr_dependencies and are disabled
# by default. The fallback runtime remains dependency-free for unit tests.

add_library(sr_common STATIC
    config/config.cpp
    spatial/h3_index.cpp
    spatial/haversine.cpp
    metrics/metrics.cpp
    clients/redis_client.cpp
    clients/postgres_client.cpp
    kafka/kafka_producer.cpp
    kafka/kafka_consumer.cpp
    composition/composition_root.cpp
    composition/metrics_event_handlers.cpp
)

target_include_directories(sr_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..    # So services can #include "common/..."
)

target_link_libraries(sr_common PUBLIC
    sr_dependencies
    signalroute_proto
)
