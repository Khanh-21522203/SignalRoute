#include "state_writer.h"
#include "../common/types/device_state.h"

namespace signalroute {

StateWriter::StateWriter(RedisClient& redis, H3Index& h3, int device_ttl_s)
    : redis_(redis), h3_(h3), device_ttl_s_(device_ttl_s) {}

bool StateWriter::write(const LocationEvent& /*event*/) {
    // TODO: Implement the full state write sequence
    //
    //   1. Encode H3 cell: int64_t new_cell = h3_.lat_lng_to_cell(event.lat, event.lon);
    //   2. Build DeviceState from event
    //   3. CAS update: bool accepted = redis_.update_device_state_cas(event.device_id, state, device_ttl_s_);
    //   4. If accepted, check if cell changed:
    //      auto old_state = redis_.get_device_state(event.device_id);
    //      if (old_state && old_state->h3_cell != new_cell) {
    //          redis_.remove_device_from_cell(old_state->h3_cell, event.device_id);
    //      }
    //      redis_.add_device_to_cell(new_cell, event.device_id);
    //   5. Return accepted
    return false;
}

} // namespace signalroute
