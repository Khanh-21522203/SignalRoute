# SignalRoute Completion Plan

## Purpose
This plan turns the current fallback runtime into a finished backend system. It is organized by delivery milestones and feature areas. Tests must be organized by function, feature, behavior, or subsystem, not by milestone name.

## Current Baseline

### Already Present
- C++20 CMake project structure.
- Service modules: gateway, processor, query, geofence, matching, workers.
- Protobuf contract drafts for location, query, geofence, matching, and admin APIs.
- Database migrations for trip history, H3 index, compression, and geofence expiry.
- Documentation for architecture, ingestion, storage, spatial query, matching, and skeleton plan.
- Dependency-free in-memory baseline for latest state, cell lookup, sequence rejection, dedup, and nearby query.
- Header-only typed in-process `EventBus` with gateway, location, state, history, geofence, matching, worker, and metrics-facing payloads.
- CTest wiring for independent unit executables.
- In-memory Kafka producer/consumer fallback with produce, poll, commit, callback, and lag behavior.
- Redis fallback behavior for device state, H3 cell membership, fence state, reservations, TTL expiry, and stale H3 cleanup.
- PostGIS fallback behavior for trip history, spatial trip filters, geofence rules, and geofence audit records.
- Processor fallback flow with dedup, sequence guard, state/history fan-out, offset commits, and shared location payload decoding that uses protobuf when enabled and CSV as fallback.
- In-process observer-style composition for processor -> state/history -> geofence -> metrics.
- Gateway fallback ingest methods with validation, rate limiting, shared location payload encoding, and typed gateway events.
- Query fallback service lifecycle for latest, nearby, trip, and spatial trip reads.
- Geofence fallback evaluation for enter, exit, old-cell exit, dwell, audit, and event publication.
- Matching fallback service lifecycle, reservation flow, nearest strategy, deadlines, cleanup, and typed matching events.
- Worker fallback `run_once` flows for H3 cleanup, DLQ replay, and metrics export.
- Fallback-first dependency build switches in `cmake/SignalRouteOptions.cmake` and central discovery/linking in `cmake/SignalRouteDependencies.cmake`.
- Stable `signalroute_proto` target that is an interface target in fallback mode, a generated protobuf message library when `SR_ENABLE_PROTOBUF=ON`, and a generated gRPC stub library when `SR_ENABLE_GRPC=ON`.
- Dependency-free domain-to-wire conversion contracts under `src/common/proto/` for location, query device state, geofence events, and matching request/result payloads.
- Protobuf package namespace is `signalroute.v1`, so generated C++ types live under `signalroute::v1` and do not collide with domain types under `signalroute`.

### Known Boundaries
- Kafka, Redis, PostGIS, gRPC, real H3, and Prometheus are not integrated. Protobuf message generation is optional and integrated behind `SR_ENABLE_PROTOBUF`.
- Production dependency switches exist but real adapter implementations still need to be written behind existing interfaces.
- Gateway, processor, geofence, matching codec, and DLQ replay boundaries now use shared payload codecs that emit protobuf when `SR_ENABLE_PROTOBUF=ON` and preserve CSV fallback decoding for skeleton tests. Real Kafka transport is still pending.
- Gateway does not expose real gRPC/UDP endpoints yet.
- Query service does not expose real gRPC/HTTP endpoints yet.
- Event bus wiring is implemented for the processor/geofence/metrics fallback path, but cross-role production deployment still needs explicit durable Kafka/protobuf boundaries.
- Geofence and matching fallback flows are implemented, but production adapters, transport handlers, and admin APIs are still pending.

## Engineering Rules
- Keep tests grouped by feature/function: `test_dedup_window`, `test_state_writer`, `test_nearby_handler`, `test_geofence_evaluator`, etc.
- Keep external infrastructure behind interfaces so unit tests can run without Kafka/Redis/PostGIS.
- Add integration tests separately from unit tests, grouped by feature: ingestion pipeline, state persistence, trip history, nearby query, geofence events, matching reservation.
- Do not replace a working in-memory contract until the real adapter has equivalent tests.
- Any CSV fallback payload format is temporary test scaffolding and must not become the production Kafka wire contract.
- Every milestone must end with build, unit tests, relevant integration tests, and updated docs.

## In-Process Event Framework Architecture

SignalRoute should use two distinct event mechanisms:

- **In-process typed events:** local component coordination inside one process. These are callback-based and Observer-style, similar to C# events/delegates.
- **Kafka events:** durable cross-process communication and replayable integration boundaries.

