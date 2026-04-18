#pragma once

/**
 * SignalRoute — Query Service
 *
 * Stateless read-path service providing:
 *   - GetLatestLocation (Redis)
 *   - NearbyDevices (Redis H3 index + haversine filter)
 *   - GetTrip (PostGIS time-range query)
 */

#include "../common/config/config.h"
#include <atomic>

namespace signalroute {

class QueryService {
public:
    QueryService();
    ~QueryService();

    void start(const Config& config);
    void stop();
    bool is_healthy() const;

private:
    std::atomic<bool> running_{false};
};

} // namespace signalroute
