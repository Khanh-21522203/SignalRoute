#include "evaluator.h"
#include "point_in_polygon.h"
#include "../common/metrics/metrics.h"

namespace signalroute {

Evaluator::Evaluator(FenceRegistry& registry, RedisClient& redis,
                     KafkaProducer& event_producer, PostgresClient& pg)
    : registry_(registry), redis_(redis),
      event_producer_(event_producer), pg_(pg) {}

void Evaluator::evaluate(const std::string& /*device_id*/,
                          int64_t /*old_h3*/, int64_t /*new_h3*/,
                          double /*lat*/, double /*lon*/, int64_t /*timestamp_ms*/) {
    // TODO: Implement the full evaluation pipeline
    //
    //   auto candidates = registry_.get_candidates(new_h3);
    //   if (old_h3 != new_h3 && old_h3 != 0) {
    //       auto old_candidates = registry_.get_candidates(old_h3);
    //       // Merge: may have exited fences only covered by old cell
    //       candidates.insert(candidates.end(), old_candidates.begin(), old_candidates.end());
    //       // Deduplicate
    //   }
    //
    //   for (const auto* fence : candidates) {
    //       bool inside = geo::point_in_polygon(lat, lon, fence->polygon_vertices);
    //       auto current_state = redis_.get_fence_state(device_id, fence->fence_id);
    //       FenceState prev = current_state.value_or(FenceState::OUTSIDE);
    //
    //       if (inside && prev == FenceState::OUTSIDE) {
    //           // ENTER transition
    //           redis_.set_fence_state(device_id, fence->fence_id, FenceState::INSIDE, timestamp_ms);
    //           // Emit ENTER event to Kafka
    //           Metrics::instance().inc_geofence_event("ENTER");
    //       } else if (!inside && (prev == FenceState::INSIDE || prev == FenceState::DWELL)) {
    //           // EXIT transition
    //           redis_.set_fence_state(device_id, fence->fence_id, FenceState::OUTSIDE, timestamp_ms);
    //           // Emit EXIT event to Kafka
    //           Metrics::instance().inc_geofence_event("EXIT");
    //       }
    //       // DWELL transitions handled by DwellChecker background worker
    //   }
}

} // namespace signalroute
