#include "evaluator.h"
#include "point_in_polygon.h"
#include "../common/metrics/metrics.h"
#include "../common/proto/geofence_payload_codec.h"

#include <unordered_map>
#include <utility>

namespace signalroute {
namespace {

GeofenceEventRecord make_event_record(const std::string& device_id,
                                      const GeofenceRule& fence,
                                      GeofenceEventType event_type,
                                      double lat,
                                      double lon,
                                      int64_t timestamp_ms) {
    GeofenceEventRecord record;
    record.device_id = device_id;
    record.fence_id = fence.fence_id;
    record.fence_name = fence.name;
    record.event_type = event_type;
    record.lat = lat;
    record.lon = lon;
    record.event_ts_ms = timestamp_ms;
    return record;
}

} // namespace

Evaluator::Evaluator(FenceRegistry& registry, RedisClient& redis,
                     KafkaProducer& event_producer, PostgresClient& pg,
                     std::string event_topic)
    : registry_(registry)
    , redis_(redis)
    , event_producer_(event_producer)
    , pg_(pg)
    , event_topic_(std::move(event_topic))
{}

void Evaluator::evaluate(const std::string& device_id,
                          int64_t old_h3, int64_t new_h3,
                          double lat, double lon, int64_t timestamp_ms) {
    if (device_id.empty()) {
        return;
    }

    std::unordered_map<std::string, const GeofenceRule*> candidates;
    for (const auto* fence : registry_.get_candidates(new_h3)) {
        if (fence && fence->active) {
            candidates[fence->fence_id] = fence;
        }
    }
    if (old_h3 != 0 && old_h3 != new_h3) {
        for (const auto* fence : registry_.get_candidates(old_h3)) {
            if (fence && fence->active) {
                candidates[fence->fence_id] = fence;
            }
        }
    }

    for (const auto& [_, fence] : candidates) {
        const bool inside = geo::point_in_polygon(lat, lon, fence->polygon_vertices);
        const FenceState previous =
            redis_.get_fence_state(device_id, fence->fence_id).value_or(FenceState::OUTSIDE);

        if (inside && previous == FenceState::OUTSIDE) {
            redis_.set_fence_state(device_id, fence->fence_id, FenceState::INSIDE, timestamp_ms);
            auto record = make_event_record(
                device_id, *fence, GeofenceEventType::ENTER, lat, lon, timestamp_ms);
            pg_.insert_geofence_event(record);
            event_producer_.produce(event_topic_, device_id, proto_boundary::encode_geofence_event_payload(record));
            Metrics::instance().inc_geofence_event("ENTER");
        } else if (!inside && (previous == FenceState::INSIDE || previous == FenceState::DWELL)) {
            redis_.set_fence_state(device_id, fence->fence_id, FenceState::OUTSIDE, timestamp_ms);
            auto record = make_event_record(
                device_id, *fence, GeofenceEventType::EXIT, lat, lon, timestamp_ms);
            pg_.insert_geofence_event(record);
            event_producer_.produce(event_topic_, device_id, proto_boundary::encode_geofence_event_payload(record));
            Metrics::instance().inc_geofence_event("EXIT");
        }
    }
}

} // namespace signalroute
