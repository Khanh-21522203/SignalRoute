#include "processing_loop.h"
#include "../common/kafka/kafka_consumer.h"
#include "../common/events/all_events.h"
#include "../common/events/event_bus.h"
#include "dedup_window.h"
#include "sequence_guard.h"
#include "state_writer.h"
#include "history_writer.h"
#include "../common/metrics/metrics.h"
#include "../common/proto/location_payload_codec.h"
#include "../common/types/location_event.h"

#include <chrono>
#include <string>

namespace signalroute {
ProcessingLoop::ProcessingLoop(
    KafkaConsumer& consumer,
    DedupWindow& dedup,
    SequenceGuard& seq_guard,
    StateWriter& state_writer,
    HistoryWriter& history_writer,
    const ProcessorConfig& config
)
    : consumer_(consumer)
    , dedup_(dedup)
    , seq_guard_(seq_guard)
    , state_writer_(state_writer)
    , history_writer_(history_writer)
    , config_(config)
{}

ProcessingLoop::ProcessingLoop(
    KafkaConsumer& consumer,
    DedupWindow& dedup,
    SequenceGuard& seq_guard,
    StateWriter& state_writer,
    HistoryWriter& history_writer,
    const ProcessorConfig& config,
    EventBus& event_bus
)
    : ProcessingLoop(consumer, dedup, seq_guard, state_writer, history_writer, config)
{
    event_bus_ = &event_bus;
}

void ProcessingLoop::run(std::atomic<bool>& should_stop) {
    auto last_flush = std::chrono::steady_clock::now();

    while (!should_stop.load()) {
        auto msg = consumer_.poll(10);
        if (!msg) {
            const auto elapsed = std::chrono::steady_clock::now() - last_flush;
            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
            if (history_writer_.buffer_size() > 0 &&
                elapsed_ms >= config_.history_flush_interval_ms) {
                history_writer_.flush();
                last_flush = std::chrono::steady_clock::now();
            }
            continue;
        }

        auto decoded = proto_boundary::decode_location_payload(msg->payload);
        if (decoded.is_err()) {
            consumer_.commit(*msg);
            continue;
        }
        auto event = decoded.value();

        if (dedup_.is_duplicate(event.device_id, event.seq)) {
            if (event_bus_) {
                event_bus_->publish(events::LocationDuplicateRejected{event});
            } else {
                Metrics::instance().inc_dedup_hit();
            }
            consumer_.commit(*msg);
            continue;
        }
        dedup_.mark_seen(event.device_id, event.seq);

        if (!seq_guard_.should_accept(event.device_id, event.seq)) {
            if (event_bus_) {
                event_bus_->publish(events::LocationStaleRejected{
                    event,
                    seq_guard_.current_seq(event.device_id)});
            } else {
                Metrics::instance().inc_seq_guard_reject();
            }
            consumer_.commit(*msg);
            continue;
        }

        if (event_bus_) {
            event_bus_->publish(events::LocationAccepted{event});
            consumer_.commit(*msg);
            continue;
        }

        if (!state_writer_.write(event)) {
            Metrics::instance().inc_truly_stale();
            consumer_.commit(*msg);
            continue;
        }

        Metrics::instance().inc_events_accepted();
        history_writer_.buffer(event);
        if (history_writer_.should_flush()) {
            history_writer_.flush();
            last_flush = std::chrono::steady_clock::now();
        }

        consumer_.commit(*msg);
    }

    history_writer_.flush();
}

} // namespace signalroute