### Event Bus Rules
- Components publish typed events and do not call every downstream component directly.
- Components subscribe during startup/composition, not lazily during hot-path processing.
- Event handlers should be small and deterministic; expensive work should enqueue internal work or move to durable infrastructure where appropriate.
- In-process events are not a durability mechanism. If a result must survive process death, publish to Kafka or persist it.
- Event payloads should be immutable value objects.
- Event names should describe completed facts or requested actions:
  - `LocationValidated`
  - `LocationAccepted`
  - `StateWriteSucceeded`
  - `TripHistoryWriteRequested`
  - `GeofenceEvaluationRequested`
  - `MatchCompleted`
- Avoid generic string event names on hot paths. Prefer C++ types for compile-time safety.
- Metrics should subscribe to domain events instead of being called manually from every component where feasible.

### Implemented/Intended Component Flow

```text
Gateway
  publishes LocationReceived
  publishes LocationValidated
  publishes LocationRejected

Processor
  currently consumes Kafka fallback payloads; later may subscribe LocationValidated in fully in-process gateway mode
  publishes LocationAccepted
  publishes LocationDuplicateRejected
  publishes LocationStaleRejected

StateWriter
  receives StateWriteRequested through ProcessorEventHandlers
  publishes StateWriteSucceeded
  publishes StateWriteRejected
  publishes StateWriteFailed

HistoryWriter
  receives TripHistoryWriteRequested through ProcessorEventHandlers
  publishes TripHistoryWritten
  publishes TripHistoryWriteFailed

GeofenceEngine
  subscribes GeofenceEvaluationRequested through GeofenceEventHandlers
  publishes GeofenceEntered/Exited/Dwell through fallback event/Kafka boundaries

MatchingService
  publishes MatchRequestReceived when handle_request is called
  publishes AgentReserved, MatchCompleted, MatchFailed, MatchExpired

Workers
  publish CleanupCompleted, DLQReplaySucceeded, DLQReplayFailed, DependencyRecovered

Metrics/Admin
  subscribe to events they observe
```

### Composition Root
The process role startup should wire subscriptions in one place. Components should expose `subscribe_to(EventBus&)` or explicit wiring functions only after their dependencies are constructed. This keeps module dependencies shallow and makes tests easy to assemble.

---

## Multi-Agent Implementation Breakdown

Status markers:
- **Done fallback:** implemented and covered by unit/lifecycle tests using deterministic in-memory adapters.
- **Production pending:** external dependency, transport, or operations work still required.
- **Refine:** fallback exists but needs hardening, API polish, or integration tests.

Use this section when running multiple agents. Each task is intentionally scoped to minimize merge conflicts. Agents should own specific files/modules and avoid editing files outside their ownership unless explicitly coordinated.

### Agent Task A1: Event Framework Core
- **Ownership:** `src/common/events/`, `tests/unit/test_event_bus.cpp`.
- **Status:** Done fallback; refine diagnostics only if needed.
- **Goal:** Mature the typed event bus.
- **Items:** subscription lifetime, unsubscribe safety, handler ordering, exception policy, optional handler count diagnostics.
- **Depends on:** none.
- **Verification:** `test_event_bus`.

### Agent Task A2: Event Payload Catalog
- **Ownership:** `src/common/events/*_events.h`, event-related docs.
- **Status:** Done fallback; refine when protobuf/API contracts are finalized.
- **Goal:** Define stable typed event payloads for gateway, processor, state, history, geofence, matching, workers, and metrics.
- **Items:** location events, state events, history events, geofence events, matching events, worker events.
- **Depends on:** A1.
- **Verification:** compile plus event payload construction tests.

### Agent Task A3: Composition Root Wiring
- **Ownership:** `src/main.cpp`, service startup wiring files if introduced.
- **Status:** Done fallback for processor/geofence/metrics standalone wiring; production cross-role wiring pending.
- **Goal:** Centralize in-process event wiring by role.
- **Items:** create shared `EventBus`, wire processor/state/history/geofence/metrics subscriptions, keep role boundaries clear.
- **Depends on:** A1, A2.
- **Verification:** startup smoke tests and feature tests.

### Agent Task B1: Test Naming And Harness Cleanup
- **Ownership:** `tests/CMakeLists.txt`, `tests/unit/` names only.
- **Goal:** Ensure tests are grouped by function/feature, never by phase.
- **Items:** maintain feature-oriented test names, split broad smoke tests into focused files as they grow.
- **Depends on:** none.
- **Verification:** `ctest` target list uses feature names.

### Agent Task B2: Core Domain Tests
- **Ownership:** `src/common/types/`, related tests.
- **Goal:** Finish unit coverage for domain objects and result handling.
- **Items:** conversion helpers, metadata behavior, invalid state tests.
- **Depends on:** none.
- **Verification:** `test_location_event`, `test_device_state`, `test_result`.

