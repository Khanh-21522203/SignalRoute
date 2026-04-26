#include "processor_service.h"

#include "dedup_window.h"
#include "history_writer.h"
#include "processing_loop.h"
#include "processor_event_handlers.h"
#include "sequence_guard.h"
#include "state_writer.h"
#include "../common/clients/postgres_client.h"
#include "../common/clients/redis_client.h"
#include "../common/composition/composition_root.h"
#include "../common/composition/metrics_event_handlers.h"
#include "../common/events/event_bus.h"
#include "../common/kafka/kafka_consumer.h"
#include "../common/kafka/kafka_producer.h"
#include "../common/spatial/h3_index.h"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace signalroute {

ProcessorService::ProcessorService() = default;
ProcessorService::~ProcessorService() { if (running_) stop(); }

void ProcessorService::start(const Config& config) {
    start_with_bus(config, nullptr);
}

void ProcessorService::start(const Config& config, EventBus& event_bus) {
    start_with_bus(config, &event_bus);
}

void ProcessorService::start_with_bus(const Config& config, EventBus* external_bus) {
    if (running_) {
        return;
    }

    std::cout << "[ProcessorService] Starting...\n";

    should_stop_.store(false);
    if (external_bus == nullptr) {
        owned_bus_ = std::make_unique<EventBus>();
        event_bus_ = owned_bus_.get();
    } else {
        owned_bus_.reset();
        event_bus_ = external_bus;
    }

    consumer_ = std::make_unique<KafkaConsumer>(
        config.kafka,
        std::vector<std::string>{config.kafka.ingest_topic});
    dlq_ = std::make_unique<KafkaProducer>(config.kafka);
    redis_ = std::make_unique<RedisClient>(config.redis);
    pg_ = std::make_unique<PostgresClient>(config.postgis);
    h3_ = std::make_unique<H3Index>(config.spatial.h3_resolution);
    dedup_ = std::make_unique<DedupWindow>(
        static_cast<std::size_t>(config.processor.dedup_max_entries),
        config.processor.dedup_ttl_s);
    seq_guard_ = std::make_unique<SequenceGuard>(*redis_);
    state_writer_ = std::make_unique<StateWriter>(*redis_, *h3_, config.redis.device_ttl_s);
    history_writer_ = std::make_unique<HistoryWriter>(*pg_, *dlq_, config.processor);

    composition_root_ = std::make_unique<CompositionRoot>(*event_bus_);
    processor_handlers_ = std::make_unique<ProcessorEventHandlers>(
        *event_bus_,
        *state_writer_,
        *history_writer_);
    metrics_handlers_ = std::make_unique<MetricsEventHandlers>(*event_bus_);

    composition_root_->wire_location_pipeline_observers();
    processor_handlers_->wire();
    metrics_handlers_->wire();

    processing_loop_ = std::make_unique<ProcessingLoop>(
        *consumer_,
        *dedup_,
        *seq_guard_,
        *state_writer_,
        *history_writer_,
        config.processor,
        *event_bus_);

    running_ = true;
    worker_ = std::thread([this] { processing_loop_->run(should_stop_); });
    std::cout << "[ProcessorService] Started.\n";
}

void ProcessorService::stop() {
    if (!running_) {
        return;
    }

    std::cout << "[ProcessorService] Stopping...\n";
    should_stop_.store(true);
    if (worker_.joinable()) {
        worker_.join();
    }
    if (history_writer_) {
        history_writer_->flush();
    }
    if (processor_handlers_) {
        processor_handlers_->clear();
    }
    if (metrics_handlers_) {
        metrics_handlers_->clear();
    }
    if (composition_root_) {
        composition_root_->clear();
    }

    running_ = false;
    std::cout << "[ProcessorService] Stopped.\n";
}

bool ProcessorService::is_healthy() const { return running_; }

bool ProcessorService::is_event_driven() const {
    return event_bus_ != nullptr && processing_loop_ != nullptr;
}

std::size_t ProcessorService::subscription_count() const {
    std::size_t count = 0;
    if (composition_root_) {
        count += composition_root_->subscription_count();
    }
    if (processor_handlers_) {
        count += processor_handlers_->subscription_count();
    }
    if (metrics_handlers_) {
        count += metrics_handlers_->subscription_count();
    }
    return count;
}

std::optional<DeviceState> ProcessorService::latest_state_for_test(const std::string& device_id) const {
    if (!redis_) {
        return std::nullopt;
    }
    return redis_->get_device_state(device_id);
}

std::size_t ProcessorService::trip_point_count_for_test() const {
    return pg_ ? pg_->trip_point_count() : 0;
}

} // namespace signalroute
