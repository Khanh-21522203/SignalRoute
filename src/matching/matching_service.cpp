#include "matching_service.h"

#include "match_context.h"
#include "nearest_strategy.h"
#include "strategy_interface.h"
#include "strategy_registry.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace signalroute {

namespace {

int64_t epoch_now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

class TrackingMatchContext final : public MatchContext {
public:
    explicit TrackingMatchContext(MatchContext& inner) : inner_(inner) {}

    bool reserve(const std::string& agent_id) override {
        if (!inner_.reserve(agent_id)) {
            return false;
        }
        if (std::find(reserved_.begin(), reserved_.end(), agent_id) == reserved_.end()) {
            reserved_.push_back(agent_id);
        }
        return true;
    }

    void release(const std::string& agent_id) override {
        inner_.release(agent_id);
        reserved_.erase(std::remove(reserved_.begin(), reserved_.end(), agent_id), reserved_.end());
    }

    std::vector<MatchCandidate> nearby(double lat, double lon, double radius_m, int limit) override {
        return inner_.nearby(lat, lon, radius_m, limit);
    }

    int64_t time_remaining_ms() const override {
        return inner_.time_remaining_ms();
    }

    const std::string& request_id() const override {
        return inner_.request_id();
    }

    void release_all() {
        auto reserved = reserved_;
        for (const auto& agent_id : reserved) {
            release(agent_id);
        }
    }

    void release_unassigned(const std::vector<std::string>& assigned_agent_ids) {
        const std::unordered_set<std::string> assigned(
            assigned_agent_ids.begin(), assigned_agent_ids.end());
        auto reserved = reserved_;
        for (const auto& agent_id : reserved) {
            if (assigned.count(agent_id) == 0) {
                release(agent_id);
            }
        }
    }

    bool owns_all(const std::vector<std::string>& agent_ids) const {
        std::unordered_set<std::string> reserved(reserved_.begin(), reserved_.end());
        for (const auto& agent_id : agent_ids) {
            if (agent_id.empty() || reserved.count(agent_id) == 0) {
                return false;
            }
        }
        return true;
    }

private:
    MatchContext& inner_;
    std::vector<std::string> reserved_;
};

MatchResult failed_result(const MatchRequest& request, std::string reason) {
    MatchResult result;
    result.request_id = request.request_id;
    result.status = MatchStatus::FAILED;
    result.reason = std::move(reason);
    return result;
}

} // namespace

MatchingService::MatchingService() = default;
MatchingService::~MatchingService() { if (running_) stop(); }

void MatchingService::start(const Config& config) {
    std::cout << "[MatchingService] Starting...\n";
    register_builtin_strategies();
    strategy_ = StrategyRegistry::instance().create(config.matching.strategy_name);
    strategy_->initialize(config);
    strategy_name_ = config.matching.strategy_name;
    running_ = true;
    // Kafka/protobuf request consumption and result publication are integrated later.
    std::cout << "[MatchingService] Started.\n";
}

void MatchingService::stop() {
    std::cout << "[MatchingService] Stopping...\n";
    running_ = false;
    strategy_.reset();
    std::cout << "[MatchingService] Stopped.\n";
}

bool MatchingService::is_healthy() const { return running_; }

const std::string& MatchingService::strategy_name() const {
    return strategy_name_;
}

MatchResult MatchingService::match_once(const MatchRequest& request,
                                        const std::vector<MatchCandidate>& candidates,
                                        MatchContext& context) {
    const int64_t started_ms = epoch_now_ms();
    if (!running_ || !strategy_) {
        return failed_result(request, "matching service is not running");
    }
    if (request.request_id.empty()) {
        return failed_result(request, "request_id is required");
    }
    if (request.deadline_ms > 0 && started_ms >= request.deadline_ms) {
        MatchResult result;
        result.request_id = request.request_id;
        result.status = MatchStatus::EXPIRED;
        result.reason = "request deadline expired";
        return result;
    }

    TrackingMatchContext tracking(context);
    try {
        auto assigned = strategy_->match(request, candidates, tracking);
        if (request.max_agents > 0 && static_cast<int>(assigned.size()) > request.max_agents) {
            assigned.resize(static_cast<size_t>(request.max_agents));
        }
        if (assigned.empty()) {
            tracking.release_all();
            return failed_result(request, "no agents matched");
        }
        if (!tracking.owns_all(assigned)) {
            tracking.release_all();
            return failed_result(request, "strategy returned unreserved agent");
        }

        tracking.release_unassigned(assigned);

        MatchResult result;
        result.request_id = request.request_id;
        result.status = MatchStatus::MATCHED;
        result.assigned_agent_ids = std::move(assigned);
        result.latency_ms = epoch_now_ms() - started_ms;
        return result;
    } catch (const std::exception& ex) {
        tracking.release_all();
        return failed_result(request, ex.what());
    } catch (...) {
        tracking.release_all();
        return failed_result(request, "matching strategy failed");
    }
}

} // namespace signalroute
