#include "common/events/all_events.h"
#include "common/events/event_bus.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

struct EventA {
    int value = 0;
};

struct EventB {
    std::string value;
};

} // namespace

void test_publish_invokes_all_subscribers() {
    signalroute::EventBus bus;
    int sum = 0;

    auto sub1 = bus.subscribe<EventA>([&](const EventA& event) { sum += event.value; });
    auto sub2 = bus.subscribe<EventA>([&](const EventA& event) { sum += event.value * 2; });

    bus.publish(EventA{3});

    assert(sub1.active());
    assert(sub2.active());
    assert(sum == 9);
    assert(bus.subscriber_count<EventA>() == 2);
}

void test_subscriptions_are_type_isolated() {
    signalroute::EventBus bus;
    int event_a_count = 0;
    int event_b_count = 0;

    auto sub_a = bus.subscribe<EventA>([&](const EventA&) { ++event_a_count; });
    auto sub_b = bus.subscribe<EventB>([&](const EventB&) { ++event_b_count; });

    bus.publish(EventA{1});

    assert(event_a_count == 1);
    assert(event_b_count == 0);
    assert(bus.subscriber_count<EventA>() == 1);
    assert(bus.subscriber_count<EventB>() == 1);
}

void test_subscription_reset_unsubscribes() {
    signalroute::EventBus bus;
    int count = 0;

    auto sub = bus.subscribe<EventA>([&](const EventA&) { ++count; });
    bus.publish(EventA{1});
    sub.reset();
    bus.publish(EventA{1});

    assert(!sub.active());
    assert(count == 1);
    assert(bus.subscriber_count<EventA>() == 0);
}

void test_subscription_move_transfers_unsubscribe_ownership() {
    signalroute::EventBus bus;
    int count = 0;

    auto sub = bus.subscribe<EventA>([&](const EventA&) { ++count; });
    signalroute::EventBus::Subscription moved = std::move(sub);

    assert(!sub.active());
    assert(moved.active());
    assert(bus.subscriber_count<EventA>() == 1);

    bus.publish(EventA{1});
    moved.reset();
    bus.publish(EventA{1});

    assert(!moved.active());
    assert(count == 1);
    assert(bus.subscriber_count<EventA>() == 0);
}

void test_publish_uses_snapshot_when_subscriptions_change() {
    signalroute::EventBus bus;
    int first_count = 0;
    int second_count = 0;
    int late_count = 0;

    signalroute::EventBus::Subscription second;
    std::vector<signalroute::EventBus::Subscription> late_subscriptions;
    auto first = bus.subscribe<EventA>([&](const EventA&) {
        ++first_count;
        second.reset();
        late_subscriptions.push_back(
            bus.subscribe<EventA>([&](const EventA&) { ++late_count; }));
    });
    second = bus.subscribe<EventA>([&](const EventA&) { ++second_count; });

    bus.publish(EventA{1});

    assert(first.active());
    assert(!second.active());
    assert(first_count == 1);
    assert(second_count == 1);
    assert(late_count == 0);
    assert(bus.subscriber_count<EventA>() == 2);
}

void test_shared_event_contracts_are_small_value_objects() {
    using namespace signalroute::events;

    static_assert(std::is_copy_constructible_v<LocationAccepted>);
    static_assert(std::is_copy_constructible_v<StateWriteSucceeded>);
    static_assert(std::is_copy_constructible_v<TripHistoryWriteRequested>);
    static_assert(std::is_copy_constructible_v<GeofenceEvaluationRequested>);

    signalroute::LocationEvent location;
    location.device_id = "dev-1";
    location.lat = 10.8231;
    location.lon = 106.6297;
    location.seq = 7;
    location.timestamp_ms = 1000;
    location.server_recv_ms = 1005;

    signalroute::DeviceState state;
    state.device_id = location.device_id;
    state.lat = location.lat;
    state.lon = location.lon;
    state.h3_cell = 123;
    state.seq = location.seq;
    state.updated_at = location.server_recv_ms;

    const LocationAccepted accepted{location};
    const StateWriteRequested state_request{accepted.event};
    const TripHistoryWriteRequested history_request{accepted.event};
    const StateWriteSucceeded state_success{state_request.event, state, 42};
    const GeofenceEvaluationRequested geofence_request{
        state_success.state.device_id,
        state_success.previous_h3_cell,
        state_success.state.h3_cell,
        state_success.state.lat,
        state_success.state.lon,
        state_success.state.updated_at};

    assert(history_request.event.device_id == "dev-1");
    assert(state_success.event.seq == 7);
    assert(geofence_request.device_id == "dev-1");
    assert(geofence_request.old_h3_cell == 42);
    assert(geofence_request.new_h3_cell == 123);
    assert(geofence_request.timestamp_ms == 1005);
}

int main() {
    std::cout << "test_event_bus:\n";
    test_publish_invokes_all_subscribers();
    test_subscriptions_are_type_isolated();
    test_subscription_reset_unsubscribes();
    test_subscription_move_transfers_unsubscribe_ownership();
    test_publish_uses_snapshot_when_subscriptions_change();
    test_shared_event_contracts_are_small_value_objects();
    std::cout << "All event bus tests passed.\n";
    return 0;
}
