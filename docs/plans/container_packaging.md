# Container Packaging Plan

## Purpose
This document records the current production image and local dependency scaffold for SignalRoute. The production image remains a dependency-free fallback runtime. Docker Compose now provides Redis, PostGIS, and Redpanda services for local environment scaffolding, but real adapters are still disabled unless future production images and CMake switches enable them.

## Image Build
Build the production fallback image from the repository root:

```sh
docker build -t signalroute:local .
```

The Dockerfile is multi-stage:
- `build`: Ubuntu 24.04 with `cmake`, `g++`, and `make`; builds `signalroute` with `SR_BUILD_TESTS=OFF`.
- `runtime`: Ubuntu 24.04 slim runtime dependencies plus an unprivileged `signalroute` user.

The image intentionally does not install Kafka, Redis, PostGIS, H3, gRPC, protobuf, or Prometheus packages. It uses the dependency-free fallback runtime unless future production adapter images are introduced.

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
| `9100` | Metrics/admin HTTP-style port | Runtime handler exists; real HTTP server binding pending |
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

Important boundary: this config does not enable real adapters by itself. The current Docker image is still built without `SR_ENABLE_REAL_KAFKA`, `SR_ENABLE_REAL_REDIS`, `SR_ENABLE_REAL_POSTGIS`, or `SR_ENABLE_REAL_H3`.

## Current Boundary
The image proves reproducible packaging for the fallback runtime, and Compose provides local dependency containers for future adapter integration. Production adapter images and real dependency-backed integration tests remain pending until package/service provisioning is ready.
