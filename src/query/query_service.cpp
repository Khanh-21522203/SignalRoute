#include "query_service.h"

#include "latest_handler.h"
#include "nearby_handler.h"
#include "trip_handler.h"
#include "../common/clients/postgres_client.h"
#include "../common/clients/redis_client.h"
#include "../common/spatial/h3_index.h"

#include <iostream>

namespace signalroute {

QueryService::QueryService() = default;
QueryService::~QueryService() { if (running_) stop(); }

void QueryService::start(const Config& config) {
    if (running_) {
        return;
    }

    std::cout << "[QueryService] Starting...\n";
    device_ttl_s_ = config.redis.device_ttl_s;
    redis_ = std::make_unique<RedisClient>(config.redis);
    pg_ = std::make_unique<PostgresClient>(config.postgis);
    h3_ = std::make_unique<H3Index>(config.spatial.h3_resolution);
    latest_handler_ = std::make_unique<LatestHandler>(*redis_);
    nearby_handler_ = std::make_unique<NearbyHandler>(*redis_, *h3_, config.spatial);
    trip_handler_ = std::make_unique<TripHandler>(*pg_);
    running_ = true;
    std::cout << "[QueryService] Started.\n";
}

void QueryService::stop() {
    if (!running_) {
        return;
    }

    std::cout << "[QueryService] Stopping...\n";
    running_ = false;
    trip_handler_.reset();
    nearby_handler_.reset();
    latest_handler_.reset();
    h3_.reset();
    pg_.reset();
    redis_.reset();
    std::cout << "[QueryService] Stopped.\n";
}

bool QueryService::is_healthy() const { return running_; }

std::optional<DeviceState> QueryService::latest(const std::string& device_id) {
    if (!running_ || !latest_handler_) {
        return std::nullopt;
    }
    return latest_handler_->handle(device_id);
}

NearbyResult QueryService::nearby(double lat, double lon, double radius_m, int limit, int last_seen_s) {
    if (!running_ || !nearby_handler_) {
        return {};
    }
    return nearby_handler_->handle(lat, lon, radius_m, limit, last_seen_s);
}

std::vector<LocationEvent> QueryService::trip(
    const std::string& device_id,
    int64_t from_ts,
    int64_t to_ts,
    int sample_interval_s,
    int limit) {
    if (!running_ || !trip_handler_) {
        return {};
    }
    return trip_handler_->handle(device_id, from_ts, to_ts, sample_interval_s, limit);
}

std::vector<LocationEvent> QueryService::trip_spatial(
    const std::string& device_id,
    int64_t from_ts,
    int64_t to_ts,
    double center_lat,
    double center_lon,
    double radius_m,
    int sample_interval_s,
    int limit) {
    if (!running_ || !trip_handler_) {
        return {};
    }
    return trip_handler_->handle_spatial(
        device_id, from_ts, to_ts, center_lat, center_lon, radius_m, sample_interval_s, limit);
}

bool QueryService::seed_device_state_for_test(DeviceState state) {
    if (!running_ || !redis_ || !h3_) {
        return false;
    }
    if (state.h3_cell == 0) {
        state.h3_cell = h3_->lat_lng_to_cell(state.lat, state.lon);
    }
    if (state.device_id.empty()) {
        return false;
    }
    return redis_->update_device_state_cas(state.device_id, state, device_ttl_s_);
}

void QueryService::seed_trip_points_for_test(const std::vector<LocationEvent>& events) {
    if (!running_ || !pg_) {
        return;
    }
    pg_->batch_insert_trip_points(events);
}

std::size_t QueryService::trip_point_count_for_test() const {
    return pg_ ? pg_->trip_point_count() : 0;
}

} // namespace signalroute
