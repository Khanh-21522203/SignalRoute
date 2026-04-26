#pragma once

#include "domain_conversions.h"

#if SIGNALROUTE_HAS_PROTOBUF
#include "signalroute/geofence.pb.h"
#include "signalroute/location.pb.h"
#include "signalroute/matching.pb.h"
#include "signalroute/query.pb.h"
#endif

namespace signalroute::proto_boundary {

#if SIGNALROUTE_HAS_PROTOBUF

signalroute::v1::LocationEvent to_generated_location_event(const LocationEvent& event);
Result<LocationEvent, std::string> location_event_from_generated(const signalroute::v1::LocationEvent& message);

signalroute::v1::DeviceStateResponse to_generated_device_state_response(const DeviceState& state);

signalroute::v1::GeofenceEvent to_generated_geofence_event(const GeofenceEventRecord& event);
Result<GeofenceEventRecord, std::string> geofence_event_from_generated(const signalroute::v1::GeofenceEvent& message);

signalroute::v1::MatchRequest to_generated_match_request(const MatchRequest& request);
Result<MatchRequest, std::string> match_request_from_generated(const signalroute::v1::MatchRequest& message);

signalroute::v1::MatchResult to_generated_match_result(const MatchResult& result,
                                                       const std::string& requester_id = {},
                                                       const std::string& strategy = {});
Result<MatchResult, std::string> match_result_from_generated(const signalroute::v1::MatchResult& message);

#endif

} // namespace signalroute::proto_boundary
