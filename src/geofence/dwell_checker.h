#pragma once

/**
 * SignalRoute — Dwell Checker
 *
 * Background worker that periodically checks devices in INSIDE state
 * and transitions them to DWELL when they exceed the dwell threshold.
 */

#include "../common/clients/redis_client.h"
#include "../common/kafka/kafka_producer.h"
#include "../common/config/config.h"
#include "fence_registry.h"
#include <atomic>

namespace signalroute {

class DwellChecker {
public:
    DwellChecker(RedisClient& redis, KafkaProducer& producer,
                 FenceRegistry& registry, const GeofenceConfig& config);

    /**
     * Run the dwell check loop. Blocks until should_stop is set.
     *
     * TODO: Implement periodic scan of INSIDE fence states:
     *   1. For each active fence, scan devices in INSIDE state
     *   2. If (now - entered_at) > dwell_threshold_s:
     *      a. Transition to DWELL
     *      b. Emit DWELL GeofenceEvent
     */
    void run(std::atomic<bool>& should_stop);

private:
    RedisClient& redis_;
    KafkaProducer& producer_;
    FenceRegistry& registry_;
    GeofenceConfig config_;
};

} // namespace signalroute
