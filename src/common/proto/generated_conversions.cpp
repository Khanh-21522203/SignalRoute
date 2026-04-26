#include "generated_conversions.h"

#if SIGNALROUTE_HAS_PROTOBUF

#include <utility>

namespace signalroute::proto_boundary {
namespace {

signalroute::v1::GeofenceEventType to_generated_type(wire::GeofenceEventTypeMessage type) {
    switch (type) {
        case wire::GeofenceEventTypeMessage::GEOFENCE_ENTER:
            return signalroute::v1::GEOFENCE_ENTER;
        case wire::GeofenceEventTypeMessage::GEOFENCE_EXIT:
            return signalroute::v1::GEOFENCE_EXIT;
        case wire::GeofenceEventTypeMessage::GEOFENCE_DWELL:
            return signalroute::v1::GEOFENCE_DWELL;
    }
    return signalroute::v1::GEOFENCE_ENTER;
}

wire::GeofenceEventTypeMessage from_generated_type(signalroute::v1::GeofenceEventType type) {
    switch (type) {
        case signalroute::v1::GEOFENCE_ENTER:
            return wire::GeofenceEventTypeMessage::GEOFENCE_ENTER;
        case signalroute::v1::GEOFENCE_EXIT:
            return wire::GeofenceEventTypeMessage::GEOFENCE_EXIT;
        case signalroute::v1::GEOFENCE_DWELL:
            return wire::GeofenceEventTypeMessage::GEOFENCE_DWELL;
        default:
            return wire::GeofenceEventTypeMessage::GEOFENCE_ENTER;
    }
}

signalroute::v1::MatchStatus to_generated_status(wire::MatchStatusMessage status) {
    switch (status) {
        case wire::MatchStatusMessage::MATCH_SUCCESS:
            return signalroute::v1::MATCH_SUCCESS;
        case wire::MatchStatusMessage::MATCH_NO_CANDIDATES:
            return signalroute::v1::MATCH_NO_CANDIDATES;
        case wire::MatchStatusMessage::MATCH_FAILED:
            return signalroute::v1::MATCH_FAILED;
        case wire::MatchStatusMessage::MATCH_EXPIRED:
            return signalroute::v1::MATCH_EXPIRED;
    }
    return signalroute::v1::MATCH_FAILED;
}

wire::MatchStatusMessage from_generated_status(signalroute::v1::MatchStatus status) {
    switch (status) {
        case signalroute::v1::MATCH_SUCCESS:
            return wire::MatchStatusMessage::MATCH_SUCCESS;
        case signalroute::v1::MATCH_NO_CANDIDATES:
            return wire::MatchStatusMessage::MATCH_NO_CANDIDATES;
        case signalroute::v1::MATCH_FAILED:
            return wire::MatchStatusMessage::MATCH_FAILED;
        case signalroute::v1::MATCH_EXPIRED:
            return wire::MatchStatusMessage::MATCH_EXPIRED;
        default:
            return wire::MatchStatusMessage::MATCH_FAILED;
    }
}

} // namespace

signalroute::v1::LocationEvent to_generated_location_event(const LocationEvent& event) {
    const auto wire = to_wire(event);
    signalroute::v1::LocationEvent message;
    message.set_device_id(wire.device_id);
    message.set_lat(wire.lat);
    message.set_lon(wire.lon);
    message.set_altitude_m(wire.altitude_m);
    message.set_accuracy_m(wire.accuracy_m);
    message.set_speed_ms(wire.speed_ms);
    message.set_heading_deg(wire.heading_deg);
    message.set_timestamp_ms(wire.timestamp_ms);
    message.set_server_recv_ms(wire.server_recv_ms);
    message.set_seq(wire.seq);
    for (const auto& [key, value] : wire.metadata) {
        (*message.mutable_metadata())[key] = value;
    }
    return message;
}

Result<LocationEvent, std::string> location_event_from_generated(const signalroute::v1::LocationEvent& message) {
    wire::LocationEventMessage wire;
    wire.device_id = message.device_id();
    wire.lat = message.lat();
    wire.lon = message.lon();
    wire.altitude_m = message.altitude_m();
    wire.accuracy_m = message.accuracy_m();
    wire.speed_ms = message.speed_ms();
    wire.heading_deg = message.heading_deg();
    wire.timestamp_ms = message.timestamp_ms();
    wire.server_recv_ms = message.server_recv_ms();
    wire.seq = message.seq();
    for (const auto& [key, value] : message.metadata()) {
        wire.metadata[key] = value;
    }
    return location_event_from_wire(wire);
}

signalroute::v1::DeviceStateResponse to_generated_device_state_response(const DeviceState& state) {
    const auto wire = to_wire(state);
    signalroute::v1::DeviceStateResponse message;
    message.set_device_id(wire.device_id);
    message.set_lat(wire.lat);
    message.set_lon(wire.lon);
    message.set_altitude_m(wire.altitude_m);
    message.set_accuracy_m(wire.accuracy_m);
    message.set_speed_ms(wire.speed_ms);
    message.set_heading_deg(wire.heading_deg);
    message.set_h3_cell(wire.h3_cell);
    message.set_seq(wire.seq);
    message.set_updated_at(wire.updated_at);
    return message;
}

signalroute::v1::GeofenceEvent to_generated_geofence_event(const GeofenceEventRecord& event) {
    const auto wire = to_wire(event);
    signalroute::v1::GeofenceEvent message;
    message.set_device_id(wire.device_id);
    message.set_fence_id(wire.fence_id);
    message.set_fence_name(wire.fence_name);
    message.set_event_type(to_generated_type(wire.event_type));
    message.set_lat(wire.lat);
    message.set_lon(wire.lon);
    message.set_event_ts_ms(wire.event_ts_ms);
    message.set_inside_duration_s(wire.inside_duration_s);
    return message;
}

Result<GeofenceEventRecord, std::string> geofence_event_from_generated(const signalroute::v1::GeofenceEvent& message) {
    wire::GeofenceEventMessage wire;
    wire.device_id = message.device_id();
    wire.fence_id = message.fence_id();
    wire.fence_name = message.fence_name();
    wire.event_type = from_generated_type(message.event_type());
    wire.lat = message.lat();
    wire.lon = message.lon();
    wire.event_ts_ms = message.event_ts_ms();
    wire.inside_duration_s = message.inside_duration_s();
    return geofence_event_from_wire(wire);
}

signalroute::v1::MatchRequest to_generated_match_request(const MatchRequest& request) {
    const auto wire = to_wire(request);
    signalroute::v1::MatchRequest message;
    message.set_request_id(wire.request_id);
    message.set_requester_id(wire.requester_id);
    message.set_lat(wire.lat);
    message.set_lon(wire.lon);
    message.set_radius_m(wire.radius_m);
    message.set_max_agents(wire.max_agents);
    message.set_deadline_ms(wire.deadline_ms);
    message.set_strategy(wire.strategy);
    for (const auto& [key, value] : wire.metadata) {
        (*message.mutable_metadata())[key] = value;
    }
    return message;
}

Result<MatchRequest, std::string> match_request_from_generated(const signalroute::v1::MatchRequest& message) {
    wire::MatchRequestMessage wire;
    wire.request_id = message.request_id();
    wire.requester_id = message.requester_id();
    wire.lat = message.lat();
    wire.lon = message.lon();
    wire.radius_m = message.radius_m();
    wire.max_agents = message.max_agents();
    wire.deadline_ms = message.deadline_ms();
    wire.strategy = message.strategy();
    for (const auto& [key, value] : message.metadata()) {
        wire.metadata[key] = value;
    }
    return match_request_from_wire(wire);
}

signalroute::v1::MatchResult to_generated_match_result(const MatchResult& result,
                                                       const std::string& requester_id,
                                                       const std::string& strategy) {
    const auto wire = to_wire(result, requester_id, strategy);
    signalroute::v1::MatchResult message;
    message.set_request_id(wire.request_id);
    message.set_requester_id(wire.requester_id);
    message.set_status(to_generated_status(wire.status));
    for (const auto& agent_id : wire.assigned_agent_ids) {
        message.add_assigned_agent_ids(agent_id);
    }
    message.set_latency_ms(wire.latency_ms);
    message.set_strategy(wire.strategy);
    message.set_failure_reason(wire.failure_reason);
    return message;
}

Result<MatchResult, std::string> match_result_from_generated(const signalroute::v1::MatchResult& message) {
    wire::MatchResultMessage wire;
    wire.request_id = message.request_id();
    wire.requester_id = message.requester_id();
    wire.status = from_generated_status(message.status());
    wire.assigned_agent_ids.assign(message.assigned_agent_ids().begin(), message.assigned_agent_ids().end());
    wire.latency_ms = message.latency_ms();
    wire.strategy = message.strategy();
    wire.failure_reason = message.failure_reason();
    return match_result_from_wire(wire);
}

} // namespace signalroute::proto_boundary

#endif
