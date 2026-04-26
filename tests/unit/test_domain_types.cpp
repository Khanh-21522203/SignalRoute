#include "common/types/device_state.h"
#include "common/types/geofence_types.h"
#include "common/types/location_event.h"
#include "common/types/result.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

void test_location_event_defaults_and_metadata() {
    signalroute::LocationEvent event;

    assert(event.device_id.empty());
    assert(event.lat == 0.0);
    assert(event.lon == 0.0);
    assert(event.timestamp_ms == 0);
    assert(event.seq == 0);

    event.device_id = "dev-1";
    event.metadata["source"] = "udp";

    assert(event.device_id == "dev-1");
    assert(event.metadata.at("source") == "udp");
}

void test_device_state_copyable_latest_state() {
    signalroute::DeviceState state;
    state.device_id = "dev-1";
    state.lat = 10.8231;
    state.lon = 106.6297;
    state.h3_cell = 12345;
    state.seq = 99;
    state.updated_at = 1000;

    const signalroute::DeviceState copy = state;

    assert(copy.device_id == "dev-1");
    assert(copy.lat == 10.8231);
    assert(copy.lon == 106.6297);
    assert(copy.h3_cell == 12345);
    assert(copy.seq == 99);
    assert(copy.updated_at == 1000);
}

void test_geofence_string_conversions() {
    using namespace signalroute;

    assert(std::string(fence_state_to_string(FenceState::OUTSIDE)) == "OUTSIDE");
    assert(std::string(fence_state_to_string(FenceState::INSIDE)) == "INSIDE");
    assert(std::string(fence_state_to_string(FenceState::DWELL)) == "DWELL");
    assert(fence_state_from_string("OUTSIDE") == FenceState::OUTSIDE);
    assert(fence_state_from_string("INSIDE") == FenceState::INSIDE);
    assert(fence_state_from_string("DWELL") == FenceState::DWELL);
    assert(fence_state_from_string("bad-value") == FenceState::OUTSIDE);

    assert(std::string(geofence_event_type_to_string(GeofenceEventType::ENTER)) == "ENTER");
    assert(std::string(geofence_event_type_to_string(GeofenceEventType::EXIT)) == "EXIT");
    assert(std::string(geofence_event_type_to_string(GeofenceEventType::DWELL)) == "DWELL");
    assert(geofence_event_type_from_string("ENTER") == GeofenceEventType::ENTER);
    assert(geofence_event_type_from_string("EXIT") == GeofenceEventType::EXIT);
    assert(geofence_event_type_from_string("DWELL") == GeofenceEventType::DWELL);
    assert(geofence_event_type_from_string("bad-value") == GeofenceEventType::ENTER);
}

void test_result_success_and_error_paths() {
    auto ok = signalroute::Result<int>::ok(42);
    auto err = signalroute::Result<int>::err("not found");

    assert(ok.is_ok());
    assert(!ok.is_err());
    assert(ok.value() == 42);
    assert(ok.value_or(7) == 42);

    assert(err.is_err());
    assert(!err.is_ok());
    assert(err.error() == "not found");
    assert(err.value_or(7) == 7);

    bool value_threw = false;
    try {
        (void)err.value();
    } catch (const std::runtime_error&) {
        value_threw = true;
    }
    assert(value_threw);

    bool error_threw = false;
    try {
        (void)ok.error();
    } catch (const std::runtime_error&) {
        error_threw = true;
    }
    assert(error_threw);
}

void test_result_void_success_and_error_paths() {
    auto ok = signalroute::Result<void, std::string>::ok();
    auto err = signalroute::Result<void, std::string>::err("failed");

    assert(ok.is_ok());
    assert(!ok.is_err());
    assert(err.is_err());
    assert(!err.is_ok());
    assert(err.error() == "failed");
}

int main() {
    std::cout << "test_domain_types:\n";
    test_location_event_defaults_and_metadata();
    test_device_state_copyable_latest_state();
    test_geofence_string_conversions();
    test_result_success_and_error_paths();
    test_result_void_success_and_error_paths();
    std::cout << "All domain type tests passed.\n";
    return 0;
}
