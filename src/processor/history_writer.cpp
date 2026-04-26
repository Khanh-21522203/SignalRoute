#include "history_writer.h"
#include "../common/metrics/metrics.h"

#include <chrono>
#include <exception>
#include <sstream>

namespace signalroute {
namespace {

std::string serialize_dlq_payload(const LocationEvent& event) {
    std::ostringstream out;
    out << event.device_id << ','
        << event.seq << ','
        << event.timestamp_ms << ','
        << event.server_recv_ms << ','
        << event.lat << ','
        << event.lon;
    return out.str();
}

} // namespace

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

    const auto start = std::chrono::steady_clock::now();
    try {
        pg_.batch_insert_trip_points(batch);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        Metrics::instance().observe_postgis_write_latency(
            std::chrono::duration<double, std::milli>(elapsed).count());
        Metrics::instance().set_history_buffer_size(0);
    } catch (const std::exception&) {
        Metrics::instance().inc_postgis_write_errors();
        for (const auto& event : batch) {
            dlq_.produce("tm.location.dlq", event.device_id, serialize_dlq_payload(event));
        }
    }
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
