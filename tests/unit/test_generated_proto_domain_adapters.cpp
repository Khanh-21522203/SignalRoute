#include "common/proto/generated_conversions.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

void test_location_event_generated_round_trip() {
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
    event.metadata["source"] = "protobuf";

    const auto generated = signalroute::proto_boundary::to_generated_location_event(event);
    assert(generated.device_id() == "dev-1");
    assert(generated.metadata().at("source") == "protobuf");

    const auto parsed = signalroute::proto_boundary::location_event_from_generated(generated);
    assert(parsed.is_ok());
    assert(parsed.value().device_id == event.device_id);
    assert(parsed.value().lat == event.lat);
    assert(parsed.value().seq == event.seq);
    assert(parsed.value().metadata.at("source") == "protobuf");
}

void test_location_event_generated_validation_error() {
    signalroute::v1::LocationEvent generated;
    generated.set_device_id("dev-2");
    generated.set_lat(100.0);
    generated.set_lon(106.0);

    const auto parsed = signalroute::proto_boundary::location_event_from_generated(generated);
    assert(parsed.is_err());
    assert(parsed.error() == "invalid coordinates");
}

void test_device_state_generated_response() {
    signalroute::DeviceState state;
    state.device_id = "dev-3";
    state.lat = 11.0;
    state.lon = 107.0;
    state.h3_cell = 123456;
    state.seq = 77;
    state.updated_at = 2000;

    const auto generated = signalroute::proto_boundary::to_generated_device_state_response(state);
    assert(generated.device_id() == "dev-3");
    assert(generated.h3_cell() == 123456);
    assert(generated.seq() == 77);
    assert(generated.updated_at() == 2000);
}

void test_geofence_event_generated_round_trip() {
    signalroute::GeofenceEventRecord event;
    event.device_id = "dev-4";
    event.fence_id = "fence-1";
    event.fence_name = "Depot";
    event.event_type = signalroute::GeofenceEventType::EXIT;
    event.lat = 10.0;
    event.lon = 106.0;
    event.event_ts_ms = 3000;

    const auto generated = signalroute::proto_boundary::to_generated_geofence_event(event);
    assert(generated.event_type() == signalroute::v1::GEOFENCE_EXIT);

    const auto parsed = signalroute::proto_boundary::geofence_event_from_generated(generated);
    assert(parsed.is_ok());
    assert(parsed.value().event_type == signalroute::GeofenceEventType::EXIT);
    assert(parsed.value().fence_id == "fence-1");
}

void test_match_request_generated_round_trip_defaults_max_agents() {
    signalroute::v1::MatchRequest generated;
    generated.set_request_id("req-1");
    generated.set_requester_id("rider-1");
    generated.set_lat(10.0);
    generated.set_lon(106.0);
    generated.set_radius_m(500.0);
    generated.set_max_agents(0);
    generated.set_strategy("nearest");

    const auto parsed = signalroute::proto_boundary::match_request_from_generated(generated);
    assert(parsed.is_ok());
    assert(parsed.value().request_id == "req-1");
    assert(parsed.value().max_agents == 1);

    const auto round_trip = signalroute::proto_boundary::to_generated_match_request(parsed.value());
    assert(round_trip.max_agents() == 1);
    assert(round_trip.strategy() == "nearest");
}

void test_match_result_generated_status_mapping() {
    signalroute::MatchResult result;
    result.request_id = "req-2";
    result.status = signalroute::MatchStatus::MATCHED;
    result.assigned_agent_ids = {"agent-1", "agent-2"};
    result.latency_ms = 25;

    const auto generated = signalroute::proto_boundary::to_generated_match_result(result, "rider-1", "nearest");
    assert(generated.status() == signalroute::v1::MATCH_SUCCESS);
    assert(generated.assigned_agent_ids_size() == 2);

    const auto parsed = signalroute::proto_boundary::match_result_from_generated(generated);
    assert(parsed.is_ok());
    assert(parsed.value().status == signalroute::MatchStatus::MATCHED);
    assert((parsed.value().assigned_agent_ids == std::vector<std::string>{"agent-1", "agent-2"}));
}

} // namespace

int main() {
    std::cout << "test_generated_proto_domain_adapters:\n";
    test_location_event_generated_round_trip();
    test_location_event_generated_validation_error();
    test_device_state_generated_response();
    test_geofence_event_generated_round_trip();
    test_match_request_generated_round_trip_defaults_max_agents();
    test_match_result_generated_status_mapping();
    std::cout << "All generated proto domain adapter tests passed.\n";
    return 0;
}
