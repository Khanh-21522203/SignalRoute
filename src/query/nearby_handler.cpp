#include "nearby_handler.h"
#include "../common/spatial/haversine.h"
#include "../common/metrics/metrics.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace signalroute {

NearbyHandler::NearbyHandler(RedisClient& redis, H3Index& h3, const SpatialConfig& cfg)
    : redis_(redis), h3_(h3), config_(cfg) {}

NearbyResult NearbyHandler::handle(double lat, double lon, double radius_m,
                                    int limit, int last_seen_s) {
    NearbyResult result;

    if (!std::isfinite(lat) || !std::isfinite(lon) || !std::isfinite(radius_m) ||
        lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0 ||
        radius_m <= 0.0 || last_seen_s < 0) {
        return result;
    }

    if (radius_m > config_.nearby_max_radius_m) {
        radius_m = config_.nearby_max_radius_m;
    }
    if (radius_m <= 0.0) {
        return result;
    }
    if (limit <= 0 || limit > config_.nearby_max_results) {
        limit = config_.nearby_max_results;
    }
    if (limit <= 0) {
        return result;
    }

    int64_t center_cell = h3_.lat_lng_to_cell(lat, lon);
    int k = h3_.radius_to_k(radius_m);
    auto cells = h3_.grid_disk(center_cell, k);

    auto device_ids = redis_.get_devices_in_cells(cells);
    result.total_candidates = static_cast<int>(device_ids.size());

    auto states = redis_.get_device_states_batch(device_ids);
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    for (const auto& opt_state : states) {
        if (!opt_state) {
            continue;
        }
        const auto& state = *opt_state;
        if (last_seen_s > 0 && (now_ms - state.updated_at) > static_cast<int64_t>(last_seen_s) * 1000) {
            continue;
        }

        double dist = geo::haversine(lat, lon, state.lat, state.lon);
        if (dist <= radius_m) {
            result.devices.emplace_back(state, dist);
        }
    }

    result.total_in_radius = static_cast<int>(result.devices.size());
    std::sort(result.devices.begin(), result.devices.end(),
        [](const auto& a, const auto& b) {
            if (a.second != b.second) {
                return a.second < b.second;
            }
            return a.first.device_id < b.first.device_id;
        });
    if (static_cast<int>(result.devices.size()) > limit) {
        result.devices.resize(static_cast<size_t>(limit));
    }

    Metrics::instance().observe_nearby_latency(0.0);
    return result;
}

} // namespace signalroute
