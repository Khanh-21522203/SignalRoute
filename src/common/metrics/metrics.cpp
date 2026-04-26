#include "metrics.h"

#include <algorithm>
#include <sstream>
#include <vector>

// TODO: #include <prometheus/counter.h>
// TODO: #include <prometheus/gauge.h>
// TODO: #include <prometheus/histogram.h>
// TODO: #include <prometheus/registry.h>
// TODO: #include <prometheus/text_serializer.h>

namespace signalroute {
namespace {

std::string labeled_key(const std::string& name, const std::string& label) {
    return name + "{" + label + "}";
}

std::string label_pair(const std::string& key, const std::string& value) {
    return key + "=\"" + value + "\"";
}

template <typename Map>
std::vector<std::string> sorted_keys(const Map& map) {
    std::vector<std::string> keys;
    keys.reserve(map.size());
    for (const auto& [key, _] : map) {
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

} // namespace

Metrics& Metrics::instance() {
    static Metrics instance;
    return instance;
}

void Metrics::initialize(const std::string& /*addr*/, int /*port*/, const std::string& /*path*/) {
    // TODO: Create prometheus-cpp registry
    // TODO: Register all counters, gauges, and histograms
    // TODO: Start HTTP exposer on addr:port/path
}

void Metrics::inc_ingest_received(int64_t count) { inc_counter("ingest_received_total", count); }
void Metrics::inc_ingest_rejected(const std::string& reason, int64_t count) {
    inc_labeled_counter("ingest_rejected_total", label_pair("reason", reason), count);
}
void Metrics::inc_ingest_queued(int64_t count) { inc_counter("ingest_queued_total", count); }
void Metrics::inc_dedup_hit(int64_t count) { inc_counter("dedup_hit_total", count); }
void Metrics::inc_seq_guard_reject(int64_t count) { inc_counter("seq_guard_reject_total", count); }
void Metrics::inc_events_accepted(int64_t count) { inc_counter("events_accepted_total", count); }
void Metrics::inc_truly_stale(int64_t count) { inc_counter("truly_stale_total", count); }
void Metrics::observe_redis_write_latency(double ms) { observe_value("redis_write_latency_ms", ms); }
void Metrics::observe_postgis_write_latency(double ms) { observe_value("postgis_write_latency_ms", ms); }
void Metrics::set_history_buffer_size(int64_t size) { set_gauge("history_buffer_size", static_cast<double>(size)); }
void Metrics::set_kafka_consumer_lag(int partition, int64_t lag) {
    set_gauge(labeled_key("kafka_consumer_lag", label_pair("partition", std::to_string(partition))),
              static_cast<double>(lag));
}
void Metrics::observe_nearby_latency(double ms) { observe_value("nearby_latency_ms", ms); }
void Metrics::observe_trip_query_latency(double ms) { observe_value("trip_query_latency_ms", ms); }
void Metrics::observe_geofence_eval_latency(double ms) { observe_value("geofence_eval_latency_ms", ms); }
void Metrics::inc_geofence_event(const std::string& type) {
    inc_labeled_counter("geofence_events_total", label_pair("type", type), 1);
}
void Metrics::inc_match_request() { inc_counter("match_requests_total"); }
void Metrics::inc_match_result(const std::string& status) {
    inc_labeled_counter("match_results_total", label_pair("status", status), 1);
}
void Metrics::observe_match_latency(double ms) { observe_value("match_latency_ms", ms); }
void Metrics::set_redis_pool_utilization(double fraction) { set_gauge("redis_pool_utilization", fraction); }
void Metrics::set_postgis_pool_utilization(double fraction) { set_gauge("postgis_pool_utilization", fraction); }
void Metrics::inc_kafka_publish_errors() { inc_counter("kafka_publish_errors_total"); }
void Metrics::inc_redis_write_errors() { inc_counter("redis_write_errors_total"); }
void Metrics::inc_postgis_write_errors() { inc_counter("postgis_write_errors_total"); }

std::string Metrics::export_text() const {
    std::lock_guard lock(mu_);
    std::ostringstream out;
    out << "# SignalRoute metrics fallback\n";
    for (const auto& key : sorted_keys(counters_)) {
        out << key << ' ' << counters_.at(key) << '\n';
    }
    for (const auto& key : sorted_keys(gauges_)) {
        out << key << ' ' << gauges_.at(key) << '\n';
    }
    for (const auto& key : sorted_keys(observations_)) {
        const auto& observation = observations_.at(key);
        out << key << "_count " << observation.count << '\n';
        out << key << "_sum " << observation.sum << '\n';
    }
    return out.str();
}

void Metrics::reset_for_test() {
    std::lock_guard lock(mu_);
    counters_.clear();
    gauges_.clear();
    observations_.clear();
}

int64_t Metrics::counter_value(const std::string& name) const {
    std::lock_guard lock(mu_);
    auto it = counters_.find(name);
    return it == counters_.end() ? 0 : it->second;
}

int64_t Metrics::counter_value(const std::string& name, const std::string& label) const {
    std::lock_guard lock(mu_);
    auto it = counters_.find(labeled_key(name, label));
    return it == counters_.end() ? 0 : it->second;
}

double Metrics::gauge_value(const std::string& name) const {
    std::lock_guard lock(mu_);
    auto it = gauges_.find(name);
    return it == gauges_.end() ? 0.0 : it->second;
}

int64_t Metrics::observation_count(const std::string& name) const {
    std::lock_guard lock(mu_);
    auto it = observations_.find(name);
    return it == observations_.end() ? 0 : it->second.count;
}

double Metrics::observation_sum(const std::string& name) const {
    std::lock_guard lock(mu_);
    auto it = observations_.find(name);
    return it == observations_.end() ? 0.0 : it->second.sum;
}

void Metrics::inc_counter(const std::string& name, int64_t count) {
    std::lock_guard lock(mu_);
    counters_[name] += count;
}

void Metrics::inc_labeled_counter(const std::string& name, const std::string& label, int64_t count) {
    std::lock_guard lock(mu_);
    counters_[labeled_key(name, label)] += count;
}

void Metrics::set_gauge(const std::string& name, double value) {
    std::lock_guard lock(mu_);
    gauges_[name] = value;
}

void Metrics::observe_value(const std::string& name, double value) {
    std::lock_guard lock(mu_);
    auto& observation = observations_[name];
    ++observation.count;
    observation.sum += value;
}

} // namespace signalroute
