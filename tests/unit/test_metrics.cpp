#include "common/metrics/metrics.h"

#include <cassert>
#include <iostream>
#include <string>

void test_counters_gauges_and_observations_are_recorded() {
    auto& metrics = signalroute::Metrics::instance();
    metrics.reset_for_test();

    metrics.inc_ingest_received(3);
    metrics.inc_ingest_received(2);
    metrics.inc_ingest_rejected("invalid", 4);
    metrics.inc_geofence_event("ENTER");
    metrics.inc_match_result("completed");
    metrics.set_history_buffer_size(7);
    metrics.set_kafka_consumer_lag(0, 9);
    metrics.observe_postgis_write_latency(2.5);
    metrics.observe_postgis_write_latency(3.5);

    assert(metrics.counter_value("ingest_received_total") == 5);
    assert(metrics.counter_value("ingest_rejected_total", "reason=\"invalid\"") == 4);
    assert(metrics.counter_value("geofence_events_total", "type=\"ENTER\"") == 1);
    assert(metrics.counter_value("match_results_total", "status=\"completed\"") == 1);
    assert(metrics.gauge_value("history_buffer_size") == 7.0);
    assert(metrics.gauge_value("kafka_consumer_lag{partition=\"0\"}") == 9.0);
    assert(metrics.observation_count("postgis_write_latency_ms") == 2);
    assert(metrics.observation_sum("postgis_write_latency_ms") == 6.0);
}

void test_export_text_contains_fallback_metrics() {
    auto& metrics = signalroute::Metrics::instance();
    metrics.reset_for_test();

    metrics.inc_kafka_publish_errors();
    metrics.set_redis_pool_utilization(0.5);
    metrics.observe_nearby_latency(1.25);

    const auto text = metrics.export_text();
    assert(text.find("# SignalRoute metrics fallback") != std::string::npos);
    assert(text.find("kafka_publish_errors_total 1") != std::string::npos);
    assert(text.find("redis_pool_utilization 0.5") != std::string::npos);
    assert(text.find("nearby_latency_ms_count 1") != std::string::npos);
    assert(text.find("nearby_latency_ms_sum 1.25") != std::string::npos);
}

int main() {
    std::cout << "test_metrics:\n";
    test_counters_gauges_and_observations_are_recorded();
    test_export_text_contains_fallback_metrics();
    std::cout << "All metrics tests passed.\n";
    return 0;
}
