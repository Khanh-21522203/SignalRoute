#pragma once

#include "../types/geofence_types.h"

#include <cstdint>
#include <string>

namespace signalroute::events {

struct GeofenceEvaluationRequested {
    std::string device_id;
    int64_t old_h3_cell = 0;
    int64_t new_h3_cell = 0;
    double lat = 0.0;
    double lon = 0.0;
    int64_t timestamp_ms = 0;
};

struct GeofenceEntered {
    GeofenceEventRecord event;
};

struct GeofenceExited {
    GeofenceEventRecord event;
};

struct GeofenceDwellDetected {
    GeofenceEventRecord event;
};

struct GeofenceEvaluationFailed {
    std::string device_id;
    std::string reason;
};

} // namespace signalroute::events
