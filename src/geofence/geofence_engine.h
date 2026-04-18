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

namespace signalroute {

class GeofenceEngine {
public:
    GeofenceEngine();
    ~GeofenceEngine();

    void start(const Config& config);
    void stop();
    bool is_healthy() const;

private:
    std::atomic<bool> running_{false};
};

} // namespace signalroute
