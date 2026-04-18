#include "latest_handler.h"

namespace signalroute {

LatestHandler::LatestHandler(RedisClient& redis) : redis_(redis) {}

std::optional<DeviceState> LatestHandler::handle(const std::string& device_id) {
    // TODO: Implement
    return redis_.get_device_state(device_id);
}

} // namespace signalroute
