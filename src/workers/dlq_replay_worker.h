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
#include <cstddef>

namespace signalroute {

class EventBus;

struct DLQReplayResult {
    std::size_t replayed_messages = 0;
    std::size_t failed_messages = 0;
};

class DLQReplayWorker {
public:
    DLQReplayWorker(PostgresClient& pg, KafkaConsumer& consumer);
    DLQReplayWorker(PostgresClient& pg, KafkaConsumer& consumer, EventBus& event_bus);

    DLQReplayResult run_once(int max_messages = 100);

    void run(std::atomic<bool>& should_stop);

private:
    PostgresClient& pg_;
    KafkaConsumer& consumer_;
    EventBus* event_bus_ = nullptr;
};

} // namespace signalroute
