# Container Packaging Plan

## Purpose
This document records the current production image, adapter image scaffold, and local dependency scaffold for SignalRoute. The default production image remains a dependency-free fallback runtime. Docker Compose now provides Redis, PostGIS, and Redpanda services for local environment scaffolding, but real adapters are still disabled unless future production images and CMake switches enable them.

## Image Build
Build the production fallback image from the repository root:

```sh
docker build -t signalroute:local .
```

The Dockerfile is multi-stage:
- `build`: Ubuntu 24.04 with `cmake`, `g++`, and `make`; builds `signalroute` with `SR_BUILD_TESTS=OFF`.
- `runtime`: Ubuntu 24.04 slim runtime dependencies plus an unprivileged `signalroute` user.

The image intentionally does not install Kafka, Redis, PostGIS, H3, gRPC, protobuf, or Prometheus packages. It uses the dependency-free fallback runtime unless future production adapter images are introduced.

## Adapter Image Scaffold
`Dockerfile.adapters` is the repeatable image path for future package-backed adapter builds. It defaults every production adapter switch to `OFF`, so it can build the fallback runtime without external packages:

```sh
docker build -f Dockerfile.adapters --target runtime -t signalroute:adapter-scaffold .
```

The Docker Bake file exposes the same default scaffold target:

```sh
docker buildx bake adapter-scaffold
```

Optional package-backed builds must opt in explicitly through build args:

```sh
docker build -f Dockerfile.adapters --target runtime -t signalroute:adapter-protobuf \
  --build-arg SR_ADAPTER_APT_PACKAGES="libprotobuf-dev protobuf-compiler" \
  --build-arg SR_ADAPTER_RUNTIME_APT_PACKAGES="libprotobuf32t64" \
  --build-arg SR_ENABLE_PROTOBUF=ON \
  .
```

The Docker Bake file also exposes this protobuf-only package-backed target:

```sh
docker buildx bake adapter-protobuf
```

`adapter-protobuf` installs `libprotobuf-dev` and `protobuf-compiler` in the build stage, installs `libprotobuf32t64` in the runtime stage, and enables only `SR_ENABLE_PROTOBUF=ON`. Kafka, Redis, PostGIS, H3, gRPC, Prometheus, and toml++ switches remain off.

The Docker Bake file also exposes a Kafka package-backed target:

```sh
docker buildx bake adapter-kafka
```

`adapter-kafka` installs `librdkafka-dev` in the build stage, installs `librdkafka++1` in the runtime stage, uses `cmake/FindRdKafka.cmake` to bridge Ubuntu pkg-config/header/library installs to `RdKafka::rdkafka++`, and enables only `SR_ENABLE_REAL_KAFKA=ON`.

The Docker Bake file also exposes H3 package-backed targets:

```sh
docker buildx bake adapter-h3
docker buildx bake adapter-h3-test
```

`adapter-h3` installs `libh3-dev` in the build stage, installs `libh3-1` in the runtime stage, uses `cmake/Findh3.cmake` to bridge Ubuntu header/library installs to `h3::h3`, and enables only `SR_ENABLE_REAL_H3=ON`. `adapter-h3-test` builds `test_h3_index` with `SR_ENABLE_REAL_H3=ON` so the package-backed H3 implementation can be verified without installing dependencies locally.

The Docker Bake file also exposes Redis package-backed targets:

```sh
docker buildx bake adapter-redis
docker buildx bake adapter-redis-test
```

`adapter-redis` installs `libhiredis-dev` in the build stage, installs `libhiredis1.1.0` in the runtime stage, uses `cmake/FindHiredis.cmake` to bridge Ubuntu header/library installs to `hiredis::hiredis`, and enables only `SR_ENABLE_REAL_REDIS=ON`. `adapter-redis-test` builds `test_redis_client` with `SR_ENABLE_REAL_REDIS=ON`; run that image against Redis with `SIGNALROUTE_REDIS_ADDRS` to verify real Redis behavior without installing dependencies locally.

The Docker Bake file also exposes a broker-backed ingestion build-stage target:

```sh
docker buildx bake integration-ingestion
```

`integration-ingestion` installs `librdkafka-dev`, `libprotobuf-dev`, and `protobuf-compiler`, enables `SR_ENABLE_REAL_KAFKA=ON` and `SR_ENABLE_PROTOBUF=ON`, and builds `test_ingestion_pipeline` in the adapter build stage. Run that image against Redpanda with `SIGNALROUTE_RUN_KAFKA_INTEGRATION=1` and `SIGNALROUTE_KAFKA_BROKERS` to verify real ingestion behavior.

Adapter build args:

