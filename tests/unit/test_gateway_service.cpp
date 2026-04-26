#include "common/events/all_events.h"
#include "common/events/event_bus.h"
#include "common/kafka/kafka_consumer.h"
#include "gateway/gateway_service.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

int64_t now_ms() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::string topic_name(const std::string& suffix) {
    return "test.gateway." + suffix;
}

signalroute::Config config_for_topic(const std::string& topic, int max_rps = 10, int max_batch = 10) {
    signalroute::Config config;
    config.kafka.ingest_topic = topic;
    config.gateway.rate_limit_rps_per_device = max_rps;
    config.gateway.max_batch_events = max_batch;
    config.gateway.timestamp_skew_tolerance_s = 60;
    return config;
}

signalroute::LocationEvent event(std::string device_id, uint64_t seq) {
    signalroute::LocationEvent out;
    out.device_id = std::move(device_id);
    out.seq = seq;
    out.lat = 10.8231;
    out.lon = 106.6297;
    out.timestamp_ms = now_ms();
    return out;
}

} // namespace

void test_ingest_one_validates_stamps_publishes_and_emits_event() {
    const auto topic = topic_name("accepted");
    auto config = config_for_topic(topic);
    signalroute::EventBus bus;
    signalroute::GatewayService gateway;
    signalroute::KafkaConsumer consumer(config.kafka, {topic});
    int published_events = 0;
    signalroute::events::IngestEventPublished captured;

    auto sub = bus.subscribe<signalroute::events::IngestEventPublished>(
        [&](const signalroute::events::IngestEventPublished& event) {
            ++published_events;
            captured = event;
        });

    gateway.start(config, bus);
    auto result = gateway.ingest_one(event("dev-1", 1));

    assert(result.is_ok());
    assert(result.value().server_recv_ms > 0);
    assert(published_events == 1);
    assert(captured.topic == topic);
    assert(captured.event.device_id == "dev-1");
    assert(gateway.tracked_devices_for_test() == 1);

    const auto msg = consumer.poll(0);
    assert(msg.has_value());
    assert(msg->key == "dev-1");
    assert(msg->payload.find("dev-1,1,") == 0);

    gateway.stop();
    assert(!gateway.is_healthy());
}

void test_invalid_event_is_rejected_without_publish() {
    const auto topic = topic_name("invalid");
    auto config = config_for_topic(topic);
    signalroute::EventBus bus;
    signalroute::GatewayService gateway;
    signalroute::KafkaConsumer consumer(config.kafka, {topic});
    int rejected_batches = 0;
    std::string reason;

    auto sub = bus.subscribe<signalroute::events::IngestBatchRejected>(
        [&](const signalroute::events::IngestBatchRejected& event) {
            ++rejected_batches;
            reason = event.reason;
        });

    gateway.start(config, bus);
    auto bad = event("", 1);
    auto result = gateway.ingest_one(bad);

    assert(result.is_err());
    assert(reason == "device_id is required");
    assert(rejected_batches == 1);
    assert(!consumer.poll(0).has_value());
}

void test_rate_limited_event_emits_backpressure_and_rejection() {
    const auto topic = topic_name("rate_limited");
    auto config = config_for_topic(topic, 1);
    signalroute::EventBus bus;
    signalroute::GatewayService gateway;
    signalroute::KafkaConsumer consumer(config.kafka, {topic});
    int backpressure = 0;
    int rejected = 0;

    auto backpressure_sub = bus.subscribe<signalroute::events::GatewayBackpressureApplied>(
        [&](const signalroute::events::GatewayBackpressureApplied&) { ++backpressure; });
    auto rejected_sub = bus.subscribe<signalroute::events::IngestBatchRejected>(
        [&](const signalroute::events::IngestBatchRejected&) { ++rejected; });

    gateway.start(config, bus);
    assert(gateway.ingest_one(event("dev-1", 1)).is_ok());
    auto second = gateway.ingest_one(event("dev-1", 2));

    assert(second.is_err());
    assert(second.error() == "rate limited");
    assert(backpressure == 1);
    assert(rejected == 1);

    auto first_msg = consumer.poll(0);
    assert(first_msg.has_value());
    assert(!consumer.poll(0).has_value());
}

void test_ingest_batch_reports_received_and_rejects_oversized_batch() {
    const auto topic = topic_name("batch_too_large");
    auto config = config_for_topic(topic, 10, 1);
    signalroute::EventBus bus;
    signalroute::GatewayService gateway;
    signalroute::KafkaConsumer consumer(config.kafka, {topic});
    int received = 0;
    int rejected = 0;

    auto received_sub = bus.subscribe<signalroute::events::IngestBatchReceived>(
        [&](const signalroute::events::IngestBatchReceived& event) {
            ++received;
            assert(event.events.size() == 2);
        });
    auto rejected_sub = bus.subscribe<signalroute::events::IngestBatchRejected>(
        [&](const signalroute::events::IngestBatchRejected& event) {
            ++rejected;
            assert(event.reason == "batch too large");
            assert(event.rejected_count == 2);
        });

    gateway.start(config, bus);
    const auto result = gateway.ingest_batch({event("dev-1", 1), event("dev-2", 1)});

    assert(!result.ok());
    assert(result.accepted == 0);
    assert(result.rejected == 2);
    assert(received == 1);
    assert(rejected == 1);
    assert(!consumer.poll(0).has_value());
}

int main() {
    std::cout << "test_gateway_service:\n";
    test_ingest_one_validates_stamps_publishes_and_emits_event();
    test_invalid_event_is_rejected_without_publish();
    test_rate_limited_event_emits_backpressure_and_rejection();
    test_ingest_batch_reports_received_and_rejects_oversized_batch();
    std::cout << "All gateway service tests passed.\n";
    return 0;
}
