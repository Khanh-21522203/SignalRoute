#include "common/events/all_events.h"
#include "common/events/event_bus.h"
#include "matching/matching_service.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

signalroute::Config matching_config() {
    signalroute::Config config;
    config.matching.strategy_name = "nearest";
    config.matching.request_ttl_ms = 5000;
    config.spatial.h3_resolution = 7;
    config.spatial.nearby_max_radius_m = 50000.0;
    config.spatial.nearby_max_results = 100;
    return config;
}

signalroute::DeviceState agent(std::string id, double lat, double lon, uint64_t seq = 1) {
    signalroute::DeviceState state;
    state.device_id = std::move(id);
    state.lat = lat;
    state.lon = lon;
    state.seq = seq;
    state.updated_at = now_ms();
    return state;
}

signalroute::MatchRequest request(std::string id) {
    signalroute::MatchRequest req;
    req.request_id = std::move(id);
    req.requester_id = "rider-1";
    req.lat = 10.8231;
    req.lon = 106.6297;
    req.radius_m = 5000.0;
    req.max_agents = 1;
    req.deadline_ms = now_ms() + 5000;
    return req;
}

} // namespace

void test_handle_request_matches_nearest_agent_and_publishes_events() {
    signalroute::EventBus bus;
    signalroute::MatchingService service;
    int received = 0;
    int completed = 0;
    signalroute::events::MatchCompleted completed_event;

    auto received_sub = bus.subscribe<signalroute::events::MatchRequestReceived>(
        [&](const signalroute::events::MatchRequestReceived& event) {
            ++received;
            assert(event.request_id == "req-1");
        });
    auto completed_sub = bus.subscribe<signalroute::events::MatchCompleted>(
        [&](const signalroute::events::MatchCompleted& event) {
            ++completed;
            completed_event = event;
        });

    service.start(matching_config(), bus);
    assert(service.seed_agent_for_test(agent("agent-far", 10.8300, 106.6400)));
    assert(service.seed_agent_for_test(agent("agent-near", 10.8232, 106.6298)));

    const auto result = service.handle_request(request("req-1"));

    assert(result.status == signalroute::MatchStatus::MATCHED);
    assert((result.assigned_agent_ids == std::vector<std::string>{"agent-near"}));
    assert(service.is_agent_reserved_for_test("agent-near"));
    assert(!service.is_agent_reserved_for_test("agent-far"));
    assert(received == 1);
    assert(completed == 1);
    assert((completed_event.assigned_agent_ids == std::vector<std::string>{"agent-near"}));
}

void test_handle_request_fails_when_no_candidates() {
    signalroute::EventBus bus;
    signalroute::MatchingService service;
    int failed = 0;
    std::string reason;
    auto failed_sub = bus.subscribe<signalroute::events::MatchFailed>(
        [&](const signalroute::events::MatchFailed& event) {
            ++failed;
            reason = event.reason;
        });

    service.start(matching_config(), bus);
    const auto result = service.handle_request(request("req-empty"));

    assert(result.status == signalroute::MatchStatus::FAILED);
    assert(result.reason == "no agents matched");
    assert(failed == 1);
    assert(reason == "no agents matched");
}

void test_handle_request_skips_reserved_candidate_and_uses_next() {
    signalroute::MatchingService service;
    service.start(matching_config());
    assert(service.seed_agent_for_test(agent("agent-near", 10.8232, 106.6298)));
    assert(service.seed_agent_for_test(agent("agent-far", 10.8300, 106.6400)));
    assert(service.reserve_agent_for_test("agent-near", "other-request"));

    const auto result = service.handle_request(request("req-2"));

    assert(result.status == signalroute::MatchStatus::MATCHED);
    assert((result.assigned_agent_ids == std::vector<std::string>{"agent-far"}));
    assert(service.is_agent_reserved_for_test("agent-near"));
    assert(service.is_agent_reserved_for_test("agent-far"));
}

void test_expired_request_publishes_expired_event() {
    signalroute::EventBus bus;
    signalroute::MatchingService service;
    int expired = 0;
    auto expired_sub = bus.subscribe<signalroute::events::MatchExpired>(
        [&](const signalroute::events::MatchExpired& event) {
            ++expired;
            assert(event.request_id == "req-expired");
        });

    service.start(matching_config(), bus);
    auto req = request("req-expired");
    req.deadline_ms = now_ms() - 1;

    const auto result = service.handle_request(req);

    assert(result.status == signalroute::MatchStatus::EXPIRED);
    assert(expired == 1);
}

void test_stopped_service_fails_safely() {
    signalroute::MatchingService service;
    auto result = service.handle_request(request("req-stopped"));
    assert(result.status == signalroute::MatchStatus::FAILED);
    assert(result.reason == "matching service is not running");

    service.start(matching_config());
    service.stop();
    result = service.handle_request(request("req-stopped-2"));
    assert(result.status == signalroute::MatchStatus::FAILED);
}

int main() {
    std::cout << "test_matching_service_lifecycle:\n";
    test_handle_request_matches_nearest_agent_and_publishes_events();
    test_handle_request_fails_when_no_candidates();
    test_handle_request_skips_reserved_candidate_and_uses_next();
    test_expired_request_publishes_expired_event();
    test_stopped_service_fails_safely();
    std::cout << "All matching service lifecycle tests passed.\n";
    return 0;
}
