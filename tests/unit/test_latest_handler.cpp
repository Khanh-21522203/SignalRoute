#include "common/clients/redis_client.h"
#include "query/latest_handler.h"

#include <cassert>
#include <iostream>
#include <string>

namespace {

signalroute::DeviceState make_state(std::string device_id, uint64_t seq) {
    signalroute::DeviceState state;
    state.device_id = std::move(device_id);
    state.seq = seq;
    state.lat = 10.8231;
    state.lon = 106.6297;
    state.h3_cell = 123;
    state.updated_at = 1000;
    return state;
}

} // namespace

void test_returns_latest_state() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});
    signalroute::LatestHandler handler(redis);

    assert(redis.update_device_state_cas("dev-1", make_state("dev-1", 1), 3600));

    const auto state = handler.handle("dev-1");
    assert(state.has_value());
    assert(state->device_id == "dev-1");
    assert(state->seq == 1);
}

void test_missing_or_empty_device_returns_nullopt() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});
    signalroute::LatestHandler handler(redis);

    assert(!handler.handle("missing").has_value());
    assert(!handler.handle("").has_value());
}

int main() {
    std::cout << "test_latest_handler:\n";
    test_returns_latest_state();
    test_missing_or_empty_device_returns_nullopt();
    std::cout << "All latest handler tests passed.\n";
    return 0;
}
