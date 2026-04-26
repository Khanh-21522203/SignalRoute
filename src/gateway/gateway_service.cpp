#include "gateway_service.h"

#include "rate_limiter.h"
#include "validator.h"
#include "../common/events/all_events.h"
#include "../common/events/event_bus.h"
#include "../common/kafka/kafka_producer.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <utility>

namespace signalroute {
namespace {

int64_t now_ms() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::string serialize_location_payload(const LocationEvent& event) {
    std::ostringstream out;
    out << event.device_id << ','
        << event.seq << ','
        << event.timestamp_ms << ','
        << event.server_recv_ms << ','
        << event.lat << ','
        << event.lon;
    return out.str();
}

} // namespace

GatewayService::GatewayService() = default;
GatewayService::~GatewayService() { if (running_) stop(); }

void GatewayService::start(const Config& config) {
    start_with_bus(config, nullptr);
}

void GatewayService::start(const Config& config, EventBus& event_bus) {
    start_with_bus(config, &event_bus);
}

void GatewayService::start_with_bus(const Config& config, EventBus* external_bus) {
    if (running_) {
        return;
    }

    std::cout << "[GatewayService] Starting...\n";
    config_ = config;
    if (external_bus == nullptr) {
        owned_bus_ = std::make_unique<EventBus>();
        event_bus_ = owned_bus_.get();
    } else {
        owned_bus_.reset();
        event_bus_ = external_bus;
    }

    producer_ = std::make_unique<KafkaProducer>(config.kafka);
    validator_ = std::make_unique<Validator>(config.gateway);
    rate_limiter_ = std::make_unique<RateLimiter>(config.gateway.rate_limit_rps_per_device);
    running_ = true;
    // gRPC/UDP servers are integrated in the transport dependency milestone.
    std::cout << "[GatewayService] Started.\n";
}

void GatewayService::stop() {
    if (!running_) {
        return;
    }

    std::cout << "[GatewayService] Stopping...\n";
    if (producer_) {
        producer_->flush(5000);
    }
    running_ = false;
    std::cout << "[GatewayService] Stopped.\n";
}

bool GatewayService::is_healthy() const {
    return running_;
}

bool GatewayService::is_event_driven() const {
    return event_bus_ != nullptr;
}

Result<LocationEvent, std::string> GatewayService::ingest_one(LocationEvent event) {
    if (!running_ || !producer_ || !validator_ || !rate_limiter_) {
        return Result<LocationEvent, std::string>::err("gateway service is not running");
    }

    if (event.server_recv_ms == 0) {
        event.server_recv_ms = now_ms();
    }

    auto validation = validator_->validate(event);
    if (validation.is_err()) {
        if (event_bus_) {
            event_bus_->publish(events::IngestBatchRejected{
                event.device_id,
                validation.error(),
                1});
        }
        return Result<LocationEvent, std::string>::err(validation.error());
    }

    if (!rate_limiter_->allow(event.device_id)) {
        const std::string reason = "rate limited";
        if (event_bus_) {
            event_bus_->publish(events::GatewayBackpressureApplied{reason, 0});
            event_bus_->publish(events::IngestBatchRejected{event.device_id, reason, 1});
        }
        return Result<LocationEvent, std::string>::err(reason);
    }

    producer_->produce(
        config_.kafka.ingest_topic,
        event.device_id,
        serialize_location_payload(event));

    if (event_bus_) {
        event_bus_->publish(events::IngestEventPublished{event, config_.kafka.ingest_topic});
    }

    return Result<LocationEvent, std::string>::ok(std::move(event));
}

IngestResult GatewayService::ingest_batch(const std::vector<LocationEvent>& batch) {
    IngestResult result;
    if (event_bus_) {
        const std::string device_id = batch.empty() ? "" : batch.front().device_id;
        event_bus_->publish(events::IngestBatchReceived{device_id, batch, now_ms()});
    }

    if (validator_) {
        const auto validation_results = validator_->validate_batch(batch);
        if (!validation_results.empty()) {
            bool batch_level_reject = true;
            for (const auto& validation : validation_results) {
                if (validation.is_ok() || validation.error() != "batch too large") {
                    batch_level_reject = false;
                    break;
                }
            }
            if (batch_level_reject) {
                result.rejected = static_cast<int>(batch.size());
                result.errors.assign(batch.size(), "batch too large");
                if (event_bus_) {
                    event_bus_->publish(events::IngestBatchRejected{
                        batch.empty() ? "" : batch.front().device_id,
                        "batch too large",
                        result.rejected});
                }
                return result;
            }
        }
    }

    for (auto event : batch) {
        auto one = ingest_one(std::move(event));
        if (one.is_ok()) {
            ++result.accepted;
        } else {
            ++result.rejected;
            result.errors.push_back(one.error());
        }
    }
    return result;
}

std::size_t GatewayService::tracked_devices_for_test() const {
    return rate_limiter_ ? rate_limiter_->tracked_devices() : 0;
}

} // namespace signalroute
