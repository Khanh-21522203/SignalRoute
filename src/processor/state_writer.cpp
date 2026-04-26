#include "state_writer.h"
#include <chrono>

namespace signalroute {

StateWriter::StateWriter(RedisClient& redis, H3Index& h3, int device_ttl_s)
    : redis_(redis), h3_(h3), device_ttl_s_(device_ttl_s) {}

bool StateWriter::write(const LocationEvent& event) {
    return write_with_result(event).accepted;
}

StateWriteOutcome StateWriter::write_with_result(const LocationEvent& event) {
    StateWriteOutcome outcome;
    const auto previous = redis_.get_device_state(event.device_id);
    if (previous) {
        outcome.previous_h3_cell = previous->h3_cell;
    }

    DeviceState state;
    state.device_id = event.device_id;
    state.lat = event.lat;
    state.lon = event.lon;
    state.altitude_m = event.altitude_m;
    state.accuracy_m = event.accuracy_m;
    state.speed_ms = event.speed_ms;
    state.heading_deg = event.heading_deg;
    state.h3_cell = h3_.lat_lng_to_cell(event.lat, event.lon);
    state.seq = event.seq;
    state.updated_at = event.server_recv_ms;
    if (state.updated_at == 0) {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        state.updated_at = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    }

    outcome.state = state;
    outcome.accepted = redis_.update_device_state_cas(event.device_id, state, device_ttl_s_);
    return outcome;
}

} // namespace signalroute
