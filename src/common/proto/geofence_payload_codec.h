#pragma once

#include "../types/geofence_types.h"
#include "../types/result.h"

#include <string>
#include <string_view>

namespace signalroute::proto_boundary {

std::string encode_geofence_event_payload(const GeofenceEventRecord& event);
Result<GeofenceEventRecord, std::string> decode_geofence_event_payload(const std::string& payload);
std::string_view protobuf_geofence_event_payload_prefix();

} // namespace signalroute::proto_boundary
