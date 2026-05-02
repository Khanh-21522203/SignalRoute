#include "common/config/config.h"
#include "common/kafka/kafka_consumer.h"
#include "common/kafka/kafka_producer.h"
#include "common/proto/location_payload_codec.h"
#include "common/types/location_event.h"
#include "processor/processor_service.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

namespace {

constexpr int kSkip = 77;

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "ingestion integration failed: " << message << '\n';
        std::exit(1);
    }
}

std::string required_env(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
}

std::string unique_suffix(const std::string& name) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    std::ostringstream out;
    out << name << "." << nanos;
    return out.str();
}

signalroute::LocationEvent location_event(std::string device_id, uint64_t seq) {
    signalroute::LocationEvent event;
    event.device_id = std::move(device_id);
    event.seq = seq;
    event.timestamp_ms = 1'700'000'000'000 + static_cast<int64_t>(seq);
    event.server_recv_ms = event.timestamp_ms + 5;
    event.lat = 10.8231;
    event.lon = 106.6297;
    event.accuracy_m = 4.0F;
    event.speed_ms = 8.5F;
    return event;
}

signalroute::KafkaConfig kafka_config(const std::string& brokers,
                                      const std::string& group_suffix) {
    signalroute::KafkaConfig config;
    config.brokers = brokers;
    config.consumer_group = "signalroute-it-" + group_suffix;
    config.linger_ms = 1;
    config.batch_size_bytes = 4096;
    return config;
}

void produce_and_wait(signalroute::KafkaProducer& producer,
                      const std::string& topic,
                      const std::string& key,
                      const std::string& payload) {
    std::atomic<bool> delivered{false};
    std::string error;

    producer.produce(topic, key, payload, [&](bool ok, const std::string& err) {
        delivered.store(ok);
        error = err;
    });
    producer.flush(10'000);
    producer.poll(0);

    require(delivered.load(), "Kafka delivery callback did not succeed: " + error);
}

template <typename Predicate>
std::optional<signalroute::KafkaMessage> poll_until(signalroute::KafkaConsumer& consumer,
                                                    Predicate predicate,
                                                    std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        auto message = consumer.poll(500);
        if (message && predicate(*message)) {
            return message;
        }
    }
    return std::nullopt;
}

template <typename Predicate>
void wait_until(Predicate predicate,
                const std::string& message,
                std::chrono::milliseconds timeout = std::chrono::seconds(15)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    require(predicate(), message);
}

void test_real_kafka_produce_consume_roundtrip(const std::string& brokers) {
    const auto suffix = unique_suffix("roundtrip");
    const auto topic = "sr.it.ingestion." + suffix;
    auto config = kafka_config(brokers, suffix);
    const auto event = location_event("device-roundtrip", 1);
    const auto payload = signalroute::proto_boundary::encode_location_payload(event);

    signalroute::KafkaProducer producer(config);
    produce_and_wait(producer, topic, event.device_id, payload);

    signalroute::KafkaConsumer consumer(config, {topic});
    auto consumed = poll_until(
        consumer,
        [&](const signalroute::KafkaMessage& msg) {
            return msg.key == event.device_id;
        },
        std::chrono::seconds(15));

    require(consumed.has_value(), "real Kafka consumer did not receive produced message");
    require(consumed->topic == topic, "consumed message topic mismatch");
    require(consumed->payload == payload, "consumed payload bytes mismatch");

    auto decoded = signalroute::proto_boundary::decode_location_payload(consumed->payload);
    require(decoded.is_ok(), "location payload did not decode");
    require(decoded.value().device_id == event.device_id, "decoded device id mismatch");
    require(decoded.value().seq == event.seq, "decoded sequence mismatch");

    consumer.commit(*consumed);
}

void test_processor_consumes_real_kafka_and_commits_after_writes(const std::string& brokers) {
    const auto suffix = unique_suffix("processor");
    const auto topic = "sr.it.processor." + suffix;
    const auto event = location_event("device-processor", 2);

    signalroute::Config config;
    config.kafka = kafka_config(brokers, suffix);
    config.kafka.ingest_topic = topic;
    config.processor.history_batch_size = 1;
    config.processor.history_flush_interval_ms = 10;
    config.processor.dedup_max_entries = 1000;

    signalroute::KafkaProducer producer(config.kafka);
    produce_and_wait(
        producer,
        topic,
        event.device_id,
        signalroute::proto_boundary::encode_location_payload(event));

    signalroute::ProcessorService service;
    service.start(config);
    wait_until(
        [&] {
            return service.latest_state_for_test(event.device_id).has_value() &&
                   service.trip_point_count_for_test() == 1;
        },
        "processor did not write state and history from real Kafka message");
    service.stop();

    const auto state = service.latest_state_for_test(event.device_id);
    require(state.has_value(), "processor state missing after stop");
    require(state->seq == event.seq, "processor state sequence mismatch");
    require(state->lat == event.lat, "processor state latitude mismatch");

    signalroute::KafkaConsumer verifier(config.kafka, {topic});
    auto committed_message = poll_until(
        verifier,
        [&](const signalroute::KafkaMessage& msg) {
            return msg.key == event.device_id;
        },
        std::chrono::seconds(3));
    require(!committed_message.has_value(),
            "processor consumer group re-read message after state/history writes");
}

} // namespace

int main() {
    if (required_env("SIGNALROUTE_RUN_KAFKA_INTEGRATION") != "1") {
        std::cout << "Skipping Kafka ingestion integration; "
                  << "set SIGNALROUTE_RUN_KAFKA_INTEGRATION=1 to run.\n";
        return kSkip;
    }

    const auto brokers = required_env("SIGNALROUTE_KAFKA_BROKERS");
    if (brokers.empty()) {
        std::cout << "Skipping Kafka ingestion integration; "
                  << "SIGNALROUTE_KAFKA_BROKERS is not set.\n";
        return kSkip;
    }

    std::cout << "test_ingestion_pipeline against " << brokers << '\n';
    test_real_kafka_produce_consume_roundtrip(brokers);
    test_processor_consumes_real_kafka_and_commits_after_writes(brokers);
    std::cout << "Kafka ingestion integration tests passed.\n";
    return 0;
}