### Agent Task C1: Config Loader
- **Ownership:** `src/common/config/`, config tests, config docs.
- **Goal:** Implement TOML parsing and validation.
- **Items:** canonical config path, default values, required fields, role validation, topic naming consistency.
- **Depends on:** none.
- **Verification:** `test_config_loader`.

### Agent Task C2: Dependency Strategy
- **Ownership:** root `CMakeLists.txt`, dependency docs, toolchain files if used.
- **Status:** Done fallback-first CMake switches; package provider/toolchain lock still pending.
- **Goal:** Choose and implement dependency management.
- **Items:** H3, protobuf, gRPC, Redis client, Kafka client, Postgres client, Prometheus.
- **Depends on:** none, but should coordinate before storage/transport work.
- **Verification:** clean configure on a fresh checkout.

### Agent Task D1: Spatial Adapter
- **Ownership:** `src/common/spatial/`, `tests/unit/test_h3_index.cpp`.
- **Goal:** Replace deterministic fallback with real H3 behind the same interface.
- **Items:** coordinate encoding, grid disk, polygon cells, edge cases.
- **Depends on:** C2.
- **Verification:** `test_h3_index`, nearby/geofence spatial tests.

### Agent Task D2: Nearby Query Feature
- **Ownership:** `src/query/nearby_handler.*`, nearby tests.
- **Status:** Done fallback; production adapter integration pending.
- **Goal:** Complete nearby behavior independent of transport.
- **Items:** candidate lookup, distance filtering, sorting, limit, freshness, metadata filters.
- **Depends on:** D1 or fallback adapter.
- **Verification:** `test_nearby_handler`.

### Agent Task E1: Redis State Adapter
- **Ownership:** `src/common/clients/redis_client.*`, Redis integration tests.
- **Goal:** Implement real Redis behavior with Lua CAS and pipelines.
- **Items:** device state, H3 cell sets, fence state, reservations, health, metrics hooks.
- **Depends on:** C2.
- **Verification:** Redis integration tests.

### Agent Task E2: State Writer And Sequence Guard
- **Ownership:** `src/processor/state_writer.*`, `src/processor/sequence_guard.*`, related tests.
- **Status:** Done fallback; Redis concurrency/integration tests pending.
- **Goal:** Make state update correctness production-grade.
- **Items:** old/new H3 cell handling, stale rejection, error mapping, event publication.
- **Depends on:** A1/A2, E1.
- **Verification:** `test_state_writer`, `test_sequence_guard`, Redis concurrency tests.

### Agent Task F1: PostGIS Adapter
- **Ownership:** `src/common/clients/postgres_client.*`, migrations, storage tests.
- **Goal:** Implement trip history and geofence persistence.
- **Items:** connection pool, batch insert, trip replay, spatial trip filters, geofence rule load, geofence audit insert.
- **Depends on:** C2.
- **Verification:** PostGIS integration tests.

### Agent Task F2: History Writer
- **Ownership:** `src/processor/history_writer.*`, history tests.
- **Status:** Done fallback; production PostGIS/DLQ integration pending.
- **Goal:** Implement history buffering, flush, DLQ fallback, and event publication.
- **Items:** buffer by size/time, idempotent write, stale-late policy support, failure events.
- **Depends on:** A1/A2, F1.
- **Verification:** `test_history_writer`, DLQ tests.

### Agent Task G1: Kafka Wrapper
- **Ownership:** `src/common/kafka/`, Kafka tests.
- **Status:** Done fallback; production Kafka client integration pending.
- **Goal:** Implement producer/consumer wrappers.
- **Items:** produce, delivery callbacks, poll, manual commit, rebalance callbacks, lag, health.
- **Depends on:** C2.
- **Verification:** Kafka integration tests.

### Agent Task G2: Protobuf Generation And Conversion
- **Ownership:** `proto/`, generated build wiring, conversion tests.
- **Status:** Conversion contracts, generated protobuf adapters, generated message round-trip tests, Kafka fallback protobuf payload round-trip tests, gateway/processor runtime protobuf location payloads, geofence runtime event payloads, matching request/result payload codecs, and DLQ replay protobuf location decoding done; generated gRPC stubs pending.
- **Goal:** Enable protobuf/gRPC generation and domain conversion.
- **Items:** CMake generation, proto library, conversion helpers, serialization tests.
- **Depends on:** C2.
- **Verification:** proto round-trip tests.

