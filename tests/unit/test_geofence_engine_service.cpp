#include "common/events/all_events.h"
#include "common/events/event_bus.h"
#include "common/types/geofence_types.h"
#include "geofence/geofence_engine.h"

#include <cassert>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

signalroute::GeofenceRule square_rule(std::string fence_id, int64_t h3_cell) {
    signalroute::GeofenceRule rule;
    rule.fence_id = std::move(fence_id);
    rule.name = "Depot";
    rule.h3_cells.insert(h3_cell);
    rule.polygon_vertices = {
        {0.0, 0.0},
        {0.0, 10.0},
        {10.0, 10.0},
        {10.0, 0.0},
    };
    rule.active = true;
    return rule;
}

} // namespace

void test_geofence_engine_subscribes_to_shared_bus() {
    signalroute::Config config;
    config.kafka.geofence_topic = "test.geofence.service.events";
    signalroute::EventBus bus;
    signalroute::GeofenceEngine engine;

    engine.start(config, bus);
    assert(engine.is_healthy());
    assert(engine.is_event_driven());
    assert(engine.subscription_count() == 1);

    engine.load_fences_for_test({square_rule("fence-1", 100)});
    assert(engine.fence_count() == 1);

    bus.publish(signalroute::events::GeofenceEvaluationRequested{
        "dev-1", 0, 100, 5.0, 5.0, 1000});

    assert(engine.geofence_event_count_for_test() == 1);

    engine.stop();
    assert(!engine.is_healthy());
    assert(engine.subscription_count() == 0);
}

void test_geofence_engine_owned_bus_lifecycle() {
    signalroute::Config config;
    signalroute::GeofenceEngine engine;

    engine.start(config);
    assert(engine.is_healthy());
    assert(engine.is_event_driven());
    assert(engine.subscription_count() == 1);

    engine.stop();
    assert(!engine.is_healthy());
    assert(engine.subscription_count() == 0);
}

int main() {
    std::cout << "test_geofence_engine_service:\n";
    test_geofence_engine_subscribes_to_shared_bus();
    test_geofence_engine_owned_bus_lifecycle();
    std::cout << "All geofence engine service tests passed.\n";
    return 0;
}
