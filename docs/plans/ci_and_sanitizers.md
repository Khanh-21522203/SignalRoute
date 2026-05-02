# CI And Sanitizer Plan

## Purpose
This document records the dependency-free CI command matrix, local sanitizer profile, manual dependency service scaffold, and disabled-by-default integration harness. Production adapter tests stay gated until the required CMake packages and service-backed integration tests exist.

## Local Verification Script
Run the default CI-equivalent local checks with:

```sh
scripts/verify-local.sh
```

The script runs:

```sh
cmake -S . -B /tmp/signalroute-build -DSR_BUILD_TESTS=ON
cmake --build /tmp/signalroute-build -j2
ctest --test-dir /tmp/signalroute-build --output-on-failure

cmake -S . -B /tmp/signalroute-protobuf-build -DSR_BUILD_TESTS=ON -DSR_ENABLE_PROTOBUF=ON
cmake --build /tmp/signalroute-protobuf-build -j2
ctest --test-dir /tmp/signalroute-protobuf-build --output-on-failure
```

Useful environment overrides:

| Variable | Default | Purpose |
|---|---:|---|
| `SIGNALROUTE_BUILD_ROOT` | `/tmp` | Parent directory for build trees |
| `SIGNALROUTE_JOBS` | `2` | Parallel build jobs |
| `SIGNALROUTE_RUN_SANITIZERS` | `0` | Set to `1` to run ASan+UBSan build/test |
| `SIGNALROUTE_ASAN_DETECT_LEAKS` | `0` | Set to `1` in CI runners where LeakSanitizer works |

`SIGNALROUTE_ASAN_DETECT_LEAKS` defaults to `0` because some agent/sandbox environments run tests under tracing where LeakSanitizer exits with `LeakSanitizer does not work under ptrace`. Real CI should set it to `1` if the runner supports LeakSanitizer.

## CMake Sanitizer Profiles
The sanitizer switches are dependency-free and work with GCC/Clang-style compilers:

| Profile | Configure command | Notes |
|---|---|---|
| ASan+UBSan | `cmake -S . -B /tmp/signalroute-asan-ubsan-build -DSR_BUILD_TESTS=ON -DSR_ENABLE_ASAN=ON -DSR_ENABLE_UBSAN=ON` | Main memory/UB profile |
| TSan | `cmake -S . -B /tmp/signalroute-tsan-build -DSR_BUILD_TESTS=ON -DSR_ENABLE_TSAN=ON` | Use separately from ASan/UBSan |

ThreadSanitizer is intentionally mutually exclusive with ASan/UBSan in `cmake/SignalRouteOptions.cmake`.

## Recommended CI Matrix

| Job | Enabled now | Commands |
|---|---:|---|
| fallback-unit | yes | `.github/workflows/ci.yml` runs fallback configure/build/CTest |
| protobuf-unit | yes | `.github/workflows/ci.yml` installs only protobuf packages, then runs protobuf configure/build/CTest |
| asan-ubsan | yes | `.github/workflows/ci.yml` runs focused sanitizer smoke with `ASAN_OPTIONS=detect_leaks=0` |
| dependency-service-scaffold | manual | `workflow_dispatch` with `run_dependency_scaffold=true`; validates Redis, PostGIS, Redpanda, Compose config, and fallback runtime build smoke |
| adapter-image-scaffold | manual | `workflow_dispatch` with `run_adapter_image_scaffold=true`; build and smoke-test `Dockerfile.adapters`/`docker buildx bake adapter-scaffold` with all real adapter switches off |
| adapter-protobuf-image | manual | `workflow_dispatch` with `run_adapter_protobuf_image=true`; builds and smoke-tests the protobuf-enabled adapter image with protobuf packages only |
| adapter-kafka-image | manual | `workflow_dispatch` with `run_adapter_kafka_image=true`; builds and smoke-tests the Kafka-enabled adapter image through `FindRdKafka.cmake` |
| integration-harness | manual | `workflow_dispatch` with `run_integration_harness=true`; builds `SR_BUILD_INTEGRATION_TESTS=ON` and runs the feature-group manifest only |
| ingestion-integration | manual | `workflow_dispatch` with `run_ingestion_integration=true`; starts Redpanda, builds with Kafka+protobuf packages, and runs `integration:ingestion` |
| tsan-smoke | optional/manual | configure/build with `-DSR_ENABLE_TSAN=ON`; select focused concurrency tests first |
| grpc-package | no | Enable after `gRPC::grpc++` and `gRPC::grpc_cpp_plugin` are installed |
| kafka-integration | manual | Broker-backed ingestion coverage exists under `run_ingestion_integration`; promote only after broader adapter stability |
| redis-integration | no | Enable after hiredis/redis++ packages and Redis service are available |
| postgis-integration | no | Enable after libpq/PostGIS packages and database service are available |
| h3-integration | no | Enable after H3 package is available |

