#pragma once

/**
 * SignalRoute — Geofence Evaluator
 *
 * Evaluates a device's position change against all candidate fences.
 * Manages state transitions (OUTSIDE ↔ INSIDE ↔ DWELL) and emits events.
 */

#include "fence_registry.h"
#include "../common/clients/redis_client.h"
#include "../common/clients/postgres_client.h"
#include "../common/kafka/kafka_producer.h"
#include <string>
#include <cstdint>

namespace signalroute {

class Evaluator {
public:
    Evaluator(FenceRegistry& registry, RedisClient& redis,
              KafkaProducer& event_producer, PostgresClient& pg);

    /**
     * Evaluate a device's movement against all candidate fences.
     *
     * Called by the Processor when a device's state is updated.
     *
     * Steps:
     *   1. Get candidate fences for new_h3 (and old_h3 if different)
     *   2. For each candidate:
     *      a. Polygon containment test (point_in_polygon)
     *      b. Load current fence state from Redis
     *      c. Compute state transition
     *      d. If transition occurred:
     *         - Update fence state in Redis
     *         - Emit GeofenceEvent to Kafka
     *         - Insert audit record to PostGIS
     *
     * @param device_id Device that moved
     * @param old_h3 Previous H3 cell (0 if new device)
     * @param new_h3 Current H3 cell
     * @param lat Current latitude
     * @param lon Current longitude
     * @param timestamp_ms Event timestamp
     *
     * TODO: Implement
     */
    void evaluate(const std::string& device_id,
                  int64_t old_h3, int64_t new_h3,
                  double lat, double lon, int64_t timestamp_ms);

private:
    FenceRegistry& registry_;
    RedisClient& redis_;
    KafkaProducer& event_producer_;
    PostgresClient& pg_;
};

} // namespace signalroute
