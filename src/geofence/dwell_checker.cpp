#include "dwell_checker.h"
#include "../common/types/geofence_types.h"

#include <chrono>
#include <sstream>
#include <thread>
#include <utility>

namespace signalroute {
namespace {

std::string serialize_dwell_event(const GeofenceEventRecord& event) {
    std::ostringstream out;
    out << event.device_id << ','
        << event.fence_id << ','
        << geofence_event_type_to_string(event.event_type) << ','
        << event.event_ts_ms << ','
        << event.inside_duration_s;
    return out.str();
}

GeofenceEventRecord make_dwell_record(const FenceStateRecord& state,
                                      const GeofenceRule& fence,
                                      int64_t now_ms) {
    GeofenceEventRecord record;
    record.device_id = state.device_id;
    record.fence_id = fence.fence_id;
    record.fence_name = fence.name;
    record.event_type = GeofenceEventType::DWELL;
    record.event_ts_ms = now_ms;
    record.inside_duration_s = static_cast<int>((now_ms - state.entered_at_ms) / 1000);
    return record;
}

int64_t now_epoch_ms() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

} // namespace

DwellChecker::DwellChecker(RedisClient& redis, KafkaProducer& producer,
                           PostgresClient& pg, FenceRegistry& registry,
                           const GeofenceConfig& config,
                           std::string event_topic)
    : redis_(redis)
    , producer_(producer)
    , pg_(pg)
    , registry_(registry)
    , config_(config)
    , event_topic_(std::move(event_topic))
{}

int DwellChecker::check_once(int64_t now_ms) {
    int transitioned = 0;
    const auto inside_states = redis_.list_fence_states(FenceState::INSIDE);

    for (const auto& state : inside_states) {
        const auto fence = registry_.get_fence(state.fence_id);
        if (!fence || !fence->active || state.entered_at_ms <= 0) {
            continue;
        }

        const int threshold_s = fence->dwell_threshold_s > 0
            ? fence->dwell_threshold_s
            : config_.dwell_threshold_s;
        const int64_t inside_ms = now_ms - state.entered_at_ms;
        if (inside_ms < static_cast<int64_t>(threshold_s) * 1000) {
            continue;
        }

        redis_.set_fence_state(state.device_id, state.fence_id, FenceState::DWELL, now_ms);
        auto record = make_dwell_record(state, *fence, now_ms);
        pg_.insert_geofence_event(record);
        producer_.produce(event_topic_, state.device_id, serialize_dwell_event(record));
        ++transitioned;
    }

    return transitioned;
}

void DwellChecker::run(std::atomic<bool>& should_stop) {
    while (!should_stop.load()) {
        (void)check_once(now_epoch_ms());
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

} // namespace signalroute
