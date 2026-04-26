#include "domain_conversions.h"

#include <cmath>
#include <utility>

namespace signalroute::proto_boundary {
namespace {

bool valid_lat_lon(double lat, double lon) {
    return std::isfinite(lat) && std::isfinite(lon) &&
           lat >= -90.0 && lat <= 90.0 &&
           lon >= -180.0 && lon <= 180.0;
}

wire::GeofenceEventTypeMessage to_wire_type(GeofenceEventType type) {
    switch (type) {
        case GeofenceEventType::ENTER:
            return wire::GeofenceEventTypeMessage::GEOFENCE_ENTER;
        case GeofenceEventType::EXIT:
            return wire::GeofenceEventTypeMessage::GEOFENCE_EXIT;
        case GeofenceEventType::DWELL:
            return wire::GeofenceEventTypeMessage::GEOFENCE_DWELL;
    }
    return wire::GeofenceEventTypeMessage::GEOFENCE_ENTER;
}

GeofenceEventType from_wire_type(wire::GeofenceEventTypeMessage type) {
    switch (type) {
        case wire::GeofenceEventTypeMessage::GEOFENCE_ENTER:
            return GeofenceEventType::ENTER;
        case wire::GeofenceEventTypeMessage::GEOFENCE_EXIT:
            return GeofenceEventType::EXIT;
        case wire::GeofenceEventTypeMessage::GEOFENCE_DWELL:
            return GeofenceEventType::DWELL;
    }
    return GeofenceEventType::ENTER;
}

wire::MatchStatusMessage to_wire_status(MatchStatus status) {
    switch (status) {
        case MatchStatus::MATCHED:
            return wire::MatchStatusMessage::MATCH_SUCCESS;
        case MatchStatus::EXPIRED:
            return wire::MatchStatusMessage::MATCH_EXPIRED;
        case MatchStatus::FAILED:
            return wire::MatchStatusMessage::MATCH_FAILED;
    }
    return wire::MatchStatusMessage::MATCH_FAILED;
}

MatchStatus from_wire_status(wire::MatchStatusMessage status) {
    switch (status) {
        case wire::MatchStatusMessage::MATCH_SUCCESS:
            return MatchStatus::MATCHED;
        case wire::MatchStatusMessage::MATCH_EXPIRED:
            return MatchStatus::EXPIRED;
        case wire::MatchStatusMessage::MATCH_NO_CANDIDATES:
        case wire::MatchStatusMessage::MATCH_FAILED:
            return MatchStatus::FAILED;
    }
    return MatchStatus::FAILED;
}

} // namespace

wire::LocationEventMessage to_wire(const LocationEvent& event) {
    wire::LocationEventMessage message;
    message.device_id = event.device_id;
    message.lat = event.lat;
    message.lon = event.lon;
    message.altitude_m = event.altitude_m;
    message.accuracy_m = event.accuracy_m;
    message.speed_ms = event.speed_ms;
    message.heading_deg = event.heading_deg;
    message.timestamp_ms = event.timestamp_ms;
    message.server_recv_ms = event.server_recv_ms;
    message.seq = event.seq;
    message.metadata = event.metadata;
    return message;
}

Result<LocationEvent, std::string> location_event_from_wire(const wire::LocationEventMessage& message) {
    if (message.device_id.empty()) {
        return Result<LocationEvent, std::string>::err("device_id is required");
    }
    if (!valid_lat_lon(message.lat, message.lon)) {
        return Result<LocationEvent, std::string>::err("invalid coordinates");
    }

    LocationEvent event;
    event.device_id = message.device_id;
    event.lat = message.lat;
    event.lon = message.lon;
    event.altitude_m = message.altitude_m;
    event.accuracy_m = message.accuracy_m;
    event.speed_ms = message.speed_ms;
    event.heading_deg = message.heading_deg;
    event.timestamp_ms = message.timestamp_ms;
    event.server_recv_ms = message.server_recv_ms;
    event.seq = message.seq;
    event.metadata = message.metadata;
    return Result<LocationEvent, std::string>::ok(std::move(event));
}

wire::DeviceStateResponseMessage to_wire(const DeviceState& state) {
    wire::DeviceStateResponseMessage message;
    message.device_id = state.device_id;
    message.lat = state.lat;
    message.lon = state.lon;
    message.altitude_m = state.altitude_m;
    message.accuracy_m = state.accuracy_m;
    message.speed_ms = state.speed_ms;
    message.heading_deg = state.heading_deg;
    message.h3_cell = state.h3_cell;
    message.seq = state.seq;
    message.updated_at = state.updated_at;
    return message;
}

wire::GeofenceEventMessage to_wire(const GeofenceEventRecord& event) {
    wire::GeofenceEventMessage message;
    message.device_id = event.device_id;
    message.fence_id = event.fence_id;
    message.fence_name = event.fence_name;
    message.event_type = to_wire_type(event.event_type);
    message.lat = event.lat;
    message.lon = event.lon;
    message.event_ts_ms = event.event_ts_ms;
    message.inside_duration_s = event.inside_duration_s;
    return message;
}

Result<GeofenceEventRecord, std::string> geofence_event_from_wire(const wire::GeofenceEventMessage& message) {
    if (message.device_id.empty()) {
        return Result<GeofenceEventRecord, std::string>::err("device_id is required");
    }
    if (message.fence_id.empty()) {
        return Result<GeofenceEventRecord, std::string>::err("fence_id is required");
    }
    if (!valid_lat_lon(message.lat, message.lon)) {
        return Result<GeofenceEventRecord, std::string>::err("invalid coordinates");
    }

    GeofenceEventRecord event;
    event.device_id = message.device_id;
    event.fence_id = message.fence_id;
    event.fence_name = message.fence_name;
    event.event_type = from_wire_type(message.event_type);
    event.lat = message.lat;
    event.lon = message.lon;
    event.event_ts_ms = message.event_ts_ms;
    event.inside_duration_s = message.inside_duration_s;
    return Result<GeofenceEventRecord, std::string>::ok(std::move(event));
}

wire::MatchRequestMessage to_wire(const MatchRequest& request) {
    wire::MatchRequestMessage message;
    message.request_id = request.request_id;
    message.requester_id = request.requester_id;
    message.lat = request.lat;
    message.lon = request.lon;
    message.radius_m = request.radius_m;
    message.max_agents = request.max_agents;
    message.deadline_ms = request.deadline_ms;
    message.strategy = request.strategy;
    return message;
}

Result<MatchRequest, std::string> match_request_from_wire(const wire::MatchRequestMessage& message) {
    if (message.request_id.empty()) {
        return Result<MatchRequest, std::string>::err("request_id is required");
    }
    if (!valid_lat_lon(message.lat, message.lon)) {
        return Result<MatchRequest, std::string>::err("invalid coordinates");
    }
    if (!std::isfinite(message.radius_m) || message.radius_m <= 0.0) {
        return Result<MatchRequest, std::string>::err("radius_m must be positive");
    }

    MatchRequest request;
    request.request_id = message.request_id;
    request.requester_id = message.requester_id;
    request.lat = message.lat;
    request.lon = message.lon;
    request.radius_m = message.radius_m;
    request.max_agents = message.max_agents > 0 ? message.max_agents : 1;
    request.deadline_ms = message.deadline_ms;
    request.strategy = message.strategy;
    return Result<MatchRequest, std::string>::ok(std::move(request));
}

wire::MatchResultMessage to_wire(const MatchResult& result,
                                 const std::string& requester_id,
                                 const std::string& strategy) {
    wire::MatchResultMessage message;
    message.request_id = result.request_id;
    message.requester_id = requester_id;
    message.status = to_wire_status(result.status);
    message.assigned_agent_ids = result.assigned_agent_ids;
    message.latency_ms = result.latency_ms;
    message.strategy = strategy;
    message.failure_reason = result.reason;
    return message;
}

Result<MatchResult, std::string> match_result_from_wire(const wire::MatchResultMessage& message) {
    if (message.request_id.empty()) {
        return Result<MatchResult, std::string>::err("request_id is required");
    }

    MatchResult result;
    result.request_id = message.request_id;
    result.status = from_wire_status(message.status);
    result.assigned_agent_ids = message.assigned_agent_ids;
    result.reason = message.failure_reason;
    if (message.status == wire::MatchStatusMessage::MATCH_NO_CANDIDATES && result.reason.empty()) {
        result.reason = "no candidates";
    }
    result.latency_ms = message.latency_ms;
    return Result<MatchResult, std::string>::ok(std::move(result));
}

} // namespace signalroute::proto_boundary
