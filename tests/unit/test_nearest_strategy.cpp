#include "matching/match_context.h"
#include "matching/nearest_strategy.h"

#include <cassert>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

class FakeContext final : public signalroute::MatchContext {
public:
    explicit FakeContext(std::string request_id) : request_id_(std::move(request_id)) {}

    bool reserve(const std::string& agent_id) override {
        reserve_calls.push_back(agent_id);
        if (failed_reservations.count(agent_id) > 0) {
            return false;
        }
        reserved.push_back(agent_id);
        return true;
    }

    void release(const std::string& agent_id) override {
        released.push_back(agent_id);
    }

    std::vector<signalroute::MatchCandidate> nearby(double, double, double, int) override {
        return nearby_result;
    }

    int64_t time_remaining_ms() const override { return remaining_ms; }
    const std::string& request_id() const override { return request_id_; }

    int64_t remaining_ms = 1000;
    std::set<std::string> failed_reservations;
    std::vector<std::string> reserve_calls;
    std::vector<std::string> reserved;
    std::vector<std::string> released;
    std::vector<signalroute::MatchCandidate> nearby_result;

private:
    std::string request_id_;
};

signalroute::MatchCandidate candidate(std::string id, double distance, bool available = true) {
    signalroute::MatchCandidate out;
    out.agent_id = std::move(id);
    out.distance_m = distance;
    out.available = available;
    return out;
}

void test_nearest_strategy_reserves_closest_available_agents() {
    signalroute::NearestStrategy strategy;
    signalroute::MatchRequest request;
    request.request_id = "request-1";
    request.max_agents = 2;

    FakeContext context(request.request_id);
    context.failed_reservations.insert("agent-x");

    auto assigned = strategy.match(request, {
        candidate("agent-b", 5.0),
        candidate("agent-hidden", 1.0, false),
        candidate("agent-x", 2.0),
        candidate("agent-a", 5.0),
    }, context);

    assert((assigned == std::vector<std::string>{"agent-a", "agent-b"}));
    assert((context.reserve_calls == std::vector<std::string>{"agent-x", "agent-a", "agent-b"}));
}

void test_nearest_strategy_respects_time_and_requested_count() {
    signalroute::NearestStrategy strategy;
    signalroute::MatchRequest request;
    request.request_id = "request-1";
    request.max_agents = 0;

    FakeContext context(request.request_id);
    auto assigned = strategy.match(request, {candidate("agent-a", 1.0)}, context);
    assert(assigned.empty());
    assert(context.reserve_calls.empty());

    request.max_agents = 1;
    context.remaining_ms = 0;
    assigned = strategy.match(request, {candidate("agent-a", 1.0)}, context);
    assert(assigned.empty());
    assert(context.reserve_calls.empty());
}

} // namespace

int main() {
    std::cout << "test_nearest_strategy:\n";
    test_nearest_strategy_reserves_closest_available_agents();
    test_nearest_strategy_respects_time_and_requested_count();
    std::cout << "All nearest strategy tests passed.\n";
    return 0;
}
