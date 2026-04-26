#include "common/clients/postgres_client.h"
#include "common/clients/redis_client.h"
#include "common/events/all_events.h"
#include "common/events/event_bus.h"
#include "common/kafka/kafka_consumer.h"
#include "common/kafka/kafka_producer.h"
#include "common/proto/geofence_payload_codec.h"
#include "geofence/evaluator.h"
#include "geofence/fence_registry.h"
#include "geofence/geofence_event_handlers.h"

#include <cassert>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string topic_name(const std::string& suffix) {
    return "test.geofence.events." + suffix;
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

struct Harness {
    explicit Harness(std::string topic)
        : topic(std::move(topic))
        , redis(signalroute::RedisConfig{})
        , pg(signalroute::PostGISConfig{})
        , producer(signalroute::KafkaConfig{})
        , consumer(signalroute::KafkaConfig{}, {this->topic})
        , evaluator(registry, redis, producer, pg, this->topic)
        , handlers(bus, evaluator)
    {}

    void load_fences(std::vector<signalroute::GeofenceRule> rules) {
        pg.set_active_fences(std::move(rules));
        registry.load(pg);
    }

    std::string topic;
    signalroute::EventBus bus;
    signalroute::RedisClient redis;
    signalroute::PostgresClient pg;
    signalroute::KafkaProducer producer;
    signalroute::KafkaConsumer consumer;
    signalroute::FenceRegistry registry;
    signalroute::Evaluator evaluator;
    signalroute::GeofenceEventHandlers handlers;
};

signalroute::GeofenceEventRecord decode_event_payload(const std::string& payload) {
    auto decoded = signalroute::proto_boundary::decode_geofence_event_payload(payload);
    assert(decoded.is_ok());
    return decoded.value();
}

} // namespace

void test_geofence_evaluation_event_invokes_evaluator() {
    Harness harness(topic_name("enter"));
    harness.load_fences({square_rule("fence-1", 100)});
    harness.handlers.wire();

    harness.bus.publish(signalroute::events::GeofenceEvaluationRequested{
        "dev-1", 0, 100, 5.0, 5.0, 1000});

    assert(harness.handlers.subscription_count() == 1);
    assert(harness.redis.get_fence_state("dev-1", "fence-1") == signalroute::FenceState::INSIDE);
    assert(harness.pg.geofence_event_count() == 1);
    const auto msg = harness.consumer.poll(0);
    assert(msg.has_value());
    const auto published = decode_event_payload(msg->payload);
    assert(published.device_id == "dev-1");
    assert(published.fence_id == "fence-1");
    assert(published.event_type == signalroute::GeofenceEventType::ENTER);
}

void test_clear_unsubscribes_geofence_handler() {
    Harness harness(topic_name("clear"));
    harness.load_fences({square_rule("fence-1", 100)});
    harness.handlers.wire();
    harness.handlers.clear();

    harness.bus.publish(signalroute::events::GeofenceEvaluationRequested{
        "dev-1", 0, 100, 5.0, 5.0, 1000});

    assert(harness.handlers.subscription_count() == 0);
    assert(!harness.redis.get_fence_state("dev-1", "fence-1").has_value());
    assert(harness.pg.geofence_event_count() == 0);
}

int main() {
    std::cout << "test_geofence_event_handlers:\n";
    test_geofence_evaluation_event_invokes_evaluator();
    test_clear_unsubscribes_geofence_handler();
    std::cout << "All geofence event handler tests passed.\n";
    return 0;
}
