#include "signalroute/geofence.pb.h"
#include "signalroute/location.pb.h"
#include "signalroute/matching.pb.h"
#include "signalroute/query.pb.h"

#include <cassert>
#include <iostream>
#include <string>

namespace {

void test_location_event_serializes_metadata() {
    signalroute::v1::LocationEvent event;
    event.set_device_id("dev-1");
    event.set_lat(10.8231);
    event.set_lon(106.6297);
    event.set_altitude_m(7.5f);
    event.set_accuracy_m(3.0f);
    event.set_speed_ms(12.5f);
    event.set_heading_deg(180.0f);
    event.set_timestamp_ms(1000);
    event.set_server_recv_ms(1100);
    event.set_seq(42);
    (*event.mutable_metadata())["source"] = "grpc";

    std::string bytes;
    assert(event.SerializeToString(&bytes));

    signalroute::v1::LocationEvent parsed;
    assert(parsed.ParseFromString(bytes));
    assert(parsed.device_id() == "dev-1");
    assert(parsed.lat() == 10.8231);
    assert(parsed.lon() == 106.6297);
    assert(parsed.seq() == 42);
    assert(parsed.metadata().at("source") == "grpc");
}

void test_query_device_state_response_serializes() {
    signalroute::v1::DeviceStateResponse state;
    state.set_device_id("dev-2");
    state.set_lat(11.0);
    state.set_lon(107.0);
    state.set_h3_cell(123456);
    state.set_seq(77);
    state.set_updated_at(2000);

    std::string bytes;
    assert(state.SerializeToString(&bytes));

    signalroute::v1::DeviceStateResponse parsed;
    assert(parsed.ParseFromString(bytes));
    assert(parsed.device_id() == "dev-2");
    assert(parsed.h3_cell() == 123456);
    assert(parsed.seq() == 77);
}

void test_geofence_event_enum_round_trip() {
    signalroute::v1::GeofenceEvent event;
    event.set_device_id("dev-3");
    event.set_fence_id("fence-1");
    event.set_fence_name("Depot");
    event.set_event_type(signalroute::v1::GEOFENCE_DWELL);
    event.set_lat(10.0);
    event.set_lon(106.0);
    event.set_event_ts_ms(3000);
    event.set_inside_duration_s(600);

    std::string bytes;
    assert(event.SerializeToString(&bytes));

    signalroute::v1::GeofenceEvent parsed;
    assert(parsed.ParseFromString(bytes));
    assert(parsed.event_type() == signalroute::v1::GEOFENCE_DWELL);
    assert(parsed.inside_duration_s() == 600);
}

void test_matching_request_and_result_round_trip() {
    signalroute::v1::MatchRequest request;
    request.set_request_id("req-1");
    request.set_requester_id("rider-1");
    request.set_lat(10.0);
    request.set_lon(106.0);
    request.set_radius_m(500.0);
    request.set_max_agents(2);
    request.set_deadline_ms(4000);
    request.set_strategy("nearest");
    (*request.mutable_metadata())["tier"] = "gold";

    std::string request_bytes;
    assert(request.SerializeToString(&request_bytes));

    signalroute::v1::MatchRequest parsed_request;
    assert(parsed_request.ParseFromString(request_bytes));
    assert(parsed_request.request_id() == "req-1");
    assert(parsed_request.metadata().at("tier") == "gold");

    signalroute::v1::MatchResult result;
    result.set_request_id("req-1");
    result.set_requester_id("rider-1");
    result.set_status(signalroute::v1::MATCH_SUCCESS);
    result.add_assigned_agent_ids("agent-1");
    result.set_latency_ms(25);
    result.set_strategy("nearest");

    std::string result_bytes;
    assert(result.SerializeToString(&result_bytes));

    signalroute::v1::MatchResult parsed_result;
    assert(parsed_result.ParseFromString(result_bytes));
    assert(parsed_result.status() == signalroute::v1::MATCH_SUCCESS);
    assert(parsed_result.assigned_agent_ids_size() == 1);
    assert(parsed_result.assigned_agent_ids(0) == "agent-1");
}

} // namespace

int main() {
    std::cout << "test_generated_protobuf_roundtrip:\n";
    test_location_event_serializes_metadata();
    test_query_device_state_response_serializes();
    test_geofence_event_enum_round_trip();
    test_matching_request_and_result_round_trip();
    std::cout << "All generated protobuf round-trip tests passed.\n";
    return 0;
}
