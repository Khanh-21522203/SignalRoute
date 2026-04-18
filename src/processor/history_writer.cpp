#include "history_writer.h"
#include "../common/metrics/metrics.h"

namespace signalroute {

HistoryWriter::HistoryWriter(PostgresClient& pg, KafkaProducer& dlq, const ProcessorConfig& cfg)
    : pg_(pg), dlq_(dlq), config_(cfg) {
    buffer_.reserve(cfg.history_batch_size);
}

void HistoryWriter::buffer(const LocationEvent& event) {
    std::lock_guard lock(mu_);
    buffer_.push_back(event);
    Metrics::instance().set_history_buffer_size(buffer_.size());
}

void HistoryWriter::flush() {
    std::vector<LocationEvent> batch;
    {
        std::lock_guard lock(mu_);
        if (buffer_.empty()) return;
        batch.swap(buffer_);
        buffer_.reserve(config_.history_batch_size);
    }

    // TODO: Implement batch insert with DLQ fallback
    //
    //   try {
    //       pg_.batch_insert_trip_points(batch);
    //   } catch (const std::exception& e) {
    //       Metrics::instance().inc_postgis_write_errors();
    //       // Send to DLQ for later replay
    //       for (const auto& event : batch) {
    //           std::string payload = /* serialize event */;
    //           dlq_.produce(config_.dlq_topic, event.device_id, payload);
    //       }
    //   }
}

size_t HistoryWriter::buffer_size() const {
    std::lock_guard lock(mu_);
    return buffer_.size();
}

bool HistoryWriter::should_flush() const {
    std::lock_guard lock(mu_);
    return static_cast<int>(buffer_.size()) >= config_.history_batch_size;
}

} // namespace signalroute
