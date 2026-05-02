# Package Strategy Lock

## Purpose
This file locks the package-backed adapter conventions for the remaining production work. It does not enable real dependencies by default. It defines how future batches add package-backed Docker Bake targets, manual CI jobs, and feature-grouped integration tests without changing fallback/unit behavior.

## Global Rules
- Default local configure remains dependency-free: `cmake -S . -B /tmp/signalroute-build -DSR_BUILD_TESTS=ON`.
- Integration tests are opt-in with `-DSR_BUILD_INTEGRATION_TESTS=ON` and are grouped by feature/function labels.
- Package-backed image jobs are manual-only workflow_dispatch jobs until the corresponding integration tests are stable.
- Do not enable `SR_ENABLE_REAL_*`, `SR_ENABLE_GRPC`, `SR_ENABLE_PROMETHEUS`, or `SR_ENABLE_TOMLPLUSPLUS` in default push/pull-request CI.
- Docker image package installation is allowed inside `Dockerfile.adapters` builds; do not require developers to install dependencies locally for default verification.
- A package-backed adapter target must include both build-stage packages and runtime-stage shared libraries when the linked binary needs them.
- CSV fallback decoding remains compatibility scaffolding until durable Kafka/protobuf integration tests pass for each production boundary.

## Locked Naming

| Area | Convention | Example |
|---|---|---|
| Docker Bake target | `adapter-<dependency-or-feature>` | `adapter-protobuf`, `adapter-kafka` |
| Docker image tag | `signalroute:adapter-<dependency-or-feature>` | `signalroute:adapter-kafka` |
| Workflow dispatch input | `run_adapter_<dependency>_image` or `run_<feature>_integration` | `run_adapter_kafka_image`, `run_ingestion_integration` |
| Manual CI job | `<feature-or-dependency> integration` or `adapter <dependency> image` | `ingestion integration`, `adapter kafka image` |
| CTest label | `integration:<feature>` | `integration:ingestion` |
| Test executable | `test_<feature_or_adapter_behavior>` | `test_ingestion_pipeline` |

## Package Matrix

Ubuntu 24.04 package names in this table were checked against the Ubuntu package index on 2026-05-02. Re-check before enabling a CI job because package availability can differ by runner image, architecture, and enabled repositories.

| Dependency | CMake switch | CMake discovery | Target candidates | Ubuntu 24.04 build packages for adapter images | Runtime packages | First integration labels |
|---|---|---|---|---|---|---|
| Protobuf | `SR_ENABLE_PROTOBUF` | `find_package(Protobuf REQUIRED)` | `protobuf::libprotobuf`, `protobuf::protoc` | `libprotobuf-dev protobuf-compiler` | `libprotobuf32t64` | `integration:ingestion`, `integration:geofence`, `integration:matching` |
| gRPC | `SR_ENABLE_GRPC` plus `SR_ENABLE_PROTOBUF` | `find_package(gRPC CONFIG REQUIRED)` | `gRPC::grpc++`, `gRPC::grpc_cpp_plugin` | `libgrpc++-dev protobuf-compiler-grpc` | `libgrpc++1.51t64 libgrpc29t64` | gateway/query/admin transport tests when server binding exists |
| Kafka | `SR_ENABLE_REAL_KAFKA` | `find_package(RdKafka REQUIRED)` | `RdKafka::rdkafka++`, `rdkafka++` | `librdkafka-dev`; Ubuntu 24.04 exposes headers/libs through pkg-config and is bridged by `cmake/FindRdKafka.cmake` | `librdkafka++1` | `integration:ingestion`, `integration:geofence`, `integration:matching` |
| Redis | `SR_ENABLE_REAL_REDIS` | `find_package(hiredis CONFIG REQUIRED)`, `find_package(redis++ CONFIG REQUIRED)` | `hiredis::hiredis`, `redis++::redis++` | `libhiredis-dev`; redis-plus-plus needs vcpkg, Conan, or a source/package overlay because no Ubuntu 24.04 archive package was found | `libhiredis1.1.0`; redis-plus-plus runtime depends on chosen provider | `integration:state`, `integration:nearby`, `integration:matching` |
| PostGIS/libpq | `SR_ENABLE_REAL_POSTGIS` | `find_package(PostgreSQL REQUIRED)` | `PostgreSQL::PostgreSQL` | `libpq-dev postgresql-client` | `libpq5` | `integration:trip-history`, `integration:nearby`, `integration:geofence` |
| H3 | `SR_ENABLE_REAL_H3` | `find_package(h3 CONFIG REQUIRED)` | `h3::h3`, `h3` | `libh3-dev` | `libh3-1` | `integration:nearby`, `integration:geofence` |
| Prometheus | `SR_ENABLE_PROMETHEUS` | `find_package(prometheus-cpp CONFIG REQUIRED)` | `prometheus-cpp::core`, `prometheus-cpp::pull` | `prometheus-cpp-dev` | `libprometheus-cpp-core1.0`, `libprometheus-cpp-pull1.0` | observability/admin integration tests |
| toml++ | `SR_ENABLE_TOMLPLUSPLUS` | `find_package(tomlplusplus CONFIG REQUIRED)` | `tomlplusplus::tomlplusplus`, `tomlplusplus_tomlplusplus`, `tomlplusplus` | `libtomlplusplus-dev` | none expected for header-only package | config parser integration tests |

