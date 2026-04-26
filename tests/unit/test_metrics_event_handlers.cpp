#include "common/composition/metrics_event_handlers.h"
#include "common/events/all_events.h"
#include "common/events/event_bus.h"
#include "common/metrics/metrics.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

signalroute::LocationEvent location() {
    signalroute::LocationEvent event;
    event.device_id = "dev-1";
    event.seq = 1;
    event.lat = 10.0;
    event.lon = 106.0;
    event.timestamp_ms = 1000;
    event.server_recv_ms = 1005;
    return event;
}

} // namespace

void test_gateway_processor_and_matching_events_update_metrics() {
    auto& metrics = signalroute::Metrics::instance();
    metrics.reset_for_test();

    signalroute::EventBus bus;
    signalroute::MetricsEventHandlers handlers(bus);
    handlers.wire();
    assert(handlers.subscription_count() == 14);

    auto event = location();
    bus.publish(signalroute::events::IngestBatchReceived{"dev-1", {event, event}, 1000});
    bus.publish(signalroute::events::IngestBatchRejected{"dev-1", "invalid", 2});
    bus.publish(signalroute::events::IngestEventPublished{event, "topic"});
    bus.publish(signalroute::events::LocationAccepted{event});
    bus.publish(signalroute::events::LocationDuplicateRejected{event});
    bus.publish(signalroute::events::LocationStaleRejected{event, 2});
    bus.publish(signalroute::events::StateWriteRejected{event, "stale"});
    bus.publish(signalroute::events::MatchRequestReceived{"req-1", "rider", 1.0, 2.0, 100.0, 1, 5000, "nearest"});
    bus.publish(signalroute::events::MatchCompleted{"req-1", {"agent-1"}, 17});
    bus.publish(signalroute::events::MatchFailed{"req-2", "none"});
    bus.publish(signalroute::events::MatchExpired{"req-3"});

    assert(metrics.counter_value("ingest_received_total") == 2);
    assert(metrics.counter_value("ingest_rejected_total", "reason=\"invalid\"") == 2);
    assert(metrics.counter_value("ingest_queued_total") == 1);
    assert(metrics.counter_value("events_accepted_total") == 1);
    assert(metrics.counter_value("dedup_hit_total") == 1);
    assert(metrics.counter_value("seq_guard_reject_total") == 1);
    assert(metrics.counter_value("truly_stale_total") == 1);
    assert(metrics.counter_value("match_requests_total") == 1);
    assert(metrics.counter_value("match_results_total", "status=\"completed\"") == 1);
    assert(metrics.counter_value("match_results_total", "status=\"failed\"") == 1);
    assert(metrics.counter_value("match_results_total", "status=\"expired\"") == 1);
    assert(metrics.observation_count("match_latency_ms") == 1);
    assert(metrics.observation_sum("match_latency_ms") == 17.0);
}

void test_geofence_metric_events_and_clear() {
    auto& metrics = signalroute::Metrics::instance();
    metrics.reset_for_test();

    signalroute::EventBus bus;
    signalroute::MetricsEventHandlers handlers(bus);
    handlers.wire();

    signalroute::GeofenceEventRecord record;
    record.device_id = "dev-1";
    record.fence_id = "fence-1";

    bus.publish(signalroute::events::GeofenceEntered{record});
    bus.publish(signalroute::events::GeofenceExited{record});
    bus.publish(signalroute::events::GeofenceDwellDetected{record});

    assert(metrics.counter_value("geofence_events_total", "type=\"ENTER\"") == 1);
    assert(metrics.counter_value("geofence_events_total", "type=\"EXIT\"") == 1);
    assert(metrics.counter_value("geofence_events_total", "type=\"DWELL\"") == 1);

    handlers.clear();
    bus.publish(signalroute::events::GeofenceEntered{record});
    assert(handlers.subscription_count() == 0);
    assert(metrics.counter_value("geofence_events_total", "type=\"ENTER\"") == 1);
}

int main() {
    std::cout << "test_metrics_event_handlers:\n";
    test_gateway_processor_and_matching_events_update_metrics();
    test_geofence_metric_events_and_clear();
    std::cout << "All metrics event handler tests passed.\n";
    return 0;
}
