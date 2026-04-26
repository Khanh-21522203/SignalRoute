#pragma once

/**
 * SignalRoute — Trip Handler
 *
 * Queries trip history from PostGIS with optional spatial filter
 * and time-based downsampling.
 */

#include "../common/clients/postgres_client.h"
#include "../common/types/location_event.h"
#include <vector>
#include <string>
#include <cstdint>

namespace signalroute {

class TripHandler {
public:
    explicit TripHandler(PostgresClient& pg);

    /**
     * Query trip points for a device.
     *
     * TODO: Implement using pg_.query_trip() or pg_.query_trip_spatial()
     *
     * P99 target: < 200 ms
     */
    std::vector<LocationEvent> handle(
        const std::string& device_id,
        int64_t from_ts, int64_t to_ts,
        int sample_interval_s, int limit);

    /**
     * Query trip points for a device with a circular spatial filter.
     */
    std::vector<LocationEvent> handle_spatial(
        const std::string& device_id,
        int64_t from_ts, int64_t to_ts,
        double center_lat, double center_lon, double radius_m,
        int sample_interval_s, int limit);

private:
    PostgresClient& pg_;
};

} // namespace signalroute
