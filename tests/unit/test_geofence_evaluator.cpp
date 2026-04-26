#include "common/clients/postgres_client.h"
#include "common/clients/redis_client.h"
#include "common/kafka/kafka_consumer.h"
#include "common/kafka/kafka_producer.h"
#include "common/proto/geofence_payload_codec.h"
#include "geofence/evaluator.h"
#include "geofence/fence_registry.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string topic_name(const std::string& suffix) {
    return "test.geofence." + suffix;
}

signalroute::GeofenceRule square_rule(std::string fence_id,
                                      std::string name,
                                      int64_t h3_cell) {
    signalroute::GeofenceRule rule;
    rule.fence_id = std::move(fence_id);
    rule.name = std::move(name);
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

struct Harness {
    explicit Harness(std::string topic)
        : topic(std::move(topic))
        , redis(signalroute::RedisConfig{})
        , pg(signalroute::PostGISConfig{})
        , producer(signalroute::KafkaConfig{})
        , consumer(signalroute::KafkaConfig{}, {this->topic})
        , evaluator(registry, redis, producer, pg, this->topic)
    {}

    void load_fences(std::vector<signalroute::GeofenceRule> rules) {
        pg.set_active_fences(std::move(rules));
        registry.load(pg);
    }

    std::string topic;
    signalroute::RedisClient redis;
    signalroute::PostgresClient pg;
    signalroute::KafkaProducer producer;
    signalroute::KafkaConsumer consumer;
    signalroute::FenceRegistry registry;
    signalroute::Evaluator evaluator;
};

signalroute::GeofenceEventRecord decode_event_payload(const std::string& payload) {
    auto decoded = signalroute::proto_boundary::decode_geofence_event_payload(payload);
    assert(decoded.is_ok());
    return decoded.value();
}

} // namespace

void test_enter_transition_sets_state_audits_and_publishes() {
    Harness harness(topic_name("enter"));
    harness.load_fences({square_rule("fence-1", "Depot", 100)});

    harness.evaluator.evaluate("dev-1", 0, 100, 5.0, 5.0, 1000);

    assert(harness.redis.get_fence_state("dev-1", "fence-1") == signalroute::FenceState::INSIDE);
    assert(harness.pg.geofence_event_count() == 1);

    const auto events = harness.pg.geofence_events();
    assert(events.size() == 1);
    assert(events.front().device_id == "dev-1");
    assert(events.front().fence_id == "fence-1");
    assert(events.front().fence_name == "Depot");
    assert(events.front().event_type == signalroute::GeofenceEventType::ENTER);
    assert(events.front().event_ts_ms == 1000);

    const auto msg = harness.consumer.poll(0);
    assert(msg.has_value());
    assert(msg->key == "dev-1");
    const auto published = decode_event_payload(msg->payload);
    assert(published.device_id == "dev-1");
    assert(published.fence_id == "fence-1");
    assert(published.event_type == signalroute::GeofenceEventType::ENTER);
    assert(published.event_ts_ms == 1000);
}

void test_inside_to_inside_does_not_emit_duplicate_enter() {
    Harness harness(topic_name("no_duplicate_enter"));
    harness.load_fences({square_rule("fence-1", "Depot", 100)});

    harness.evaluator.evaluate("dev-1", 0, 100, 5.0, 5.0, 1000);
    harness.evaluator.evaluate("dev-1", 100, 100, 6.0, 6.0, 2000);

    assert(harness.redis.get_fence_state("dev-1", "fence-1") == signalroute::FenceState::INSIDE);
    assert(harness.pg.geofence_event_count() == 1);
}

void test_exit_transition_sets_outside_audits_and_publishes() {
    Harness harness(topic_name("exit"));
    harness.load_fences({square_rule("fence-1", "Depot", 100)});

    harness.redis.set_fence_state("dev-1", "fence-1", signalroute::FenceState::INSIDE, 1000);
    harness.evaluator.evaluate("dev-1", 100, 100, 20.0, 20.0, 2000);

    assert(harness.redis.get_fence_state("dev-1", "fence-1") == signalroute::FenceState::OUTSIDE);
    assert(harness.pg.geofence_event_count() == 1);

    const auto events = harness.pg.geofence_events();
    assert(events.front().event_type == signalroute::GeofenceEventType::EXIT);

    const auto msg = harness.consumer.poll(0);
    assert(msg.has_value());
    const auto published = decode_event_payload(msg->payload);
    assert(published.device_id == "dev-1");
    assert(published.fence_id == "fence-1");
    assert(published.event_type == signalroute::GeofenceEventType::EXIT);
    assert(published.event_ts_ms == 2000);
}

void test_old_cell_candidates_allow_exit_when_new_cell_has_no_fence() {
    Harness harness(topic_name("old_cell_exit"));
    harness.load_fences({square_rule("fence-1", "Depot", 100)});

    harness.redis.set_fence_state("dev-1", "fence-1", signalroute::FenceState::DWELL, 1000);
    harness.evaluator.evaluate("dev-1", 100, 200, 20.0, 20.0, 2000);

    assert(harness.redis.get_fence_state("dev-1", "fence-1") == signalroute::FenceState::OUTSIDE);
    assert(harness.pg.geofence_event_count() == 1);
    assert(harness.pg.geofence_events().front().event_type == signalroute::GeofenceEventType::EXIT);
}

void test_outside_to_outside_does_not_emit() {
    Harness harness(topic_name("outside_noop"));
    harness.load_fences({square_rule("fence-1", "Depot", 100)});

    harness.evaluator.evaluate("dev-1", 0, 100, 20.0, 20.0, 1000);

    assert(!harness.redis.get_fence_state("dev-1", "fence-1").has_value());
    assert(harness.pg.geofence_event_count() == 0);
    assert(!harness.consumer.poll(0).has_value());
}

int main() {
    std::cout << "test_geofence_evaluator:\n";
    test_enter_transition_sets_state_audits_and_publishes();
    test_inside_to_inside_does_not_emit_duplicate_enter();
    test_exit_transition_sets_outside_audits_and_publishes();
    test_old_cell_candidates_allow_exit_when_new_cell_has_no_fence();
    test_outside_to_outside_does_not_emit();
    std::cout << "All geofence evaluator tests passed.\n";
    return 0;
}
