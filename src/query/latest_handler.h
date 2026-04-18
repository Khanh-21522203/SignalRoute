#pragma once

/**
 * SignalRoute — Latest Location Handler
 *
 * Reads the latest device state from Redis.
 * Simple HGETALL on the device key.
 */

#include "../common/clients/redis_client.h"
#include "../common/types/device_state.h"
#include <optional>
#include <string>

namespace signalroute {

class LatestHandler {
public:
    explicit LatestHandler(RedisClient& redis);

    /**
     * Get the latest location for a device.
     *
     * TODO: Implement using redis_.get_device_state(device_id)
     *
     * P99 target: < 5 ms
     */
    std::optional<DeviceState> handle(const std::string& device_id);

private:
    RedisClient& redis_;
};

} // namespace signalroute
