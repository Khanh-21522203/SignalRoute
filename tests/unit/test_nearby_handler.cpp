#include "common/clients/redis_client.h"
#include "common/spatial/h3_index.h"
#include "processor/state_writer.h"
#include "query/nearby_handler.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>

namespace {

int64_t now_ms() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

signalroute::LocationEvent make_event(std::string device_id, double lat, double lon, uint64_t seq) {
    signalroute::LocationEvent event;
    event.device_id = std::move(device_id);
    event.lat = lat;
    event.lon = lon;
    event.seq = seq;
    event.timestamp_ms = now_ms();
    event.server_recv_ms = event.timestamp_ms;
    return event;
}

} // namespace

void test_returns_devices_sorted_by_distance() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});
    signalroute::H3Index h3(7);
    signalroute::StateWriter writer(redis, h3, 3600);
    signalroute::SpatialConfig cfg;
    cfg.nearby_max_radius_m = 50000.0;
    cfg.nearby_max_results = 100;

    assert(writer.write(make_event("near", 10.8231, 106.6297, 1)));
    assert(writer.write(make_event("far", 10.8300, 106.6400, 1)));

    signalroute::NearbyHandler handler(redis, h3, cfg);
    auto result = handler.handle(10.8231, 106.6297, 5000.0, 10, 60);

    assert(result.devices.size() == 2);
    assert(result.devices[0].first.device_id == "near");
    assert(result.devices[0].second <= result.devices[1].second);
}

void test_applies_limit() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});
    signalroute::H3Index h3(7);
    signalroute::StateWriter writer(redis, h3, 3600);
    signalroute::SpatialConfig cfg;
    cfg.nearby_max_radius_m = 50000.0;
    cfg.nearby_max_results = 100;

    assert(writer.write(make_event("dev-1", 10.8231, 106.6297, 1)));
    assert(writer.write(make_event("dev-2", 10.8240, 106.6300, 1)));

    signalroute::NearbyHandler handler(redis, h3, cfg);
    auto result = handler.handle(10.8231, 106.6297, 5000.0, 1, 60);
    assert(result.total_in_radius >= 2);
    assert(result.devices.size() == 1);
}

void test_applies_freshness_filter() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});
    signalroute::H3Index h3(7);
    signalroute::StateWriter writer(redis, h3, 3600);
    signalroute::SpatialConfig cfg;
    cfg.nearby_max_radius_m = 50000.0;
    cfg.nearby_max_results = 100;

    auto fresh = make_event("fresh", 10.8231, 106.6297, 1);
    auto stale = make_event("stale", 10.8240, 106.6300, 1);
    stale.server_recv_ms = now_ms() - 120000;
    assert(writer.write(fresh));
    assert(writer.write(stale));

    signalroute::NearbyHandler handler(redis, h3, cfg);
    auto result = handler.handle(10.8231, 106.6297, 5000.0, 10, 60);

    assert(result.devices.size() == 1);
    assert(result.devices[0].first.device_id == "fresh");
}

int main() {
    std::cout << "test_nearby_handler:\n";
    test_returns_devices_sorted_by_distance();
    test_applies_limit();
    test_applies_freshness_filter();
    std::cout << "All nearby handler tests passed.\n";
    return 0;
}
