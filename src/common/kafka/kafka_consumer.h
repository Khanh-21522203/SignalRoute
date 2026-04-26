#pragma once

/**
 * SignalRoute — Kafka Consumer Wrapper
 *
 * Kafka consumer with manual offset commit, rebalance handling,
 * and configurable poll loop.
 *
 * Used by Location Processor and Matching Server.
 *
 * Dependencies: librdkafka / cppkafka
 */

#include "../config/config.h"
#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <cstdint>
#include <atomic>
#include <unordered_map>

namespace signalroute {

/// A consumed Kafka message.
struct KafkaMessage {
    std::string topic;
    int32_t     partition;
    int64_t     offset;
    std::string key;
    std::string payload;
    int64_t     timestamp_ms;
};

/**
 * Message handler callback.
 * Return true to indicate successful processing (offset will be committed).
 */
using MessageHandler = std::function<bool(const KafkaMessage& msg)>;

class KafkaConsumer {
public:
    /**
     * @param config Kafka configuration
     * @param topics Topics to subscribe to
     */
    KafkaConsumer(const KafkaConfig& config, const std::vector<std::string>& topics);
    ~KafkaConsumer();

    KafkaConsumer(const KafkaConsumer&) = delete;
    KafkaConsumer& operator=(const KafkaConsumer&) = delete;

    /**
     * Poll for the next message.
     *
     * @param timeout_ms Poll timeout
     * @return Message if available, std::nullopt on timeout
     *
     * TODO: Implement using cppkafka::Consumer::poll()
     */
    std::optional<KafkaMessage> poll(int timeout_ms = 100);

    /**
     * Poll and dispatch messages to the handler in a loop.
     * Call from the processor's main thread.
     *
     * @param handler Callback for each message. Return true to commit.
     * @param should_stop Atomic flag checked each iteration.
     *
     * TODO: Implement poll loop with manual offset commit
     */
    void poll_loop(MessageHandler handler, const std::atomic<bool>& should_stop);

    /**
     * Manually commit the current offsets.
     * Called after successful processing to ensure at-least-once delivery.
     *
     * TODO: Implement using cppkafka::Consumer::commit()
     */
    void commit();

    /**
     * Commit a specific message's offset.
     */
    void commit(const KafkaMessage& msg);

    /**
     * Get the current consumer lag per partition.
     * Used for metrics and backpressure signaling.
     */
    std::vector<std::pair<int32_t, int64_t>> get_lag() const;

    /// Check connectivity.
    bool is_connected() const;

private:
    KafkaConfig config_;
    std::vector<std::string> topics_;
    std::unordered_map<std::string, int64_t> next_offsets_;
    std::unordered_map<std::string, int64_t> committed_offsets_;
    std::optional<KafkaMessage> last_polled_;

    // TODO: Add cppkafka::Consumer instance
    // std::unique_ptr<cppkafka::Consumer> consumer_;
};

} // namespace signalroute
