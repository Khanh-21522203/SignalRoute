#include "dlq_replay_worker.h"
#include <thread>
#include <chrono>

namespace signalroute {

DLQReplayWorker::DLQReplayWorker(PostgresClient& pg, KafkaConsumer& consumer)
    : pg_(pg), consumer_(consumer) {}

void DLQReplayWorker::run(std::atomic<bool>& should_stop) {
    // TODO: Implement DLQ replay with exponential backoff
    //
    //   int backoff_ms = 1000;
    //   while (!should_stop.load()) {
    //       auto msg = consumer_.poll(1000);
    //       if (!msg) continue;
    //
    //       // Deserialize event from DLQ payload
    //       // auto event = deserialize_location_event(msg->payload);
    //
    //       try {
    //           // pg_.batch_insert_trip_points({event});
    //           consumer_.commit(*msg);
    //           backoff_ms = 1000;  // Reset on success
    //       } catch (...) {
    //           // PostGIS still down — back off
    //           std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
    //           backoff_ms = std::min(backoff_ms * 2, 60000);
    //       }
    //   }

    while (!should_stop.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

} // namespace signalroute
