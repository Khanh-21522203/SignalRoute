#include "kafka_producer.h"
#include "kafka_consumer.h"

#include <iostream>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

// TODO: #include <cppkafka/cppkafka.h>

namespace signalroute {
namespace kafka_fallback {

struct BrokerMessage {
    std::string topic;
    int32_t partition = 0;
    int64_t offset = 0;
    std::string key;
    std::string payload;
    int64_t timestamp_ms = 0;
};

std::mutex& broker_mutex() {
    static std::mutex mu;
    return mu;
}

std::unordered_map<std::string, std::vector<BrokerMessage>>& broker_topics() {
    static std::unordered_map<std::string, std::vector<BrokerMessage>> topics;
    return topics;
}

int64_t append_message(const std::string& topic,
                       const std::string& key,
                       const std::string& payload) {
    std::lock_guard lock(broker_mutex());
    auto& messages = broker_topics()[topic];
    const int64_t offset = static_cast<int64_t>(messages.size());
    messages.push_back(BrokerMessage{
        topic,
        0,
        offset,
        key,
        payload,
        0,
    });
    return offset;
}

std::optional<KafkaMessage> read_message(const std::string& topic, int64_t offset) {
    std::lock_guard lock(broker_mutex());
    auto it = broker_topics().find(topic);
    if (it == broker_topics().end() || offset < 0 ||
        offset >= static_cast<int64_t>(it->second.size())) {
        return std::nullopt;
    }

    const auto& message = it->second[static_cast<size_t>(offset)];
    return KafkaMessage{
        message.topic,
        message.partition,
        message.offset,
        message.key,
        message.payload,
        message.timestamp_ms,
    };
}

int64_t topic_size(const std::string& topic) {
    std::lock_guard lock(broker_mutex());
    auto it = broker_topics().find(topic);
    if (it == broker_topics().end()) {
        return 0;
    }
    return static_cast<int64_t>(it->second.size());
}

} // namespace kafka_fallback

KafkaProducer::KafkaProducer(const KafkaConfig& config) : config_(config) {
    // TODO: Configure and create cppkafka producer.
    std::cerr << "[KafkaProducer] WARNING: using in-memory Kafka fallback.\n";
}

KafkaProducer::~KafkaProducer() {
    flush(5000);
}

void KafkaProducer::produce(
    const std::string& topic,
    const std::string& key,
    const std::string& payload,
    DeliveryCallback callback)
{
    if (topic.empty()) {
        if (callback) {
            callback(false, "topic must not be empty");
        }
        throw std::invalid_argument("Kafka topic must not be empty");
    }

    (void)kafka_fallback::append_message(topic, key, payload);
    if (callback) {
        callback(true, "");
    }
}

void KafkaProducer::flush(int /*timeout_ms*/) {
    // In-memory fallback produces synchronously.
}

int KafkaProducer::poll(int /*timeout_ms*/) {
    // Delivery callbacks fire synchronously in the fallback.
    return 0;
}

bool KafkaProducer::is_connected() const {
    return true;
}

} // namespace signalroute
