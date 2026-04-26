#include "common/clients/postgres_client.h"
#include "common/clients/redis_client.h"
#include "common/kafka/kafka_consumer.h"
#include "common/kafka/kafka_producer.h"
#include "geofence/dwell_checker.h"
#include "geofence/fence_registry.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string topic_name(const std::string& suffix) {
    return "test.dwell." + suffix;
}

signalroute::GeofenceRule rule(std::string fence_id, int64_t h3_cell, int dwell_threshold_s) {
    signalroute::GeofenceRule fence;
    fence.fence_id = std::move(fence_id);
    fence.name = "Depot";
    fence.h3_cells.insert(h3_cell);
    fence.dwell_threshold_s = dwell_threshold_s;
    fence.active = true;
    fence.polygon_vertices = {
        {0.0, 0.0},
        {0.0, 10.0},
        {10.0, 10.0},
        {10.0, 0.0},
    };
    return fence;
}

struct Harness {
    explicit Harness(std::string topic)
        : topic(std::move(topic))
        , redis(signalroute::RedisConfig{})
        , pg(signalroute::PostGISConfig{})
        , producer(signalroute::KafkaConfig{})
        , consumer(signalroute::KafkaConfig{}, {this->topic})
        , checker(redis, producer, pg, registry, config, this->topic)
    {}

    void load_fences(std::vector<signalroute::GeofenceRule> fences) {
        pg.set_active_fences(std::move(fences));
        registry.load(pg);
    }

    std::string topic;
    signalroute::RedisClient redis;
    signalroute::PostgresClient pg;
    signalroute::KafkaProducer producer;
    signalroute::KafkaConsumer consumer;
    signalroute::FenceRegistry registry;
    signalroute::GeofenceConfig config;
    signalroute::DwellChecker checker;
};

} // namespace

void test_inside_state_transitions_to_dwell_after_threshold() {
    Harness harness(topic_name("transition"));
    harness.load_fences({rule("fence-1", 100, 5)});
    harness.redis.set_fence_state("dev-1", "fence-1", signalroute::FenceState::INSIDE, 1000);

    const int transitioned = harness.checker.check_once(6000);

    assert(transitioned == 1);
    assert(harness.redis.get_fence_state("dev-1", "fence-1") == signalroute::FenceState::DWELL);
    const auto record = harness.redis.get_fence_state_record("dev-1", "fence-1");
    assert(record.has_value());
    assert(record->entered_at_ms == 1000);
    assert(record->updated_at_ms == 6000);

    const auto events = harness.pg.geofence_events();
    assert(events.size() == 1);
    assert(events.front().device_id == "dev-1");
    assert(events.front().fence_id == "fence-1");
    assert(events.front().event_type == signalroute::GeofenceEventType::DWELL);
    assert(events.front().inside_duration_s == 5);

    const auto msg = harness.consumer.poll(0);
    assert(msg.has_value());
    assert(msg->key == "dev-1");
    assert(msg->payload.find("DWELL") != std::string::npos);
}

void test_inside_state_before_threshold_does_not_transition() {
    Harness harness(topic_name("before_threshold"));
    harness.load_fences({rule("fence-1", 100, 5)});
    harness.redis.set_fence_state("dev-1", "fence-1", signalroute::FenceState::INSIDE, 1000);

    const int transitioned = harness.checker.check_once(5999);

    assert(transitioned == 0);
    assert(harness.redis.get_fence_state("dev-1", "fence-1") == signalroute::FenceState::INSIDE);
    assert(harness.pg.geofence_event_count() == 0);
    assert(!harness.consumer.poll(0).has_value());
}

void test_dwell_state_is_not_emitted_twice() {
    Harness harness(topic_name("no_duplicate"));
    harness.load_fences({rule("fence-1", 100, 5)});
    harness.redis.set_fence_state("dev-1", "fence-1", signalroute::FenceState::INSIDE, 1000);

    assert(harness.checker.check_once(6000) == 1);
    assert(harness.checker.check_once(7000) == 0);

    assert(harness.redis.get_fence_state("dev-1", "fence-1") == signalroute::FenceState::DWELL);
    assert(harness.pg.geofence_event_count() == 1);
}

void test_missing_or_inactive_fence_is_skipped() {
    Harness harness(topic_name("missing_fence"));
    auto inactive = rule("inactive", 100, 5);
    inactive.active = false;
    harness.load_fences({inactive});

    harness.redis.set_fence_state("dev-1", "missing", signalroute::FenceState::INSIDE, 1000);
    harness.redis.set_fence_state("dev-2", "inactive", signalroute::FenceState::INSIDE, 1000);

    assert(harness.checker.check_once(6000) == 0);
    assert(harness.redis.get_fence_state("dev-1", "missing") == signalroute::FenceState::INSIDE);
    assert(harness.redis.get_fence_state("dev-2", "inactive") == signalroute::FenceState::INSIDE);
    assert(harness.pg.geofence_event_count() == 0);
}

int main() {
    std::cout << "test_dwell_checker:\n";
    test_inside_state_transitions_to_dwell_after_threshold();
    test_inside_state_before_threshold_does_not_transition();
    test_dwell_state_is_not_emitted_twice();
    test_missing_or_inactive_fence_is_skipped();
    std::cout << "All dwell checker tests passed.\n";
    return 0;
}
