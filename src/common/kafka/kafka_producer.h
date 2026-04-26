#pragma once

/**
 * SignalRoute — Kafka Producer Wrapper
 *
 * Async Kafka producer with batching, compression, and backpressure.
 * Partitions by device_id (consistent hashing via Kafka partitioner).
 *
 * Dependencies: librdkafka / cppkafka
 */

#include "../config/config.h"
#include <string>
#include <functional>
#include <cstdint>
#include <memory>

namespace signalroute {

/**
 * Delivery callback signature.
 * Called asynchronously when a message is acknowledged (or fails).
 */
using DeliveryCallback = std::function<void(bool success, const std::string& error)>;

class KafkaProducer {
public:
    explicit KafkaProducer(const KafkaConfig& config);
    ~KafkaProducer();

    KafkaProducer(const KafkaProducer&) = delete;
    KafkaProducer& operator=(const KafkaProducer&) = delete;

    /**
     * Produce a message to a topic.
     * Partition is selected by key (device_id) via consistent hashing.
     *
     * @param topic Kafka topic name
     * @param key Partitioning key (device_id)
     * @param payload Serialized message bytes
     * @param callback Optional delivery callback
     *
     * TODO: Implement using cppkafka::Producer::produce()
     *       Configure: acks=all, linger.ms, batch.size, compression.type=lz4
     */
    void produce(const std::string& topic,
                 const std::string& key,
                 const std::string& payload,
                 DeliveryCallback callback = nullptr);

    /**
     * Flush all pending messages. Blocks until all messages are delivered
     * or timeout expires.
     *
     * @param timeout_ms Max time to wait (0 = infinite)
     */
    void flush(int timeout_ms = 5000);

    /**
     * Poll for delivery callbacks. Should be called periodically from
     * the producing thread's event loop.
     *
     * @param timeout_ms Poll timeout
     * @return Number of callbacks fired
     */
    int poll(int timeout_ms = 0);

    /// Check if broker is reachable.
    bool is_connected() const;

private:
    KafkaConfig config_;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace signalroute
