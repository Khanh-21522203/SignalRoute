#pragma once

#include "wire_types.h"
#include "../types/device_state.h"
#include "../types/geofence_types.h"
#include "../types/location_event.h"
#include "../types/matching_types.h"
#include "../types/result.h"

#include <string>

namespace signalroute::proto_boundary {

wire::LocationEventMessage to_wire(const LocationEvent& event);
Result<LocationEvent, std::string> location_event_from_wire(const wire::LocationEventMessage& message);

wire::DeviceStateResponseMessage to_wire(const DeviceState& state);

wire::GeofenceEventMessage to_wire(const GeofenceEventRecord& event);
Result<GeofenceEventRecord, std::string> geofence_event_from_wire(const wire::GeofenceEventMessage& message);

wire::MatchRequestMessage to_wire(const MatchRequest& request);
Result<MatchRequest, std::string> match_request_from_wire(const wire::MatchRequestMessage& message);

wire::MatchResultMessage to_wire(const MatchResult& result,
                                 const std::string& requester_id = {},
                                 const std::string& strategy = {});
Result<MatchResult, std::string> match_result_from_wire(const wire::MatchResultMessage& message);

} // namespace signalroute::proto_boundary
