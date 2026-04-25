#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace signalroute::events {

struct MatchRequestReceived {
    std::string request_id;
    std::string requester_id;
    double lat = 0.0;
    double lon = 0.0;
    double radius_m = 0.0;
    int max_agents = 0;
    int64_t deadline_ms = 0;
    std::string strategy;
};

struct AgentReserved {
    std::string request_id;
    std::string agent_id;
};

struct AgentReservationFailed {
    std::string request_id;
    std::string agent_id;
    std::string reason;
};

struct MatchCompleted {
    std::string request_id;
    std::vector<std::string> assigned_agent_ids;
    int64_t latency_ms = 0;
};

struct MatchFailed {
    std::string request_id;
    std::string reason;
};

struct MatchExpired {
    std::string request_id;
};

} // namespace signalroute::events
