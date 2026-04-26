# Parallel Implementation Guardrails

## Purpose
This file defines the preparation required before running multiple agents or developers in parallel. The goal is to prevent merge conflicts, contract drift, duplicate abstractions, and incompatible event/config names.

## Current Baseline
- The project builds and tests from the shared baseline.
- Tests are organized by feature/function, not by phase.
- In-process communication uses typed `EventBus` payloads under `src/common/events/`.
- Durable cross-process communication remains Kafka.
- The tracked completion roadmap is `docs/plans/finish_plan.md`.

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
| Composition wiring | Framework owner | `src/common/composition/`, `src/main.cpp` |
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
Start work in this order unless deliberately coordinated:

1. Config loader and canonical config cleanup
2. Event payload/composition contract hardening
3. Domain conversion tests
4. Dependency strategy and CMake dependency integration
5. H3 adapter
6. Redis adapter and state writer
7. PostGIS adapter and history writer
8. Kafka/protobuf transport
9. Gateway and processor loops
10. Query service transport
11. Geofence engine
12. Matching framework
13. Workers, metrics, admin, CI, packaging

## Safe Initial Parallel Batch
These tasks can run in parallel with low conflict risk. This is the recommended first batch because the scopes are naturally disjoint and avoid cross-cutting runtime wiring.

| Task | Owner Scope | Avoid Touching |
|---|---|---|
| Config loader | `src/common/config/`, config tests | Event payloads, storage clients |
| Domain tests | `src/common/types/`, type tests | Config parser, transport |
| Event payload review | `src/common/events/` | Service implementations |
| Spatial tests | `src/common/spatial/`, spatial tests | Real dependency CMake until dependency strategy is decided |
| README/build docs | `README.md`, docs only | Source interfaces |

Run only the tasks above in the first parallel batch unless the owners explicitly agree to widen scope. Defer adapters, service loops, dependency integration, and generated protobuf work until the contracts and canonical config names are stable.

## Unsafe Parallel Pairs
Do not run these at the same time without coordination:

- Redis adapter and state writer interface changes
- Protobuf generation and gateway/query service transport
- Event payload redesign and processor/geofence/matching event wiring
- Config schema changes and config loader implementation
- CMake dependency strategy and any task adding external dependencies

## First Recommended Implementation Task
Implement `Config::load` and config validation using `config/signalroute.toml` as the canonical source. Add `test_config_loader` before starting Redis/Kafka/PostGIS work.
