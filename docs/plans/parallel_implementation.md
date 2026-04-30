# Parallel Implementation Guardrails

## Purpose
This file defines the preparation required before running multiple agents or developers in parallel. The goal is to prevent merge conflicts, contract drift, duplicate abstractions, and incompatible event/config names.

## Current Baseline
- The project builds and tests from the shared fallback runtime baseline.
- Tests are organized by feature/function, not by phase.
- In-process communication uses typed `EventBus` payloads under `src/common/events/`.
- Durable cross-process communication remains Kafka.
- The tracked completion roadmap is `docs/plans/finish_plan.md`.
- Dependency strategy is tracked in `docs/plans/dependency_strategy.md`.
- Gateway, processor, geofence, matching payload codecs, and DLQ replay use shared codecs. Protobuf payloads are emitted when `SR_ENABLE_PROTOBUF=ON`; CSV remains fallback-build scaffolding and decoder compatibility.
- Gateway and query now expose dependency-free transport-facing request/response handlers over existing service methods plus gated gRPC adapter skeletons. Gateway transport handlers also have optional API-key admission and bounded in-flight backpressure contracts. Real gRPC server binding and UDP/HTTP endpoints remain pending.
- Admin health/metrics now has a dependency-free service boundary for component health aggregation, service/dependency/lifecycle probe helpers, split liveness/readiness snapshots, config-driven production-adapter readiness policy flags, dependency health source registry, Prometheus-text metric snapshots, transport-neutral health/metrics endpoint responses, configurable HTTP-style response binding exposed through runtime composition, a lifecycle-aware in-process request loop, and a gated gRPC adapter skeleton. Real socket/gRPC server binding remains pending.
- Runtime startup now flows through a dependency-free `RuntimeApplication` composition boundary that owns role-selected services, validates post-load config, registers process-level admin lifecycle probes, reports startup failures through admin health, aggregates readiness/health, and stops services in reverse order.
- Process startup/shutdown paths emit structured logfmt events through a dependency-free formatter.
- Metrics currently has deterministic fallback behavior for unit and lifecycle tests. DLQ replay now retries transient history-write failures and defers uncommitted messages after retry exhaustion. PostGIS keeps deterministic fallback behavior by default and has an optional `SR_ENABLE_REAL_POSTGIS` libpq adapter path pending real-PostGIS verification. Redis keeps deterministic fallback behavior by default and has an optional `SR_ENABLE_REAL_REDIS` redis-plus-plus adapter path pending real-Redis verification. H3 keeps deterministic fallback behavior by default and has an optional `SR_ENABLE_REAL_H3` C API adapter path pending real-H3 verification. Kafka keeps deterministic fallback behavior by default, has an optional `SR_ENABLE_REAL_KAFKA` librdkafka++ adapter path pending broker-backed verification, and drives the matching request/result loop through the shared matching codec.
- Processor/geofence/metrics observer wiring is implemented for in-process fallback composition.
- Domain-to-wire conversion contracts live under `src/common/proto/`; generated protobuf code should adapt through that boundary.

## Required Before Parallel Work

### 1. Assign File Ownership
Each parallel agent must own a disjoint file set. Shared files require explicit coordination.

Ownership is exclusive for the duration of a batch:
- Do not edit files outside the assigned scope.
- Do not reformat, rename, or clean up files owned by another agent.
- If a task needs a shared file, stop and coordinate before editing it.
- If another agent has already changed a file in your scope, inspect and adapt to that change instead of reverting it.
- Documentation-only agents may edit only their assigned docs; they must not edit source, tests, task files, config files, or generated artifacts.

| Area | Primary Owner | Files |
|---|---|---|
| Event contracts | Framework owner | `src/common/events/` |
| Composition wiring | Framework owner | `src/runtime/`, `src/common/composition/`, `src/main.cpp` |
| Config | Config owner | `src/common/config/`, `config/signalroute.toml`, config docs/tests |
| Spatial | Spatial owner | `src/common/spatial/`, spatial tests |
| Redis/state | Storage owner | `src/common/clients/redis_client.*`, state tests |
| PostGIS/history | Storage owner | `src/common/clients/postgres_client.*`, `db/migrations/`, history tests |
| Kafka/protobuf | Transport owner | `src/common/kafka/`, `proto/`, transport tests |
| Gateway | Gateway owner | `src/gateway/`, gateway tests |
| Processor | Processor owner | `src/processor/`, processor tests |
| Query | Query owner | `src/query/`, query tests |
| Geofence | Geofence owner | `src/geofence/`, geofence tests |
| Matching | Matching owner | `src/matching/`, matching tests |
| Workers/ops | Ops owner | `src/workers/`, Docker/CI/metrics/admin docs/tests |

