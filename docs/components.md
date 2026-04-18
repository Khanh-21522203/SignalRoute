# SignalRoute — Component Specifications

> **Related:** [architecture.md](./architecture.md)
> **Version:** 0.1 (Draft)

Detailed specifications for every component from the C3 diagrams.

---

## Table of Contents

1. [Ingestion Gateway](#ingestion-gateway)
2. [Event Queue (Kafka Layer)](#event-queue-kafka-layer)
3. [Location Processor](#location-processor)
4. [Dedup Window](#dedup-window)
5. [Sequence Guard](#sequence-guard)
6. [Location State Store](#location-state-store)
7. [Trip History Store](#trip-history-store)
8. [Query Service](#query-service)
9. [Geofence Engine](#geofence-engine)
10. [Background Workers](#background-workers)

---

## Ingestion Gateway

### Purpose

Stateless front-door for all device location events. Validates, authenticates, rate-limits, and enqueues events without maintaining any per-device state.

### Request Lifecycle

```mermaid
flowchart TB
    Recv["Receive LocationBatch
    (gRPC IngestRequest or UDP datagram)"]
    Auth["Auth check
    • API key → device registry
    • Reject unknown keys (401)"]
    Schema["Schema validation
    • required fields: device_id, lat, lon, timestamp, seq
    • lat ∈ [-90, 90] · lon ∈ [-180, 180]
    • timestamp ≤ now + skew_tolerance
    • batch size ≤ max_batch_events"]
    Rate["Rate limiting
    • Sliding window per device_id
    • Default: 100 events/s per device
    • Reject with 429 on excess"]
    Enqueue["Kafka publish
    • Partition key = device_id
    • Encode payload as Protobuf
    • Fire-and-forget (ack from Kafka broker)"]
    Ack["Return ACK to device
    • gRPC: OK status
    • UDP: optional ACK packet (unreliable mode skips)"]

    Recv --> Auth --> Schema --> Rate --> Enqueue --> Ack
```

### Ingest API

| RPC | Request | Response | Notes |
|-----|---------|----------|-------|
| `IngestBatch` | `{ device_id, events[] }` | `{ accepted, rejected, reason }` | Primary gRPC method |
| `IngestSingle` | `LocationEvent` | `{ ok, reason }` | Convenience for single-event devices |

### UDP Mode

UDP mode is an opt-in alternative for devices that cannot afford TLS/HTTP overhead. Packets are self-contained Protobuf-encoded `LocationEvent` messages. The gateway accepts and enqueues them with no reliability guarantee — the device is responsible for retransmission. Rate limiting still applies.

### Error Responses

| Code | Condition |
|------|-----------|
| `INVALID_ARGUMENT` | Schema violation |
| `UNAUTHENTICATED` | Unknown or expired API key |
| `RESOURCE_EXHAUSTED` | Rate limit exceeded |
| `UNAVAILABLE` | Kafka producer backpressure |

---

## Event Queue (Kafka Layer)

### Purpose

Decouple ingestion from processing. Provides durable, ordered, replayable storage of location events, partitioned by `device_id` to guarantee per-device ordering.

### Topic Design

| Topic | Partitioning | Retention | Description |
|-------|-------------|-----------|-------------|
| `tm.location.events` | `hash(device_id) % N` | 24 h | Raw validated location events |
| `tm.geofence.events` | `hash(device_id) % N` | 7 days | Geofence enter/exit/dwell events for consumers |
| `tm.location.dlq` | round-robin | 30 days | Dead-letter queue for failed PostGIS writes |

### Partition Count

`num_partitions` is set at cluster creation and determines the maximum number of Processor instances. Rule of thumb: **1 partition per 10,000 active devices per second**. Cannot be changed without rebalancing consumers.

### Message Format (Protobuf)

```protobuf
message LocationEvent {
    string  device_id  = 1;
    double  lat        = 2;
    double  lon        = 3;
    float   altitude_m = 4;
    float   accuracy_m = 5;
    float   speed_ms   = 6;
    float   heading_deg = 7;
    int64   timestamp_ms = 8;   // device clock, Unix epoch ms
    int64   server_recv_ms = 9; // gateway receive time
    uint64  seq        = 10;    // monotonically increasing per device
    map<string, string> metadata = 11;
}
```

---

## Location Processor

### Purpose

Stateful per-partition consumer. Owns the correctness path: dedup, sequence guard, H3 encoding, state update, history write, geofence notification.

### Processing Loop

```mermaid
flowchart TB
    Poll["Poll Kafka batch
    (max 500 events, 100ms timeout)"]
    ForEach["For each event:"]
    Dedup["Dedup window check
    → skip if (device_id, seq) seen"]
    SeqGuard["Sequence guard
    → skip if seq ≤ stored last_seq"]
    Encode["H3 encode (lat, lon) → cell_id"]
    StateW["Redis state update
    HSET device:{id} ... (atomic with MULTI/EXEC)"]
    HistW["Batch-buffer trip_point row
    → flush to PostGIS when buffer full or timer fires"]
    GeoN["Notify Geofence Engine
    (if H3 cell changed or periodic tick)"]
    Commit["Commit Kafka offset
    (after all writes succeed)"]

    Poll --> ForEach --> Dedup --> SeqGuard --> Encode
    Encode --> StateW --> HistW --> GeoN --> Commit
    Commit --> Poll
```

### Error Handling in Processing Loop

| Error | Action |
|-------|--------|
| Redis write timeout | Retry with exponential backoff (max 3 retries); then rewind Kafka offset and re-process |
| PostGIS write failure | Buffer to dead-letter topic `tm.location.dlq`; continue processing |
| H3 encoding error (invalid coords) | Log and discard (validation should have caught this; treat as corrupted event) |
| Geofence Engine unreachable | Log and continue; geofence eval is best-effort for transient failures |

---

## Dedup Window

### Purpose

Prevent duplicate state mutations caused by UDP retransmissions, Kafka at-least-once redelivery, or device retry logic.

### Data Structure

An **LRU hash set** keyed by `u64` fingerprint: `hash(device_id ‖ seq)`.

```mermaid
stateDiagram-v2
    [*] --> Miss : (device_id, seq) not found
    [*] --> Hit  : (device_id, seq) found
    Miss --> Insert : add to LRU; proceed to Sequence Guard
    Hit  --> Discard : increment dedup_hit_counter; skip event
    Insert --> Evict : LRU evicts oldest entry when capacity exceeded
    Evict --> [*]
```

### Configuration

| Parameter | Default | Notes |
|-----------|---------|-------|
| `dedup_ttl_s` | 300 (5 min) | Entries older than this are considered expired |
| `dedup_max_entries` | 500,000 per partition | Bounded memory; LRU evicts when full |

### Memory Cost

At 500,000 entries × 64 bytes each = **~32 MB per Processor instance**. This is the primary Processor memory cost.

---

## Sequence Guard

### Purpose

Ensure that only events strictly newer than the last accepted event for a device can update the Location State Store. Prevents stale GPS coordinates from overwriting fresh ones.

### Algorithm

```mermaid
sequenceDiagram
    participant LP as Location Processor
    participant R  as Redis

    LP->>R: HGET device:{id} seq
    R-->>LP: last_seq (or nil if new device)

    alt incoming.seq > last_seq (or nil)
        LP->>LP: Proceed to state update
    else incoming.seq ≤ last_seq
        LP->>LP: Increment stale_event_counter
        LP->>LP: Discard event
    end
```

### Redis CAS for Atomic Update

State update uses `MULTI`/`EXEC` to atomically check-and-set `seq`:

```
MULTI
HGET  device:{id} seq           ← read current seq
HSET  device:{id} lat lon h3 seq updated_at
EXEC
```

The application layer checks the `HGET` result before deciding to commit. On a `nil` result (new device), the write proceeds unconditionally.

> **Note:** For high-throughput deployments consider Lua scripts (`EVALSHA`) to reduce round-trips.

### Out-of-Order Tolerance

Events that arrive late but within `out_of_order_tolerance_s` (default 60 s) of the current server time are still written to **trip history** with their original `event_time`, even if they fail the sequence guard for the State Store. This ensures history completeness without compromising the latest-position invariant.

---

## Location State Store

### Purpose

Single-record per device — the latest known position. Serves as the primary source of truth for all real-time reads.

### Redis Key Schema

| Key | Type | Fields |
|-----|------|--------|
| `{prefix}:dev:{device_id}` | Hash | `lat`, `lon`, `alt`, `accuracy`, `speed`, `heading`, `h3`, `seq`, `updated_at` |
| `{prefix}:h3:{cell_id}` | Set | `device_id` members — all devices currently in this H3 cell |

### H3 Cell Index Maintenance

On every accepted state update:

```mermaid
flowchart LR
    OldCell["old_h3 (from previous state)"]
    NewCell["new_h3 (from incoming event)"]
    Same{Same cell?}
    Skip["No index update needed"]
    Remove["SREM h3:{old_cell} device_id"]
    Add["SADD h3:{new_cell} device_id"]

    OldCell --> Same
    NewCell --> Same
    Same -- Yes --> Skip
    Same -- No --> Remove --> Add
```

### Device TTL

Devices that have not sent an update in `device_ttl_s` (default 1 hour) are expired from Redis via TTL. The H3 cell index membership is cleaned up by the Background Cleanup Worker (see [Background Workers](#background-workers)).

---

## Trip History Store

### Purpose

Persistent append-only store of all accepted location events, enabling trip replay, analytics, distance calculation, and historical geofence queries.

### Schema

```sql
-- Hypertable: partitioned by event_time daily
CREATE TABLE trip_points (
    device_id   TEXT        NOT NULL,
    seq         BIGINT      NOT NULL,
    event_time  TIMESTAMPTZ NOT NULL,    -- device clock (original)
    recv_time   TIMESTAMPTZ NOT NULL,    -- gateway receive time
    location    GEOGRAPHY(Point, 4326) NOT NULL,
    altitude_m  REAL,
    accuracy_m  REAL,
    speed_ms    REAL,
    heading_deg REAL,
    h3_r7       BIGINT,                 -- H3 cell at resolution 7
    metadata    JSONB,
    PRIMARY KEY (device_id, seq)        -- idempotent inserts
);

SELECT create_hypertable('trip_points', 'event_time',
    chunk_time_interval => INTERVAL '1 day');

CREATE INDEX ON trip_points (device_id, event_time DESC);
CREATE INDEX ON trip_points USING GIST (location);
CREATE INDEX ON trip_points (h3_r7);
```

### Write Protocol

```mermaid
flowchart TB
    Buffer["Processor write buffer
    Accumulate rows in memory"]
    Flush{Flush trigger?}
    Flush -- "buffer > 500 rows" --> Insert
    Flush -- "flush_interval > 500ms" --> Insert
    Flush -- "Kafka offset commit" --> Insert
    Insert["INSERT INTO trip_points
    (batch, ON CONFLICT DO NOTHING)"]
    Buffer --> Flush
```

### Supported Queries

| Query | SQL Pattern | Index used |
|-------|-------------|-----------|
| Trip replay | `WHERE device_id=$1 AND event_time BETWEEN $2 AND $3` | `(device_id, event_time DESC)` |
| Latest N points | `WHERE device_id=$1 ORDER BY event_time DESC LIMIT N` | Same |
| Spatial range | `WHERE ST_DWithin(location, ST_Point($lon,$lat), $radius)` | GIST index |
| H3 cell history | `WHERE h3_r7 = $cell_id AND event_time > $from` | `h3_r7` + time |
| Distance traveled | `SUM(ST_Distance(lag(location) OVER ..., location))` | Sequential scan (analytics) |

---

## Query Service

### Purpose

Serves all consumer-facing read requests. Stateless — reads from Redis (latest, nearby) and PostGIS (history). Horizontally scalable.

### Latest Location Handler

```mermaid
flowchart LR
    Req["GetLatestLocation(device_id)"]
    Redis["HGETALL {prefix}:dev:{device_id}"]
    Miss{Found?}
    Return["DeviceState response"]
    NotFound["NOT_FOUND error"]

    Req --> Redis --> Miss
    Miss -- Yes --> Return
    Miss -- No --> NotFound
```

**SLA target:** P99 < 5 ms end-to-end (Redis HGETALL is typically < 1 ms on same network).

### Nearby Handler

```mermaid
flowchart TB
    Req["NearbyDevices(lat, lon, radius_m, limit, last_seen_within_s)"]
    Center["Encode (lat, lon) → H3 cell"]
    Ring["Compute k = radius_to_k_ring(radius_m, resolution)
    cells = h3_k_ring(center_cell, k)"]
    Fetch["For each cell in cells:
    SMEMBERS h3:{cell} → device_id list"]
    Filter["HGETALL per device →
    • haversine distance < radius_m
    • updated_at > now - last_seen_within_s (if set)
    • apply limit"]
    Resp["NearbyDevicesResponse{devices[], total_candidates}"]

    Req --> Center --> Ring --> Fetch --> Filter --> Resp
```

**k-ring radius formula:** `k = ceil(radius_m / h3_avg_edge_length_m(resolution))`

For resolution 7 (avg edge ~1.4 km):
- 1 km radius → k = 1 (7 cells)
- 5 km radius → k = 4 (61 cells)
- 10 km radius → k = 8 (217 cells)

### Trip Replay Handler

```mermaid
flowchart LR
    Req["GetTrip(device_id, from_ts, to_ts, sample_interval_s?)"]
    Query["SELECT * FROM trip_points
    WHERE device_id=$1
    AND event_time BETWEEN $2 AND $3
    ORDER BY event_time ASC"]
    Sample{sample_interval_s set?}
    Full["Stream all rows"]
    Down["Downsample:
    keep first point per interval bucket"]
    Resp["TripPoint stream"]

    Req --> Query --> Sample
    Sample -- No --> Full --> Resp
    Sample -- Yes --> Down --> Resp
```

---

## Geofence Engine

### Purpose

Evaluates device position changes against registered geofences and emits enter/exit/dwell events to downstream consumers.

### Fence Registry

```mermaid
flowchart TB
    Startup["Load all fences from PostGIS
    SELECT * FROM geofence_rules WHERE active = true"]
    Parse["For each fence:
    • Parse GeoJSON geometry
    • Compute H3 polyfill (covering cell set)
    • Build in-memory polygon (for containment test)"]
    Store["Store in FenceRegistry
    fence_id → FenceRule{cells, polygon, rule}"]

    Startup --> Parse --> Store
```

```sql
CREATE TABLE geofence_rules (
    fence_id    UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name        TEXT NOT NULL,
    geometry    GEOGRAPHY(Polygon, 4326) NOT NULL,
    h3_cells    BIGINT[] NOT NULL,     -- H3 polyfill at resolution 7
    active      BOOLEAN DEFAULT true,
    dwell_threshold_s INT DEFAULT 300,
    metadata    JSONB,
    created_at  TIMESTAMPTZ DEFAULT now()
);
CREATE INDEX ON geofence_rules USING GIN (h3_cells);
```

### Evaluation Flow

```mermaid
sequenceDiagram
    participant LP as Location Processor
    participant GE as Geofence Engine
    participant R  as Redis
    participant K  as Kafka

    LP->>GE: EvalRequest(device_id, old_h3, new_h3, lat, lon)
    GE->>GE: Find candidate fences:
              candidates = fences where h3_cells ∩ {new_h3} ≠ ∅
    loop For each candidate fence
        GE->>GE: ST_Contains(fence.polygon, (lat, lon))
        GE->>R: HGET device:{id}:fence:{fence_id} state
        R-->>GE: prior_state (OUTSIDE | INSIDE | DWELL)
        alt was OUTSIDE, now inside
            GE->>R: HSET device:{id}:fence:{fence_id} state=INSIDE ts=now
            GE->>K: Publish GeofenceEvent{type=ENTER, device_id, fence_id, ts}
        else was INSIDE, now outside
            GE->>R: HSET device:{id}:fence:{fence_id} state=OUTSIDE ts=now
            GE->>K: Publish GeofenceEvent{type=EXIT, device_id, fence_id, ts}
        end
    end
```

### Dwell Detection

A background timer in the Geofence Engine scans devices that are in `INSIDE` state and transitions them to `DWELL` if they have been continuously inside the fence for longer than `dwell_threshold_s`. A `DWELL` event is emitted to Kafka.

### Geofence Event Schema (Protobuf)

```protobuf
enum GeofenceEventType { ENTER = 0; EXIT = 1; DWELL = 2; }

message GeofenceEvent {
    string             device_id   = 1;
    string             fence_id    = 2;
    string             fence_name  = 3;
    GeofenceEventType  event_type  = 4;
    double             lat         = 5;
    double             lon         = 6;
    int64              event_ts_ms = 7;
    int64              inside_duration_s = 8;  // for DWELL
}
```

---

## Background Workers

All workers run as background threads within their respective service processes.

```mermaid
flowchart TB
    BG["Background Worker Pool"]

    BG --> H3C["🗺 H3 Cell Cleanup Worker
    Trigger: device TTL expiry event from Redis keyspace notification
    Action: SREM h3:{cell} device_id for expired devices"]

    BG --> DLQ["📬 DLQ Replay Worker
    Trigger: timer (every 60 s)
    Action: consume tm.location.dlq → retry INSERT into PostGIS
    Back-off on repeated failure; alert after N failures"]

    BG --> FC["🔄 Fence Cache Reloader
    Trigger: every reload_interval_s (default 60 s) or Admin API signal
    Action: diff new fence set vs. registry → add/remove fences hot
    No restart required"]

    BG --> GD["⏱ Geofence Dwell Checker
    Trigger: every 30 s
    Action: scan device:{id}:fence:{id} INSIDE entries
    → emit DWELL event if (now - entered_at) > dwell_threshold_s"]

    BG --> MR["📊 Metrics Reporter
    Trigger: every 15 s
    Action: export Prometheus gauges:
    • ingest_rate_eps · dedup_hit_rate · seq_guard_reject_rate
    • nearby_p99_ms · trip_query_p99_ms · geofence_eval_latency_ms
    • redis_pool_utilization · postgis_pool_utilization"]

    BG --> TS["🗑 Trip History Compaction (Phase 3)
    Trigger: configurable schedule (e.g. daily)
    Action: TimescaleDB compression policy on chunks older than 7 days
    Maintains columnar compression for cold data"]
```

### DLQ Replay Worker — Detailed Flow

```mermaid
sequenceDiagram
    participant DLQ as DLQ Replay Worker
    participant K   as Kafka (dlq topic)
    participant PG  as PostGIS

    loop Every 60 s (or on demand)
        DLQ->>K: Poll tm.location.dlq batch
        DLQ->>PG: INSERT INTO trip_points (batch) ON CONFLICT DO NOTHING
        alt Success
            DLQ->>K: Commit offset
        else Failure (N consecutive attempts)
            DLQ->>DLQ: Increment failure counter
            DLQ->>DLQ: Emit alert metric (dlq_replay_failures_total)
            DLQ->>DLQ: Exponential back-off (max 10 min)
        end
    end
```
