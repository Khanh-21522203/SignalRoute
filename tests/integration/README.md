# Integration Test Harness

Integration tests are service-backed and disabled by default:

```sh
cmake -S . -B /tmp/signalroute-integration-build \
  -DSR_BUILD_TESTS=ON \
  -DSR_BUILD_INTEGRATION_TESTS=ON
cmake --build /tmp/signalroute-integration-build --target test_integration_harness -j2
ctest --test-dir /tmp/signalroute-integration-build -L integration --output-on-failure
```

Feature groups are intentionally named by behavior, not by phase or batch:

| Feature group | CTest label | Required services | Production switches |
|---|---|---|---|
| `ingestion_pipeline` | `integration:ingestion` | Redpanda | `SR_ENABLE_REAL_KAFKA`, `SR_ENABLE_PROTOBUF` |
| `state_persistence` | `integration:state` | Redis | `SR_ENABLE_REAL_REDIS` |
| `trip_history` | `integration:trip-history` | PostGIS | `SR_ENABLE_REAL_POSTGIS` |
| `nearby_query` | `integration:nearby` | Redis, PostGIS, H3 | `SR_ENABLE_REAL_REDIS`, `SR_ENABLE_REAL_POSTGIS`, `SR_ENABLE_REAL_H3` |
| `geofence_events` | `integration:geofence` | Redpanda, PostGIS, H3 | `SR_ENABLE_REAL_KAFKA`, `SR_ENABLE_REAL_POSTGIS`, `SR_ENABLE_REAL_H3`, `SR_ENABLE_PROTOBUF` |
| `matching_reservation` | `integration:matching` | Redpanda, Redis | `SR_ENABLE_REAL_KAFKA`, `SR_ENABLE_REAL_REDIS`, `SR_ENABLE_PROTOBUF` |

Add real service tests with `sr_add_feature_integration_test(...)` in `tests/integration/CMakeLists.txt`. Unit tests must stay dependency-free; do not place Redis, Kafka, PostGIS, H3, or gRPC requirements under `tests/unit/`.

## Kafka Ingestion

The Kafka ingestion integration is compiled only when real Kafka is enabled:

```sh
cmake -S . -B /tmp/signalroute-ingestion-build \
  -DSR_BUILD_TESTS=ON \
  -DSR_BUILD_INTEGRATION_TESTS=ON \
  -DSR_ENABLE_REAL_KAFKA=ON \
  -DSR_ENABLE_PROTOBUF=ON
cmake --build /tmp/signalroute-ingestion-build --target test_ingestion_pipeline -j2
SIGNALROUTE_RUN_KAFKA_INTEGRATION=1 \
SIGNALROUTE_KAFKA_BROKERS=127.0.0.1:9092 \
ctest --test-dir /tmp/signalroute-ingestion-build -L integration:ingestion --output-on-failure
```

If `SIGNALROUTE_RUN_KAFKA_INTEGRATION=1` or `SIGNALROUTE_KAFKA_BROKERS` is absent, the test exits with CTest skip code `77`. This keeps default local verification dependency-free while allowing manual Redpanda-backed runs.
