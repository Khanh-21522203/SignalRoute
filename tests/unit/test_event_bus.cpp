#include "common/events/event_bus.h"

#include <cassert>
#include <iostream>
#include <string>

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

int main() {
    std::cout << "test_event_bus:\n";
    test_publish_invokes_all_subscribers();
    test_subscriptions_are_type_isolated();
    test_subscription_reset_unsubscribes();
    std::cout << "All event bus tests passed.\n";
    return 0;
}
