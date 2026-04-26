#include "common/events/all_events.h"
#include "common/events/event_bus.h"
#include "common/kafka/kafka_producer.h"
#include "processor/processor_service.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace {

std::string topic_name(const std::string& suffix) {
    return "test.processor.service." + suffix;
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

signalroute::Config config_for_topic(const std::string& topic) {
    signalroute::Config config;
    config.kafka.ingest_topic = topic;
    config.processor.history_batch_size = 1;
    config.processor.history_flush_interval_ms = 10;
    config.processor.dedup_max_entries = 1000;
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

} // namespace

void test_processor_service_wires_event_driven_runtime() {
    const auto topic = topic_name("runtime");
    auto config = config_for_topic(topic);
    signalroute::EventBus bus;
    signalroute::ProcessorService service;
    signalroute::KafkaProducer producer(config.kafka);
    int state_successes = 0;
    int history_writes = 0;

    auto state_sub = bus.subscribe<signalroute::events::StateWriteSucceeded>(
        [&](const signalroute::events::StateWriteSucceeded&) { ++state_successes; });
    auto history_sub = bus.subscribe<signalroute::events::TripHistoryWritten>(
        [&](const signalroute::events::TripHistoryWritten&) { ++history_writes; });

    service.start(config, bus);
    assert(service.is_healthy());
    assert(service.is_event_driven());
    assert(service.subscription_count() == 11);

    producer.produce(topic, "dev-1", payload("dev-1", 1, 1000, 10.8231, 106.6297));

    wait_until([&] {
        return state_successes == 1 &&
               history_writes == 1 &&
               service.latest_state_for_test("dev-1").has_value() &&
               service.trip_point_count_for_test() == 1;
    });

    service.stop();
    assert(!service.is_healthy());
    assert(service.subscription_count() == 0);
}

void test_processor_service_owned_bus_startup_and_shutdown() {
    auto config = config_for_topic(topic_name("owned_bus"));
    signalroute::ProcessorService service;

    service.start(config);
    assert(service.is_healthy());
    assert(service.is_event_driven());
    assert(service.subscription_count() == 11);

    service.stop();
    assert(!service.is_healthy());
    assert(service.subscription_count() == 0);
}

int main() {
    std::cout << "test_processor_service:\n";
    test_processor_service_wires_event_driven_runtime();
    test_processor_service_owned_bus_startup_and_shutdown();
    std::cout << "All processor service tests passed.\n";
    return 0;
}