### Agent Task H1: Gateway Service
- **Ownership:** `src/gateway/`, gateway tests.
- **Status:** Done fallback for direct ingest methods; gateway runtime emits protobuf location payloads when `SR_ENABLE_PROTOBUF=ON` and CSV in default fallback builds; gRPC/UDP pending.
- **Goal:** Implement gRPC/UDP ingestion and event publication.
- **Items:** service handlers, validation, rate limiting, server timestamp, Kafka publish, in-process event publish for standalone mode.
- **Depends on:** A1/A2, G1/G2.
- **Verification:** gateway unit/integration tests.

### Agent Task I1: Processor Loop
- **Ownership:** `src/processor/processing_loop.*`, processor tests.
- **Status:** Done fallback and EventBus fan-out; processor runtime decodes protobuf location payloads when `SR_ENABLE_PROTOBUF=ON` and still accepts CSV fallback payloads.
- **Goal:** Complete Kafka-to-state/history processing.
- **Items:** deserialize, dedup, sequence guard, publish internal events, commit offsets, DLQ behavior.
- **Current skeleton note:** CSV remains only as default fallback-build scaffolding and decoder compatibility. Do not extend CSV as a durable or public Kafka payload format.
- **Depends on:** A1/A2, E2, F2, G1/G2.
- **Verification:** processor loop integration tests.

### Agent Task J1: Query Transport
- **Ownership:** `src/query/query_service.*`, query proto handlers/tests.
- **Status:** Handler/service fallback done; gRPC/HTTP transport pending.
- **Goal:** Expose latest, nearby, and trip APIs.
- **Items:** gRPC service implementation, response mapping, validation, error mapping, streaming decision.
- **Depends on:** G2, D2, F1.
- **Verification:** query service integration tests.

### Agent Task K1: Geofence Registry And Evaluator
- **Ownership:** `src/geofence/fence_registry.*`, `src/geofence/evaluator.*`, geofence tests.
- **Status:** Done fallback; runtime geofence event payloads use protobuf when enabled and CSV in default fallback builds; production H3/PostGIS/Kafka integration pending.
- **Goal:** Complete event-driven geofence evaluation.
- **Items:** registry reload, candidate lookup, old-cell exit checks, enter/exit events, audit writes, Kafka events.
- **Depends on:** A1/A2, D1, E1, F1, G1.
- **Verification:** geofence evaluator tests.

### Agent Task K2: Dwell Checker
- **Ownership:** `src/geofence/dwell_checker.*`, dwell tests.
- **Status:** Done fallback; runtime dwell event payloads use protobuf when enabled and CSV in default fallback builds; production scheduling/indexing pending.
- **Goal:** Complete dwell transition behavior.
- **Items:** inside state tracking, threshold detection, no duplicate dwell events, event publication.
- **Depends on:** K1.
- **Verification:** `test_dwell_checker`.

### Agent Task L1: Matching Framework
- **Ownership:** `src/matching/`, matching tests.
- **Status:** Done fallback; matching request/result payload codecs are ready; Kafka loop and integration tests pending.
- **Goal:** Complete match context, strategy registry usage, reservations, and result flow.
- **Items:** built-in nearest strategy, request deadline, reservation cleanup, result publishing, events.
- **Depends on:** A1/A2, D2, E1, G1/G2.
- **Verification:** matching unit/integration tests.

### Agent Task M1: Workers
- **Ownership:** `src/workers/`, worker tests.
- **Status:** Done fallback; DLQ replay decodes shared location payloads, including protobuf when enabled; production retry/backoff and external dependency integration pending.
- **Goal:** Complete cleanup, DLQ replay, and metrics worker behavior.
- **Items:** H3 cleanup, DLQ replay, retry/backoff, event publication.
- **Depends on:** A1/A2, E1, F1, G1.
- **Verification:** worker tests.

### Agent Task N1: Observability And Admin
- **Ownership:** `src/common/metrics/`, admin proto/service, observability tests.
- **Goal:** Complete metrics, health, readiness, and admin APIs.
- **Items:** Prometheus exporter, health aggregation, component status events, admin service.
- **Depends on:** A1/A2, C2.
- **Verification:** metrics/admin tests.

### Agent Task O1: Packaging And CI
- **Ownership:** Docker/CI/build docs.
- **Goal:** Make the project reproducible.
- **Items:** Docker Compose, production Dockerfile, CI build/test/integration jobs, sanitizer profile.
- **Depends on:** C2 and enough integration tests to run in CI.
- **Verification:** CI passes from clean environment.

---

## Milestone 1: Baseline Cleanup And Test Reorganization

### Goal
Make the skeleton clean, consistently named, and ready for parallel development.

