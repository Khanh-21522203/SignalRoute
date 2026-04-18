#pragma once

/**
 * SignalRoute — Nearby Devices Handler
 *
 * Two-phase spatial search:
 *   Phase 1 (Coarse): H3 k-ring expansion → Redis SMEMBERS → candidate set
 *   Phase 2 (Precise): Haversine distance filter → sort by distance → limit
 */

#include "../common/clients/redis_client.h"
#include "../common/spatial/h3_index.h"
#include "../common/config/config.h"
#include "../common/types/device_state.h"
#include <vector>
#include <utility>

namespace signalroute {

struct NearbyResult {
    std::vector<std::pair<DeviceState, double>> devices; // state + distance_m
    int total_candidates = 0;  // Before haversine filter
    int total_in_radius  = 0;  // After filter, before limit
};

class NearbyHandler {
public:
    NearbyHandler(RedisClient& redis, H3Index& h3, const SpatialConfig& cfg);

    /**
     * Find nearby devices.
     *
     * Algorithm:
     *   1. Encode (lat, lon) → center H3 cell
     *   2. Compute k = radius_to_k(radius_m)
     *   3. grid_disk(center, k) → candidate cells
     *   4. Redis SMEMBERS on each cell → candidate device IDs
     *   5. Redis batch HGETALL → device states
     *   6. Haversine filter: keep only devices within radius_m
     *   7. Optional: filter by last_seen_s (freshness)
     *   8. Sort by distance ascending
     *   9. Truncate to limit
     *
     * P99 target: < 50 ms
     *
     * TODO: Implement
     */
    NearbyResult handle(double lat, double lon, double radius_m,
                        int limit, int last_seen_s);

private:
    RedisClient& redis_;
    H3Index& h3_;
    SpatialConfig config_;
};

} // namespace signalroute
