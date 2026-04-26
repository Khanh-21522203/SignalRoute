#include "matching/match_context.h"
#include "matching/matching_service.h"
#include "matching/strategy_registry.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

class FakeContext final : public signalroute::MatchContext {
public:
    explicit FakeContext(std::string request_id) : request_id_(std::move(request_id)) {}

    bool reserve(const std::string& agent_id) override {
        reserve_calls.push_back(agent_id);
        reserved.push_back(agent_id);
        return true;
    }

    void release(const std::string& agent_id) override {
        released.push_back(agent_id);
        reserved.erase(std::remove(reserved.begin(), reserved.end(), agent_id), reserved.end());
    }

    std::vector<signalroute::MatchCandidate> nearby(double, double, double, int) override {
        return nearby_result;
    }

    int64_t time_remaining_ms() const override { return remaining_ms; }
    const std::string& request_id() const override { return request_id_; }

    int64_t remaining_ms = 1000;
    std::vector<std::string> reserve_calls;
    std::vector<std::string> reserved;
    std::vector<std::string> released;
    std::vector<signalroute::MatchCandidate> nearby_result;

private:
    std::string request_id_;
};

class LeakySuccessStrategy final : public signalroute::IMatchStrategy {
public:
    void initialize(const signalroute::Config&) override {}

    std::vector<std::string> match(
        const signalroute::MatchRequest&,
        const std::vector<signalroute::MatchCandidate>&,
        signalroute::MatchContext& context) override {
        assert(context.reserve("agent-1"));
        assert(context.reserve("agent-2"));
        return {"agent-1"};
    }

    std::string name() const override { return "unit.service.leaky_success"; }
};

class EmptyResultStrategy final : public signalroute::IMatchStrategy {
public:
    void initialize(const signalroute::Config&) override {}

    std::vector<std::string> match(
        const signalroute::MatchRequest&,
        const std::vector<signalroute::MatchCandidate>&,
        signalroute::MatchContext& context) override {
        assert(context.reserve("agent-1"));
        return {};
    }

    std::string name() const override { return "unit.service.empty_result"; }
};

class UnreservedResultStrategy final : public signalroute::IMatchStrategy {
public:
    void initialize(const signalroute::Config&) override {}

    std::vector<std::string> match(
        const signalroute::MatchRequest&,
        const std::vector<signalroute::MatchCandidate>&,
        signalroute::MatchContext& context) override {
        assert(context.reserve("agent-1"));
        return {"agent-2"};
    }

    std::string name() const override { return "unit.service.unreserved_result"; }
};

signalroute::MatchRequest request(std::string id) {
    signalroute::MatchRequest out;
    out.request_id = std::move(id);
    out.max_agents = 1;
    out.deadline_ms = now_ms() + 10000;
    return out;
}

void test_start_loads_builtin_default_strategy() {
    signalroute::Config config;
    signalroute::MatchingService service;
    service.start(config);
    assert(service.is_healthy());
    assert(service.strategy_name() == "default");
    service.stop();
    assert(!service.is_healthy());
}

void test_match_success_releases_unassigned_reservations() {
    auto& registry = signalroute::StrategyRegistry::instance();
    registry.register_strategy("unit.service.leaky_success", [] {
        return std::make_unique<LeakySuccessStrategy>();
    });

    signalroute::Config config;
    config.matching.strategy_name = "unit.service.leaky_success";
    signalroute::MatchingService service;
    service.start(config);

    auto req = request("request-1");
    FakeContext context(req.request_id);
    auto result = service.match_once(req, {}, context);

    assert(result.status == signalroute::MatchStatus::MATCHED);
    assert((result.assigned_agent_ids == std::vector<std::string>{"agent-1"}));
    assert(std::find(context.released.begin(), context.released.end(), "agent-2") != context.released.end());
    assert(std::find(context.released.begin(), context.released.end(), "agent-1") == context.released.end());
}

void test_match_failure_releases_all_reservations() {
    auto& registry = signalroute::StrategyRegistry::instance();
    registry.register_strategy("unit.service.empty_result", [] {
        return std::make_unique<EmptyResultStrategy>();
    });

    signalroute::Config config;
    config.matching.strategy_name = "unit.service.empty_result";
    signalroute::MatchingService service;
    service.start(config);

    auto req = request("request-2");
    FakeContext context(req.request_id);
    auto result = service.match_once(req, {}, context);

    assert(result.status == signalroute::MatchStatus::FAILED);
    assert(result.reason == "no agents matched");
    assert((context.released == std::vector<std::string>{"agent-1"}));
}

void test_unreserved_strategy_result_fails_safely() {
    auto& registry = signalroute::StrategyRegistry::instance();
    registry.register_strategy("unit.service.unreserved_result", [] {
        return std::make_unique<UnreservedResultStrategy>();
    });

    signalroute::Config config;
    config.matching.strategy_name = "unit.service.unreserved_result";
    signalroute::MatchingService service;
    service.start(config);

    auto req = request("request-4");
    FakeContext context(req.request_id);
    auto result = service.match_once(req, {}, context);

    assert(result.status == signalroute::MatchStatus::FAILED);
    assert(result.reason == "strategy returned unreserved agent");
    assert((context.released == std::vector<std::string>{"agent-1"}));
}

void test_expired_request_does_not_call_strategy() {
    signalroute::Config config;
    signalroute::MatchingService service;
    service.start(config);

    auto req = request("request-3");
    req.deadline_ms = now_ms() - 1;
    FakeContext context(req.request_id);
    auto result = service.match_once(req, {}, context);

    assert(result.status == signalroute::MatchStatus::EXPIRED);
    assert(context.reserve_calls.empty());
}

} // namespace

int main() {
    std::cout << "test_matching_service:\n";
    test_start_loads_builtin_default_strategy();
    test_match_success_releases_unassigned_reservations();
    test_match_failure_releases_all_reservations();
    test_unreserved_strategy_result_fails_safely();
    test_expired_request_does_not_call_strategy();
    std::cout << "All matching service tests passed.\n";
    return 0;
}
