#include "common/kafka/kafka_consumer.h"
#include "common/kafka/kafka_producer.h"

#include <cassert>
#include <atomic>
#include <stdexcept>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string topic_name(const std::string& suffix) {
    return "test.kafka." + suffix;
}

} // namespace

void test_producer_callback_and_consumer_poll() {
    const std::string topic = topic_name("produce_poll");
    signalroute::KafkaProducer producer(signalroute::KafkaConfig{});
    signalroute::KafkaConsumer consumer(signalroute::KafkaConfig{}, {topic});

    bool callback_called = false;
    producer.produce(topic, "dev-1", "payload-1", [&](bool success, const std::string& error) {
        callback_called = true;
        assert(success);
        assert(error.empty());
    });

    assert(callback_called);
    assert(producer.is_connected());
    assert(consumer.is_connected());

    auto msg = consumer.poll(0);
    assert(msg.has_value());
    assert(msg->topic == topic);
    assert(msg->partition == 0);
    assert(msg->offset == 0);
    assert(msg->key == "dev-1");
    assert(msg->payload == "payload-1");
}

void test_commit_updates_lag() {
    const std::string topic = topic_name("commit_lag");
    signalroute::KafkaProducer producer(signalroute::KafkaConfig{});
    signalroute::KafkaConsumer consumer(signalroute::KafkaConfig{}, {topic});

    producer.produce(topic, "dev-1", "payload-1");
    producer.produce(topic, "dev-2", "payload-2");

    auto lag = consumer.get_lag();
    assert(lag.size() == 1);
    assert(lag.front().second == 2);

    auto first = consumer.poll(0);
    assert(first.has_value());
    consumer.commit(*first);

    lag = consumer.get_lag();
    assert(lag.front().second == 1);

    auto second = consumer.poll(0);
    assert(second.has_value());
    consumer.commit();

    lag = consumer.get_lag();
    assert(lag.front().second == 0);
}

void test_poll_loop_commits_successful_messages() {
    const std::string topic = topic_name("poll_loop");
    signalroute::KafkaProducer producer(signalroute::KafkaConfig{});
    signalroute::KafkaConsumer consumer(signalroute::KafkaConfig{}, {topic});

    producer.produce(topic, "dev-1", "payload-1");
    producer.produce(topic, "dev-2", "payload-2");

    std::atomic<bool> stop{false};
    std::vector<std::string> seen;

    std::thread thread([&] {
        consumer.poll_loop([&](const signalroute::KafkaMessage& msg) {
            seen.push_back(msg.payload);
            if (seen.size() == 2) {
                stop.store(true);
            }
            return true;
        }, stop);
    });
    thread.join();

    assert(seen.size() == 2);
    assert(seen[0] == "payload-1");
    assert(seen[1] == "payload-2");
    assert(consumer.get_lag().front().second == 0);
}

void test_empty_topic_reports_callback_error_and_throws() {
    signalroute::KafkaProducer producer(signalroute::KafkaConfig{});

    bool callback_called = false;
    bool callback_success = true;
    std::string callback_error;
    bool threw = false;

    try {
        producer.produce("", "dev-1", "payload", [&](bool success, const std::string& error) {
            callback_called = true;
            callback_success = success;
            callback_error = error;
        });
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assert(threw);
    assert(callback_called);
    assert(!callback_success);
    assert(callback_error == "topic must not be empty");
}

void test_poll_loop_does_not_commit_failed_messages() {
    const std::string topic = topic_name("poll_loop_failed");
    signalroute::KafkaProducer producer(signalroute::KafkaConfig{});
    signalroute::KafkaConsumer consumer(signalroute::KafkaConfig{}, {topic});

    producer.produce(topic, "dev-1", "payload-1");

    std::atomic<bool> stop{false};
    int seen = 0;
    std::thread thread([&] {
        consumer.poll_loop([&](const signalroute::KafkaMessage&) {
            ++seen;
            stop.store(true);
            return false;
        }, stop);
    });
    thread.join();

    assert(seen == 1);
    assert(consumer.get_lag().front().second == 1);
}

int main() {
    std::cout << "test_kafka_transport:\n";
    test_producer_callback_and_consumer_poll();
    test_commit_updates_lag();
    test_poll_loop_commits_successful_messages();
    test_empty_topic_reports_callback_error_and_throws();
    test_poll_loop_does_not_commit_failed_messages();
    std::cout << "All Kafka transport tests passed.\n";
    return 0;
}
