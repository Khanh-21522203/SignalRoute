#pragma once

/**
 * SignalRoute — PostgreSQL/PostGIS Client
 *
 * Connection-pooled client for:
 *   - Trip history writes (batch INSERT with ON CONFLICT DO NOTHING)
 *   - Trip replay reads (time-range + optional spatial filter)
 *   - Geofence rule loading (startup + hot-reload)
 *   - Geofence event audit log writes
 *
 * Dependencies: libpq / libpqxx
 */

#include "../config/config.h"
#include "../types/location_event.h"
#include "../types/geofence_types.h"

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <set>
#include <utility>

namespace signalroute {

class PostgresClient {
public:
    explicit PostgresClient(const PostGISConfig& config);
    ~PostgresClient();

    PostgresClient(const PostgresClient&) = delete;
    PostgresClient& operator=(const PostgresClient&) = delete;

    /// Test connectivity.
    bool ping();

    // ── Trip History Writes ──

    /**
     * Batch-insert trip points into the trip_points hypertable.
     * Uses ON CONFLICT (device_id, seq) DO NOTHING for idempotency.
     *
     * @param events Vector of events to insert
     *
     * TODO: Implement using prepared statement + batch COPY or multi-row INSERT
     */
    void batch_insert_trip_points(const std::vector<LocationEvent>& events);

    // ── Trip History Reads ──

    /**
     * Query trip points for a device in a time range.
     *
     * TODO: Implement using parameterized SELECT with idx_trip_device_time
     */
    std::vector<LocationEvent> query_trip(
        const std::string& device_id,
        int64_t from_ts, int64_t to_ts,
        int limit);

    /**
     * Query trip points with spatial filter (circle).
     *
     * TODO: Implement using ST_DWithin on GEOGRAPHY column
     */
    std::vector<LocationEvent> query_trip_spatial(
        const std::string& device_id,
        int64_t from_ts, int64_t to_ts,
        double center_lat, double center_lon, double radius_m,
        int limit);

    // ── Geofence Rules ──

    /**
     * Load all active geofence rules from PostGIS.
     * Called at startup and on hot-reload.
     *
     * TODO: Implement using SELECT * FROM geofence_rules WHERE active = true
     */
    std::vector<GeofenceRule> load_active_fences();

    // ── Geofence Events (Audit Log) ──

    /**
     * Insert a geofence event into the audit log.
     *
     * TODO: Implement using INSERT INTO geofence_events
     */
    void insert_geofence_event(const GeofenceEventRecord& event);

    // In-memory fallback inspection/seeding helpers. These keep unit tests and
    // higher-level service slices deterministic until the real PostGIS adapter
    // is wired in.
    size_t trip_point_count() const;
    size_t geofence_event_count() const;
    std::vector<GeofenceEventRecord> geofence_events() const;
    void set_active_fences(std::vector<GeofenceRule> fences);

private:
    PostGISConfig config_;
    mutable std::mutex mu_;
    std::vector<LocationEvent> trip_points_;
    std::vector<GeofenceRule> active_fences_;
    std::vector<GeofenceEventRecord> geofence_events_;
    std::set<std::pair<std::string, uint64_t>> trip_point_keys_;

    // TODO: Add connection pool
    // std::unique_ptr<pqxx::connection_pool> pool_;
    //
    // TODO: Pre-prepared statement names
};

} // namespace signalroute
