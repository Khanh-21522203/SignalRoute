-- SignalRoute — Add H3 cell column index

-- Index on H3 cell for spatial aggregation queries
CREATE INDEX idx_trip_h3 ON trip_points (h3_r7, event_time DESC);
