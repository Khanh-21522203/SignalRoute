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
#include "../common/types/device_state.h"
#include <memory>
#include <atomic>
#include <cstddef>
#include <optional>
#include <thread>

namespace signalroute {

class RedisClient;
class PostgresClient;
class KafkaConsumer;
class KafkaProducer;
class H3Index;
class DedupWindow;
class EventBus;
class CompositionRoot;
class MetricsEventHandlers;
class SequenceGuard;
class StateWriter;
class HistoryWriter;
class ProcessingLoop;
class ProcessorEventHandlers;

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
    void start(const Config& config, EventBus& event_bus);

    void stop();
    bool is_healthy() const;
    bool is_event_driven() const;
    std::size_t subscription_count() const;

    std::optional<DeviceState> latest_state_for_test(const std::string& device_id) const;
    std::size_t trip_point_count_for_test() const;

private:
    void start_with_bus(const Config& config, EventBus* external_bus);

    std::atomic<bool> running_{false};
    std::atomic<bool> should_stop_{false};

    std::unique_ptr<EventBus> owned_bus_;
    EventBus* event_bus_ = nullptr;

    std::unique_ptr<KafkaConsumer> consumer_;
    std::unique_ptr<KafkaProducer> dlq_;
    std::unique_ptr<RedisClient> redis_;
    std::unique_ptr<PostgresClient> pg_;
    std::unique_ptr<H3Index> h3_;
    std::unique_ptr<DedupWindow> dedup_;
    std::unique_ptr<SequenceGuard> seq_guard_;
    std::unique_ptr<StateWriter> state_writer_;
    std::unique_ptr<HistoryWriter> history_writer_;
    std::unique_ptr<CompositionRoot> composition_root_;
    std::unique_ptr<ProcessorEventHandlers> processor_handlers_;
    std::unique_ptr<MetricsEventHandlers> metrics_handlers_;
    std::unique_ptr<ProcessingLoop> processing_loop_;
    std::thread worker_;
};

} // namespace signalroute
