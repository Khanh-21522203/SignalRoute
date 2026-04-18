-- SignalRoute — Compression Policy
-- Enable TimescaleDB compression on trip_points for older data.

ALTER TABLE trip_points SET (
    timescaledb.compress,
    timescaledb.compress_segmentby = 'device_id',
    timescaledb.compress_orderby = 'event_time DESC'
);

-- Automatically compress chunks older than 7 days
SELECT add_compression_policy('trip_points', INTERVAL '7 days');
