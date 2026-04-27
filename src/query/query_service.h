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

struct LatestLocationRequest {
    std::string device_id;
};

struct LatestLocationResponse {
    bool ok = false;
    bool found = false;
    DeviceState state;
    std::string error;
};

struct NearbyDevicesRequest {
    double lat = 0.0;
    double lon = 0.0;
    double radius_m = 0.0;
    int limit = 0;
    int last_seen_s = 0;
};

struct NearbyDevicesResponse {
    bool ok = false;
    NearbyResult result;
    std::string error;
};

struct TripQueryRequest {
    std::string device_id;
    int64_t from_ts = 0;
    int64_t to_ts = 0;
    int sample_interval_s = 0;
    int limit = 0;
};

struct SpatialTripQueryRequest {
    std::string device_id;
    int64_t from_ts = 0;
    int64_t to_ts = 0;
    double center_lat = 0.0;
    double center_lon = 0.0;
    double radius_m = 0.0;
    int sample_interval_s = 0;
    int limit = 0;
};

struct TripQueryResponse {
    bool ok = false;
    std::vector<LocationEvent> events;
    std::string error;
};

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
    LatestLocationResponse handle_latest(LatestLocationRequest request);
    NearbyDevicesResponse handle_nearby(NearbyDevicesRequest request);
    TripQueryResponse handle_trip(TripQueryRequest request);
    TripQueryResponse handle_trip_spatial(SpatialTripQueryRequest request);

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
