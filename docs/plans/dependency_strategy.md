# SignalRoute Dependency Strategy

## Purpose
Batch 17 established the build contract for production dependencies without replacing the fallback runtime. Batch 18 adds domain-to-wire conversion contracts that mirror the protobuf schemas while still compiling without generated protobuf code.

## Default Build Mode
The default build is fallback mode:

```sh
cmake -S . -B /tmp/signalroute-build -DSR_BUILD_TESTS=ON
cmake --build /tmp/signalroute-build -j2
ctest --test-dir /tmp/signalroute-build --output-on-failure
```

Fallback mode does not require Kafka, Redis, PostGIS, protobuf, gRPC, H3, Prometheus, or toml++.

## Dependency Provider
The build exposes `SR_DEPENDENCY_PROVIDER` as a coordination field:

| Value | Meaning |
|---|---|
| `system` | Dependencies are found through the system CMake package path. This is the current default. |
| `vcpkg` | Use a vcpkg toolchain file and installed ports. |
| `conan` | Use Conan-generated CMake files/toolchain. |
| `fetchcontent` | Future option for source-based dependency retrieval. |

The provider value does not install dependencies by itself. It records the intended source of packages so each parallel task uses the same strategy.

## Build Switches

| Option | Default | Required package target intent | Unblocks |
|---|---:|---|---|
| `SR_ENABLE_PROTOBUF_GRPC` | `OFF` | `protobuf::libprotobuf`, `protobuf::protoc`, `gRPC::grpc++`, `gRPC::grpc_cpp_plugin` | Generated proto APIs, gRPC gateway/query/admin services, protobuf Kafka payloads |
| `SR_ENABLE_REAL_H3` | `OFF` | `h3::h3` or `h3` | Real H3 cell encoding, grid disk, polygon covering |
| `SR_ENABLE_REAL_REDIS` | `OFF` | `hiredis::hiredis`, `redis++::redis++` | Real state, H3 index, fence state, reservations |
| `SR_ENABLE_REAL_POSTGIS` | `OFF` | `PostgreSQL::PostgreSQL` | Real trip history, spatial queries, geofence repositories |
| `SR_ENABLE_REAL_KAFKA` | `OFF` | `RdKafka::rdkafka++` or equivalent | Durable publish/consume, offsets, lag, DLQ transport |
| `SR_ENABLE_PROMETHEUS` | `OFF` | `prometheus-cpp::core` | Prometheus exporter |
| `SR_ENABLE_TOMLPLUSPLUS` | `OFF` | `tomlplusplus::tomlplusplus` | Production TOML parser |

## CMake Contract
- `cmake/SignalRouteOptions.cmake` owns all build switches.
- `cmake/SignalRouteDependencies.cmake` owns `find_package` and external target linking.
- `sr_dependencies` is the central interface target for production dependencies.
- `signalroute_proto` always exists:
  - fallback mode: interface target with no generated files;
  - protobuf/gRPC mode: static library generated from `proto/signalroute/*.proto`.
- `sr_common` links `sr_dependencies` and `signalroute_proto`, so adapter code can use stable compile definitions.
- `src/common/proto/` owns dependency-free wire-shape structs and domain conversion helpers. Generated protobuf types should adapt to these helpers instead of leaking through service/domain code.

## Adapter Rule
Do not remove fallback behavior when enabling a real dependency. Each production adapter must:
- preserve the same public interface used by current feature tests;
- keep unit tests dependency-free;
- add integration tests grouped by feature;
- fail clearly when enabled without the required package;
- avoid changing CSV fallback payload semantics until protobuf Kafka serialization is ready.
- keep generated protobuf includes out of domain headers. Generated types belong at API/transport boundaries.

## Recommended Implementation Order
1. Enable protobuf/gRPC generation against the existing `signalroute_proto` target.
2. Adapt generated protobuf messages to the `src/common/proto/` conversion contracts.
3. Implement protobuf Kafka payload serialization for location, geofence, matching, and DLQ payloads.
4. Replace the deterministic H3 fallback behind `H3Index`.
5. Add Redis integration behind `RedisClient`.
6. Add PostGIS integration behind `PostgresClient`.
7. Add librdkafka integration behind `KafkaProducer` and `KafkaConsumer`.
8. Add gRPC gateway/query/admin services on top of existing handlers.
9. Add Prometheus exporter and health/readiness endpoints.

## Verification Matrix

| Build | Command | Expected |
|---|---|---|
| Fallback default | `cmake -S . -B /tmp/signalroute-build -DSR_BUILD_TESTS=ON` | Configure succeeds without external packages |
| Production dependency missing | `cmake -S . -B /tmp/signalroute-h3 -DSR_ENABLE_REAL_H3=ON` | Configure fails at package discovery with a clear missing package error |
| Production dependency present | Same option with package available in CMake path | Configure succeeds and links through `sr_dependencies` |

## Current Boundary
Batch 18 prepares conversion contracts and tests. It does not implement real protobuf serialization, install packages, remove CSV fallback parsing, or change runtime behavior.
