#pragma once

/**
 * SignalRoute — Sequence Guard
 *
 * Ensures monotonic sequence ordering per device. An event is accepted
 * only if its seq > stored last_seq in Redis.
 *
 * Uses Redis EVALSHA for atomic compare-and-set within the
 * update_device_state_cas operation.
 */

#include "../common/clients/redis_client.h"
#include <string>
#include <cstdint>

namespace signalroute {

class SequenceGuard {
public:
    explicit SequenceGuard(RedisClient& redis);

    /**
     * Check if an event should be accepted for state update.
     *
     * @param device_id Device identifier
     * @param seq Incoming sequence number
     * @return true if seq > stored last_seq (or device is new)
     *
     * Note: The actual CAS operation is in RedisClient::update_device_state_cas.
     * This method provides a pre-check using cached state to avoid unnecessary
     * Redis round-trips for obviously stale events.
     *
     * TODO: Implement pre-check logic
     */
    bool should_accept(const std::string& device_id, uint64_t seq);

    /**
     * Return the currently stored sequence for diagnostics/events.
     */
    uint64_t current_seq(const std::string& device_id) const;

private:
    RedisClient& redis_;
    // TODO: Add local last_seq cache (per-partition, no sync needed)
    // std::unordered_map<std::string, uint64_t> last_seq_cache_;
};

} // namespace signalroute
