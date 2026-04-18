#pragma once

/**
 * SignalRoute — Processing Loop
 *
 * Main Kafka poll → process → commit loop for the Location Processor.
 * Orchestrates DedupWindow, SequenceGuard, StateWriter, HistoryWriter,
 * and Geofence Engine notification.
 */

#include "../common/config/config.h"
#include <atomic>
#include <memory>

namespace signalroute {

class KafkaConsumer;
class DedupWindow;
class SequenceGuard;
class StateWriter;
class HistoryWriter;

class ProcessingLoop {
public:
    ProcessingLoop(
        KafkaConsumer& consumer,
        DedupWindow& dedup,
        SequenceGuard& seq_guard,
        StateWriter& state_writer,
        HistoryWriter& history_writer,
        const ProcessorConfig& config
    );

    /**
     * Run the processing loop. Blocks until should_stop is set.
     *
     * Per-message steps:
     *   1. Deserialize LocationEvent from Kafka payload
     *   2. Dedup check → if duplicate, skip + ack
     *   3. Sequence guard → if stale, skip + ack + metric
     *   4. State write (Redis CAS + H3 index)
     *   5. Buffer for history write
     *   6. If buffer full or timer expired → flush history
     *   7. Notify geofence engine (if cell changed)
     *   8. Commit Kafka offset
     *
     * TODO: Implement the full loop
     */
    void run(std::atomic<bool>& should_stop);

private:
    KafkaConsumer& consumer_;
    DedupWindow& dedup_;
    SequenceGuard& seq_guard_;
    StateWriter& state_writer_;
    HistoryWriter& history_writer_;
    ProcessorConfig config_;
};

} // namespace signalroute
