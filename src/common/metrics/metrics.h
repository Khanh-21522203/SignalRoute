#pragma once

/**
 * SignalRoute — Prometheus Metrics
 *
 * Centralized metric registration and export for all services.
 * Each service updates the singleton registry. The runtime metrics exporter
 * serves the current snapshot over the configured scrape endpoint.
 *
 * Dependencies: prometheus-cpp
 */

#include <string>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace signalroute {

/**
 * Singleton metrics registry.
 *
 * TODO: Implement using prometheus-cpp library.
 *       Each counter/gauge/histogram is registered once and
 *       accessed by reference throughout the service lifetime.
 */
class Metrics {
public:
    static Metrics& instance();

    /// Initialize the registry. Call once at startup.
    void initialize(const std::string& addr, int port, const std::string& path);

    // ── Ingestion Gateway ──
    void inc_ingest_received(int64_t count = 1);
    void inc_ingest_rejected(const std::string& reason, int64_t count = 1);
    void inc_ingest_queued(int64_t count = 1);

    // ── Location Processor ──
    void inc_dedup_hit(int64_t count = 1);
    void inc_seq_guard_reject(int64_t count = 1);
    void inc_events_accepted(int64_t count = 1);
    void inc_truly_stale(int64_t count = 1);
    void observe_redis_write_latency(double ms);
    void observe_postgis_write_latency(double ms);
    void set_history_buffer_size(int64_t size);
    void set_kafka_consumer_lag(int partition, int64_t lag);

    // ── Query Service ──
    void observe_nearby_latency(double ms);
    void observe_trip_query_latency(double ms);

    // ── Geofence Engine ──
    void observe_geofence_eval_latency(double ms);
    void inc_geofence_event(const std::string& type);  // ENTER, EXIT, DWELL

    // ── Matching Server ──
    void inc_match_request();
    void inc_match_result(const std::string& status);
    void observe_match_latency(double ms);

    // ── General ──
    void set_redis_pool_utilization(double fraction);
    void set_postgis_pool_utilization(double fraction);
    void inc_kafka_publish_errors();
    void inc_redis_write_errors();
    void inc_postgis_write_errors();

    /// Export all metrics as Prometheus text format.
    std::string export_text() const;

    /// Fallback/test inspection helpers. Prometheus integration replaces export internals later.
    void reset_for_test();
    int64_t counter_value(const std::string& name) const;
    int64_t counter_value(const std::string& name, const std::string& label) const;
    double gauge_value(const std::string& name) const;
    int64_t observation_count(const std::string& name) const;
    double observation_sum(const std::string& name) const;

private:
    Metrics() = default;

    void inc_counter(const std::string& name, int64_t count = 1);
    void inc_labeled_counter(const std::string& name, const std::string& label, int64_t count = 1);
    void set_gauge(const std::string& name, double value);
    void observe_value(const std::string& name, double value);

    mutable std::mutex mu_;
    std::unordered_map<std::string, int64_t> counters_;
    std::unordered_map<std::string, double> gauges_;

    struct Observation {
        int64_t count = 0;
        double sum = 0.0;
    };
    std::unordered_map<std::string, Observation> observations_;

    // TODO: Add prometheus-cpp registry, counter, gauge, histogram members
    // prometheus::Registry registry_;
};

} // namespace signalroute
