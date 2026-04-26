#include "common/clients/postgres_client.h"
#include "common/clients/redis_client.h"
#include "common/kafka/kafka_consumer.h"
#include "common/spatial/h3_index.h"
#include "gateway/gateway_service.h"
#include "processor/dedup_window.h"
#include "processor/history_writer.h"
#include "processor/processing_loop.h"
#include "processor/sequence_guard.h"
#include "processor/state_writer.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

namespace {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string topic_name(const std::string& suffix) {
    return "test.gateway.processor.protobuf." + suffix;
}

signalroute::Config runtime_config(const std::string& topic) {
    signalroute::Config config;
    config.kafka.ingest_topic = topic;
    config.gateway.rate_limit_rps_per_device = 100;
    config.gateway.max_batch_events = 10;
    config.gateway.timestamp_skew_tolerance_s = 60;
    config.processor.history_batch_size = 1;
    config.processor.history_flush_interval_ms = 10;
    return config;
}

signalroute::LocationEvent event(std::string device_id, uint64_t seq) {
    signalroute::LocationEvent out;
    out.device_id = std::move(device_id);
    out.seq = seq;
    out.lat = 10.8231;
    out.lon = 106.6297;
    out.timestamp_ms = now_ms();
    out.metadata["source"] = "gateway-runtime";
    return out;
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

struct ProcessorHarness {
    explicit ProcessorHarness(const signalroute::Config& config)
        : consumer(config.kafka, {config.kafka.ingest_topic})
        , redis(config.redis)
        , pg(config.postgis)
        , dlq(config.kafka)
        , h3(config.spatial.h3_resolution)
        , dedup(1000, 300)
        , seq_guard(redis)
        , state_writer(redis, h3, config.redis.device_ttl_s)
        , history_writer(pg, dlq, config.processor)
        , loop(consumer, dedup, seq_guard, state_writer, history_writer, config.processor)
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

void test_gateway_protobuf_payload_is_processed_by_processor_runtime() {
    auto config = runtime_config(topic_name("accepted"));
    signalroute::GatewayService gateway;
    ProcessorHarness processor(config);

    gateway.start(config);
    processor.start();

    auto result = gateway.ingest_one(event("dev-1", 1));
    assert(result.is_ok());

    wait_until([&] {
        return processor.consumer.get_lag().front().second == 0 &&
               processor.redis.get_device_state("dev-1").has_value() &&
               processor.pg.trip_point_count() == 1;
    });

    processor.stop_and_join();
    gateway.stop();

    const auto state = processor.redis.get_device_state("dev-1");
    assert(state.has_value());
    assert(state->seq == 1);
    assert(state->lat == 10.8231);

    const auto trip = processor.pg.query_trip("dev-1", 0, now_ms() + 1000, 10);
    assert(trip.size() == 1);
    assert(trip.front().metadata.at("source") == "gateway-runtime");
}

void test_gateway_protobuf_runtime_still_deduplicates() {
    auto config = runtime_config(topic_name("duplicate"));
    signalroute::GatewayService gateway;
    ProcessorHarness processor(config);

    gateway.start(config);
    processor.start();

    assert(gateway.ingest_one(event("dev-2", 1)).is_ok());
    assert(gateway.ingest_one(event("dev-2", 1)).is_ok());

    wait_until([&] { return processor.consumer.get_lag().front().second == 0; });

    processor.stop_and_join();
    gateway.stop();

    assert(processor.pg.trip_point_count() == 1);
    assert(processor.dedup.size() == 1);
}

int main() {
    std::cout << "test_gateway_processor_protobuf_runtime:\n";
    test_gateway_protobuf_payload_is_processed_by_processor_runtime();
    test_gateway_protobuf_runtime_still_deduplicates();
    std::cout << "All gateway/processor protobuf runtime tests passed.\n";
    return 0;
}
