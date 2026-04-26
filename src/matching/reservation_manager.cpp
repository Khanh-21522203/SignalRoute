#include "reservation_manager.h"

namespace signalroute {

ReservationManager::ReservationManager(RedisClient& redis, int default_ttl_ms)
    : redis_(redis), default_ttl_ms_(default_ttl_ms) {}

bool ReservationManager::reserve(const std::string& agent_id,
                                  const std::string& request_id) {
    if (agent_id.empty() || request_id.empty() || default_ttl_ms_ <= 0) {
        return false;
    }
    return redis_.try_reserve_agent(agent_id, request_id, default_ttl_ms_);
}

void ReservationManager::release(const std::string& agent_id,
                                  const std::string& request_id) {
    redis_.release_agent(agent_id, request_id);
}

bool ReservationManager::is_reserved(const std::string& agent_id) const {
    if (agent_id.empty()) {
        return false;
    }
    return redis_.is_agent_reserved(agent_id);
}

} // namespace signalroute
