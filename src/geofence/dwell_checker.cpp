#include "dwell_checker.h"
#include <thread>
#include <chrono>

namespace signalroute {

DwellChecker::DwellChecker(RedisClient& redis, KafkaProducer& producer,
                           FenceRegistry& registry, const GeofenceConfig& config)
    : redis_(redis), producer_(producer), registry_(registry), config_(config) {}

void DwellChecker::run(std::atomic<bool>& should_stop) {
    while (!should_stop.load()) {
        // TODO: Implement dwell scan
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

} // namespace signalroute