### Work Items
- Rename any phase-oriented tests to feature names.
- Keep feature-oriented test names such as `test_location_state_query.cpp`; do not add phase-oriented test names.
- Update `tests/CMakeLists.txt` to match feature test names.
- Remove or quarantine the root-level CLion sample `main.cpp` if it is not part of the build.
- Decide whether `config.toml` or `config/signalroute.toml` is canonical; remove or mark the other as example/legacy.
- Normalize service role naming: `matcher` vs `matching`, `postgis` vs `postgres`, `ingest_topic` vs `location_topic`.
- Add a top-level `README.md` with build/test/run commands.
- Add `.gitignore` coverage for local build directories, IDE metadata, generated protobuf files, and temporary outputs.

### Acceptance Criteria
- Fresh configure/build passes from an empty build directory.
- `ctest` passes.
- Test names describe behavior or subsystem, not implementation phase.
- There is one canonical example config path.
- New contributors can run build and tests from `README.md`.

### Test Targets
- `test_haversine`
- `test_point_in_polygon`
- `test_validator`
- `test_location_state_query`
- `test_dedup_window`
- `test_sequence_guard`

---

## Milestone 2: Domain Model And Config

### Goal
Finish the dependency-free domain core and configuration layer.

### Work Items
- Implement TOML parsing in `Config::load`.
- Validate required config fields at startup.
- Add config defaulting rules and clear error messages.
- Add protobuf-to-domain and domain-to-protobuf conversion boundaries.
- Finalize internal domain types:
  - `LocationEvent`
  - `DeviceState`
  - `GeofenceRule`
  - `GeofenceEventRecord`
  - matching request/candidate/result internal types
- Add time utilities for epoch milliseconds and monotonic elapsed time.
- Add structured error types where string errors are too weak.
- Decide metadata representation and serialization rules.

### Acceptance Criteria
- Config file values actually affect runtime behavior.
- Invalid config fails fast with actionable messages.
- Domain conversion tests cover required fields, defaults, invalid values, and metadata.
- No service directly depends on generated protobuf types outside API/transport boundaries.

### Test Targets
- `test_config_loader`
- `test_location_event_conversion`
- `test_device_state_conversion`
- `test_geofence_types`
- `test_result`

---

## Milestone 3: Real Spatial Index Adapter

### Goal
Replace the deterministic local grid fallback with real H3 while preserving the same public interface.

### Work Items
- Integrate H3 dependency through the chosen dependency manager.
- Implement `lat_lng_to_cell` using real H3.
- Implement `grid_disk` using real H3.
- Implement `polygon_to_cells` using overlapping containment semantics.
- Verify radius-to-k mapping against documented resolution tables.
- Add edge-case handling:
  - invalid coordinates
  - antimeridian polygons
  - polar regions
  - empty polygons
  - high radius limits
- Keep a fake or deterministic adapter for unit tests if useful.

### Acceptance Criteria
- Real H3 cells are used in production builds.
- Unit tests cover stable known H3 outputs where appropriate.
- Nearby query and geofence prefilter tests pass with real H3.
- Documentation states exact H3 dependency/version and containment mode.

### Test Targets
- `test_h3_index`
- `test_nearby_cell_expansion`
- `test_geofence_polyfill`
- `test_spatial_edge_cases`

---

## Milestone 4: Redis State Store

### Goal
Implement production Redis adapter for latest state, H3 cell index, fence state, and matching reservations.

### Work Items
- Choose Redis C++ client and connection pooling strategy.
- Parse Redis config fields correctly.
- Implement `ping` and health reporting.
- Implement atomic device state CAS with Lua:
  - reject stale sequence
  - update device hash
  - move H3 cell membership atomically
  - apply device TTL
- Implement batch `HGETALL` pipeline for device states.
- Implement pipelined `SMEMBERS` for nearby candidate cells.
- Implement fence state read/write.
- Implement agent reservation with `SET NX PX` and Lua compare-delete release.
- Add retry/backoff policy for transient Redis failures.
- Add metrics for latency, error rate, pool usage, and CAS rejects.

### Acceptance Criteria
- Redis adapter passes integration tests against real Redis.
- In-memory adapter remains usable for unit tests.
- CAS prevents stale state overwrite under concurrent writes.
- H3 cell index does not leave devices in old cells after movement.
- Reservation release only releases the holder's reservation.

### Test Targets
- `test_redis_device_state`
- `test_redis_h3_cell_index`
- `test_redis_fence_state`
- `test_redis_reservation`
- `test_state_writer_concurrency`

---

## Milestone 5: PostGIS And Trip History

