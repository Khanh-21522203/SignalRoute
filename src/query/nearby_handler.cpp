#include "nearby_handler.h"
#include "../common/spatial/haversine.h"
#include "../common/metrics/metrics.h"
#include <algorithm>
#include <chrono>

namespace signalroute {

NearbyHandler::NearbyHandler(RedisClient& redis, H3Index& h3, const SpatialConfig& cfg)
    : redis_(redis), h3_(h3), config_(cfg) {}

NearbyResult NearbyHandler::handle(double lat, double lon, double radius_m,
                                    int limit, int last_seen_s) {
    NearbyResult result;

    // Clamp radius
    if (radius_m > config_.nearby_max_radius_m) {
        radius_m = config_.nearby_max_radius_m;
    }
    if (limit > config_.nearby_max_results) {
        limit = config_.nearby_max_results;
    }

    // TODO: Implement the full two-phase search
    //
    //   // Phase 1: Coarse — H3 k-ring
    //   int64_t center_cell = h3_.lat_lng_to_cell(lat, lon);
    //   int k = h3_.radius_to_k(radius_m);
    //   auto cells = h3_.grid_disk(center_cell, k);
    //
    //   // Get candidate device IDs from all cells
    //   auto device_ids = redis_.get_devices_in_cells(cells);
    //   result.total_candidates = device_ids.size();
    //
    //   // Batch-fetch device states
    //   auto states = redis_.get_device_states_batch(device_ids);
    //
    //   // Phase 2: Precise — haversine filter
    //   auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    //       std::chrono::system_clock::now().time_since_epoch()).count();
    //
    //   for (size_t i = 0; i < states.size(); ++i) {
    //       if (!states[i]) continue;
    //       const auto& state = *states[i];
    //
    //       // Freshness filter
    //       if (last_seen_s > 0 && (now_ms - state.updated_at) > last_seen_s * 1000LL) {
    //           continue;
    //       }
    //
    //       double dist = geo::haversine(lat, lon, state.lat, state.lon);
    //       if (dist <= radius_m) {
    //           result.devices.emplace_back(state, dist);
    //       }
    //   }
    //
    //   result.total_in_radius = result.devices.size();
    //
    //   // Sort by distance and truncate
    //   std::sort(result.devices.begin(), result.devices.end(),
    //       [](const auto& a, const auto& b) { return a.second < b.second; });
    //   if (static_cast<int>(result.devices.size()) > limit) {
    //       result.devices.resize(limit);
    //   }

    return result;
}

} // namespace signalroute
