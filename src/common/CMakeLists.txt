# Common shared library.
# Production dependencies are exposed through sr_dependencies and are disabled
# by default. The fallback runtime remains dependency-free for unit tests.

add_library(sr_common STATIC
    admin/admin_service.cpp
    admin/dependency_health.cpp
    admin/admin_endpoint_handler.cpp
    admin/admin_http_handler.cpp
    admin/lifecycle.cpp
    config/config.cpp
    logging/structured_log.cpp
    spatial/h3_index.cpp
    spatial/haversine.cpp
    metrics/metrics.cpp
    clients/redis_client.cpp
    clients/postgres_client.cpp
    kafka/kafka_producer.cpp
    kafka/kafka_consumer.cpp
    proto/domain_conversions.cpp
    proto/location_payload_codec.cpp
    proto/geofence_payload_codec.cpp
    proto/matching_payload_codec.cpp
    composition/composition_root.cpp
    composition/metrics_event_handlers.cpp
)

if(SR_ENABLE_PROTOBUF)
    target_sources(sr_common PRIVATE proto/generated_conversions.cpp)
endif()

target_include_directories(sr_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..    # So services can #include "common/..."
)

target_link_libraries(sr_common PUBLIC
    sr_dependencies
    signalroute_proto
)
