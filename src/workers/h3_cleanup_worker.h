#pragma once

/**
 * SignalRoute — H3 Cleanup Worker
 *
 * Listens to Redis keyspace notifications for device TTL expiry.
 * When a device key expires, removes the device from its H3 cell set
 * to prevent stale entries in the spatial index.
 *
 * Subscribes to: __keyevent@0__:expired
 * Matches keys: {prefix}:dev:*
 */

#include "../common/clients/redis_client.h"
#include "../common/config/config.h"
#include <atomic>
#include <cstddef>

namespace signalroute {

class EventBus;

struct H3CleanupResult {
    std::size_t removed_devices = 0;
    std::size_t touched_cells = 0;
};

class H3CleanupWorker {
public:
    H3CleanupWorker(RedisClient& redis, const RedisConfig& config);
    H3CleanupWorker(RedisClient& redis, const RedisConfig& config, EventBus& event_bus);

    H3CleanupResult run_once();

    /**
     * Run the cleanup listener. Blocks until should_stop is set.
     *
     * TODO: Implement:
     *   1. Subscribe to Redis keyspace notification channel
     *   2. On expired event for a device key:
     *      a. Parse device_id from key
     *      b. Read last known H3 cell (may need shadow key or cached)
     *      c. SREM device from H3 cell set
     */
    void run(std::atomic<bool>& should_stop);

private:
    RedisClient& redis_;
    RedisConfig config_;
    EventBus* event_bus_ = nullptr;
};

} // namespace signalroute