| Build arg | Default | Purpose |
|---|---:|---|
| `SR_ADAPTER_APT_PACKAGES` | empty | Extra build-stage packages installed before CMake configure |
| `SR_ADAPTER_RUNTIME_APT_PACKAGES` | empty | Extra runtime-stage shared libraries/packages |
| `SR_DEPENDENCY_PROVIDER` | `system` | CMake dependency provider coordination value |
| `CMAKE_PREFIX_PATH` | empty | Additional package prefix path for system/vcpkg/conan outputs |
| `CMAKE_TOOLCHAIN_FILE` | empty | Optional toolchain file, for example vcpkg or Conan |
| `SR_BUILD_INTEGRATION_TESTS` | `OFF` | Build manually gated integration tests when `SR_BUILD_TESTS=ON` |
| `SR_ADAPTER_BUILD_TARGETS` | `signalroute` | CMake target or targets built by `Dockerfile.adapters` |
| `SR_ENABLE_*` | `OFF` | Explicit CMake switches for protobuf/gRPC/real adapters/prometheus/toml++ |

Important boundary: this image scaffold does not make unavailable packages appear. If a real adapter switch is enabled without the matching package target, CMake should still fail at dependency discovery.

Package-backed adapter images must follow `docs/plans/package_strategy_lock.md` for target names, image tags, build packages, runtime packages, manual CI inputs, and feature-grouped integration labels. Add new adapter image targets one dependency at a time unless the feature integration test requires a coordinated combination.

## Runtime Contract
Default command:

```sh
docker run --rm signalroute:local
```

Run with a mounted config:

```sh
docker run --rm \
  -v "$PWD/config/signalroute.toml:/etc/signalroute/signalroute.toml:ro" \
  -p 9090:9090 \
  -p 9091:9091 \
  -p 9100:9100 \
  -p 9101:9101 \
  signalroute:local
```

Override role:

```sh
docker run --rm signalroute:local --config=/etc/signalroute/signalroute.toml --role=query
```

Exposed ports:

| Port | Purpose | Current state |
|---:|---|---|
| `9090` | gRPC service port | Real server binding pending |
| `9091` | UDP ingest port | Real endpoint pending |
| `9100` | Metrics scrape port | Runtime-owned fallback Prometheus text exporter when `metrics_exporter_enabled = true` |
| `9101` | Admin socket | Optional, disabled by default in config |

## Security Defaults
- Runtime process runs as unprivileged `signalroute` user.
- Default config is copied to `/etc/signalroute/signalroute.toml`.
- Runtime working directory is `/var/lib/signalroute`.
- Build tools are not copied into the runtime stage.

## Docker Compose Dependency Scaffold
Start local dependency services only:

```sh
docker compose -f compose.yml up -d redis postgis redpanda
```

Check the resolved Compose model:

```sh
docker compose -f compose.yml config
```

Stop services while keeping named volumes:

```sh
docker compose -f compose.yml down
```

Remove services and local data volumes:

```sh
docker compose -f compose.yml down -v
```

The `signalroute` service is profile-gated so dependency services can start without running the application:

```sh
docker compose -f compose.yml --profile app up signalroute
```

The profile-mounted config is `config/signalroute.docker.toml`. It points to Compose service hostnames:
- Kafka-compatible broker: `redpanda:29092`
- Redis: `redis:6379`
- PostGIS: `postgis:5432`

Important boundary: this config does not enable real adapters by itself. The current default Docker image is still built without `SR_ENABLE_REAL_KAFKA`, `SR_ENABLE_REAL_REDIS`, `SR_ENABLE_REAL_POSTGIS`, or `SR_ENABLE_REAL_H3`.

## CI Dependency Service Scaffold
The GitHub Actions workflow includes a manual `dependency-service-scaffold` job. Start it with `workflow_dispatch` and `run_dependency_scaffold=true`.

That job:
- validates `compose.yml`;
- starts Redis and PostGIS as GitHub Actions service containers;
- starts Redpanda through Docker with the same exposed Kafka/admin ports used by local Compose;
- verifies service readiness with `redis-cli`, `pg_isready`/`psql`, and `rpk`;
- builds a focused fallback runtime smoke without enabling real adapter CMake switches.

## CI Adapter Image Scaffold
The GitHub Actions workflow also includes a manual `adapter-image-scaffold` job. Start it with `workflow_dispatch` and `run_adapter_image_scaffold=true`.

That job:
- validates the `adapter-scaffold` Docker Bake target;
- builds `signalroute:adapter-scaffold` from `Dockerfile.adapters`;
- verifies the packaged binary exists;
- runs a short query-role fallback runtime smoke and expects timeout exit `124` after clean SIGTERM.

Important boundary: this job verifies the image path only. It does not install adapter packages and does not enable real adapter switches.

## CI Protobuf Adapter Image
The GitHub Actions workflow includes a manual `adapter-protobuf-image` job. Start it with `workflow_dispatch` and `run_adapter_protobuf_image=true`.

