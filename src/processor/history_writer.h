#pragma once

/**
 * SignalRoute — History Writer
 *
 * Buffers trip points and batch-inserts them to PostGIS.
 * On PostGIS failure, sends events to the DLQ topic for later replay.
 *
 * Flush triggers: buffer full OR flush interval elapsed.
 */

#include "../common/clients/postgres_client.h"
#include "../common/kafka/kafka_producer.h"
#include "../common/config/config.h"
#include "../common/types/location_event.h"
#include <vector>
#include <mutex>

namespace signalroute {

class HistoryWriter {
public:
    HistoryWriter(PostgresClient& pg, KafkaProducer& dlq, const ProcessorConfig& cfg);

    /// Buffer a trip point for batch insert.
    void buffer(const LocationEvent& event);

    /**
     * Flush buffered rows to PostGIS.
     * On failure, send events to DLQ topic for later replay.
     *
     * TODO: Implement:
     *   1. Copy buffer to local vector (under lock)
     *   2. Try pg_.batch_insert_trip_points(batch)
     *   3. On success: clear buffer, update metrics
     *   4. On failure: publish each event to DLQ topic, increment error counter
     */
    void flush();

    /// Current buffer depth.
    size_t buffer_size() const;

    /// Returns true if buffer has reached the max and should be flushed.
    bool should_flush() const;

private:
    PostgresClient& pg_;
    KafkaProducer& dlq_;
    ProcessorConfig config_;
    std::vector<LocationEvent> buffer_;
    mutable std::mutex mu_;
};

} // namespace signalroute
