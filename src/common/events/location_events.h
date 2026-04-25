#pragma once

#include "../types/device_state.h"
#include "../types/location_event.h"

#include <cstdint>
#include <string>

namespace signalroute::events {

struct LocationReceived {
    LocationEvent event;
    int64_t received_at_ms = 0;
};

struct LocationValidated {
    LocationEvent event;
};

struct LocationRejected {
    LocationEvent event;
    std::string reason;
};

struct LocationAccepted {
    LocationEvent event;
};

struct LocationDuplicateRejected {
    LocationEvent event;
};

struct LocationStaleRejected {
    LocationEvent event;
    uint64_t current_seq = 0;
};

struct StateWriteSucceeded {
    LocationEvent event;
    DeviceState state;
    int64_t previous_h3_cell = 0;
};

struct StateWriteRejected {
    LocationEvent event;
    std::string reason;
};

struct StateWriteFailed {
    LocationEvent event;
    std::string reason;
};

struct TripHistoryWriteRequested {
    LocationEvent event;
};

struct TripHistoryWritten {
    LocationEvent event;
};

struct TripHistoryWriteFailed {
    LocationEvent event;
    std::string reason;
};

struct GeofenceEvaluationRequested {
    std::string device_id;
    int64_t old_h3_cell = 0;
    int64_t new_h3_cell = 0;
    double lat = 0.0;
    double lon = 0.0;
    int64_t timestamp_ms = 0;
};

} // namespace signalroute::events
