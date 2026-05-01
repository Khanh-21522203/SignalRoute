#include "common/admin/dependency_health.h"
#include "common/clients/postgres_client.h"
#include "common/clients/redis_client.h"
#include "common/kafka/kafka_consumer.h"
#include "common/kafka/kafka_producer.h"
#include "common/spatial/h3_index.h"

#include <cassert>
#include <iostream>
#include <string>

void test_build_flag_source_reports_enabled_and_disabled_states() {
    const auto disabled = signalroute::build_flag_dependency_health("redis", false);
    assert(disabled.name == "redis");
    assert(!disabled.healthy);
    assert(disabled.detail == "production adapter required but not enabled in this build");

    const auto enabled = signalroute::build_flag_dependency_health("kafka", true);
    assert(enabled.name == "kafka");
    assert(enabled.healthy);
    assert(enabled.detail == "production adapter enabled");
}

void test_registry_reports_missing_source_as_unhealthy() {
    signalroute::DependencyHealthRegistry registry;

    const auto snapshot = registry.check("postgis");
    const auto component = registry.readiness_component("postgis");

    assert(snapshot.name == "postgis");
    assert(!snapshot.healthy);
    assert(snapshot.detail == "dependency health source not registered");
    assert(component.name == "postgis");
    assert(!component.healthy);
    assert(component.required);
}

void test_registry_uses_registered_source_and_fills_missing_name() {
    signalroute::DependencyHealthRegistry registry;
    registry.register_source("redis", [] {
        return signalroute::DependencyHealthSnapshot{"", true, "redis ping ok"};
    });

    const auto snapshot = registry.check("redis");
    const auto component = registry.readiness_component("redis");

    assert(registry.source_count() == 1);
    assert(snapshot.name == "redis");
    assert(snapshot.healthy);
    assert(snapshot.detail == "redis ping ok");
    assert(component.name == "redis");
    assert(component.healthy);
    assert(component.detail == "redis ping ok");
}

void test_default_registry_has_known_dependency_sources() {
    const auto registry = signalroute::default_dependency_health_registry();

    assert(registry.source_count() == 4);
    assert(registry.check("kafka").name == "kafka");
    assert(registry.check("redis").name == "redis");
    assert(registry.check("postgis").name == "postgis");
    assert(registry.check("h3").name == "h3");
}

void test_adapter_health_helpers_use_existing_adapter_interfaces() {
    signalroute::KafkaProducer producer(signalroute::KafkaConfig{});
    signalroute::KafkaConsumer consumer(signalroute::KafkaConfig{}, {"dependency-health-topic"});
    signalroute::RedisClient redis(signalroute::RedisConfig{});
    signalroute::PostgresClient postgis(signalroute::PostGISConfig{});
    signalroute::H3Index h3(7);

    assert(signalroute::kafka_producer_health(producer).healthy);
    assert(signalroute::kafka_producer_health(producer).detail == "kafka producer connected");
    assert(signalroute::kafka_consumer_health(consumer).healthy);
    assert(signalroute::kafka_consumer_health(consumer).detail == "kafka consumer connected");
    assert(signalroute::redis_health(redis).healthy);
    assert(signalroute::redis_health(redis).detail == "redis ping ok");
    assert(signalroute::postgis_health(postgis).healthy);
    assert(signalroute::postgis_health(postgis).detail == "postgis ping ok");
    assert(signalroute::h3_health(h3).healthy);
    assert(signalroute::h3_health(h3).detail == "h3 index ready");
}

int main() {
    std::cout << "test_dependency_health_sources:\n";
    test_build_flag_source_reports_enabled_and_disabled_states();
    test_registry_reports_missing_source_as_unhealthy();
    test_registry_uses_registered_source_and_fills_missing_name();
    test_default_registry_has_known_dependency_sources();
    test_adapter_health_helpers_use_existing_adapter_interfaces();
    std::cout << "All dependency health source tests passed.\n";
    return 0;
}
