#include "common/proto/domain_conversions.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

void test_location_event_round_trip_preserves_fields_and_metadata() {
    signalroute::LocationEvent event;
    event.device_id = "dev-1";
    event.lat = 10.8231;
    event.lon = 106.6297;
    event.altitude_m = 7.5f;
    event.accuracy_m = 3.0f;
    event.speed_ms = 12.5f;
    event.heading_deg = 180.0f;
    event.timestamp_ms = 1000;
    event.server_recv_ms = 1100;
    event.seq = 42;
    event.metadata["source"] = "grpc";
    event.metadata["tenant"] = "fleet-a";

    const auto wire = signalroute::proto_boundary::to_wire(event);
    assert(wire.device_id == "dev-1");
    assert(wire.metadata.at("source") == "grpc");

    const auto parsed = signalroute::proto_boundary::location_event_from_wire(wire);
    assert(parsed.is_ok());
    const auto& round_trip = parsed.value();
    assert(round_trip.device_id == event.device_id);
    assert(round_trip.lat == event.lat);
    assert(round_trip.lon == event.lon);
    assert(round_trip.seq == event.seq);
    assert(round_trip.metadata.at("tenant") == "fleet-a");
}

void test_location_event_rejects_missing_required_fields_and_bad_coordinates() {
    signalroute::wire::LocationEventMessage missing_device;
    missing_device.lat = 1.0;
    missing_device.lon = 2.0;
    assert(signalroute::proto_boundary::location_event_from_wire(missing_device).is_err());

    signalroute::wire::LocationEventMessage bad_coordinates;
    bad_coordinates.device_id = "dev-1";
    bad_coordinates.lat = 91.0;
    bad_coordinates.lon = 2.0;
    auto parsed = signalroute::proto_boundary::location_event_from_wire(bad_coordinates);
    assert(parsed.is_err());
    assert(parsed.error() == "invalid coordinates");
}

void test_device_state_to_query_response_wire() {
    signalroute::DeviceState state;
    state.device_id = "dev-2";
    state.lat = 11.0;
    state.lon = 107.0;
    state.h3_cell = 123456;
    state.seq = 77;
    state.updated_at = 2000;

    const auto wire = signalroute::proto_boundary::to_wire(state);
    assert(wire.device_id == "dev-2");
    assert(wire.h3_cell == 123456);
    assert(wire.seq == 77);
    assert(wire.updated_at == 2000);
}

void test_geofence_event_round_trip_maps_enum() {
    signalroute::GeofenceEventRecord event;
    event.device_id = "dev-3";
    event.fence_id = "fence-1";
    event.fence_name = "Depot";
    event.event_type = signalroute::GeofenceEventType::DWELL;
    event.lat = 10.0;
    event.lon = 106.0;
    event.event_ts_ms = 3000;
    event.inside_duration_s = 600;

    const auto wire = signalroute::proto_boundary::to_wire(event);
    assert(wire.event_type == signalroute::wire::GeofenceEventTypeMessage::GEOFENCE_DWELL);

    const auto parsed = signalroute::proto_boundary::geofence_event_from_wire(wire);
    assert(parsed.is_ok());
    assert(parsed.value().event_type == signalroute::GeofenceEventType::DWELL);
    assert(parsed.value().inside_duration_s == 600);
}

void test_geofence_event_rejects_missing_required_fields() {
    signalroute::wire::GeofenceEventMessage wire;
    wire.device_id = "dev-3";
    wire.lat = 10.0;
    wire.lon = 106.0;

    const auto parsed = signalroute::proto_boundary::geofence_event_from_wire(wire);
    assert(parsed.is_err());
    assert(parsed.error() == "fence_id is required");
}

void test_match_request_round_trip_defaults_max_agents() {
    signalroute::wire::MatchRequestMessage wire;
    wire.request_id = "req-1";
    wire.requester_id = "rider-1";
    wire.lat = 10.0;
    wire.lon = 106.0;
    wire.radius_m = 500.0;
    wire.max_agents = 0;
    wire.deadline_ms = 4000;
    wire.strategy = "nearest";

    const auto parsed = signalroute::proto_boundary::match_request_from_wire(wire);
    assert(parsed.is_ok());
    assert(parsed.value().request_id == "req-1");
    assert(parsed.value().requester_id == "rider-1");
    assert(parsed.value().max_agents == 1);
    assert(parsed.value().strategy == "nearest");

    const auto round_trip = signalroute::proto_boundary::to_wire(parsed.value());
    assert(round_trip.max_agents == 1);
    assert(round_trip.radius_m == 500.0);
}

void test_match_request_rejects_invalid_radius() {
    signalroute::wire::MatchRequestMessage wire;
    wire.request_id = "req-2";
    wire.lat = 10.0;
    wire.lon = 106.0;
    wire.radius_m = 0.0;

    const auto parsed = signalroute::proto_boundary::match_request_from_wire(wire);
    assert(parsed.is_err());
    assert(parsed.error() == "radius_m must be positive");
}

void test_match_result_status_mapping() {
    signalroute::MatchResult result;
    result.request_id = "req-3";
    result.status = signalroute::MatchStatus::MATCHED;
    result.assigned_agent_ids = {"agent-1", "agent-2"};
    result.latency_ms = 25;

    const auto wire = signalroute::proto_boundary::to_wire(result, "rider-1", "nearest");
    assert(wire.status == signalroute::wire::MatchStatusMessage::MATCH_SUCCESS);
    assert(wire.requester_id == "rider-1");
    assert(wire.strategy == "nearest");

    const auto parsed = signalroute::proto_boundary::match_result_from_wire(wire);
    assert(parsed.is_ok());
    assert(parsed.value().status == signalroute::MatchStatus::MATCHED);
    assert((parsed.value().assigned_agent_ids == std::vector<std::string>{"agent-1", "agent-2"}));
}

void test_match_no_candidates_maps_to_failed_reason() {
    signalroute::wire::MatchResultMessage wire;
    wire.request_id = "req-4";
    wire.status = signalroute::wire::MatchStatusMessage::MATCH_NO_CANDIDATES;

    const auto parsed = signalroute::proto_boundary::match_result_from_wire(wire);
    assert(parsed.is_ok());
    assert(parsed.value().status == signalroute::MatchStatus::FAILED);
    assert(parsed.value().reason == "no candidates");
}

} // namespace

int main() {
    std::cout << "test_proto_domain_conversions:\n";
    test_location_event_round_trip_preserves_fields_and_metadata();
    test_location_event_rejects_missing_required_fields_and_bad_coordinates();
    test_device_state_to_query_response_wire();
    test_geofence_event_round_trip_maps_enum();
    test_geofence_event_rejects_missing_required_fields();
    test_match_request_round_trip_defaults_max_agents();
    test_match_request_rejects_invalid_radius();
    test_match_result_status_mapping();
    test_match_no_candidates_maps_to_failed_reason();
    std::cout << "All proto domain conversion tests passed.\n";
    return 0;
}