That job:
- validates the `adapter-protobuf` Docker Bake target;
- builds `signalroute:adapter-protobuf` from `Dockerfile.adapters`;
- verifies the packaged binary exists and that runtime shared libraries resolve with `ldd`;
- runs a short query-role protobuf-enabled runtime smoke and expects timeout exit `124` after clean SIGTERM.

Important boundary: this job proves protobuf package-backed image wiring only. It does not enable real Kafka, Redis, PostGIS, H3, gRPC, Prometheus, or toml++ switches.

## CI Kafka Adapter Image
The GitHub Actions workflow includes a manual `adapter-kafka-image` job. Start it with `workflow_dispatch` and `run_adapter_kafka_image=true`.

That job:
- validates the `adapter-kafka` Docker Bake target;
- attempts to build `signalroute:adapter-kafka` from `Dockerfile.adapters`;
- verifies the packaged binary and runtime smoke only after the image build succeeds.

Important boundary: this job proves Kafka package compile/runtime linking only. Broker-backed publish/consume semantics are covered by the separate ingestion integration job.

## CI H3 Adapter Image
The GitHub Actions workflow includes a manual `adapter-h3-image` job. Start it with `workflow_dispatch` and `run_adapter_h3_image=true`.

That job:
- validates the `adapter-h3` and `adapter-h3-test` Docker Bake targets;
- builds `signalroute:adapter-h3` from `Dockerfile.adapters`;
- verifies the packaged binary and that `libh3.so.1` resolves at runtime;
- builds `signalroute:adapter-h3-test`;
- runs `test_h3_index` from the H3 test image;
- runs a short query-role H3-enabled runtime smoke and expects timeout exit `124` after clean SIGTERM.

Important boundary: this job proves H3 package compile/runtime linking only. Nearby and geofence service-backed behavior still requires Redis/PostGIS provider verification and feature-grouped integration tests.

## CI Redis Adapter Image
The GitHub Actions workflow includes a manual `adapter-redis-image` job. Start it with `workflow_dispatch` and `run_adapter_redis_image=true`.

That job:
- starts Redis as a GitHub Actions service container;
- validates the `adapter-redis` and `adapter-redis-test` Docker Bake targets;
- builds `signalroute:adapter-redis` from `Dockerfile.adapters`;
- verifies the packaged binary and that `libhiredis.so.1.1.0` resolves at runtime;
- builds `signalroute:adapter-redis-test`;
- runs `test_redis_client` from the Redis test image against Redis;
- runs a short query-role Redis-enabled runtime smoke and expects timeout exit `124` after clean SIGTERM.

Important boundary: this job proves Redis package compile/runtime linking and focused client behavior only. State, nearby, matching, and cross-service integration tests remain feature-grouped future work.

## CI Ingestion Integration
The GitHub Actions workflow includes a manual `ingestion-integration` job. Start it with `workflow_dispatch` and `run_ingestion_integration=true`.

That job:
- installs `librdkafka-dev`, `libprotobuf-dev`, and `protobuf-compiler`;
- starts Redpanda through Docker;
- configures with `SR_BUILD_TESTS=ON`, `SR_BUILD_INTEGRATION_TESTS=ON`, `SR_ENABLE_REAL_KAFKA=ON`, and `SR_ENABLE_PROTOBUF=ON`;
- builds and runs `test_ingestion_pipeline` with CTest label `integration:ingestion`.

Important boundary: this job proves ingestion Kafka/protobuf transport and processor state/history writes with fallback Redis/PostGIS clients. It does not prove real Redis, real PostGIS, geofence production registry, or matching integration.

## CI Integration Harness
The GitHub Actions workflow includes a manual `integration-harness` job. Start it with `workflow_dispatch` and `run_integration_harness=true`.

That job:
- configures with `SR_BUILD_TESTS=ON` and `SR_BUILD_INTEGRATION_TESTS=ON`;
- builds `test_integration_harness`;
- runs CTest label `integration`;
- does not start Kafka, Redis, PostGIS, H3, gRPC, Prometheus, or toml++ services/packages.

Important boundary: this job validates the feature-group integration scaffold only. Real service-backed integration jobs must use the package and label conventions from `docs/plans/package_strategy_lock.md`.

## Current Boundary
The default image proves reproducible packaging for the fallback runtime, `Dockerfile.adapters` provides repeatable package-backed build paths, Compose provides local dependency containers, and CI can manually validate dependency service provisioning plus fallback-safe, protobuf-enabled, Kafka-enabled, H3-enabled, Redis-enabled, and broker-backed ingestion paths. The integration harness remains available for manifest validation. Real PostGIS, geofence, matching, and endpoint integration tests remain pending; H3 and Redis package compile/runtime are verified, but nearby/geofence service-backed behavior still needs PostGIS-backed integration coverage.
