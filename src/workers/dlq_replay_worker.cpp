#include "dlq_replay_worker.h"
#include "../common/events/all_events.h"
#include "../common/events/event_bus.h"
#include <thread>
#include <chrono>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace signalroute {
namespace {

std::vector<std::string> split_csv(const std::string& payload) {
    std::vector<std::string> fields;
    std::stringstream in(payload);
    std::string field;
    while (std::getline(in, field, ',')) {
        fields.push_back(field);
    }
    return fields;
}

std::optional<LocationEvent> parse_location_payload(const std::string& payload) {
    const auto fields = split_csv(payload);
    if (fields.size() != 6) {
        return std::nullopt;
    }

    try {
        LocationEvent event;
        event.device_id = fields[0];
        event.seq = static_cast<uint64_t>(std::stoull(fields[1]));
        event.timestamp_ms = std::stoll(fields[2]);
        event.server_recv_ms = std::stoll(fields[3]);
        event.lat = std::stod(fields[4]);
        event.lon = std::stod(fields[5]);
        if (event.device_id.empty()) {
            return std::nullopt;
        }
        return event;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace

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

        auto event = parse_location_payload(msg->payload);
        if (!event) {
            ++result.failed_messages;
            consumer_.commit(*msg);
            continue;
        }

        pg_.batch_insert_trip_points({*event});
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
