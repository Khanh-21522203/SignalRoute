#include "kafka_consumer.h"

#if SIGNALROUTE_HAS_KAFKA
#include <rdkafkacpp.h>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace signalroute {
namespace kafka_fallback {

std::optional<KafkaMessage> read_message(const std::string& topic, int64_t offset);
int64_t topic_size(const std::string& topic);

} // namespace kafka_fallback
namespace {

std::string partition_key(const std::string& topic, int32_t partition) {
    return topic + "#" + std::to_string(partition);
}

#if SIGNALROUTE_HAS_KAFKA

void set_required(RdKafka::Conf& conf, const std::string& key, const std::string& value) {
    std::string error;
    if (conf.set(key, value, error) != RdKafka::Conf::CONF_OK) {
        throw std::runtime_error("Kafka consumer config failed for " + key + ": " + error);
    }
}

void set_if_supported(RdKafka::Conf& conf, const std::string& key, const std::string& value) {
    std::string error;
    (void)conf.set(key, value, error);
}

void destroy_topic_partitions(std::vector<RdKafka::TopicPartition*>& partitions) {
    RdKafka::TopicPartition::destroy(partitions);
    partitions.clear();
}

#endif

} // namespace

struct KafkaConsumer::Impl {
#if SIGNALROUTE_HAS_KAFKA
    std::unique_ptr<RdKafka::KafkaConsumer> consumer;
#endif
};

KafkaConsumer::KafkaConsumer(const KafkaConfig& config,
                             const std::vector<std::string>& topics)
    : config_(config), topics_(topics), impl_(std::make_unique<Impl>())
{
    for (const auto& topic : topics_) {
        next_offsets_[topic] = 0;
        committed_offsets_[partition_key(topic, 0)] = 0;
    }

#if SIGNALROUTE_HAS_KAFKA
    std::unique_ptr<RdKafka::Conf> conf(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    if (!conf) {
        throw std::runtime_error("failed to create Kafka consumer config");
    }

    set_required(*conf, "metadata.broker.list", config_.brokers);
    set_required(*conf, "group.id", config_.consumer_group);
    set_required(*conf, "enable.auto.commit", "false");
    set_if_supported(*conf, "auto.offset.reset", "earliest");
    set_if_supported(*conf, "enable.partition.eof", "false");

    std::string error;
    impl_->consumer.reset(RdKafka::KafkaConsumer::create(conf.get(), error));
    if (!impl_->consumer) {
        throw std::runtime_error("failed to create Kafka consumer: " + error);
    }

    const auto subscribe_error = impl_->consumer->subscribe(topics_);
    if (subscribe_error != RdKafka::ERR_NO_ERROR) {
        throw std::runtime_error("Kafka subscribe failed: " + RdKafka::err2str(subscribe_error));
    }
#else
    std::cerr << "[KafkaConsumer] WARNING: using in-memory Kafka fallback.\n";
#endif
}

KafkaConsumer::~KafkaConsumer() {
#if SIGNALROUTE_HAS_KAFKA
    if (impl_ && impl_->consumer) {
        impl_->consumer->close();
    }
#endif
}

std::optional<KafkaMessage> KafkaConsumer::poll(int timeout_ms) {
#if SIGNALROUTE_HAS_KAFKA
    if (!impl_->consumer) {
        return std::nullopt;
    }

    std::unique_ptr<RdKafka::Message> message(
        impl_->consumer->consume(timeout_ms < 0 ? 0 : timeout_ms));
    if (!message) {
        return std::nullopt;
    }

    if (message->err() == RdKafka::ERR__TIMED_OUT) {
        return std::nullopt;
    }
    if (message->err() != RdKafka::ERR_NO_ERROR) {
        throw std::runtime_error("Kafka consume failed: " + message->errstr());
    }

    KafkaMessage out;
    out.topic = message->topic_name();
    out.partition = message->partition();
    out.offset = message->offset();
    if (const auto* key = message->key()) {
        out.key = *key;
    }
    if (message->payload() && message->len() > 0) {
        out.payload.assign(
            static_cast<const char*>(message->payload()),
            static_cast<size_t>(message->len()));
    }
    out.timestamp_ms = message->timestamp().timestamp;
    last_polled_ = out;
    return out;
#else
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
#endif
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
#if SIGNALROUTE_HAS_KAFKA
    if (!impl_->consumer) {
        return;
    }

    std::vector<RdKafka::TopicPartition*> offsets;
    offsets.push_back(RdKafka::TopicPartition::create(msg.topic, msg.partition, msg.offset + 1));
    const auto error = impl_->consumer->commitSync(offsets);
    destroy_topic_partitions(offsets);
    if (error != RdKafka::ERR_NO_ERROR) {
        throw std::runtime_error("Kafka commit failed: " + RdKafka::err2str(error));
    }
#else
    const auto key = partition_key(msg.topic, msg.partition);
    auto& committed = committed_offsets_[key];
    committed = std::max(committed, msg.offset + 1);
#endif
}

std::vector<std::pair<int32_t, int64_t>> KafkaConsumer::get_lag() const {
    std::vector<std::pair<int32_t, int64_t>> result;

#if SIGNALROUTE_HAS_KAFKA
    if (!impl_->consumer) {
        return result;
    }

    std::vector<RdKafka::TopicPartition*> partitions;
    auto error = impl_->consumer->assignment(partitions);
    if (error != RdKafka::ERR_NO_ERROR || partitions.empty()) {
        for (std::size_t i = 0; i < topics_.size(); ++i) {
            result.push_back({0, 0});
        }
        destroy_topic_partitions(partitions);
        return result;
    }

    error = impl_->consumer->committed(partitions, 1000);
    if (error != RdKafka::ERR_NO_ERROR) {
        destroy_topic_partitions(partitions);
        return result;
    }

    result.reserve(partitions.size());
    for (const auto* partition : partitions) {
        int64_t low = 0;
        int64_t high = 0;
        const auto watermark_error = impl_->consumer->query_watermark_offsets(
            partition->topic(), partition->partition(), &low, &high, 1000);
        if (watermark_error != RdKafka::ERR_NO_ERROR) {
            result.push_back({partition->partition(), 0});
            continue;
        }
        const int64_t committed = partition->offset() < 0 ? low : partition->offset();
        result.push_back({partition->partition(), std::max<int64_t>(0, high - committed)});
    }

    destroy_topic_partitions(partitions);
    return result;
#else
    result.reserve(topics_.size());

    for (const auto& topic : topics_) {
        const int64_t high_watermark = kafka_fallback::topic_size(topic);
        const auto it = committed_offsets_.find(partition_key(topic, 0));
        const int64_t committed = it == committed_offsets_.end() ? 0 : it->second;
        result.push_back({0, std::max<int64_t>(0, high_watermark - committed)});
    }

    return result;
#endif
}

bool KafkaConsumer::is_connected() const {
#if SIGNALROUTE_HAS_KAFKA
    return impl_ && impl_->consumer != nullptr;
#else
    return true;
#endif
}

} // namespace signalroute
