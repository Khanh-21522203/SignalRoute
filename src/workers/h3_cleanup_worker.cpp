#include "h3_cleanup_worker.h"
#include <thread>
#include <chrono>

namespace signalroute {

H3CleanupWorker::H3CleanupWorker(RedisClient& redis, const RedisConfig& config)
    : redis_(redis), config_(config) {}

void H3CleanupWorker::run(std::atomic<bool>& should_stop) {
    // TODO: Subscribe to Redis keyspace notifications
    while (!should_stop.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

} // namespace signalroute
