# CI And Sanitizer Plan

## Purpose
This document records the dependency-free CI command matrix and local sanitizer profile. Production dependency jobs stay documented but disabled until the required packages and services exist in the runner image.

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
| tsan-smoke | optional/manual | configure/build with `-DSR_ENABLE_TSAN=ON`; select focused concurrency tests first |
| grpc-package | no | Enable after `gRPC::grpc++` and `gRPC::grpc_cpp_plugin` are installed |
| kafka-integration | no | Enable after RdKafka package and broker service are available |
| redis-integration | no | Enable after hiredis/redis++ packages and Redis service are available |
| postgis-integration | no | Enable after libpq/PostGIS packages and database service are available |
| h3-integration | no | Enable after H3 package is available |

## Hosted Workflow
The hosted workflow lives at `.github/workflows/ci.yml` and runs on push, pull request, and manual dispatch.

Current jobs:
- `fallback-unit`: dependency-free fallback configure/build/CTest.
- `protobuf-unit`: installs `libprotobuf-dev` and `protobuf-compiler`, then runs protobuf configure/build/CTest.
- `asan-ubsan`: dependency-free focused sanitizer smoke over admin HTTP, config, and runtime application tests.

Dependency-backed jobs for gRPC, Kafka, Redis, PostGIS, and H3 remain intentionally absent until package/service provisioning is ready.

## Current Boundary
Batch 48 adds the hosted GitHub Actions workflow for fallback, protobuf, and focused ASan+UBSan smoke. Dependency-backed jobs remain documented placeholders until their packages and service containers are available.
