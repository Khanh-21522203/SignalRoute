#include "common/clients/postgres_client.h"
#include "common/clients/redis_client.h"
#include "common/composition/composition_root.h"
#include "common/composition/metrics_event_handlers.h"
#include "common/kafka/kafka_producer.h"
#include "common/spatial/h3_index.h"
#include "processor/history_writer.h"
#include "processor/processor_event_handlers.h"
#include "processor/state_writer.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <utility>

namespace {

int64_t now_ms() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

signalroute::LocationEvent event(std::string device_id, uint64_t seq, double lat, double lon) {
    signalroute::LocationEvent out;
    out.device_id = std::move(device_id);
    out.seq = seq;
    out.lat = lat;
    out.lon = lon;
    out.timestamp_ms = now_ms();
    out.server_recv_ms = out.timestamp_ms;
    return out;
}

signalroute::ProcessorConfig processor_config() {
    signalroute::ProcessorConfig config;
    config.history_batch_size = 1;
    config.history_flush_interval_ms = 1000;
    return config;
}

struct Harness {
    Harness()
        : redis(signalroute::RedisConfig{})
        , pg(signalroute::PostGISConfig{})
        , dlq(signalroute::KafkaConfig{})
        , h3(7)
        , state_writer(redis, h3, 3600)
        , history_writer(pg, dlq, processor_config())
        , root(bus)
        , processor_handlers(bus, state_writer, history_writer)
        , metrics_handlers(bus)
    {}

    signalroute::EventBus bus;
    signalroute::RedisClient redis;
    signalroute::PostgresClient pg;
    signalroute::KafkaProducer dlq;
    signalroute::H3Index h3;
    signalroute::StateWriter state_writer;
    signalroute::HistoryWriter history_writer;
    signalroute::CompositionRoot root;
    signalroute::ProcessorEventHandlers processor_handlers;
    signalroute::MetricsEventHandlers metrics_handlers;
};

} // namespace

void test_location_accepted_flows_to_state_history_and_geofence_request() {
    Harness harness;
    int state_successes = 0;
    int history_written = 0;
    int geofence_requests = 0;
    signalroute::events::GeofenceEvaluationRequested geofence_request;

    auto state_sub = harness.bus.subscribe<signalroute::events::StateWriteSucceeded>(
        [&](const signalroute::events::StateWriteSucceeded&) { ++state_successes; });
    auto history_sub = harness.bus.subscribe<signalroute::events::TripHistoryWritten>(
        [&](const signalroute::events::TripHistoryWritten&) { ++history_written; });
    auto geofence_sub = harness.bus.subscribe<signalroute::events::GeofenceEvaluationRequested>(
        [&](const signalroute::events::GeofenceEvaluationRequested& event) {
            ++geofence_requests;
            geofence_request = event;
        });

    harness.root.wire_location_pipeline_observers();
    harness.processor_handlers.wire();
    harness.metrics_handlers.wire();

    const auto input = event("dev-1", 1, 10.8231, 106.6297);
    harness.bus.publish(signalroute::events::LocationAccepted{input});

    const auto state = harness.redis.get_device_state("dev-1");
    assert(state.has_value());
    assert(state->seq == 1);
    assert(harness.pg.trip_point_count() == 1);
    assert(state_successes == 1);
    assert(history_written == 1);
    assert(geofence_requests == 1);
    assert(geofence_request.device_id == "dev-1");
    assert(geofence_request.old_h3_cell == 0);
    assert(geofence_request.new_h3_cell == state->h3_cell);
    assert(harness.processor_handlers.subscription_count() == 2);
    assert(harness.metrics_handlers.subscription_count() == 7);
}

void test_stale_state_write_publishes_rejection_and_skips_history_when_requested_directly() {
    Harness harness;
    int state_rejections = 0;

    auto reject_sub = harness.bus.subscribe<signalroute::events::StateWriteRejected>(
        [&](const signalroute::events::StateWriteRejected& event) {
            ++state_rejections;
            assert(event.event.device_id == "dev-1");
        });

    harness.processor_handlers.wire();
    harness.bus.publish(signalroute::events::StateWriteRequested{event("dev-1", 2, 10.0, 106.0)});
    harness.bus.publish(signalroute::events::StateWriteRequested{event("dev-1", 1, 11.0, 107.0)});

    const auto state = harness.redis.get_device_state("dev-1");
    assert(state.has_value());
    assert(state->seq == 2);
    assert(state->lat == 10.0);
    assert(state_rejections == 1);
}

void test_handler_clear_unsubscribes() {
    Harness harness;
    harness.processor_handlers.wire();
    harness.processor_handlers.clear();

    harness.bus.publish(signalroute::events::StateWriteRequested{event("dev-1", 1, 10.0, 106.0)});

    assert(harness.processor_handlers.subscription_count() == 0);
    assert(!harness.redis.get_device_state("dev-1").has_value());
}

int main() {
    std::cout << "test_processor_event_handlers:\n";
    test_location_accepted_flows_to_state_history_and_geofence_request();
    test_stale_state_write_publishes_rejection_and_skips_history_when_requested_directly();
    test_handler_clear_unsubscribes();
    std::cout << "All processor event handler tests passed.\n";
    return 0;
}
