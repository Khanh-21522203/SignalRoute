#include "sequence_guard.h"

namespace signalroute {

SequenceGuard::SequenceGuard(RedisClient& redis) : redis_(redis) {}

bool SequenceGuard::should_accept(const std::string& /*device_id*/, uint64_t /*seq*/) {
    // TODO: Check local cache first, then fallback to Redis
    return true;
}

} // namespace signalroute
