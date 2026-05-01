# Container Packaging Plan

## Purpose
This document records the current production image contract for SignalRoute. Batch 49 provides a dependency-free fallback runtime image. Docker Compose for Kafka/Redis/PostGIS/H3-backed integration remains a later task.

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

## Current Boundary
The image proves reproducible packaging for the fallback runtime. Production adapter images and Docker Compose service dependencies remain pending until package/service provisioning is ready.
