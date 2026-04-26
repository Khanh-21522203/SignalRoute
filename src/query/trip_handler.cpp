#include "trip_handler.h"

#include <cmath>
#include <utility>

namespace signalroute {
namespace {

std::vector<LocationEvent> downsample_by_interval(std::vector<LocationEvent> events,
                                                  int sample_interval_s) {
    if (sample_interval_s <= 0 || events.size() <= 1) {
        return events;
    }

    const int64_t interval_ms = static_cast<int64_t>(sample_interval_s) * 1000;
    std::vector<LocationEvent> sampled;
    sampled.reserve(events.size());
    int64_t last_kept_bucket = 0;
    bool has_kept = false;

    for (const auto& event : events) {
        const int64_t ts = event.timestamp_ms != 0 ? event.timestamp_ms : event.server_recv_ms;
        const int64_t bucket = ts / interval_ms;
        if (!has_kept || bucket != last_kept_bucket) {
            sampled.push_back(event);
            last_kept_bucket = bucket;
            has_kept = true;
        }
    }

    return sampled;
}

} // namespace

TripHandler::TripHandler(PostgresClient& pg) : pg_(pg) {}

std::vector<LocationEvent> TripHandler::handle(
    const std::string& device_id,
    int64_t from_ts, int64_t to_ts,
    int sample_interval_s, int limit)
{
    if (device_id.empty() || from_ts > to_ts || limit <= 0) {
        return {};
    }
    auto events = pg_.query_trip(device_id, from_ts, to_ts, limit);
    return downsample_by_interval(std::move(events), sample_interval_s);
}

std::vector<LocationEvent> TripHandler::handle_spatial(
    const std::string& device_id,
    int64_t from_ts, int64_t to_ts,
    double center_lat, double center_lon, double radius_m,
    int sample_interval_s, int limit)
{
    if (device_id.empty() || from_ts > to_ts || limit <= 0 ||
        !std::isfinite(center_lat) || !std::isfinite(center_lon) || !std::isfinite(radius_m) ||
        center_lat < -90.0 || center_lat > 90.0 ||
        center_lon < -180.0 || center_lon > 180.0 ||
        radius_m < 0.0) {
        return {};
    }

    auto events = pg_.query_trip_spatial(
        device_id, from_ts, to_ts, center_lat, center_lon, radius_m, limit);
    return downsample_by_interval(std::move(events), sample_interval_s);
}

} // namespace signalroute
