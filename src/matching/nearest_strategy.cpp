#include "nearest_strategy.h"

#include "match_context.h"
#include "strategy_registry.h"

#include <algorithm>
#include <memory>

namespace signalroute {

void NearestStrategy::initialize(const Config& /*config*/) {}

std::vector<std::string> NearestStrategy::match(
    const MatchRequest& request,
    const std::vector<MatchCandidate>& candidates,
    MatchContext& context) {
    std::vector<std::string> assigned;
    if (request.max_agents <= 0) {
        return assigned;
    }

    std::vector<MatchCandidate> sorted;
    sorted.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        if (candidate.available && !candidate.agent_id.empty()) {
            sorted.push_back(candidate);
        }
    }

    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        if (a.distance_m != b.distance_m) {
            return a.distance_m < b.distance_m;
        }
        return a.agent_id < b.agent_id;
    });

    for (const auto& candidate : sorted) {
        if (context.time_remaining_ms() == 0) {
            break;
        }
        if (context.reserve(candidate.agent_id)) {
            assigned.push_back(candidate.agent_id);
            if (static_cast<int>(assigned.size()) >= request.max_agents) {
                break;
            }
        }
    }

    return assigned;
}

std::string NearestStrategy::name() const {
    return "nearest";
}

void register_builtin_strategies() {
    auto& registry = StrategyRegistry::instance();
    registry.register_strategy("default", [] { return std::make_unique<NearestStrategy>(); });
    registry.register_strategy("nearest", [] { return std::make_unique<NearestStrategy>(); });
}

} // namespace signalroute
