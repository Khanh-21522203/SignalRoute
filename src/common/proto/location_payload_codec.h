#pragma once

#include "../types/location_event.h"
#include "../types/result.h"

#include <string>
#include <string_view>

namespace signalroute::proto_boundary {

std::string encode_location_payload(const LocationEvent& event);
Result<LocationEvent, std::string> decode_location_payload(const std::string& payload);
std::string_view protobuf_location_payload_prefix();

} // namespace signalroute::proto_boundary