### Goal
Implement trip persistence, trip replay, spatial history filters, geofence rule loading, and geofence event audit writes.

### Work Items
- Choose `libpq` or `libpqxx` and connection pooling strategy.
- Implement migration runner or document migration execution clearly.
- Review schema consistency with docs:
  - primary key and unique idempotency constraints
  - geography vs geometry usage
  - `h3_r7` naming if resolution can vary
  - Timescale hypertable constraints
- Implement batch trip insert with `ON CONFLICT DO NOTHING`.
- Implement trip query by device/time range.
- Implement spatial trip query with `ST_DWithin`.
- Implement optional bbox filter if kept in API.
- Implement geofence rule loading and parsing into `GeofenceRule`.
- Implement geofence event audit insert.
- Add query timeout handling and error mapping.
- Add metrics for query latency and write latency.

### Acceptance Criteria
- Integration tests pass against TimescaleDB/PostGIS.
- Duplicate trip events are idempotent.
- Late events appear in correct event-time order.
- Trip spatial filters use expected indexes in query plans.
- Geofence rules load correctly at startup.

### Test Targets
- `test_trip_history_writer`
- `test_trip_replay_query`
- `test_trip_spatial_filter`
- `test_geofence_rule_repository`
- `test_geofence_event_audit`

---

## Milestone 6: Kafka And Protobuf Pipeline

### Goal
Implement durable messaging and generated protobuf contracts.

Shared payload codecs now cover location, geofence events, matching request/result messages, and DLQ location replay. CSV remains only as fallback-build scaffolding and protobuf-build decoder compatibility. This milestone must still replace the in-memory Kafka fallback with real durable Kafka transport and keep domain code independent of generated protobuf types.

### Work Items
- Enable protobuf and gRPC generation in CMake.
- Link generated proto library to relevant modules.
- Implement Kafka producer wrapper:
  - topic/key/payload produce
  - delivery callback
  - flush
  - error reporting
  - backpressure detection
- Implement Kafka consumer wrapper:
  - subscribe
  - poll
  - manual commit
  - rebalance callbacks
  - lag reporting
- Implement location event serialization/deserialization. (Done for shared runtime codec; real Kafka adapter pending.)
- Implement geofence event serialization. (Done for shared runtime codec; real Kafka adapter pending.)
- Implement matching request/result serialization. (Done for shared runtime codec; matching Kafka loop pending.)
- Add topic config validation.
- Add DLQ payload format and replay metadata. (Shared location payload replay done; retry metadata pending.)

### Acceptance Criteria
- Location events round-trip through protobuf serialization.
- Kafka integration tests publish and consume real messages.
- Processor commits offsets only after successful processing.
- Producer errors are visible through metrics/logs.
- Consumer lag is exposed.

### Test Targets
- `test_location_event_proto`
- `test_kafka_producer`
- `test_kafka_consumer`
- `test_kafka_location_roundtrip`
- `test_dlq_payload`

---

## Milestone 7: Ingestion Gateway

### Goal
Expose real device ingestion through gRPC and UDP, then publish validated events to Kafka.

### Work Items
- Implement gRPC `IngestService`.
- Implement `IngestSingle` and `IngestBatch` handlers.
- Validate required fields, coordinates, sequence, timestamp, accuracy, and batch size.
- Implement API key/auth placeholder or final auth decision.
- Implement per-device rate limiter with bounded memory.
- Stamp `server_recv_ms`.
- Publish accepted events to Kafka using `device_id` as key.
- Return partial batch rejection details if supported.
- Implement UDP listener if still in scope.
- Add backpressure behavior when Kafka is unavailable or producer queue is full.
- Add gateway metrics.

### Acceptance Criteria
- gRPC ingestion accepts valid events and rejects invalid events with correct status.
- Rate limiting works per device.
- Kafka publish key is `device_id`.
- Gateway remains stateless with no latest-state writes.
- UDP mode behavior is explicitly tested or deferred from scope.

### Test Targets
- `test_ingest_validator`
- `test_rate_limiter`
- `test_ingest_grpc_service`
- `test_ingest_batch_partial_rejection`
- `test_gateway_kafka_publish`
- `test_udp_ingest` if UDP remains in scope

---

## Milestone 8: Location Processor

### Goal
Implement the complete correctness path from Kafka message to Redis state and PostGIS history.

### Work Items
- Deserialize Kafka location messages.
- Apply dedup window.
- Apply sequence guard pre-check.
- Write state through Redis CAS.
- Write accepted fresh events to history.
- Handle stale-but-late events according to tolerance rules.
- Handle truly stale events.
- Batch history writes and flush by size/time.
- Send failed history writes to DLQ.
- Commit Kafka offset only after required writes succeed.
- Notify geofence engine or publish evaluation requests.
- Add processor metrics for accepted, duplicate, stale, truly stale, DLQ, write errors, and lag.

