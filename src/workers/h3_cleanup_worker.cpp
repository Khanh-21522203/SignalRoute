#include "h3_cleanup_worker.h"
#include "../common/events/all_events.h"
#include "../common/events/event_bus.h"
#include <thread>
#include <chrono>

namespace signalroute {

H3CleanupWorker::H3CleanupWorker(RedisClient& redis, const RedisConfig& config)
    : redis_(redis), config_(config) {}

H3CleanupWorker::H3CleanupWorker(RedisClient& redis, const RedisConfig& config, EventBus& event_bus)
    : redis_(redis), config_(config), event_bus_(&event_bus) {}

H3CleanupResult H3CleanupWorker::run_once() {
    const auto [removed, touched] = redis_.remove_stale_cell_members();
    H3CleanupResult result{removed, touched};
    if (event_bus_ && (removed > 0 || touched > 0)) {
        event_bus_->publish(events::H3CleanupCompleted{removed, touched});
    }
    return result;
}

void H3CleanupWorker::run(std::atomic<bool>& should_stop) {
    // TODO: Subscribe to Redis keyspace notifications
    while (!should_stop.load()) {
        (void)run_once();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

} // namespace signalroute
