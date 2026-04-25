#include "common/clients/redis_client.h"
#include "processor/sequence_guard.h"

#include <cassert>
#include <iostream>

void test_accepts_new_device() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});
    signalroute::SequenceGuard guard(redis);
    assert(guard.should_accept("dev-1", 1));
}

void test_rejects_stale_sequence() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});
    signalroute::DeviceState state;
    state.device_id = "dev-1";
    state.seq = 10;
    assert(redis.update_device_state_cas("dev-1", state, 3600));

    signalroute::SequenceGuard guard(redis);
    assert(!guard.should_accept("dev-1", 10));
    assert(!guard.should_accept("dev-1", 9));
    assert(guard.should_accept("dev-1", 11));
}

int main() {
    std::cout << "test_sequence_guard:\n";
    test_accepts_new_device();
    test_rejects_stale_sequence();
    std::cout << "All sequence guard tests passed.\n";
    return 0;
}
