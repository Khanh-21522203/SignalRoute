#pragma once

/**
 * SignalRoute — DLQ Replay Worker
 *
 * Periodically consumes from tm.location.dlq and retries INSERT into PostGIS.
 * Uses exponential backoff to avoid hammering a recovering database.
 */

#include "../common/clients/postgres_client.h"
#include "../common/kafka/kafka_consumer.h"
#include "../common/config/config.h"
#include <atomic>
#include <functional>
#include <cstddef>
#include <string>

namespace signalroute {

class EventBus;
struct LocationEvent;

struct DLQReplayResult {
    std::size_t replayed_messages = 0;
    std::size_t failed_messages = 0;
    std::size_t retried_messages = 0;
    std::size_t deferred_messages = 0;
    std::string last_error;
};

struct DLQReplayPolicy {
    int max_write_attempts = 3;
    int initial_backoff_ms = 10;
    int max_backoff_ms = 1000;
};

class DLQReplayWorker {
public:
    using TripPointWriter = std::function<void(const LocationEvent&)>;

    DLQReplayWorker(PostgresClient& pg, KafkaConsumer& consumer);
    DLQReplayWorker(PostgresClient& pg, KafkaConsumer& consumer, EventBus& event_bus);

    DLQReplayResult run_once(int max_messages = 100);
    void set_retry_policy(DLQReplayPolicy policy);
    void set_trip_point_writer_for_test(TripPointWriter writer);

    void run(std::atomic<bool>& should_stop);

private:
    void write_trip_point(const LocationEvent& event);

    PostgresClient& pg_;
    KafkaConsumer& consumer_;
    EventBus* event_bus_ = nullptr;
    DLQReplayPolicy retry_policy_;
    TripPointWriter trip_point_writer_;
};

} // namespace signalroute
