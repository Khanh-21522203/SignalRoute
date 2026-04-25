#pragma once

#include "../types/device_state.h"
#include "../types/location_event.h"

#include <cstdint>
#include <string>

namespace signalroute::events {

struct StateWriteRequested {
    LocationEvent event;
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

} // namespace signalroute::events
