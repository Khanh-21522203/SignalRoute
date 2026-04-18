#pragma once

/**
 * SignalRoute — Matching Service
 *
 * Framework orchestrator for the matching pipeline:
 *   1. Consume MatchRequest from Kafka
 *   2. Fetch nearby available agents via NearbyHandler
 *   3. Instantiate the configured strategy
 *   4. Provide MatchContext (reserve, release, nearby, time_remaining)
 *   5. Execute strategy.match()
 *   6. Publish MatchResult to Kafka
 */

#include "../common/config/config.h"
#include <atomic>

namespace signalroute {

class MatchingService {
public:
    MatchingService();
    ~MatchingService();

    void start(const Config& config);
    void stop();
    bool is_healthy() const;

private:
    std::atomic<bool> running_{false};
};

} // namespace signalroute
