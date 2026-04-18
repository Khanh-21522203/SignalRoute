#include "matching_service.h"
#include <iostream>

namespace signalroute {

MatchingService::MatchingService() = default;
MatchingService::~MatchingService() { if (running_) stop(); }

void MatchingService::start(const Config& /*config*/) {
    std::cout << "[MatchingService] Starting...\n";
    running_ = true;
    // TODO: Create KafkaConsumer (config.matching.request_topic)
    // TODO: Create KafkaProducer (config.matching.result_topic)
    // TODO: Create NearbyHandler for agent search
    // TODO: Create ReservationManager
    // TODO: Load strategy from StrategyRegistry
    // TODO: Start Kafka poll loop (CPU-spin for low latency)
    std::cout << "[MatchingService] Started.\n";
}

void MatchingService::stop() {
    std::cout << "[MatchingService] Stopping...\n";
    running_ = false;
    std::cout << "[MatchingService] Stopped.\n";
}

bool MatchingService::is_healthy() const { return running_; }

} // namespace signalroute
