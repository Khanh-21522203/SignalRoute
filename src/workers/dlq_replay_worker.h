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

namespace signalroute {

class DLQReplayWorker {
public:
    DLQReplayWorker(PostgresClient& pg, KafkaConsumer& consumer);

    void run(std::atomic<bool>& should_stop);

private:
    PostgresClient& pg_;
    KafkaConsumer& consumer_;
};

} // namespace signalroute
