#include "common/clients/postgres_client.h"
#include "common/clients/redis_client.h"
#include "common/kafka/kafka_consumer.h"
#include "common/kafka/kafka_producer.h"
#include "common/spatial/h3_index.h"
#include "processor/dedup_window.h"
#include "processor/history_writer.h"
#include "processor/processing_loop.h"
#include "processor/sequence_guard.h"
#include "processor/state_writer.h"

#include <cassert>
#include <atomic>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace {

std::string topic_name(const std::string& suffix) {
    return "test.processing." + suffix;
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

signalroute::ProcessorConfig processor_config(int batch_size = 1, int flush_ms = 10) {
    signalroute::ProcessorConfig config;
    config.history_batch_size = batch_size;
    config.history_flush_interval_ms = flush_ms;
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

struct Harness {
    explicit Harness(std::string topic, signalroute::ProcessorConfig cfg = processor_config())
        : topic(std::move(topic))
        , producer(signalroute::KafkaConfig{})
        , consumer(signalroute::KafkaConfig{}, {this->topic})
        , redis(signalroute::RedisConfig{})
        , pg(signalroute::PostGISConfig{})
        , dlq(signalroute::KafkaConfig{})
        , h3(7)
        , dedup(1000, 300)
        , seq_guard(redis)
        , state_writer(redis, h3, 3600)
        , history_writer(pg, dlq, cfg)
        , loop(consumer, dedup, seq_guard, state_writer, history_writer, cfg)
    {}

    void start() {
        worker = std::thread([&] { loop.run(stop); });
    }

    void stop_and_join() {
        stop.store(true);
        if (worker.joinable()) {
            worker.join();
        }
    }

    std::string topic;
    signalroute::KafkaProducer producer;
    signalroute::KafkaConsumer consumer;
    signalroute::RedisClient redis;
    signalroute::PostgresClient pg;
    signalroute::KafkaProducer dlq;
    signalroute::H3Index h3;
    signalroute::DedupWindow dedup;
    signalroute::SequenceGuard seq_guard;
    signalroute::StateWriter state_writer;
    signalroute::HistoryWriter history_writer;
    signalroute::ProcessingLoop loop;
    std::atomic<bool> stop{false};
    std::thread worker;
};

} // namespace

void test_accepts_message_updates_state_history_and_commits() {
    Harness harness(topic_name("accepted"));

    harness.producer.produce(harness.topic, "dev-1", payload("dev-1", 1, 1000, 10.8231, 106.6297));
    harness.start();

    wait_until([&] {
        return harness.consumer.get_lag().front().second == 0 &&
               harness.pg.trip_point_count() == 1;
    });
    harness.stop_and_join();

    const auto state = harness.redis.get_device_state("dev-1");
    assert(state.has_value());
    assert(state->seq == 1);
    assert(state->lat == 10.8231);

    const auto trip = harness.pg.query_trip("dev-1", 0, 2000, 10);
    assert(trip.size() == 1);
    assert(trip.front().seq == 1);
}

void test_duplicate_message_is_committed_without_second_history_write() {
    Harness harness(topic_name("duplicate"), processor_config(10, 1000));

    harness.producer.produce(harness.topic, "dev-1", payload("dev-1", 1, 1000, 10.8231, 106.6297));
    harness.producer.produce(harness.topic, "dev-1", payload("dev-1", 1, 1000, 10.8231, 106.6297));
    harness.start();

    wait_until([&] { return harness.consumer.get_lag().front().second == 0; });
    harness.stop_and_join();

    assert(harness.pg.trip_point_count() == 1);
    assert(harness.dedup.size() == 1);
}

void test_stale_message_is_committed_without_state_or_history_update() {
    Harness harness(topic_name("stale"), processor_config(10, 1000));

    harness.producer.produce(harness.topic, "dev-1", payload("dev-1", 2, 2000, 10.8231, 106.6297));
    harness.producer.produce(harness.topic, "dev-1", payload("dev-1", 1, 1000, 10.0, 106.0));
    harness.start();

    wait_until([&] { return harness.consumer.get_lag().front().second == 0; });
    harness.stop_and_join();

    const auto state = harness.redis.get_device_state("dev-1");
    assert(state.has_value());
    assert(state->seq == 2);
    assert(state->lat == 10.8231);
    assert(harness.pg.trip_point_count() == 1);
}

void test_invalid_payload_is_committed_and_skipped() {
    Harness harness(topic_name("invalid"));

    harness.producer.produce(harness.topic, "dev-1", "not,a,location");
    harness.start();

    wait_until([&] { return harness.consumer.get_lag().front().second == 0; });
    harness.stop_and_join();

    assert(!harness.redis.get_device_state("dev-1").has_value());
    assert(harness.pg.trip_point_count() == 0);
}

int main() {
    std::cout << "test_processing_loop:\n";
    test_accepts_message_updates_state_history_and_commits();
    test_duplicate_message_is_committed_without_second_history_write();
    test_stale_message_is_committed_without_state_or_history_update();
    test_invalid_payload_is_committed_and_skipped();
    std::cout << "All processing loop tests passed.\n";
    return 0;
}
