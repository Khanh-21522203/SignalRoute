#include "common/proto/geofence_payload_codec.h"
#include "common/proto/location_payload_codec.h"
#include "common/proto/matching_payload_codec.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool starts_with(const std::string& value, std::string_view prefix) {
    return value.size() >= prefix.size() &&
           value.compare(0, prefix.size(), prefix.data(), prefix.size()) == 0;
}

void test_location_payload_codec_preserves_fallback_csv_decoder() {
    const auto decoded = signalroute::proto_boundary::decode_location_payload(
        "dev-1,7,1000,1005,10.0,106.0");
    assert(decoded.is_ok());
    assert(decoded.value().device_id == "dev-1");
    assert(decoded.value().seq == 7);
    assert(decoded.value().lat == 10.0);
}

void test_geofence_event_payload_codec_round_trips_enter_and_dwell() {
    signalroute::GeofenceEventRecord enter;
    enter.device_id = "dev-1";
    enter.fence_id = "fence-1";
    enter.fence_name = "Depot";
    enter.event_type = signalroute::GeofenceEventType::ENTER;
    enter.event_ts_ms = 1000;
    enter.lat = 10.5;
    enter.lon = 106.5;

    const auto enter_payload = signalroute::proto_boundary::encode_geofence_event_payload(enter);
#if SIGNALROUTE_HAS_PROTOBUF
    assert(starts_with(enter_payload, signalroute::proto_boundary::protobuf_geofence_event_payload_prefix()));
#endif
    const auto decoded_enter = signalroute::proto_boundary::decode_geofence_event_payload(enter_payload);
    assert(decoded_enter.is_ok());
    assert(decoded_enter.value().device_id == "dev-1");
    assert(decoded_enter.value().event_type == signalroute::GeofenceEventType::ENTER);
    assert(decoded_enter.value().lat == 10.5);

    signalroute::GeofenceEventRecord dwell;
    dwell.device_id = "dev-2";
    dwell.fence_id = "fence-2";
    dwell.fence_name = "Warehouse";
    dwell.event_type = signalroute::GeofenceEventType::DWELL;
    dwell.event_ts_ms = 2000;
    dwell.inside_duration_s = 300;

    const auto dwell_payload = signalroute::proto_boundary::encode_geofence_event_payload(dwell);
#if SIGNALROUTE_HAS_PROTOBUF
    assert(starts_with(dwell_payload, signalroute::proto_boundary::protobuf_geofence_event_payload_prefix()));
#endif
    const auto decoded_dwell = signalroute::proto_boundary::decode_geofence_event_payload(dwell_payload);
    assert(decoded_dwell.is_ok());
    assert(decoded_dwell.value().event_type == signalroute::GeofenceEventType::DWELL);
    assert(decoded_dwell.value().inside_duration_s == 300);
}

void test_geofence_event_payload_codec_preserves_legacy_csv_decoder() {
    const auto enter = signalroute::proto_boundary::decode_geofence_event_payload(
        "dev-3,fence-3,ENTER,3000,11.0,107.0");
    assert(enter.is_ok());
    assert(enter.value().event_type == signalroute::GeofenceEventType::ENTER);
    assert(enter.value().lon == 107.0);

    const auto dwell = signalroute::proto_boundary::decode_geofence_event_payload(
        "dev-3,fence-3,DWELL,4000,25");
    assert(dwell.is_ok());
    assert(dwell.value().event_type == signalroute::GeofenceEventType::DWELL);
    assert(dwell.value().inside_duration_s == 25);
}

void test_matching_payload_codecs_round_trip_request_and_result() {
    signalroute::MatchRequest request;
    request.request_id = "req-1";
    request.requester_id = "rider-1";
    request.lat = 10.0;
    request.lon = 106.0;
    request.radius_m = 500.0;
    request.max_agents = 2;
    request.deadline_ms = 9000;
    request.strategy = "nearest";

    const auto request_payload = signalroute::proto_boundary::encode_match_request_payload(request);
#if SIGNALROUTE_HAS_PROTOBUF
    assert(starts_with(request_payload, signalroute::proto_boundary::protobuf_match_request_payload_prefix()));
#endif
    const auto decoded_request = signalroute::proto_boundary::decode_match_request_payload(request_payload);
    assert(decoded_request.is_ok());
    assert(decoded_request.value().request_id == "req-1");
    assert(decoded_request.value().max_agents == 2);
    assert(decoded_request.value().strategy == "nearest");

    signalroute::MatchResult result;
    result.request_id = "req-1";
    result.status = signalroute::MatchStatus::MATCHED;
    result.assigned_agent_ids = {"agent-1", "agent-2"};
    result.latency_ms = 35;

    const auto result_payload = signalroute::proto_boundary::encode_match_result_payload(
        result, request.requester_id, request.strategy);
#if SIGNALROUTE_HAS_PROTOBUF
    assert(starts_with(result_payload, signalroute::proto_boundary::protobuf_match_result_payload_prefix()));
#endif
    const auto decoded_result = signalroute::proto_boundary::decode_match_result_payload(result_payload);
    assert(decoded_result.is_ok());
    assert(decoded_result.value().status == signalroute::MatchStatus::MATCHED);
    assert((decoded_result.value().assigned_agent_ids == std::vector<std::string>{"agent-1", "agent-2"}));
}

void test_matching_payload_codecs_preserve_legacy_csv_decoder() {
    const auto request = signalroute::proto_boundary::decode_match_request_payload(
        "req-2,rider-2,10.5,106.5,750,1,0,nearest");
    assert(request.is_ok());
    assert(request.value().request_id == "req-2");
    assert(request.value().radius_m == 750.0);

    const auto result = signalroute::proto_boundary::decode_match_result_payload(
        "req-2,FAILED,12,no agents,");
    assert(result.is_ok());
    assert(result.value().status == signalroute::MatchStatus::FAILED);
    assert(result.value().reason == "no agents");
}

} // namespace

int main() {
    std::cout << "test_runtime_payload_codecs:\n";
    test_location_payload_codec_preserves_fallback_csv_decoder();
    test_geofence_event_payload_codec_round_trips_enter_and_dwell();
    test_geofence_event_payload_codec_preserves_legacy_csv_decoder();
    test_matching_payload_codecs_round_trip_request_and_result();
    test_matching_payload_codecs_preserve_legacy_csv_decoder();
    std::cout << "All runtime payload codec tests passed.\n";
    return 0;
}
