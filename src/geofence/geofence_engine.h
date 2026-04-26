#pragma once

/**
 * SignalRoute — Geofence Engine
 *
 * Event-driven geofence evaluation. Receives notifications from the
 * Location Processor when a device's H3 cell changes, then:
 *   1. H3 pre-filter: find candidate fences whose polyfill contains the cell
 *   2. Exact polygon containment test
 *   3. State transition: OUTSIDE ↔ INSIDE ↔ DWELL
 *   4. Emit GeofenceEvent to Kafka on state change
 *   5. Background dwell checker for INSIDE → DWELL transitions
 */

#include "../common/config/config.h"
#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>

namespace signalroute {

class EventBus;
class Evaluator;
class FenceRegistry;
class GeofenceEventHandlers;
class KafkaProducer;
class PostgresClient;
class RedisClient;
struct GeofenceRule;

class GeofenceEngine {
public:
    GeofenceEngine();
    ~GeofenceEngine();

    void start(const Config& config);
    void start(const Config& config, EventBus& event_bus);
    void stop();
    bool is_healthy() const;
    bool is_event_driven() const;
    std::size_t subscription_count() const;
    std::size_t fence_count() const;
    std::size_t geofence_event_count_for_test() const;
    void load_fences_for_test(std::vector<GeofenceRule> fences);

private:
    void start_with_bus(const Config& config, EventBus* external_bus);

    std::atomic<bool> running_{false};
    std::unique_ptr<EventBus> owned_bus_;
    EventBus* event_bus_ = nullptr;

    std::unique_ptr<RedisClient> redis_;
    std::unique_ptr<PostgresClient> pg_;
    std::unique_ptr<KafkaProducer> producer_;
    std::unique_ptr<FenceRegistry> registry_;
    std::unique_ptr<Evaluator> evaluator_;
    std::unique_ptr<GeofenceEventHandlers> handlers_;
};

} // namespace signalroute
