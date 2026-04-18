#include "processing_loop.h"
#include "../common/kafka/kafka_consumer.h"
#include "dedup_window.h"
#include "sequence_guard.h"
#include "state_writer.h"
#include "history_writer.h"
#include "../common/metrics/metrics.h"

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

void ProcessingLoop::run(std::atomic<bool>& should_stop) {
    // TODO: Implement the main processing loop
    //
    //   auto flush_timer = steady_clock::now();
    //
    //   while (!should_stop.load()) {
    //       // 1. Poll Kafka
    //       auto msg = consumer_.poll(100);
    //       if (!msg) {
    //           // Check flush timer
    //           if (history_writer_.buffer_size() > 0 &&
    //               elapsed(flush_timer) > config_.history_flush_interval_ms) {
    //               history_writer_.flush();
    //               flush_timer = steady_clock::now();
    //           }
    //           continue;
    //       }
    //
    //       // 2. Deserialize
    //       auto event = deserialize_location_event(msg->payload);
    //
    //       // 3. Dedup
    //       if (dedup_.is_duplicate(event.device_id, event.seq)) {
    //           Metrics::instance().inc_dedup_hit();
    //           consumer_.commit(*msg);
    //           continue;
    //       }
    //       dedup_.mark_seen(event.device_id, event.seq);
    //
    //       // 4. Sequence guard
    //       if (!seq_guard_.should_accept(event.device_id, event.seq)) {
    //           Metrics::instance().inc_seq_guard_reject();
    //           consumer_.commit(*msg);
    //           continue;
    //       }
    //
    //       // 5. State write
    //       bool accepted = state_writer_.write(event);
    //       if (!accepted) {
    //           Metrics::instance().inc_truly_stale();
    //           consumer_.commit(*msg);
    //           continue;
    //       }
    //       Metrics::instance().inc_events_accepted();
    //
    //       // 6. Buffer for history
    //       history_writer_.buffer(event);
    //       if (history_writer_.should_flush()) {
    //           history_writer_.flush();
    //           flush_timer = steady_clock::now();
    //       }
    //
    //       // 7. TODO: Notify geofence engine
    //
    //       // 8. Commit offset
    //       consumer_.commit(*msg);
    //   }
    //
    //   // Drain: flush remaining history on shutdown
    //   history_writer_.flush();

    (void)should_stop; // suppress unused warning
}

} // namespace signalroute
