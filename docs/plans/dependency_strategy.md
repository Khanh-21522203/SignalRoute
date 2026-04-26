# SignalRoute Dependency Strategy

## Purpose
Batch 17 established the build contract for production dependencies without replacing the fallback runtime. Batch 18 added domain-to-wire conversion contracts. Batch 19 adds protobuf-only generated builds and keeps gRPC stubs optional because local gRPC packages may not be installed.

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
| `SR_ENABLE_PROTOBUF` | `OFF` | `protobuf::libprotobuf`, `protobuf::protoc` | Generated protobuf message APIs and protobuf Kafka payload tests |
| `SR_ENABLE_GRPC` | `OFF` | `gRPC::grpc++`, `gRPC::grpc_cpp_plugin`; requires `SR_ENABLE_PROTOBUF=ON` | Generated gRPC gateway/query/admin service stubs |
| `SR_ENABLE_PROTOBUF_GRPC` | `OFF` | Compatibility switch that enables both `SR_ENABLE_PROTOBUF` and `SR_ENABLE_GRPC` | One-switch generated protobuf plus gRPC builds |
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
  - protobuf mode: static library generated from `proto/signalroute/*.proto`;
  - gRPC mode: adds generated `.grpc.pb.*` sources to the same target.
- `sr_common` links `sr_dependencies` and `signalroute_proto`, so adapter code can use stable compile definitions.
- `src/common/proto/` owns dependency-free wire-shape structs and domain conversion helpers. Generated protobuf types should adapt to these helpers instead of leaking through service/domain code.
- `.proto` files use package `signalroute.v1`, producing generated C++ types under `signalroute::v1`. This intentionally avoids name collisions with domain types such as `signalroute::LocationEvent`.

## Adapter Rule
Do not remove fallback behavior when enabling a real dependency. Each production adapter must:
- preserve the same public interface used by current feature tests;
- keep unit tests dependency-free;
- add integration tests grouped by feature;
- fail clearly when enabled without the required package;
- avoid changing CSV fallback payload semantics until protobuf Kafka serialization is ready.
- keep generated protobuf includes out of domain headers. Generated types belong at API/transport boundaries.

## Recommended Implementation Order
1. Add real Kafka producer/consumer adapters behind `KafkaProducer` and `KafkaConsumer`, routing existing shared protobuf payload codecs through durable topics.
2. Add the matching production Kafka request/result loop using the matching payload codec.
3. Enable gRPC service stub generation once `gRPC::grpc++` and `gRPC::grpc_cpp_plugin` are available.
4. Remove or narrow runtime CSV public paths only after durable Kafka/protobuf integration tests pass for each boundary.
5. Replace the deterministic H3 fallback behind `H3Index`.
6. Add Redis integration behind `RedisClient`.
7. Add PostGIS integration behind `PostgresClient`.
8. Add gRPC gateway/query/admin services on top of existing handlers.
9. Add Prometheus exporter and health/readiness endpoints.

## Verification Matrix

| Build | Command | Expected |
|---|---|---|
| Fallback default | `cmake -S . -B /tmp/signalroute-build -DSR_BUILD_TESTS=ON` | Configure succeeds without external packages |
| Protobuf generated messages | `cmake -S . -B /tmp/signalroute-protobuf-build -DSR_BUILD_TESTS=ON -DSR_ENABLE_PROTOBUF=ON` | Configure/build succeeds when protobuf is available and runs generated protobuf round-trip tests |
| Protobuf Kafka payloads | `ctest --test-dir /tmp/signalroute-protobuf-build --output-on-failure` | Runs generated adapter, runtime codec, and Kafka fallback protobuf payload round-trip tests |
| Runtime protobuf boundaries | Same protobuf CTest command | Runs `test_gateway_processor_protobuf_runtime`, `test_runtime_payload_codecs`, geofence tests, and DLQ worker tests with protobuf-enabled payloads |
| gRPC missing | `cmake -S . -B /tmp/signalroute-grpc-missing -DSR_ENABLE_PROTOBUF=ON -DSR_ENABLE_GRPC=ON` | Configure fails clearly if gRPC package is unavailable |
| Production dependency missing | `cmake -S . -B /tmp/signalroute-h3 -DSR_ENABLE_REAL_H3=ON` | Configure fails at package discovery with a clear missing package error |
| Production dependency present | Same option with package available in CMake path | Configure succeeds and links through `sr_dependencies` |

## Current Boundary
Batch 22 has shared runtime payload codecs for location, geofence events, and matching request/result messages. Gateway, processor, geofence evaluator/dwell checker, and DLQ replay use protobuf payloads when `SR_ENABLE_PROTOBUF=ON` and preserve CSV as the default fallback build format plus decoder fallback. Matching still needs the durable Kafka request/result loop, and gRPC service stubs remain gated by `SR_ENABLE_GRPC`.
