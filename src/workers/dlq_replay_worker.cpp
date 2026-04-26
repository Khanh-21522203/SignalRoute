#include "dlq_replay_worker.h"
#include "../common/events/all_events.h"
#include "../common/events/event_bus.h"
#include "../common/proto/location_payload_codec.h"

#include <chrono>
#include <thread>

namespace signalroute {

DLQReplayWorker::DLQReplayWorker(PostgresClient& pg, KafkaConsumer& consumer)
    : pg_(pg), consumer_(consumer) {}

DLQReplayWorker::DLQReplayWorker(PostgresClient& pg, KafkaConsumer& consumer, EventBus& event_bus)
    : pg_(pg), consumer_(consumer), event_bus_(&event_bus) {}

DLQReplayResult DLQReplayWorker::run_once(int max_messages) {
    DLQReplayResult result;
    if (max_messages <= 0) {
        return result;
    }

    for (int i = 0; i < max_messages; ++i) {
        auto msg = consumer_.poll(0);
        if (!msg) {
            break;
        }

        auto event = proto_boundary::decode_location_payload(msg->payload);
        if (event.is_err()) {
            ++result.failed_messages;
            consumer_.commit(*msg);
            continue;
        }

        pg_.batch_insert_trip_points({event.value()});
        consumer_.commit(*msg);
        ++result.replayed_messages;
    }

    if (event_bus_) {
        if (result.replayed_messages > 0) {
            event_bus_->publish(events::DLQReplaySucceeded{result.replayed_messages});
        }
        if (result.failed_messages > 0) {
            event_bus_->publish(events::DLQReplayFailed{"invalid DLQ payload", result.failed_messages});
        }
    }

    return result;
}

void DLQReplayWorker::run(std::atomic<bool>& should_stop) {
    while (!should_stop.load()) {
        (void)run_once();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

} // namespace signalroute
