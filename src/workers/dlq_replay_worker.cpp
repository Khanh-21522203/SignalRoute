#include "dlq_replay_worker.h"
#include "../common/events/all_events.h"
#include "../common/events/event_bus.h"
#include "../common/proto/location_payload_codec.h"

#include <chrono>
#include <algorithm>
#include <exception>
#include <string>
#include <thread>
#include <utility>

namespace signalroute {
namespace {

int bounded_attempts(int attempts) {
    return std::max(1, attempts);
}

int bounded_backoff_ms(int backoff_ms) {
    return std::max(0, backoff_ms);
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

        auto event = proto_boundary::decode_location_payload(msg->payload);
        if (event.is_err()) {
            ++result.failed_messages;
            consumer_.commit(*msg);
            continue;
        }

        bool replayed = false;
        int backoff_ms = bounded_backoff_ms(retry_policy_.initial_backoff_ms);
        const int max_attempts = bounded_attempts(retry_policy_.max_write_attempts);
        for (int attempt = 1; attempt <= max_attempts; ++attempt) {
            try {
                write_trip_point(event.value());
                replayed = true;
                break;
            } catch (const std::exception& ex) {
                result.last_error = ex.what();
            } catch (...) {
                result.last_error = "unknown DLQ replay write failure";
            }

            if (attempt < max_attempts) {
                ++result.retried_messages;
                if (backoff_ms > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
                    backoff_ms = retry_policy_.max_backoff_ms <= 0
                        ? backoff_ms
                        : std::min(backoff_ms * 2, retry_policy_.max_backoff_ms);
                }
            }
        }

        if (!replayed) {
            ++result.deferred_messages;
            if (event_bus_) {
                event_bus_->publish(events::DLQReplayFailed{
                    result.last_error.empty() ? "DLQ replay write failed" : result.last_error,
                    1});
            }
            break;
        }

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

void DLQReplayWorker::set_retry_policy(DLQReplayPolicy policy) {
    retry_policy_ = policy;
}

void DLQReplayWorker::set_trip_point_writer_for_test(TripPointWriter writer) {
    trip_point_writer_ = std::move(writer);
}

void DLQReplayWorker::write_trip_point(const LocationEvent& event) {
    if (trip_point_writer_) {
        trip_point_writer_(event);
        return;
    }
    pg_.batch_insert_trip_points({event});
}

void DLQReplayWorker::run(std::atomic<bool>& should_stop) {
    while (!should_stop.load()) {
        (void)run_once();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

} // namespace signalroute
