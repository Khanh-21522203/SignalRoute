#pragma once

/**
 * SignalRoute — Query Service
 *
 * Stateless read-path service providing:
 *   - GetLatestLocation (Redis)
 *   - NearbyDevices (Redis H3 index + haversine filter)
 *   - GetTrip (PostGIS time-range query)
 */

#include "../common/config/config.h"
#include "../common/types/device_state.h"
#include "../common/types/location_event.h"
#include "nearby_handler.h"

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace signalroute {

class H3Index;
class LatestHandler;
class PostgresClient;
class RedisClient;
class TripHandler;

class QueryService {
public:
    QueryService();
    ~QueryService();

    void start(const Config& config);
    void stop();
    bool is_healthy() const;

    std::optional<DeviceState> latest(const std::string& device_id);
    NearbyResult nearby(double lat, double lon, double radius_m, int limit, int last_seen_s);
    std::vector<LocationEvent> trip(
        const std::string& device_id,
        int64_t from_ts,
        int64_t to_ts,
        int sample_interval_s,
        int limit);
    std::vector<LocationEvent> trip_spatial(
        const std::string& device_id,
        int64_t from_ts,
        int64_t to_ts,
        double center_lat,
        double center_lon,
        double radius_m,
        int sample_interval_s,
        int limit);

    bool seed_device_state_for_test(DeviceState state);
    void seed_trip_points_for_test(const std::vector<LocationEvent>& events);
    std::size_t trip_point_count_for_test() const;

private:
    std::atomic<bool> running_{false};
    std::unique_ptr<RedisClient> redis_;
    std::unique_ptr<PostgresClient> pg_;
    std::unique_ptr<H3Index> h3_;
    std::unique_ptr<LatestHandler> latest_handler_;
    std::unique_ptr<NearbyHandler> nearby_handler_;
    std::unique_ptr<TripHandler> trip_handler_;
    int device_ttl_s_ = 3600;
};

} // namespace signalroute