## Hosted Workflow
The hosted workflow lives at `.github/workflows/ci.yml` and runs on push, pull request, and manual dispatch.

Current jobs:
- `fallback-unit`: dependency-free fallback configure/build/CTest.
- `protobuf-unit`: installs `libprotobuf-dev` and `protobuf-compiler`, then runs protobuf configure/build/CTest.
- `asan-ubsan`: dependency-free focused sanitizer smoke over admin HTTP, config, and runtime application tests.
- `dependency-service-scaffold`: manual-only service scaffold that starts Redis and PostGIS as GitHub Actions services, starts Redpanda through Docker, validates service readiness, validates `compose.yml`, and builds a focused fallback runtime smoke.
- `adapter-image-scaffold`: manual-only image scaffold job that validates the Docker Bake target, builds `signalroute:adapter-scaffold`, checks the binary exists, and runs a short fallback runtime smoke.
- `adapter-protobuf-image`: manual-only package-backed image job that validates `adapter-protobuf`, builds `signalroute:adapter-protobuf`, checks runtime shared library resolution with `ldd`, and runs a short protobuf-enabled runtime smoke.
- `adapter-kafka-image`: manual-only package-backed image job that validates `adapter-kafka`, builds `signalroute:adapter-kafka`, checks runtime shared library resolution with `ldd`, and runs a short Kafka-enabled runtime smoke.
- `integration-harness`: manual-only harness job that configures `SR_BUILD_INTEGRATION_TESTS=ON`, builds `test_integration_harness`, and runs CTest label `integration` without starting external services.
- `ingestion-integration`: manual-only Redpanda-backed job that installs Kafka plus protobuf packages, builds `test_ingestion_pipeline`, and runs CTest label `integration:ingestion` with `SIGNALROUTE_RUN_KAFKA_INTEGRATION=1`.

Run the manual scaffold from GitHub Actions with `workflow_dispatch` and `run_dependency_scaffold=true`. This job does not set `SR_ENABLE_REAL_KAFKA`, `SR_ENABLE_REAL_REDIS`, `SR_ENABLE_REAL_POSTGIS`, or `SR_ENABLE_REAL_H3`; it proves service provisioning only. Package-backed adapter jobs remain separate until the corresponding CMake packages are available.

Future adapter CI jobs should build from `Dockerfile.adapters` or `docker-bake.hcl` targets. Keep `adapter-scaffold` package-free and fallback-safe. Add package-backed targets only when the CI job also installs/provides the matching CMake package targets and runs feature-grouped integration tests.

Run the adapter image scaffold from GitHub Actions with `workflow_dispatch` and `run_adapter_image_scaffold=true`. This job does not install adapter packages and does not enable real adapter switches.

Run the protobuf adapter image from GitHub Actions with `workflow_dispatch` and `run_adapter_protobuf_image=true`. This job installs protobuf build packages and the protobuf runtime library inside the Docker image only, enables `SR_ENABLE_PROTOBUF=ON`, and keeps real Kafka, Redis, PostGIS, H3, gRPC, Prometheus, and toml++ switches off.

Run the Kafka adapter image check from GitHub Actions with `workflow_dispatch` and `run_adapter_kafka_image=true`. This job installs `librdkafka-dev`/`librdkafka++1` inside the Docker image, uses the repository `FindRdKafka.cmake` bridge, enables only `SR_ENABLE_REAL_KAFKA=ON`, and keeps protobuf, gRPC, Redis, PostGIS, H3, Prometheus, and toml++ off.

Run the integration harness from GitHub Actions with `workflow_dispatch` and `run_integration_harness=true`. This job validates the feature-group integration manifest only. Run the Kafka ingestion integration with `workflow_dispatch` and `run_ingestion_integration=true`; it starts Redpanda and runs `integration:ingestion` against real Kafka plus protobuf packages.

## Current Boundary
Batches 61-62 add a manual `ingestion-integration` job for broker-backed Kafka ingestion. Default push and pull request CI remains dependency-free fallback, protobuf, and focused ASan+UBSan. The protobuf image proves package-backed generated-message packaging, the Kafka image proves package-backed compile/runtime linking, the integration harness proves feature-group naming and labels, and the ingestion job proves Redpanda-backed produce/consume plus processor state/history writes.