Before enabling a new CI image job, verify the package names against the runner image with `apt-cache policy` or an equivalent package-index check inside the same Ubuntu base image. If a package is unavailable, keep the CMake switch gated and record the blocker instead of replacing the fallback implementation.

## Package Verification Results

| Batch | Dependency | Result | Action |
|---|---|---|---|
| 55 | Protobuf | `libprotobuf-dev`, `protobuf-compiler`, and `libprotobuf32t64` build/run successfully in `adapter-protobuf` | Keep protobuf image target available |
| 58/59 | Kafka | `librdkafka-dev` and `librdkafka++1` install, but no `RdKafkaConfig.cmake` or `rdkafka-config.cmake` is provided | Added explicit provider work in Batch 60 |
| 60 | Kafka | `cmake/FindRdKafka.cmake` bridges Ubuntu pkg-config/header/library installs to `RdKafka::rdkafka++`; `adapter-kafka` builds and smokes successfully | Keep Kafka image target available |
| 61/62 | Kafka + Protobuf | `integration-ingestion` builds `test_ingestion_pipeline` with real Kafka and protobuf, and Redpanda-backed run verifies produce/consume plus processor state/history writes and committed offsets | Use this as the ingestion regression while expanding real dependency coverage |

## Feature-Grouped Integration Labels

| Feature group | CTest label | Required services | Primary production switches |
|---|---|---|---|
| Ingestion pipeline | `integration:ingestion` | Redpanda | `SR_ENABLE_REAL_KAFKA`, `SR_ENABLE_PROTOBUF` |
| State persistence | `integration:state` | Redis | `SR_ENABLE_REAL_REDIS` |
| Trip history | `integration:trip-history` | PostGIS | `SR_ENABLE_REAL_POSTGIS` |
| Nearby query | `integration:nearby` | Redis, PostGIS, H3 | `SR_ENABLE_REAL_REDIS`, `SR_ENABLE_REAL_POSTGIS`, `SR_ENABLE_REAL_H3` |
| Geofence events | `integration:geofence` | Redpanda, PostGIS, H3 | `SR_ENABLE_REAL_KAFKA`, `SR_ENABLE_REAL_POSTGIS`, `SR_ENABLE_REAL_H3`, `SR_ENABLE_PROTOBUF` |
| Matching reservation | `integration:matching` | Redpanda, Redis | `SR_ENABLE_REAL_KAFKA`, `SR_ENABLE_REAL_REDIS`, `SR_ENABLE_PROTOBUF` |

## CI Promotion Path

1. Add or update a package-backed `docker-bake.hcl` target with explicit build and runtime packages.
2. Add a manual workflow_dispatch image job that validates `docker buildx bake --print`, builds the image, runs `ldd`, and performs a bounded smoke test.
3. Add service-backed integration tests under `tests/integration/` using the matching feature label.
4. Add a manual workflow_dispatch integration job that starts only the needed services and runs `ctest -L integration:<feature>`.
5. Promote the job to default CI only after it is stable and does not require unavailable external packages.

## Current Boundary
`adapter-protobuf` is the first working package-backed image path and enables only generated protobuf messages. `adapter-kafka` builds with Ubuntu 24.04 `librdkafka-dev` through the explicit `FindRdKafka.cmake` bridge and enables only `SR_ENABLE_REAL_KAFKA=ON`. `integration-ingestion` builds a Kafka+protobuf test image and the Redpanda-backed `integration:ingestion` test now covers real produce/consume, processor state/history writes, and committed offsets. Redis, PostGIS, H3, gRPC, Prometheus, and toml++ remain off by default.
