#pragma once

/**
 * SignalRoute — Reservation Manager
 *
 * Manages atomic agent reservations using Redis SET NX.
 * Prevents double-assignment of agents across concurrent match requests.
 */

#include "../common/clients/redis_client.h"
#include <string>

namespace signalroute {

class ReservationManager {
public:
    explicit ReservationManager(RedisClient& redis, int default_ttl_ms = 10000);

    /// Reserve an agent. Returns true if successful.
    bool reserve(const std::string& agent_id, const std::string& request_id);

    /// Release a reservation. Only succeeds if this request owns it.
    void release(const std::string& agent_id, const std::string& request_id);

    /// Check if an agent is currently reserved.
    bool is_reserved(const std::string& agent_id) const;

private:
    RedisClient& redis_;
    int default_ttl_ms_;
};

} // namespace signalroute
