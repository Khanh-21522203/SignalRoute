#include "geofence_engine.h"
#include <iostream>

namespace signalroute {

GeofenceEngine::GeofenceEngine() = default;
GeofenceEngine::~GeofenceEngine() { if (running_) stop(); }

void GeofenceEngine::start(const Config& /*config*/) {
    std::cout << "[GeofenceEngine] Starting...\n";
    running_ = true;
    // TODO: Load FenceRegistry from PostGIS, start dwell checker
    std::cout << "[GeofenceEngine] Started.\n";
}

void GeofenceEngine::stop() {
    std::cout << "[GeofenceEngine] Stopping...\n";
    running_ = false;
    std::cout << "[GeofenceEngine] Stopped.\n";
}

bool GeofenceEngine::is_healthy() const { return running_; }

} // namespace signalroute
