#include "kafka_producer.h"
#include "kafka_consumer.h"

#if SIGNALROUTE_HAS_KAFKA
#include <rdkafkacpp.h>
#endif

#include <algorithm>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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

namespace {

#if SIGNALROUTE_HAS_KAFKA

void set_required(RdKafka::Conf& conf, const std::string& key, const std::string& value) {
    std::string error;
    if (conf.set(key, value, error) != RdKafka::Conf::CONF_OK) {
        throw std::runtime_error("Kafka producer config failed for " + key + ": " + error);
    }
}

void set_required(RdKafka::Conf& conf, const std::string& key, RdKafka::DeliveryReportCb* value) {
    std::string error;
    if (conf.set(key, value, error) != RdKafka::Conf::CONF_OK) {
        throw std::runtime_error("Kafka producer config failed for " + key + ": " + error);
    }
}

void set_if_supported(RdKafka::Conf& conf, const std::string& key, const std::string& value) {
    std::string error;
    (void)conf.set(key, value, error);
}

int flush_timeout_ms(int timeout_ms) {
    return timeout_ms == 0 ? -1 : std::max(0, timeout_ms);
}

struct PendingDelivery {
    explicit PendingDelivery(DeliveryCallback cb) : callback(std::move(cb)) {}
    DeliveryCallback callback;
};

class ProducerDeliveryCallback final : public RdKafka::DeliveryReportCb {
public:
    void dr_cb(RdKafka::Message& message) override {
        std::unique_ptr<PendingDelivery> pending(
            static_cast<PendingDelivery*>(message.msg_opaque()));
        if (!pending || !pending->callback) {
            return;
        }

        if (message.err() == RdKafka::ERR_NO_ERROR) {
            pending->callback(true, "");
        } else {
            pending->callback(false, message.errstr());
        }
    }
};

#endif

} // namespace

struct KafkaProducer::Impl {
#if SIGNALROUTE_HAS_KAFKA
    ProducerDeliveryCallback delivery_callback;
    std::unique_ptr<RdKafka::Producer> producer;
#endif
};

KafkaProducer::KafkaProducer(const KafkaConfig& config)
    : config_(config), impl_(std::make_unique<Impl>()) {
#if SIGNALROUTE_HAS_KAFKA
    std::unique_ptr<RdKafka::Conf> conf(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    if (!conf) {
        throw std::runtime_error("failed to create Kafka producer config");
    }

    set_required(*conf, "metadata.broker.list", config_.brokers);
    set_required(*conf, "dr_cb", &impl_->delivery_callback);
    set_if_supported(*conf, "acks", "all");
    set_if_supported(*conf, "compression.type", "lz4");
    set_if_supported(*conf, "queue.buffering.max.ms", std::to_string(config_.linger_ms));
    set_if_supported(*conf, "batch.size", std::to_string(config_.batch_size_bytes));

    std::string error;
    impl_->producer.reset(RdKafka::Producer::create(conf.get(), error));
    if (!impl_->producer) {
        throw std::runtime_error("failed to create Kafka producer: " + error);
    }
#else
    std::cerr << "[KafkaProducer] WARNING: using in-memory Kafka fallback.\n";
#endif
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

#if SIGNALROUTE_HAS_KAFKA
    auto* pending = callback ? new PendingDelivery(std::move(callback)) : nullptr;
    const auto error = impl_->producer->produce(
        topic,
        RdKafka::Topic::PARTITION_UA,
        RdKafka::Producer::RK_MSG_COPY,
        const_cast<char*>(payload.data()),
        payload.size(),
        key.data(),
        key.size(),
        0,
        pending);

    if (error != RdKafka::ERR_NO_ERROR) {
        std::unique_ptr<PendingDelivery> cleanup(pending);
        const std::string message = RdKafka::err2str(error);
        if (cleanup && cleanup->callback) {
            cleanup->callback(false, message);
        }
        throw std::runtime_error("Kafka produce failed: " + message);
    }
#else
    (void)kafka_fallback::append_message(topic, key, payload);
    if (callback) {
        callback(true, "");
    }
#endif
}

void KafkaProducer::flush(int timeout_ms) {
#if SIGNALROUTE_HAS_KAFKA
    if (impl_->producer) {
        (void)impl_->producer->flush(flush_timeout_ms(timeout_ms));
    }
#else
    (void)timeout_ms;
    // In-memory fallback produces synchronously.
#endif
}

int KafkaProducer::poll(int timeout_ms) {
#if SIGNALROUTE_HAS_KAFKA
    if (!impl_->producer) {
        return 0;
    }
    return impl_->producer->poll(timeout_ms < 0 ? 0 : timeout_ms);
#else
    (void)timeout_ms;
    // Delivery callbacks fire synchronously in the fallback.
    return 0;
#endif
}

bool KafkaProducer::is_connected() const {
#if SIGNALROUTE_HAS_KAFKA
    return impl_ && impl_->producer != nullptr;
#else
    return true;
#endif
}

} // namespace signalroute
