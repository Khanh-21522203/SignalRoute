#include "common/clients/redis_client.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace {

signalroute::DeviceState make_state(std::string device_id, uint64_t seq, int64_t h3_cell) {
    signalroute::DeviceState state;
    state.device_id = std::move(device_id);
    state.seq = seq;
    state.h3_cell = h3_cell;
    state.lat = 10.0 + static_cast<double>(seq);
    state.lon = 106.0 + static_cast<double>(seq);
    state.updated_at = 1000 + static_cast<int64_t>(seq);
    return state;
}

std::unordered_set<std::string> as_set(const std::vector<std::string>& values) {
    return {values.begin(), values.end()};
}

void assert_contains_exactly(const std::vector<std::string>& values,
                             const std::unordered_set<std::string>& expected) {
    assert(values.size() == expected.size());
    assert(as_set(values) == expected);
}

} // namespace

void test_ping_returns_true() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});
    assert(redis.ping());
}

void test_cas_accepts_new_sequence_and_rejects_same_or_lower() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});

    assert(redis.update_device_state_cas("dev-1", make_state("ignored", 10, 100), 3600));
    assert(!redis.update_device_state_cas("dev-1", make_state("dev-1", 10, 100), 3600));
    assert(!redis.update_device_state_cas("dev-1", make_state("dev-1", 9, 100), 3600));
    assert(redis.update_device_state_cas("dev-1", make_state("dev-1", 11, 100), 3600));

    auto state = redis.get_device_state("dev-1");
    assert(state.has_value());
    assert(state->device_id == "dev-1");
    assert(state->seq == 11);
}

void test_moving_device_updates_h3_cell_membership() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});

    assert(redis.update_device_state_cas("dev-1", make_state("dev-1", 1, 101), 3600));
    assert_contains_exactly(redis.get_devices_in_cell(101), {"dev-1"});

    assert(redis.update_device_state_cas("dev-1", make_state("dev-1", 2, 202), 3600));
    assert(redis.get_devices_in_cell(101).empty());
    assert_contains_exactly(redis.get_devices_in_cell(202), {"dev-1"});
}

void test_batch_reads_preserve_input_order_and_missing_devices() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});

    assert(redis.update_device_state_cas("dev-a", make_state("dev-a", 1, 100), 3600));
    assert(redis.update_device_state_cas("dev-b", make_state("dev-b", 2, 200), 3600));

    const auto states = redis.get_device_states_batch({"missing-1", "dev-b", "dev-a", "missing-2"});
    assert(states.size() == 4);
    assert(!states[0].has_value());
    assert(states[1].has_value());
    assert(states[1]->device_id == "dev-b");
    assert(states[1]->seq == 2);
    assert(states[2].has_value());
    assert(states[2]->device_id == "dev-a");
    assert(states[2]->seq == 1);
    assert(!states[3].has_value());
}

void test_get_devices_in_cells_returns_unique_device_ids() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});

    redis.add_device_to_cell(100, "dev-1");
    redis.add_device_to_cell(100, "dev-2");
    redis.add_device_to_cell(200, "dev-2");
    redis.add_device_to_cell(200, "dev-3");

    const auto devices = redis.get_devices_in_cells({100, 200, 100, 300});
    assert_contains_exactly(devices, {"dev-1", "dev-2", "dev-3"});
}

void test_fence_state_set_get() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});

    assert(!redis.get_fence_state("dev-1", "fence-1").has_value());
    redis.set_fence_state("dev-1", "fence-1", signalroute::FenceState::INSIDE, 1234);
    assert(redis.get_fence_state("dev-1", "fence-1") == signalroute::FenceState::INSIDE);
    auto record = redis.get_fence_state_record("dev-1", "fence-1");
    assert(record.has_value());
    assert(record->entered_at_ms == 1234);
    assert(record->updated_at_ms == 1234);

    redis.set_fence_state("dev-1", "fence-1", signalroute::FenceState::DWELL, 2345);
    assert(redis.get_fence_state("dev-1", "fence-1") == signalroute::FenceState::DWELL);
    record = redis.get_fence_state_record("dev-1", "fence-1");
    assert(record.has_value());
    assert(record->entered_at_ms == 1234);
    assert(record->updated_at_ms == 2345);

    const auto dwell_states = redis.list_fence_states(signalroute::FenceState::DWELL);
    assert(dwell_states.size() == 1);
    assert(dwell_states.front().device_id == "dev-1");
}

void test_agent_reservation_release_requires_matching_request_id() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});

    assert(redis.try_reserve_agent("agent-1", "request-1", 5000));
    assert(!redis.try_reserve_agent("agent-1", "request-2", 5000));

    redis.release_agent("agent-1", "request-2");
    assert(!redis.try_reserve_agent("agent-1", "request-2", 5000));

    redis.release_agent("agent-1", "request-1");
    assert(redis.try_reserve_agent("agent-1", "request-2", 5000));
}

void test_agent_reservation_expires() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});

    assert(redis.try_reserve_agent("agent-1", "request-1", 1));
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    assert(!redis.is_agent_reserved("agent-1"));
    assert(!redis.get_agent_reservation_holder("agent-1").has_value());
    assert(redis.try_reserve_agent("agent-1", "request-2", 5000));
}

void test_stale_cell_cleanup_removes_orphan_members() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});

    assert(redis.update_device_state_cas("dev-live", make_state("dev-live", 1, 100), 3600));
    redis.add_device_to_cell(100, "dev-orphan");
    redis.add_device_to_cell(200, "dev-other-orphan");

    const auto [removed, touched] = redis.remove_stale_cell_members();

    assert(removed == 2);
    assert(touched == 2);
    assert_contains_exactly(redis.get_devices_in_cell(100), {"dev-live"});
    assert(redis.get_devices_in_cell(200).empty());
}

int main() {
    std::cout << "test_redis_client:\n";
    test_ping_returns_true();
    test_cas_accepts_new_sequence_and_rejects_same_or_lower();
    test_moving_device_updates_h3_cell_membership();
    test_batch_reads_preserve_input_order_and_missing_devices();
    test_get_devices_in_cells_returns_unique_device_ids();
    test_fence_state_set_get();
    test_agent_reservation_release_requires_matching_request_id();
    test_agent_reservation_expires();
    test_stale_cell_cleanup_removes_orphan_members();
    std::cout << "All RedisClient fallback tests passed.\n";
    return 0;
}
