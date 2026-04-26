#include "common/clients/postgres_client.h"
#include "common/clients/redis_client.h"
#include "common/composition/composition_root.h"
#include "common/composition/metrics_event_handlers.h"
#include "common/kafka/kafka_consumer.h"
#include "common/kafka/kafka_producer.h"
#include "common/proto/geofence_payload_codec.h"
#include "common/spatial/h3_index.h"
#include "geofence/evaluator.h"
#include "geofence/fence_registry.h"
#include "geofence/geofence_event_handlers.h"
#include "processor/dedup_window.h"
#include "processor/history_writer.h"
#include "processor/processing_loop.h"
#include "processor/processor_event_handlers.h"
#include "processor/sequence_guard.h"
#include "processor/state_writer.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

std::string topic_name(const std::string& suffix) {
    return "test.processing.events." + suffix;
}

std::string payload(const std::string& device_id,
                    uint64_t seq,
                    int64_t timestamp_ms,
                    double lat,
                    double lon) {
    std::ostringstream out;
    out << device_id << ','
        << seq << ','
        << timestamp_ms << ','
        << (timestamp_ms + 5) << ','
        << lat << ','
        << lon;
    return out.str();
}

signalroute::ProcessorConfig processor_config() {
    signalroute::ProcessorConfig config;
    config.history_batch_size = 1;
    config.history_flush_interval_ms = 10;
    return config;
}

template <typename Predicate>
void wait_until(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    assert(predicate());
}

signalroute::GeofenceRule square_rule(std::string fence_id, int64_t h3_cell) {
    signalroute::GeofenceRule rule;
    rule.fence_id = std::move(fence_id);
    rule.name = "Depot";
    rule.h3_cells.insert(h3_cell);
    rule.polygon_vertices = {
        {0.0, 0.0},
        {0.0, 10.0},
        {10.0, 10.0},
        {10.0, 0.0},
    };
    rule.active = true;
    return rule;
}

signalroute::GeofenceEventRecord decode_geofence_payload(const std::string& payload) {
    auto decoded = signalroute::proto_boundary::decode_geofence_event_payload(payload);
    assert(decoded.is_ok());
    return decoded.value();
}

struct Harness {
    explicit Harness(std::string ingest_topic, std::string geofence_topic)
        : ingest_topic(std::move(ingest_topic))
        , geofence_topic(std::move(geofence_topic))
        , producer(signalroute::KafkaConfig{})
        , consumer(signalroute::KafkaConfig{}, {this->ingest_topic})
        , redis(signalroute::RedisConfig{})
        , pg(signalroute::PostGISConfig{})
        , dlq(signalroute::KafkaConfig{})
        , geofence_producer(signalroute::KafkaConfig{})
        , geofence_consumer(signalroute::KafkaConfig{}, {this->geofence_topic})
        , h3(7)
        , dedup(1000, 300)
        , seq_guard(redis)
        , state_writer(redis, h3, 3600)
        , history_writer(pg, dlq, processor_config())
        , evaluator(registry, redis, geofence_producer, pg, this->geofence_topic)
        , root(bus)
        , processor_handlers(bus, state_writer, history_writer)
        , geofence_handlers(bus, evaluator)
        , metrics_handlers(bus)
        , loop(consumer, dedup, seq_guard, state_writer, history_writer, processor_config(), bus)
    {}

    void wire() {
        root.wire_location_pipeline_observers();
        processor_handlers.wire();
        geofence_handlers.wire();
        metrics_handlers.wire();
    }

    void load_fences(std::vector<signalroute::GeofenceRule> rules) {
        pg.set_active_fences(std::move(rules));
        registry.load(pg);
    }

    void start() {
        worker = std::thread([&] { loop.run(stop); });
    }

    void stop_and_join() {
        stop.store(true);
        if (worker.joinable()) {
            worker.join();
        }
    }

