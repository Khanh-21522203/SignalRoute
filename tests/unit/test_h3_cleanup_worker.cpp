#include "common/clients/redis_client.h"
#include "common/events/all_events.h"
#include "common/events/event_bus.h"
#include "workers/h3_cleanup_worker.h"

#include <cassert>
#include <iostream>

void test_removes_stale_cell_members_and_publishes_event() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});
    signalroute::EventBus bus;
    int cleanup_events = 0;
    signalroute::events::H3CleanupCompleted captured;

    auto sub = bus.subscribe<signalroute::events::H3CleanupCompleted>(
        [&](const signalroute::events::H3CleanupCompleted& event) {
            ++cleanup_events;
            captured = event;
        });

    signalroute::DeviceState live;
    live.device_id = "live";
    live.seq = 1;
    live.h3_cell = 100;
    assert(redis.update_device_state_cas("live", live, 3600));
    redis.add_device_to_cell(100, "stale");

    signalroute::H3CleanupWorker worker(redis, signalroute::RedisConfig{}, bus);
    const auto result = worker.run_once();

    assert(result.removed_devices == 1);
    assert(result.touched_cells == 1);
    assert(cleanup_events == 1);
    assert(captured.removed_devices == 1);
    assert(redis.get_devices_in_cell(100).size() == 1);
    assert(redis.get_devices_in_cell(100).front() == "live");
}

void test_noop_when_no_stale_members() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});
    signalroute::EventBus bus;
    int cleanup_events = 0;
    auto sub = bus.subscribe<signalroute::events::H3CleanupCompleted>(
        [&](const signalroute::events::H3CleanupCompleted&) { ++cleanup_events; });

    signalroute::DeviceState live;
    live.device_id = "live";
    live.seq = 1;
    live.h3_cell = 100;
    assert(redis.update_device_state_cas("live", live, 3600));

    signalroute::H3CleanupWorker worker(redis, signalroute::RedisConfig{}, bus);
    const auto result = worker.run_once();

    assert(result.removed_devices == 0);
    assert(result.touched_cells == 0);
    assert(cleanup_events == 0);
}

int main() {
    std::cout << "test_h3_cleanup_worker:\n";
    test_removes_stale_cell_members_and_publishes_event();
    test_noop_when_no_stale_members();
    std::cout << "All H3 cleanup worker tests passed.\n";
    return 0;
}
