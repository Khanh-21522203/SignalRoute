#include "kafka_consumer.h"
#include <iostream>

// TODO: #include <cppkafka/cppkafka.h>

namespace signalroute {

KafkaConsumer::KafkaConsumer(const KafkaConfig& config,
                             const std::vector<std::string>& topics)
    : config_(config), topics_(topics)
{
    // TODO: Configure and create consumer
    //
    //   cppkafka::Configuration kafka_config = {
    //       {"metadata.broker.list", config.brokers},
    //       {"group.id", config.consumer_group},
    //       {"enable.auto.commit", false},      // Manual commit for at-least-once
    //       {"auto.offset.reset", "earliest"},
    //       {"max.poll.interval.ms", 300000},
    //       {"session.timeout.ms", 30000},
    //   };
    //
    //   consumer_ = std::make_unique<cppkafka::Consumer>(kafka_config);
    //   consumer_->subscribe(topics);
    //
    //   // Set rebalance callback for graceful partition reassignment
    //   consumer_->set_assignment_callback([](const auto& partitions) {
    //       // Log partition assignment
    //   });
    //   consumer_->set_revocation_callback([](const auto& partitions) {
    //       // Commit offsets before partition loss
    //   });

    std::cerr << "[KafkaConsumer] WARNING: Kafka consumer not yet implemented.\n";
}

KafkaConsumer::~KafkaConsumer() {
    // TODO: consumer_->unsubscribe();
}

std::optional<KafkaMessage> KafkaConsumer::poll(int /*timeout_ms*/) {
    // TODO: Implement using cppkafka::Consumer::poll()
    //   auto msg = consumer_->poll(std::chrono::milliseconds(timeout_ms));
    //   if (!msg || msg.get_error()) return std::nullopt;
    //   return KafkaMessage{msg.get_topic(), msg.get_partition(), msg.get_offset(),
    //                       std::string(msg.get_key()), std::string(msg.get_payload()),
    //                       msg.get_timestamp().get_timestamp().count()};
    return std::nullopt;
}

void KafkaConsumer::poll_loop(MessageHandler handler,
                               const std::atomic<bool>& should_stop) {
    // TODO: Implement main processing loop
    //
    //   while (!should_stop.load()) {
    //       auto msg = poll(100);
    //       if (!msg) continue;
    //
    //       bool ok = handler(*msg);
    //       if (ok) {
    //           commit(*msg);
    //       }
    //       // If !ok, message will be redelivered on restart (at-least-once)
    //   }
}

void KafkaConsumer::commit() {
    // TODO: consumer_->commit();
}

void KafkaConsumer::commit(const KafkaMessage& /*msg*/) {
    // TODO: consumer_->commit(msg);
}

std::vector<std::pair<int32_t, int64_t>> KafkaConsumer::get_lag() const {
    // TODO: Query committed vs high watermark offsets
    return {};
}

bool KafkaConsumer::is_connected() const {
    return false;
}

} // namespace signalroute
