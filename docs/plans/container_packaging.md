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

Adapter build args:

| Build arg | Default | Purpose |
|---|---:|---|
| `SR_ADAPTER_APT_PACKAGES` | empty | Extra build-stage packages installed before CMake configure |
| `SR_ADAPTER_RUNTIME_APT_PACKAGES` | empty | Extra runtime-stage shared libraries/packages |
| `SR_DEPENDENCY_PROVIDER` | `system` | CMake dependency provider coordination value |
| `CMAKE_PREFIX_PATH` | empty | Additional package prefix path for system/vcpkg/conan outputs |
| `CMAKE_TOOLCHAIN_FILE` | empty | Optional toolchain file, for example vcpkg or Conan |
| `SR_ENABLE_*` | `OFF` | Explicit CMake switches for protobuf/gRPC/real adapters/prometheus/toml++ |

Important boundary: this image scaffold does not make unavailable packages appear. If a real adapter switch is enabled without the matching package target, CMake should still fail at dependency discovery.

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

## Current Boundary
The default image proves reproducible packaging for the fallback runtime, `Dockerfile.adapters` provides a repeatable image path for package-backed builds, Compose provides local dependency containers, and CI can manually validate dependency service provisioning plus fallback-safe and protobuf-enabled adapter image paths. Real dependency-backed integration tests remain pending until package installation and adapter-specific integration tests are ready.
