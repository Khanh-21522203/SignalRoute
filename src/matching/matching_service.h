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
#include "../common/types/device_state.h"
#include "matching_types.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace signalroute {

class IMatchStrategy;
class MatchContext;
class EventBus;
class H3Index;
class NearbyHandler;
class RedisClient;
class ReservationManager;

class MatchingService {
public:
    MatchingService();
    ~MatchingService();

    void start(const Config& config);
    void start(const Config& config, EventBus& event_bus);
    void stop();
    bool is_healthy() const;
    const std::string& strategy_name() const;

    MatchResult match_once(const MatchRequest& request,
                           const std::vector<MatchCandidate>& candidates,
                           MatchContext& context);

    MatchResult handle_request(MatchRequest request);

    bool seed_agent_for_test(DeviceState state);
    bool reserve_agent_for_test(const std::string& agent_id, const std::string& request_id);
    bool is_agent_reserved_for_test(const std::string& agent_id) const;

private:
    void start_with_bus(const Config& config, EventBus* external_bus);

    std::atomic<bool> running_{false};
    Config config_;
    std::unique_ptr<IMatchStrategy> strategy_;
    std::string strategy_name_;
    std::unique_ptr<EventBus> owned_bus_;
    EventBus* event_bus_ = nullptr;
    std::unique_ptr<RedisClient> redis_;
    std::unique_ptr<H3Index> h3_;
    std::unique_ptr<NearbyHandler> nearby_handler_;
    std::unique_ptr<ReservationManager> reservations_;
};

} // namespace signalroute
