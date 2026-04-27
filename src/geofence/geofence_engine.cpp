#include "geofence_engine.h"

#include "evaluator.h"
#include "fence_registry.h"
#include "geofence_event_handlers.h"
#include "../common/clients/postgres_client.h"
#include "../common/clients/redis_client.h"
#include "../common/events/event_bus.h"
#include "../common/kafka/kafka_producer.h"

#include <iostream>
#include <utility>

namespace signalroute {

GeofenceEngine::GeofenceEngine() = default;
GeofenceEngine::~GeofenceEngine() { if (running_) stop(); }

void GeofenceEngine::start(const Config& config) {
    start_with_bus(config, nullptr);
}

void GeofenceEngine::start(const Config& config, EventBus& event_bus) {
    start_with_bus(config, &event_bus);
}

void GeofenceEngine::start_with_bus(const Config& config, EventBus* external_bus) {
    if (running_) {
        return;
    }

    std::cout << "[GeofenceEngine] Starting...\n";
    lifecycle_state_.store(ServiceLifecycleState::Starting);

    if (external_bus == nullptr) {
        owned_bus_ = std::make_unique<EventBus>();
        event_bus_ = owned_bus_.get();
    } else {
        owned_bus_.reset();
        event_bus_ = external_bus;
    }

    redis_ = std::make_unique<RedisClient>(config.redis);
    pg_ = std::make_unique<PostgresClient>(config.postgis);
    producer_ = std::make_unique<KafkaProducer>(config.kafka);
    registry_ = std::make_unique<FenceRegistry>();
    registry_->load(*pg_);
    evaluator_ = std::make_unique<Evaluator>(
        *registry_,
        *redis_,
        *producer_,
        *pg_,
        config.kafka.geofence_topic);
    handlers_ = std::make_unique<GeofenceEventHandlers>(*event_bus_, *evaluator_);
    handlers_->wire();

    running_ = true;
    lifecycle_state_.store(ServiceLifecycleState::Ready);
    std::cout << "[GeofenceEngine] Started.\n";
}

void GeofenceEngine::stop() {
    if (!running_) {
        return;
    }

    std::cout << "[GeofenceEngine] Stopping...\n";
    lifecycle_state_.store(ServiceLifecycleState::Draining);
    if (handlers_) {
        handlers_->clear();
    }
    running_ = false;
    lifecycle_state_.store(ServiceLifecycleState::Stopped);
    std::cout << "[GeofenceEngine] Stopped.\n";
}

bool GeofenceEngine::is_healthy() const { return running_; }

bool GeofenceEngine::is_ready() const {
    return lifecycle_state_.load() == ServiceLifecycleState::Ready;
}

ServiceHealthSnapshot GeofenceEngine::health_snapshot() const {
    switch (lifecycle_state_.load()) {
        case ServiceLifecycleState::Ready: return ready_health("geofence evaluating events");
        case ServiceLifecycleState::Starting: return starting_health("geofence starting");
        case ServiceLifecycleState::Draining: return draining_health("geofence draining");
        case ServiceLifecycleState::Failed: return failed_health("geofence failed");
        case ServiceLifecycleState::Stopped: return stopped_health("geofence stopped");
    }
    return failed_health("geofence unknown lifecycle state");
}

bool GeofenceEngine::is_event_driven() const {
    return event_bus_ != nullptr && handlers_ != nullptr;
}

std::size_t GeofenceEngine::subscription_count() const {
    return handlers_ ? handlers_->subscription_count() : 0;
}

std::size_t GeofenceEngine::fence_count() const {
    return registry_ ? registry_->fence_count() : 0;
}

std::size_t GeofenceEngine::geofence_event_count_for_test() const {
    return pg_ ? pg_->geofence_event_count() : 0;
}

void GeofenceEngine::load_fences_for_test(std::vector<GeofenceRule> fences) {
    if (!pg_ || !registry_) {
        return;
    }
    pg_->set_active_fences(std::move(fences));
    registry_->load(*pg_);
}

} // namespace signalroute