### 2. Freeze Canonical Names
Use these names unless the framework owner updates all references in one coordinated change.

- Canonical config file: `config/signalroute.toml`
- Legacy config file: root `config.toml` should not drive runtime behavior
- Role values: `standalone`, `gateway`, `processor`, `query`, `geofence`, `matcher`
- Config section for matching: `[matching]`
- Matching role value: `matcher`
- PostGIS section: `[postgis]`
- Redis endpoint key: `[redis].addrs`
- Kafka location topic key: `[kafka].ingest_topic`
- Kafka DLQ topic key: `[kafka].dlq_topic`
- Geofence enable key: `[geofence].eval_enabled`

### 3. Event Contract Rules
- Add or change event payloads only in `src/common/events/`.
- Prefer completed-fact event names: `LocationAccepted`, `StateWriteSucceeded`, `TripHistoryWritten`.
- Prefer requested-action event names only for internal fan-out: `StateWriteRequested`, `TripHistoryWriteRequested`, `GeofenceEvaluationRequested`.
- Do not introduce stringly typed events.
- Do not make event handlers own durable guarantees; persist or publish to Kafka for durability.
- Event payloads should be small value objects with no owning service pointers.

### 4. Test Rules
- Every agent adds or updates feature/function tests for its area.
- No tests named by phase, milestone, sprint, or implementation wave.
- Unit tests must not require Kafka, Redis, PostGIS, or gRPC unless explicitly placed in integration test structure.
- Integration tests must be grouped by feature: ingestion, state persistence, trip history, nearby query, geofence events, matching reservation.

### 5. Merge Rules
Before a code or test agent returns work, it must run:

```sh
cmake -S . -B /tmp/signalroute-build -DSR_BUILD_TESTS=ON
cmake --build /tmp/signalroute-build -j2
ctest --test-dir /tmp/signalroute-build --output-on-failure
```

For docs-only work, no build is required. The agent must inspect its diff instead:

```sh
git diff -- README.md docs/plans/parallel_implementation.md
git diff --name-only
```

An agent must report:
- files changed
- tests added or updated
- commands run
- known limitations
- whether it touched shared contracts

### 6. Dependency Order
Start remaining production work in this order unless deliberately coordinated:

1. Broker-backed Kafka compile/integration verification once RdKafka is installed
2. Real-H3 compile/integration verification once H3 is installed
3. Real-Redis compile/integration verification once hiredis/redis++ are installed
4. Real-PostGIS compile/integration verification once PostgreSQL/libpq and a PostGIS database are available
5. Gateway gRPC/UDP transport over the existing validation/rate-limit/publish flow
6. Query gRPC/HTTP transport over the existing latest/nearby/trip handlers
7. Processor production Kafka-to-state/history loop narrowing CSV fallback parsing
8. Geofence production registry loading, Kafka event serialization, and admin CRUD
9. Workers, Prometheus/admin health, retry/backoff, CI, packaging, and performance tests

## Safe Initial Parallel Batch
These tasks can run in parallel with low conflict risk from the current fallback baseline. They are recommended before broad production adapter work because they clarify external dependency choices and preserve existing contracts.

| Task | Owner Scope | Avoid Touching |
|---|---|---|
| Protobuf generation spike | `proto/`, generated build wiring in isolated branch/scope | Gateway/query service logic, CSV fallback removal |
| H3 adapter spike | `src/common/spatial/`, `test_h3_index` | Query/geofence behavior changes |
| Redis adapter spike | `src/common/clients/redis_client.*`, Redis integration tests | State writer interface changes |
| PostGIS adapter spike | `src/common/clients/postgres_client.*`, migrations, PostGIS integration tests | History writer policy changes |
| API transport design docs | docs/API and transport docs only | Source interfaces |

Run only disjoint scopes in a batch unless owners explicitly agree to widen scope. Do not remove fallback behavior until equivalent production adapter tests exist.

## Unsafe Parallel Pairs
Do not run these at the same time without coordination:

- Redis adapter and state writer interface changes
- Protobuf generation and gateway/query service transport
- Event payload redesign and processor/geofence/matching event wiring
- Config schema changes and config loader implementation
- CMake dependency strategy and any task adding external dependencies

## Next Recommended Implementation Task
Continue with dependency-free operations hardening while gRPC packages remain unavailable: add real adapter health implementations behind existing Kafka, Redis, PostGIS, and H3 client interfaces, or add a minimal admin socket binding over the existing request loop. If gRPC packages are installed, compile `SR_ENABLE_GRPC=ON`, fix any package-backed adapter compile issues, then add a minimal server composition object that registers admin/gateway/query gRPC services by role. Keep CSV fallback decoding until durable Kafka integration tests pass.
