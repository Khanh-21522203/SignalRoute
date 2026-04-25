#include "sequence_guard.h"

namespace signalroute {

SequenceGuard::SequenceGuard(RedisClient& redis) : redis_(redis) {}

bool SequenceGuard::should_accept(const std::string& device_id, uint64_t seq) {
    auto state = redis_.get_device_state(device_id);
    return !state || seq > state->seq;
}

} // namespace signalroute
