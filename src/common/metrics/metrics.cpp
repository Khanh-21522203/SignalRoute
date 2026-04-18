#include "metrics.h"

// TODO: #include <prometheus/counter.h>
// TODO: #include <prometheus/gauge.h>
// TODO: #include <prometheus/histogram.h>
// TODO: #include <prometheus/registry.h>
// TODO: #include <prometheus/text_serializer.h>

namespace signalroute {

Metrics& Metrics::instance() {
    static Metrics instance;
    return instance;
}

void Metrics::initialize(const std::string& /*addr*/, int /*port*/, const std::string& /*path*/) {
    // TODO: Create prometheus-cpp registry
    // TODO: Register all counters, gauges, and histograms
    // TODO: Start HTTP exposer on addr:port/path
}

// ── Stub implementations ──
// TODO: Replace all stubs with real prometheus-cpp metric operations

void Metrics::inc_ingest_received(int64_t) {}
void Metrics::inc_ingest_rejected(const std::string&, int64_t) {}
void Metrics::inc_ingest_queued(int64_t) {}
void Metrics::inc_dedup_hit(int64_t) {}
void Metrics::inc_seq_guard_reject(int64_t) {}
void Metrics::inc_events_accepted(int64_t) {}
void Metrics::inc_truly_stale(int64_t) {}
void Metrics::observe_redis_write_latency(double) {}
void Metrics::observe_postgis_write_latency(double) {}
void Metrics::set_history_buffer_size(int64_t) {}
void Metrics::set_kafka_consumer_lag(int, int64_t) {}
void Metrics::observe_nearby_latency(double) {}
void Metrics::observe_trip_query_latency(double) {}
void Metrics::observe_geofence_eval_latency(double) {}
void Metrics::inc_geofence_event(const std::string&) {}
void Metrics::inc_match_request() {}
void Metrics::inc_match_result(const std::string&) {}
void Metrics::observe_match_latency(double) {}
void Metrics::set_redis_pool_utilization(double) {}
void Metrics::set_postgis_pool_utilization(double) {}
void Metrics::inc_kafka_publish_errors() {}
void Metrics::inc_redis_write_errors() {}
void Metrics::inc_postgis_write_errors() {}

std::string Metrics::export_text() const {
    // TODO: Use prometheus::TextSerializer to export all registered metrics
    return "# SignalRoute metrics (not yet implemented)\n";
}

} // namespace signalroute
