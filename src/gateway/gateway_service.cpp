#include "gateway_service.h"

#include "admission_control.h"
#include "rate_limiter.h"
#include "validator.h"
#include "../common/events/all_events.h"
#include "../common/events/event_bus.h"
#include "../common/kafka/kafka_producer.h"
#include "../common/proto/location_payload_codec.h"

#include <chrono>
#include <iostream>
#include <utility>

namespace signalroute {
namespace {

int64_t now_ms() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
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
    lifecycle_state_.store(ServiceLifecycleState::Starting);
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
    admission_ = std::make_unique<GatewayAdmissionControl>(config.gateway);
    running_ = true;
    lifecycle_state_.store(ServiceLifecycleState::Ready);
    // gRPC/UDP servers are integrated in the transport dependency milestone.
    std::cout << "[GatewayService] Started.\n";
}

void GatewayService::stop() {
    if (!running_) {
        return;
    }

    std::cout << "[GatewayService] Stopping...\n";
    lifecycle_state_.store(ServiceLifecycleState::Draining);
    if (producer_) {
        producer_->flush(5000);
    }
    running_ = false;
    lifecycle_state_.store(ServiceLifecycleState::Stopped);
    std::cout << "[GatewayService] Stopped.\n";
}

bool GatewayService::is_healthy() const {
    return running_;
}

bool GatewayService::is_ready() const {
    return lifecycle_state_.load() == ServiceLifecycleState::Ready;
}

ServiceHealthSnapshot GatewayService::health_snapshot() const {
    switch (lifecycle_state_.load()) {
        case ServiceLifecycleState::Ready: return ready_health("gateway accepting ingest");
        case ServiceLifecycleState::Starting: return starting_health("gateway starting");
        case ServiceLifecycleState::Draining: return draining_health("gateway draining");
        case ServiceLifecycleState::Failed: return failed_health("gateway failed");
        case ServiceLifecycleState::Stopped: return stopped_health("gateway stopped");
    }
    return failed_health("gateway unknown lifecycle state");
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
        proto_boundary::encode_location_payload(event));

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

IngestResponse GatewayService::handle_ingest_one(IngestOneRequest request) {
    if (!admission_) {
        return reject_transport_request("gateway service is not running", 1, request.event.device_id, false);
    }
    auto auth = admission_->authorize(request.api_key);
    if (auth.is_err()) {
        return reject_transport_request(auth.error(), 1, request.event.device_id, false);
    }
    auto lease = admission_->try_acquire();
    if (lease.is_err()) {
        return reject_transport_request(lease.error(), 1, request.event.device_id, true);
    }

    IngestResponse response;
    auto result = ingest_one(std::move(request.event));
    if (result.is_ok()) {
        response.accepted = true;
        response.accepted_count = 1;
        response.accepted_events.push_back(result.value());
    } else {
        response.rejected_count = 1;
        response.errors.push_back(result.error());
    }
    return response;
}

IngestResponse GatewayService::handle_ingest_batch(IngestBatchRequest request) {
    const std::string device_id = request.events.empty() ? "" : request.events.front().device_id;
    const int rejected_count = request.events.empty() ? 1 : static_cast<int>(request.events.size());
    if (!admission_) {
        return reject_transport_request("gateway service is not running", rejected_count, device_id, false);
    }
    auto auth = admission_->authorize(request.api_key);
    if (auth.is_err()) {
        return reject_transport_request(auth.error(), rejected_count, device_id, false);
    }
    auto lease = admission_->try_acquire();
    if (lease.is_err()) {
        return reject_transport_request(lease.error(), rejected_count, device_id, true);
    }

    IngestResponse response;
    const auto result = ingest_batch(request.events);
    response.accepted = result.ok();
    response.accepted_count = result.accepted;
    response.rejected_count = result.rejected;
    response.errors = result.errors;
    return response;
}

std::size_t GatewayService::tracked_devices_for_test() const {
    return rate_limiter_ ? rate_limiter_->tracked_devices() : 0;
}

int GatewayService::in_flight_requests_for_test() const {
    return admission_ ? admission_->in_flight() : 0;
}

IngestResponse GatewayService::reject_transport_request(
    const std::string& reason,
    int rejected_count,
    const std::string& device_id,
    bool backpressure) {
    IngestResponse response;
    response.rejected_count = rejected_count;
    response.errors.push_back(reason);
    if (event_bus_) {
        if (backpressure) {
            event_bus_->publish(events::GatewayBackpressureApplied{reason, config_.gateway.queue_full_timeout_ms});
        }
        event_bus_->publish(events::IngestBatchRejected{device_id, reason, rejected_count});
    }
    return response;
}

} // namespace signalroute
