#pragma once

/**
 * SignalRoute — Location Processor Service
 *
 * Stateful per-partition Kafka consumer that processes location events:
 *   1. Dedup check (LRU cache)
 *   2. Sequence guard (reject stale events)
 *   3. Encode (lat, lon) → H3 cell
 *   4. Update device state in Redis (Lua CAS)
 *   5. Update H3 cell index in Redis
 *   6. Buffer trip points for batch insert to PostGIS
 *   7. Notify Geofence Engine on state change
 *   8. Commit Kafka offset only after successful writes
 */

#include "../common/config/config.h"
#include <memory>
#include <atomic>

namespace signalroute {

class RedisClient;
class PostgresClient;
class KafkaConsumer;
class KafkaProducer;
class H3Index;

class ProcessorService {
public:
    ProcessorService();
    ~ProcessorService();

    /**
     * Start the processor.
     *
     * TODO: Implement:
     *   1. Create RedisClient, PostgresClient
     *   2. Create KafkaConsumer (subscribe to tm.location.events)
     *   3. Create KafkaProducer (for DLQ)
     *   4. Create H3Index with config.spatial.h3_resolution
     *   5. Create DedupWindow, SequenceGuard, StateWriter, HistoryWriter
     *   6. Start processing loop
     */
    void start(const Config& config);

    void stop();
    bool is_healthy() const;

private:
    std::atomic<bool> running_{false};
};

} // namespace signalroute
