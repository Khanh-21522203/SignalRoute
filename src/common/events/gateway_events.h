#pragma once

#include "../types/location_event.h"

#include <cstdint>
#include <string>
#include <vector>

namespace signalroute::events {

struct IngestBatchReceived {
    std::string device_id;
    std::vector<LocationEvent> events;
    int64_t received_at_ms = 0;
};

struct IngestBatchRejected {
    std::string device_id;
    std::string reason;
    int rejected_count = 0;
};

struct IngestEventPublished {
    LocationEvent event;
    std::string topic;
};

struct GatewayBackpressureApplied {
    std::string reason;
    int64_t duration_ms = 0;
};

} // namespace signalroute::events