### Acceptance Criteria
- Duplicate events do not duplicate state or history.
- Stale events do not overwrite latest state.
- Late tolerated events are written to trip history when intended.
- Failed PostGIS writes go to DLQ without blocking state updates if that policy is retained.
- Offset commit behavior matches at-least-once semantics.

### Test Targets
- `test_processing_loop_dedup`
- `test_processing_loop_sequence_guard`
- `test_processing_loop_late_event_history`
- `test_processing_loop_dlq`
- `test_processor_offset_commit`
- `test_history_flush_policy`

---

## Milestone 9: Query Service

### Goal
Expose production query APIs for latest location, nearby devices, and trip replay.

### Work Items
- Implement gRPC `QueryService`.
- Decide whether HTTP/JSON is required in addition to gRPC.
- Implement latest location handler over Redis.
- Implement nearby handler over Redis H3 index:
  - radius clamp
  - limit clamp
  - freshness filter
  - metadata filter if kept
  - haversine exact filtering
  - distance sorting
- Implement trip handler over PostGIS:
  - time range validation
  - limit validation
  - optional downsampling
  - optional spatial filters
- Add pagination or streaming for large trip replays.
- Add query metrics and structured errors.

### Acceptance Criteria
- Query API returns correct protobuf responses.
- Nearby query does not miss in-radius devices covered by candidate cells.
- Nearby results are sorted by distance and respect limit/freshness filters.
- Trip replay is event-time ordered.
- Large trip replay behavior is bounded by limit/streaming.

### Test Targets
- `test_latest_handler`
- `test_nearby_handler`
- `test_nearby_freshness_filter`
- `test_nearby_limit_sorting`
- `test_trip_handler`
- `test_query_grpc_service`

---

## Milestone 10: Geofence Engine

### Goal
Implement event-driven geofence evaluation with state transitions and event emission.

### Work Items
- Implement `FenceRegistry` reload behavior.
- Implement evaluator:
  - candidate lookup by new and old H3 cells
  - deduplicate candidate fences
  - point-in-polygon containment
  - Redis fence state transition
  - Kafka geofence event publish
  - PostGIS audit insert
- Implement dwell checker:
  - scan inside states or maintain active inside index
  - emit dwell once per threshold policy
  - avoid duplicate dwell events
- Implement Admin fence CRUD if still in scope.
- Implement geofence query: device current fence states.
- Add fallback strategy for complex polygons if PostGIS containment is required.
- Add geofence metrics.

### Acceptance Criteria
- OUTSIDE -> INSIDE emits ENTER.
- INSIDE -> OUTSIDE emits EXIT.
- INSIDE -> DWELL emits DWELL once per policy.
- Moving between cells does not miss exits from old-cell fences.
- Fence reload does not corrupt concurrent evaluations.

### Test Targets
- `test_fence_registry`
- `test_geofence_evaluator_enter_exit`
- `test_geofence_evaluator_old_cell_exit`
- `test_dwell_checker`
- `test_geofence_event_publisher`
- `test_geofence_admin_service`

---

## Milestone 11: Matching Service

### Goal
Finish the matching framework with strategy plugins and atomic reservations.

### Work Items
- Finalize internal matching domain types.
- Implement `MatchContext` concrete class:
  - reserve
  - release
  - nearby search expansion
  - time remaining
- Implement Kafka consumer for match requests.
- Implement Kafka producer for match results.
- Implement strategy registry loading from config.
- Provide at least one built-in strategy, likely nearest-available.
- Implement reservation cleanup on failure/expiry.
- Implement request deadline enforcement.
- Implement strategy-specific config loading.
- Add matching metrics.

### Acceptance Criteria
- A request with available nearby agents produces a successful result.
- Concurrent requests cannot reserve the same agent.
- Expired requests release reservations.
- Strategy failure releases reservations.
- Unknown strategy fails clearly and safely.

### Test Targets
- `test_strategy_registry`
- `test_reservation_manager`
- `test_match_context`
- `test_nearest_strategy`
- `test_matching_service_success`
- `test_matching_service_expiry`
- `test_matching_concurrent_reservation`

---

## Milestone 12: Workers And Operations

### Goal
Implement background operational jobs and observability.

### Work Items
- Implement H3 cleanup worker for expired device state.
- Implement DLQ replay worker.
- Implement metrics reporter or direct Prometheus exposer.
- Implement admin health service.
- Add structured logging.
- Add graceful shutdown for all services.
- Add readiness/liveness semantics.
- Add resource limit configuration.

