# SignalRoute Dependency Strategy

## Purpose
Batch 17 established the build contract for production dependencies without replacing the fallback runtime. Batch 18 added domain-to-wire conversion contracts. Batch 19 adds protobuf-only generated builds and keeps gRPC stubs optional because local gRPC packages may not be installed. Batch 47 keeps that contract while adding dependency-free local CI and sanitizer build profiles. Batch 57 locks package-backed adapter naming, package candidates, CI promotion rules, and feature-grouped integration labels in `docs/plans/package_strategy_lock.md`.

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
| `SR_ENABLE_REAL_KAFKA` | `OFF` | `RdKafka::rdkafka++` or `rdkafka++` | Durable publish/consume, offsets, lag, DLQ transport |
| `SR_ENABLE_PROMETHEUS` | `OFF` | `prometheus-cpp::core` | Native Prometheus registry/exposer integration |
| `SR_ENABLE_TOMLPLUSPLUS` | `OFF` | `tomlplusplus::tomlplusplus` | Production TOML parser |
| `SR_BUILD_INTEGRATION_TESTS` | `OFF` | none by itself | Manually build feature-grouped integration test harness under `tests/integration/` |
| `SR_ENABLE_ASAN` | `OFF` | GCC/Clang sanitizer runtime | AddressSanitizer local/CI profile |
| `SR_ENABLE_UBSAN` | `OFF` | GCC/Clang sanitizer runtime | UndefinedBehaviorSanitizer local/CI profile |
| `SR_ENABLE_TSAN` | `OFF` | GCC/Clang sanitizer runtime | ThreadSanitizer local/CI profile; mutually exclusive with ASan/UBSan |

## CMake Contract
- `cmake/SignalRouteOptions.cmake` owns all build switches.
- `cmake/SignalRouteDependencies.cmake` owns `find_package` and external target linking.
- `sr_dependencies` is the central interface target for production dependencies.
- `SR_BUILD_INTEGRATION_TESTS` requires `SR_BUILD_TESTS=ON`, is off by default, and only adds the `tests/integration/` harness unless future batches add real service-backed tests.
- `signalroute_proto` always exists:
  - fallback mode: interface target with no generated files;
  - protobuf mode: static library generated from `proto/signalroute/*.proto`;
  - gRPC mode: adds generated `.grpc.pb.*` sources to the same target.
- `sr_grpc_transport` always exists:
  - fallback/protobuf mode: interface target with no gRPC headers or sources compiled;
  - gRPC mode: static library with admin, gateway, and query adapter skeletons over existing service handlers.
- `sr_common` links `sr_dependencies` and `signalroute_proto`, so adapter code can use stable compile definitions.
- `src/common/proto/` owns dependency-free wire-shape structs and domain conversion helpers. Generated protobuf types should adapt to these helpers instead of leaking through service/domain code.
- Real Kafka adapter code is hidden behind the existing wrapper API and pimpl storage; public headers do not expose librdkafka types.
- Real Redis adapter code is hidden behind the existing wrapper API and pimpl storage; public headers do not expose redis-plus-plus types.
- Real PostGIS adapter code is hidden behind the existing wrapper API and pimpl storage; public headers do not expose libpq types.
- `.proto` files use package `signalroute.v1`, producing generated C++ types under `signalroute::v1`. This intentionally avoids name collisions with domain types such as `signalroute::LocationEvent`.
- `cmake/SignalRouteSanitizers.cmake` owns sanitizer compile/link options. Sanitizer switches do not enable external dependencies.

## Adapter Rule
Do not remove fallback behavior when enabling a real dependency. Each production adapter must:
- preserve the same public interface used by current feature tests;
- keep unit tests dependency-free;
- add integration tests grouped by feature;
- fail clearly when enabled without the required package;
- avoid changing CSV fallback payload semantics until protobuf Kafka serialization is ready.
- keep generated protobuf includes out of domain headers. Generated types belong at API/transport boundaries.
- if a deployment marks an adapter as readiness-critical, ensure the matching `SR_ENABLE_REAL_*` switch is enabled and the adapter registers a real dependency health source before relying on it for production traffic.

