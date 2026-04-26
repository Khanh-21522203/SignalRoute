#include "kafka_consumer.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

// TODO: #include <cppkafka/cppkafka.h>

namespace signalroute {
namespace kafka_fallback {

std::optional<KafkaMessage> read_message(const std::string& topic, int64_t offset);
int64_t topic_size(const std::string& topic);

} // namespace kafka_fallback
namespace {

std::string partition_key(const std::string& topic, int32_t partition) {
    return topic + "#" + std::to_string(partition);
}

} // namespace

KafkaConsumer::KafkaConsumer(const KafkaConfig& config,
                             const std::vector<std::string>& topics)
    : config_(config), topics_(topics)
{
    // TODO: Configure and create cppkafka consumer.
    for (const auto& topic : topics_) {
        next_offsets_[topic] = 0;
        committed_offsets_[partition_key(topic, 0)] = 0;
    }
    std::cerr << "[KafkaConsumer] WARNING: using in-memory Kafka fallback.\n";
}

KafkaConsumer::~KafkaConsumer() = default;

std::optional<KafkaMessage> KafkaConsumer::poll(int timeout_ms) {
    for (const auto& topic : topics_) {
        const int64_t next_offset = next_offsets_[topic];
        auto message = kafka_fallback::read_message(topic, next_offset);
        if (message) {
            next_offsets_[topic] = next_offset + 1;
            last_polled_ = *message;
            return message;
        }
    }

    if (timeout_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
    }
    return std::nullopt;
}

void KafkaConsumer::poll_loop(MessageHandler handler,
                               const std::atomic<bool>& should_stop) {
    while (!should_stop.load()) {
        auto msg = poll(10);
        if (!msg) {
            continue;
        }

        if (handler(*msg)) {
            commit(*msg);
        }
    }
}

void KafkaConsumer::commit() {
    if (last_polled_) {
        commit(*last_polled_);
    }
}

void KafkaConsumer::commit(const KafkaMessage& msg) {
    const auto key = partition_key(msg.topic, msg.partition);
    auto& committed = committed_offsets_[key];
    committed = std::max(committed, msg.offset + 1);
}

std::vector<std::pair<int32_t, int64_t>> KafkaConsumer::get_lag() const {
    std::vector<std::pair<int32_t, int64_t>> result;
    result.reserve(topics_.size());

    for (const auto& topic : topics_) {
        const int64_t high_watermark = kafka_fallback::topic_size(topic);
        const auto it = committed_offsets_.find(partition_key(topic, 0));
        const int64_t committed = it == committed_offsets_.end() ? 0 : it->second;
        result.push_back({0, std::max<int64_t>(0, high_watermark - committed)});
    }

    return result;
}

bool KafkaConsumer::is_connected() const {
    return true;
}

} // namespace signalroute