### Acceptance Criteria
- Expired devices are removed from H3 cell sets.
- DLQ replay can reprocess failed history writes safely.
- `/metrics` or equivalent exposes useful counters/gauges/histograms.
- Health checks reflect Kafka/Redis/PostGIS connectivity.
- Services stop without dropping buffered history writes.

### Test Targets
- `test_h3_cleanup_worker`
- `test_dlq_replay_worker`
- `test_metrics_exporter`
- `test_admin_health_service`
- `test_graceful_shutdown`

---

## Milestone 13: Packaging, Deployment, And CI

### Goal
Make the project reproducible for development, CI, and production deployment.

### Work Items
- Choose dependency manager: vcpkg, Conan, system packages, or FetchContent.
- Add reproducible dependency lock/config.
- Add Docker Compose for local dependencies:
  - Kafka or Redpanda
  - Redis
  - TimescaleDB/PostGIS
- Add production Dockerfile.
- Add CI pipeline:
  - format check
  - build
  - unit tests
  - integration tests with services
  - static analysis if available
- Add sanitizers profile for local/CI runs.
- Add benchmark/load test entrypoints.

### Acceptance Criteria
- New machine can build from documented commands.
- CI fails on compile/test regressions.
- Local integration environment starts with one command.
- Production image runs with a mounted config.

### Test Targets
- CI build job
- CI unit test job
- CI integration test job
- Docker smoke job

---

## Milestone 14: Performance And Reliability Hardening

### Goal
Validate the system against stated latency, throughput, and reliability goals.

### Work Items
- Build load generator for ingestion and queries.
- Measure gateway throughput.
- Measure processor throughput.
- Measure latest query P99 latency.
- Measure nearby query P99 latency by radius and density.
- Measure trip replay latency by time range size.
- Test Redis failure behavior.
- Test PostGIS failure behavior.
- Test Kafka broker restart behavior.
- Add memory budget checks for dedup and H3 index.
- Tune batching, pooling, thread counts, and timeouts.

### Acceptance Criteria
- Performance results are documented.
- Bottlenecks are identified and tracked.
- System degrades predictably when dependencies fail.
- No known data-loss paths remain for accepted events under documented guarantees.

### Test Targets
- `bench_ingest_gateway`
- `bench_processor_pipeline`
- `bench_nearby_query`
- `bench_trip_replay`
- `test_redis_failure_recovery`
- `test_postgis_failure_dlq`
- `test_kafka_restart_recovery`

---

## Milestone 15: Documentation Finalization

### Goal
Make docs match the implemented system exactly.

### Work Items
- Update architecture docs to reflect actual component boundaries.
- Update ingestion docs with exact retry, DLQ, and commit policies.
- Update storage docs with final schema and migration order.
- Update query docs with exact API limits and response semantics.
- Update geofence docs with final state transition rules.
- Update matching docs with plugin development guide and example strategy.
- Add operations guide:
  - config reference
  - deployment
  - migration
  - metrics
  - alerts
  - troubleshooting
- Add developer guide:
  - local setup
  - tests
  - dependency management
  - code layout

### Acceptance Criteria
- Docs no longer describe unimplemented behavior as complete.
- Every public API has examples.
- Every config field is documented.
- Every operational runbook has commands and expected outputs.

---

## Suggested Parallel Workstreams

### Workstream A: Core Runtime
- Domain model
- Config loader
- State writer
- Processor loop
- Error types

### Workstream B: Storage
- Redis adapter
- PostGIS adapter
- Migrations
- DLQ persistence/replay

### Workstream C: APIs And Transport
- Protobuf generation
- gRPC gateway
- gRPC query
- Admin service
- Kafka wrappers

### Workstream D: Spatial And Geofence
- H3 adapter
- Nearby query
- Fence registry
- Evaluator
- Dwell checker

### Workstream E: Matching
- Match context
- Reservation manager
- Strategy registry
- Built-in strategy
- Matching service loop

### Workstream F: Ops
- Docker Compose
- CI
- Metrics
- Health checks
- Load tests
- Documentation

---

## Recommended Immediate Next Steps

1. Keep tests feature/function-oriented as new coverage is added.
2. Clean up config drift and make `config/signalroute.toml` canonical.
3. Implement TOML parsing and config validation.
4. Add real `test_dedup_window`, `test_sequence_guard`, `test_state_writer`, and `test_nearby_handler` files.
5. Pick the dependency strategy before integrating H3, Redis, PostGIS, Kafka, protobuf, and gRPC.
