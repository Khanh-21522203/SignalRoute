-- SignalRoute — Initial Schema
-- TimescaleDB + PostGIS

-- Enable extensions
CREATE EXTENSION IF NOT EXISTS timescaledb;
CREATE EXTENSION IF NOT EXISTS postgis;

-- ═══════════════════════════════════════════════
-- Trip History (Hypertable)
-- ═══════════════════════════════════════════════

CREATE TABLE trip_points (
    device_id    TEXT        NOT NULL,
    seq          BIGINT      NOT NULL,
    event_time   TIMESTAMPTZ NOT NULL,
    recv_time    TIMESTAMPTZ NOT NULL,
    location     GEOGRAPHY(Point, 4326) NOT NULL,
    altitude_m   REAL,
    accuracy_m   REAL,
    speed_ms     REAL,
    heading_deg  REAL,
    h3_r7        BIGINT,
    metadata     JSONB,

    PRIMARY KEY (device_id, event_time, seq)
);

-- Convert to hypertable (partitioned by event_time)
SELECT create_hypertable('trip_points', 'event_time',
    chunk_time_interval => INTERVAL '1 day'
);

-- Index for device + time range queries
CREATE INDEX idx_trip_device_time
    ON trip_points (device_id, event_time DESC);

-- Spatial index for proximity queries on trip history
CREATE INDEX idx_trip_location
    ON trip_points USING GIST (location);

-- Deduplication constraint
CREATE UNIQUE INDEX idx_trip_dedup
    ON trip_points (device_id, seq);

-- ═══════════════════════════════════════════════
-- Geofence Rules
-- ═══════════════════════════════════════════════

CREATE TABLE geofence_rules (
    fence_id          TEXT PRIMARY KEY DEFAULT gen_random_uuid()::text,
    name              TEXT NOT NULL,
    geometry          GEOMETRY(Polygon, 4326) NOT NULL,
    h3_cells          BIGINT[] NOT NULL DEFAULT '{}',
    dwell_threshold_s INTEGER NOT NULL DEFAULT 300,
    active            BOOLEAN NOT NULL DEFAULT true,
    metadata          JSONB,
    created_at        TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at        TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Spatial index for fence geometry
CREATE INDEX idx_fence_geometry
    ON geofence_rules USING GIST (geometry);

-- Index for active fence loading
CREATE INDEX idx_fence_active
    ON geofence_rules (active) WHERE active = true;

-- ═══════════════════════════════════════════════
-- Geofence Events (Audit Log)
-- ═══════════════════════════════════════════════

CREATE TABLE geofence_events (
    id                BIGSERIAL PRIMARY KEY,
    device_id         TEXT NOT NULL,
    fence_id          TEXT NOT NULL REFERENCES geofence_rules(fence_id),
    fence_name        TEXT,
    event_type        TEXT NOT NULL CHECK (event_type IN ('ENTER', 'EXIT', 'DWELL')),
    lat               DOUBLE PRECISION NOT NULL,
    lon               DOUBLE PRECISION NOT NULL,
    event_ts          TIMESTAMPTZ NOT NULL,
    inside_duration_s INTEGER DEFAULT 0,
    created_at        TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Index for querying events by device
CREATE INDEX idx_geofence_events_device
    ON geofence_events (device_id, event_ts DESC);

-- Index for querying events by fence
CREATE INDEX idx_geofence_events_fence
    ON geofence_events (fence_id, event_ts DESC);