    std::string ingest_topic;
    std::string geofence_topic;
    signalroute::EventBus bus;
    signalroute::KafkaProducer producer;
    signalroute::KafkaConsumer consumer;
    signalroute::RedisClient redis;
    signalroute::PostgresClient pg;
    signalroute::KafkaProducer dlq;
    signalroute::KafkaProducer geofence_producer;
    signalroute::KafkaConsumer geofence_consumer;
    signalroute::H3Index h3;
    signalroute::DedupWindow dedup;
    signalroute::SequenceGuard seq_guard;
    signalroute::StateWriter state_writer;
    signalroute::HistoryWriter history_writer;
    signalroute::FenceRegistry registry;
    signalroute::Evaluator evaluator;
    signalroute::CompositionRoot root;
    signalroute::ProcessorEventHandlers processor_handlers;
    signalroute::GeofenceEventHandlers geofence_handlers;
    signalroute::MetricsEventHandlers metrics_handlers;
    signalroute::ProcessingLoop loop;
    std::atomic<bool> stop{false};
    std::thread worker;
};

} // namespace

void test_processing_loop_can_drive_state_history_and_geofence_through_events() {
    Harness harness(topic_name("ingest"), topic_name("geofence"));
    const auto fence_cell = harness.h3.lat_lng_to_cell(5.0, 5.0);
    harness.load_fences({square_rule("fence-1", fence_cell)});
    harness.wire();

    harness.producer.produce(harness.ingest_topic, "dev-1", payload("dev-1", 1, 1000, 5.0, 5.0));
    harness.start();

    wait_until([&] {
        return harness.consumer.get_lag().front().second == 0 &&
               harness.redis.get_device_state("dev-1").has_value() &&
               harness.pg.trip_point_count() == 1 &&
               harness.pg.geofence_event_count() == 1;
    });
    harness.stop_and_join();

    const auto state = harness.redis.get_device_state("dev-1");
    assert(state.has_value());
    assert(state->seq == 1);
    assert(harness.redis.get_fence_state("dev-1", "fence-1") == signalroute::FenceState::INSIDE);

    const auto geofence_msg = harness.geofence_consumer.poll(0);
    assert(geofence_msg.has_value());
    const auto published = decode_geofence_payload(geofence_msg->payload);
    assert(published.device_id == "dev-1");
    assert(published.fence_id == "fence-1");
    assert(published.event_type == signalroute::GeofenceEventType::ENTER);
}

void test_duplicate_message_publishes_rejection_without_second_event_handler_write() {
    Harness harness(topic_name("dup_ingest"), topic_name("dup_geofence"));
    harness.wire();
    int duplicate_rejections = 0;
    auto duplicate_sub = harness.bus.subscribe<signalroute::events::LocationDuplicateRejected>(
        [&](const signalroute::events::LocationDuplicateRejected&) { ++duplicate_rejections; });

    harness.producer.produce(harness.ingest_topic, "dev-1", payload("dev-1", 1, 1000, 5.0, 5.0));
    harness.producer.produce(harness.ingest_topic, "dev-1", payload("dev-1", 1, 1000, 5.0, 5.0));
    harness.start();

    wait_until([&] { return harness.consumer.get_lag().front().second == 0; });
    harness.stop_and_join();

    assert(duplicate_rejections == 1);
    assert(harness.pg.trip_point_count() == 1);
}

void test_stale_message_event_carries_current_sequence() {
    Harness harness(topic_name("stale_ingest"), topic_name("stale_geofence"));
    harness.wire();
    uint64_t current_seq = 0;
    auto stale_sub = harness.bus.subscribe<signalroute::events::LocationStaleRejected>(
        [&](const signalroute::events::LocationStaleRejected& event) {
            current_seq = event.current_seq;
        });

    harness.producer.produce(harness.ingest_topic, "dev-1", payload("dev-1", 2, 2000, 5.0, 5.0));
    harness.producer.produce(harness.ingest_topic, "dev-1", payload("dev-1", 1, 1000, 5.0, 5.0));
    harness.start();

    wait_until([&] { return harness.consumer.get_lag().front().second == 0; });
    harness.stop_and_join();

    assert(current_seq == 2);
    assert(harness.pg.trip_point_count() == 1);
}

int main() {
    std::cout << "test_processing_event_pipeline:\n";
    test_processing_loop_can_drive_state_history_and_geofence_through_events();
    test_duplicate_message_publishes_rejection_without_second_event_handler_write();
    test_stale_message_event_carries_current_sequence();
    std::cout << "All processing event pipeline tests passed.\n";
    return 0;
}
