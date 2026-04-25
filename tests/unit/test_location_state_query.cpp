#include "common/clients/redis_client.h"
#include "common/events/event_bus.h"
#include "common/events/location_events.h"
#include "common/spatial/h3_index.h"
#include "processor/dedup_window.h"
#include "processor/sequence_guard.h"
#include "processor/state_writer.h"
#include "query/latest_handler.h"
#include "query/nearby_handler.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace {

int64_t now_ms() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

signalroute::LocationEvent make_event(std::string device_id, double lat, double lon, uint64_t seq) {
    signalroute::LocationEvent e;
    e.device_id = std::move(device_id);
    e.lat = lat;
    e.lon = lon;
    e.seq = seq;
    e.timestamp_ms = now_ms();
    e.server_recv_ms = e.timestamp_ms;
    return e;
}

} // namespace

void test_location_state_update_rejects_stale_events() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});
    signalroute::H3Index h3(7);
    signalroute::StateWriter writer(redis, h3, 3600);
    signalroute::SequenceGuard guard(redis);

    auto fresh = make_event("dev-1", 10.8231, 106.6297, 2);
    assert(guard.should_accept(fresh.device_id, fresh.seq));
    assert(writer.write(fresh));

    auto stale = make_event("dev-1", 10.0, 106.0, 1);
    assert(!guard.should_accept(stale.device_id, stale.seq));
    assert(!writer.write(stale));

    signalroute::LatestHandler latest(redis);
    auto state = latest.handle("dev-1");
    assert(state.has_value());
    assert(state->seq == 2);
    assert(state->lat == fresh.lat);
}

void test_nearby_query_reads_h3_cell_index() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});
    signalroute::H3Index h3(7);
    signalroute::StateWriter writer(redis, h3, 3600);

    assert(writer.write(make_event("dev-1", 10.8231, 106.6297, 1)));
    assert(writer.write(make_event("dev-2", 10.8240, 106.6300, 1)));

    signalroute::SpatialConfig spatial_cfg;
    spatial_cfg.nearby_max_radius_m = 50000.0;
    spatial_cfg.nearby_max_results = 100;
    signalroute::NearbyHandler nearby(redis, h3, spatial_cfg);

    auto result = nearby.handle(10.8231, 106.6297, 500.0, 10, 60);
    assert(result.total_candidates >= 2);
    assert(result.total_in_radius >= 2);
    assert(result.devices.size() >= 2);
    assert(result.devices.front().first.device_id == "dev-1");
}

void test_components_can_coordinate_through_location_events() {
    signalroute::EventBus bus;
    std::vector<std::string> calls;

    auto state_writer_subscription = bus.subscribe<signalroute::events::LocationAccepted>(
        [&](const signalroute::events::LocationAccepted& event) {
            calls.push_back("state:" + event.event.device_id);
        });
    auto history_writer_subscription = bus.subscribe<signalroute::events::LocationAccepted>(
        [&](const signalroute::events::LocationAccepted& event) {
            calls.push_back("history:" + event.event.device_id);
        });
    auto metrics_subscription = bus.subscribe<signalroute::events::StateWriteSucceeded>(
        [&](const signalroute::events::StateWriteSucceeded& event) {
            calls.push_back("metrics:" + event.state.device_id);
        });

    auto event = make_event("dev-1", 10.8231, 106.6297, 1);
    bus.publish(signalroute::events::LocationAccepted{event});

    signalroute::DeviceState state;
    state.device_id = event.device_id;
    state.seq = event.seq;
    bus.publish(signalroute::events::StateWriteSucceeded{event, state, 0});

    assert(calls.size() == 3);
    assert(calls[0] == "state:dev-1");
    assert(calls[1] == "history:dev-1");
    assert(calls[2] == "metrics:dev-1");
}

int main() {
    std::cout << "test_location_state_query:\n";
    test_location_state_update_rejects_stale_events();
    test_nearby_query_reads_h3_cell_index();
    test_components_can_coordinate_through_location_events();
    std::cout << "All location state/query tests passed.\n";
    return 0;
}
