#pragma once

#include "../types/location_event.h"

#include <cstddef>
#include <string>
#include <vector>

namespace signalroute::events {

struct TripHistoryWriteRequested {
    LocationEvent event;
};

struct TripHistoryBatchWriteRequested {
    std::vector<LocationEvent> events;
};

struct TripHistoryWritten {
    LocationEvent event;
};

struct TripHistoryBatchWritten {
    std::size_t count = 0;
};

struct TripHistoryWriteFailed {
    LocationEvent event;
    std::string reason;
};

struct TripHistoryBatchWriteFailed {
    std::vector<LocationEvent> events;
    std::string reason;
};

} // namespace signalroute::events
