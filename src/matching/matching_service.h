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
#include "matching_types.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace signalroute {

class IMatchStrategy;
class MatchContext;

class MatchingService {
public:
    MatchingService();
    ~MatchingService();

    void start(const Config& config);
    void stop();
    bool is_healthy() const;
    const std::string& strategy_name() const;

    MatchResult match_once(const MatchRequest& request,
                           const std::vector<MatchCandidate>& candidates,
                           MatchContext& context);

private:
    std::atomic<bool> running_{false};
    std::unique_ptr<IMatchStrategy> strategy_;
    std::string strategy_name_;
};

} // namespace signalroute
