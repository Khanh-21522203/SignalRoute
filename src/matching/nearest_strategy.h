#pragma once

#include "strategy_interface.h"

namespace signalroute {

class NearestStrategy final : public IMatchStrategy {
public:
    void initialize(const Config& config) override;
    std::vector<std::string> match(
        const MatchRequest& request,
        const std::vector<MatchCandidate>& candidates,
        MatchContext& context) override;
    std::string name() const override;
};

void register_builtin_strategies();

} // namespace signalroute