## Recommended Implementation Order
1. Use the package naming, Bake target, CI input, and feature-label conventions in `docs/plans/package_strategy_lock.md`.
2. Install/provide the RdKafka CMake package and run broker-backed compile/integration verification for the `SR_ENABLE_REAL_KAFKA` adapter path.
3. Enable gRPC service stub/server verification once `gRPC::grpc++` and `gRPC::grpc_cpp_plugin` are available.
4. Remove or narrow runtime CSV public paths only after durable Kafka/protobuf integration tests pass for each boundary.
5. Install/provide the H3 CMake package and run real-H3 compile/integration verification for `SR_ENABLE_REAL_H3`; the adapter path is present behind `H3Index`.
6. Install/provide hiredis and redis-plus-plus CMake packages and run real-Redis compile/integration verification for `SR_ENABLE_REAL_REDIS`; the adapter path is present behind `RedisClient`.
7. Install/provide PostgreSQL/libpq headers and libraries, then run real-PostGIS compile/integration verification for `SR_ENABLE_REAL_POSTGIS`; the adapter path is present behind `PostgresClient`.
8. Add gRPC gateway/query/admin services on top of existing handlers.
9. Replace the fallback metrics text registry with package-backed prometheus-cpp once the package is available.

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
| Metrics exporter | Set `observability.metrics_exporter_enabled = true` with `metrics_port = 0` in a test config | Runtime binds an ephemeral scrape socket and serves Prometheus text at `metrics_path` |
| Readiness-critical adapter missing | Set `observability.require_redis_readiness = true` in fallback build | `/health` remains liveness `200`; `/ready` reports `503` with a required `redis` component from the dependency health registry |
| Local CI script | `scripts/verify-local.sh` | Runs fallback and protobuf configure/build/CTest without dependency installation |
| ASan+UBSan smoke | `cmake -S . -B /tmp/signalroute-asan-ubsan-build -DSR_BUILD_TESTS=ON -DSR_ENABLE_ASAN=ON -DSR_ENABLE_UBSAN=ON` | Configures/builds with sanitizer instrumentation; in traced sandboxes run tests with `ASAN_OPTIONS=detect_leaks=0` |
| Manual dependency service scaffold | GitHub Actions `workflow_dispatch` with `run_dependency_scaffold=true` | Starts Redis, PostGIS, and Redpanda services, verifies service readiness, validates Compose config, and runs fallback runtime build smoke without enabling real adapter switches |
| Adapter image scaffold | `docker build -f Dockerfile.adapters --target runtime -t signalroute:adapter-scaffold .` or GitHub Actions `workflow_dispatch` with `run_adapter_image_scaffold=true` | Builds and smoke-tests a fallback-safe adapter image path with all real adapter switches off by default |
| Protobuf adapter image | `docker buildx bake adapter-protobuf` or GitHub Actions `workflow_dispatch` with `run_adapter_protobuf_image=true` | Builds and smoke-tests a protobuf-enabled adapter image with protobuf build/runtime packages only; real Kafka, Redis, PostGIS, H3, gRPC, Prometheus, and toml++ switches stay off |
| Integration harness manifest | `cmake -S . -B /tmp/signalroute-integration-build -DSR_BUILD_TESTS=ON -DSR_BUILD_INTEGRATION_TESTS=ON && cmake --build /tmp/signalroute-integration-build --target test_integration_harness -j2 && ctest --test-dir /tmp/signalroute-integration-build -L integration --output-on-failure` | Builds and runs the feature-group manifest without requiring external services |

## Current Boundary
Batch 57 adds the feature-grouped integration harness and package strategy lock. `Dockerfile.adapters` and `docker-bake.hcl` remain the repeatable image path for package-backed adapter builds. The default adapter scaffold keeps all real adapter switches off and only proves image/build wiring; the protobuf adapter image enables only generated protobuf message support and includes the protobuf runtime library. The manual dependency service scaffold verifies Redis, PostGIS, and Redpanda service availability, but it intentionally does not enable `SR_ENABLE_REAL_*` adapter switches. The Docker Compose scaffold still provides local dependency services while the SignalRoute service remains fallback-runtime unless future production images enable real adapters. The gRPC configure check still fails clearly at `find_package(gRPC)` because local gRPC CMake packages are not installed, so package-backed gRPC verification remains pending. Redis package-backed verification needs a redis-plus-plus provider because Ubuntu 24.04 does not provide a standard redis-plus-plus package in the checked archive. gRPC service stubs and adapters remain gated by `SR_ENABLE_GRPC`; `SR_ENABLE_PROMETHEUS` remains reserved for future prometheus-cpp registry/exposer integration.
