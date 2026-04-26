#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace signalroute {

enum class MatchStatus {
    MATCHED,
    FAILED,
    EXPIRED
};

struct MatchRequest {
    std::string request_id;
    std::string requester_id;
    double lat = 0.0;
    double lon = 0.0;
    double radius_m = 0.0;
    int max_agents = 1;
    int64_t deadline_ms = 0;
    std::string strategy;
};

struct MatchCandidate {
    std::string agent_id;
    double lat = 0.0;
    double lon = 0.0;
    double distance_m = 0.0;
    bool available = true;
};

struct MatchResult {
    std::string request_id;
    MatchStatus status = MatchStatus::FAILED;
    std::vector<std::string> assigned_agent_ids;
    std::string reason;
    int64_t latency_ms = 0;

    bool matched() const {
        return status == MatchStatus::MATCHED && !assigned_agent_ids.empty();
    }
};

} // namespace signalroute
