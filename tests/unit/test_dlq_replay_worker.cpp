#include "common/clients/postgres_client.h"
#include "common/events/all_events.h"
#include "common/events/event_bus.h"
#include "common/kafka/kafka_consumer.h"
#include "common/kafka/kafka_producer.h"
#include "common/proto/location_payload_codec.h"
#include "workers/dlq_replay_worker.h"

#include <cassert>
#include <iostream>
#include <string>

namespace {

std::string topic_name(const std::string& suffix) {
    return "test.dlq." + suffix;
}

} // namespace

void test_replays_valid_dlq_payloads_and_commits() {
    const auto topic = topic_name("valid");
    signalroute::PostgresClient pg(signalroute::PostGISConfig{});
    signalroute::KafkaProducer producer(signalroute::KafkaConfig{});
    signalroute::KafkaConsumer consumer(signalroute::KafkaConfig{}, {topic});
    signalroute::EventBus bus;
    int succeeded = 0;
    auto sub = bus.subscribe<signalroute::events::DLQReplaySucceeded>(
        [&](const signalroute::events::DLQReplaySucceeded& event) {
            succeeded += static_cast<int>(event.replayed_messages);
        });

    producer.produce(topic, "dev-1", "dev-1,1,1000,1005,10.0,106.0");
    signalroute::DLQReplayWorker worker(pg, consumer, bus);
    const auto result = worker.run_once();

    assert(result.replayed_messages == 1);
    assert(result.failed_messages == 0);
    assert(pg.trip_point_count() == 1);
    assert(succeeded == 1);
    assert(consumer.get_lag().front().second == 0);
}

void test_invalid_payload_is_reported_and_committed() {
    const auto topic = topic_name("invalid");
    signalroute::PostgresClient pg(signalroute::PostGISConfig{});
    signalroute::KafkaProducer producer(signalroute::KafkaConfig{});
    signalroute::KafkaConsumer consumer(signalroute::KafkaConfig{}, {topic});
    signalroute::EventBus bus;
    int failed = 0;
    auto sub = bus.subscribe<signalroute::events::DLQReplayFailed>(
        [&](const signalroute::events::DLQReplayFailed& event) {
            failed += static_cast<int>(event.failed_messages);
            assert(event.reason == "invalid DLQ payload");
        });

    producer.produce(topic, "dev-1", "not,a,location");
    signalroute::DLQReplayWorker worker(pg, consumer, bus);
    const auto result = worker.run_once();

    assert(result.replayed_messages == 0);
    assert(result.failed_messages == 1);
    assert(pg.trip_point_count() == 0);
    assert(failed == 1);
    assert(consumer.get_lag().front().second == 0);
}

void test_replays_shared_location_codec_payloads() {
    const auto topic = topic_name("shared_codec");
    signalroute::PostgresClient pg(signalroute::PostGISConfig{});
    signalroute::KafkaProducer producer(signalroute::KafkaConfig{});
    signalroute::KafkaConsumer consumer(signalroute::KafkaConfig{}, {topic});

    signalroute::LocationEvent event;
    event.device_id = "dev-2";
    event.seq = 9;
    event.timestamp_ms = 2000;
    event.server_recv_ms = 2005;
    event.lat = 10.5;
    event.lon = 106.5;

    producer.produce(topic, event.device_id, signalroute::proto_boundary::encode_location_payload(event));
    signalroute::DLQReplayWorker worker(pg, consumer);
    const auto result = worker.run_once();

    assert(result.replayed_messages == 1);
    assert(result.failed_messages == 0);
    assert(pg.trip_point_count() == 1);
    const auto trips = pg.query_trip("dev-2", 0, 3000, 10);
    assert(trips.size() == 1);
    assert(trips.front().device_id == "dev-2");
    assert(trips.front().seq == 9);
}

int main() {
    std::cout << "test_dlq_replay_worker:\n";
    test_replays_valid_dlq_payloads_and_commits();
    test_invalid_payload_is_reported_and_committed();
    test_replays_shared_location_codec_payloads();
    std::cout << "All DLQ replay worker tests passed.\n";
    return 0;
}
