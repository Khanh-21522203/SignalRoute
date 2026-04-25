#include "common/composition/composition_root.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>

namespace {

int64_t now_ms() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

signalroute::LocationEvent make_event() {
    signalroute::LocationEvent event;
    event.device_id = "dev-1";
    event.lat = 10.8231;
    event.lon = 106.6297;
    event.seq = 1;
    event.timestamp_ms = now_ms();
    event.server_recv_ms = event.timestamp_ms;
    return event;
}

} // namespace

void test_location_accepted_fans_out_to_state_and_history_requests() {
    signalroute::EventBus bus;
    signalroute::CompositionRoot root(bus);
    int state_requests = 0;
    int history_requests = 0;

    auto state_sub = bus.subscribe<signalroute::events::StateWriteRequested>(
        [&](const signalroute::events::StateWriteRequested&) { ++state_requests; });
    auto history_sub = bus.subscribe<signalroute::events::TripHistoryWriteRequested>(
        [&](const signalroute::events::TripHistoryWriteRequested&) { ++history_requests; });

    root.wire_location_pipeline_observers();
    bus.publish(signalroute::events::LocationAccepted{make_event()});

    assert(root.subscription_count() == 2);
    assert(state_requests == 1);
    assert(history_requests == 1);
}

void test_state_success_requests_geofence_evaluation() {
    signalroute::EventBus bus;
    signalroute::CompositionRoot root(bus);
    int geofence_requests = 0;
    signalroute::events::GeofenceEvaluationRequested captured;

    auto geofence_sub = bus.subscribe<signalroute::events::GeofenceEvaluationRequested>(
        [&](const signalroute::events::GeofenceEvaluationRequested& event) {
            ++geofence_requests;
            captured = event;
        });

    root.wire_location_pipeline_observers();

    auto event = make_event();
    signalroute::DeviceState state;
    state.device_id = event.device_id;
    state.lat = event.lat;
    state.lon = event.lon;
    state.h3_cell = 123;
    state.updated_at = event.server_recv_ms;

    bus.publish(signalroute::events::StateWriteSucceeded{event, state, 42});

    assert(geofence_requests == 1);
    assert(captured.device_id == "dev-1");
    assert(captured.old_h3_cell == 42);
    assert(captured.new_h3_cell == 123);
}

void test_clear_removes_root_subscriptions() {
    signalroute::EventBus bus;
    signalroute::CompositionRoot root(bus);
    int state_requests = 0;

    auto state_sub = bus.subscribe<signalroute::events::StateWriteRequested>(
        [&](const signalroute::events::StateWriteRequested&) { ++state_requests; });

    root.wire_location_pipeline_observers();
    root.clear();
    bus.publish(signalroute::events::LocationAccepted{make_event()});

    assert(root.subscription_count() == 0);
    assert(state_requests == 0);
}

int main() {
    std::cout << "test_composition_root:\n";
    test_location_accepted_fans_out_to_state_and_history_requests();
    test_state_success_requests_geofence_evaluation();
    test_clear_removes_root_subscriptions();
    std::cout << "All composition root tests passed.\n";
    return 0;
}
