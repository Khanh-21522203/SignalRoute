#pragma once

#include "../types/location_event.h"
#include "state_events.h"
#include "history_events.h"
#include "geofence_events.h"

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

} // namespace signalroute::events
