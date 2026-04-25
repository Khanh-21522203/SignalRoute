# Common shared library
# Depends on: protobuf, h3, hiredis, libpq, cppkafka, prometheus-cpp, toml++

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
)

target_include_directories(sr_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..    # So services can #include "common/..."
)

# TODO: Link external dependencies once integrated
# target_link_libraries(sr_common PUBLIC
#     signalroute_proto
#     h3::h3
#     hiredis::hiredis
#     redis++::redis++
#     PostgreSQL::PostgreSQL
#     RdKafka::rdkafka++
#     prometheus-cpp::core
#     tomlplusplus::tomlplusplus
# )
