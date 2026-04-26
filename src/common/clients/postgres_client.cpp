#include "postgres_client.h"
#include "../spatial/haversine.h"

#include <algorithm>
#include <iostream>
#include <utility>

// TODO: #include <pqxx/pqxx>  // or #include <libpq-fe.h>

namespace signalroute {
namespace {

int64_t event_time_ms(const LocationEvent& event) {
    return event.timestamp_ms != 0 ? event.timestamp_ms : event.server_recv_ms;
}

bool in_time_range(const LocationEvent& event, int64_t from_ts, int64_t to_ts) {
    const int64_t ts = event_time_ms(event);
    return ts >= from_ts && ts <= to_ts;
}

void sort_trip_points(std::vector<LocationEvent>& events) {
    std::sort(events.begin(), events.end(), [](const LocationEvent& lhs, const LocationEvent& rhs) {
        const int64_t lhs_time = event_time_ms(lhs);
        const int64_t rhs_time = event_time_ms(rhs);
        if (lhs_time != rhs_time) {
            return lhs_time < rhs_time;
        }
        if (lhs.device_id != rhs.device_id) {
            return lhs.device_id < rhs.device_id;
        }
        return lhs.seq < rhs.seq;
    });
}

} // namespace

PostgresClient::PostgresClient(const PostGISConfig& config) : config_(config) {
    // TODO: Initialize connection pool
    //   auto conn = std::make_unique<pqxx::connection>(config.dsn);
    //   // Prepare statements for hot-path operations
    //   conn->prepare("insert_trip_point", "INSERT INTO trip_points ...");
    //   conn->prepare("query_trip", "SELECT ... FROM trip_points WHERE ...");
    std::cerr << "[PostgresClient] WARNING: using in-memory PostGIS fallback.\n";
}

PostgresClient::~PostgresClient() = default;

bool PostgresClient::ping() {
    // The in-memory fallback is always reachable. The real adapter will execute
    // SELECT 1 on a pooled connection.
    return true;
}

void PostgresClient::batch_insert_trip_points(const std::vector<LocationEvent>& events) {
    std::lock_guard lock(mu_);
    for (const auto& event : events) {
        auto [_, inserted] = trip_point_keys_.emplace(event.device_id, event.seq);
        if (inserted) {
            trip_points_.push_back(event);
        }
    }
}

std::vector<LocationEvent> PostgresClient::query_trip(
    const std::string& device_id,
    int64_t from_ts, int64_t to_ts,
    int limit)
{
    if (limit <= 0 || from_ts > to_ts) {
        return {};
    }

    std::lock_guard lock(mu_);
    std::vector<LocationEvent> result;
    for (const auto& event : trip_points_) {
        if (event.device_id == device_id && in_time_range(event, from_ts, to_ts)) {
            result.push_back(event);
        }
    }

    sort_trip_points(result);
    if (static_cast<int>(result.size()) > limit) {
        result.resize(static_cast<size_t>(limit));
    }
    return result;
}

std::vector<LocationEvent> PostgresClient::query_trip_spatial(
    const std::string& device_id,
    int64_t from_ts, int64_t to_ts,
    double center_lat, double center_lon, double radius_m,
    int limit)
{
    if (limit <= 0 || radius_m < 0.0 || from_ts > to_ts) {
        return {};
    }

    std::lock_guard lock(mu_);
    std::vector<LocationEvent> result;
    for (const auto& event : trip_points_) {
        if (event.device_id != device_id || !in_time_range(event, from_ts, to_ts)) {
            continue;
        }
        const double distance_m = geo::haversine(center_lat, center_lon, event.lat, event.lon);
        if (distance_m <= radius_m) {
            result.push_back(event);
        }
    }

    sort_trip_points(result);
    if (static_cast<int>(result.size()) > limit) {
        result.resize(static_cast<size_t>(limit));
    }
    return result;
}

std::vector<GeofenceRule> PostgresClient::load_active_fences() {
    std::lock_guard lock(mu_);
    std::vector<GeofenceRule> result;
    for (const auto& fence : active_fences_) {
        if (fence.active) {
            result.push_back(fence);
        }
    }
    return result;
}

void PostgresClient::insert_geofence_event(const GeofenceEventRecord& event) {
    std::lock_guard lock(mu_);
    geofence_events_.push_back(event);
}

size_t PostgresClient::trip_point_count() const {
    std::lock_guard lock(mu_);
    return trip_points_.size();
}

size_t PostgresClient::geofence_event_count() const {
    std::lock_guard lock(mu_);
    return geofence_events_.size();
}

std::vector<GeofenceEventRecord> PostgresClient::geofence_events() const {
    std::lock_guard lock(mu_);
    return geofence_events_;
}

void PostgresClient::set_active_fences(std::vector<GeofenceRule> fences) {
    std::lock_guard lock(mu_);
    active_fences_ = std::move(fences);
}

} // namespace signalroute
