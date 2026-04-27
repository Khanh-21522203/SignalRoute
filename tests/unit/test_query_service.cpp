#include "query/query_service.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

int64_t now_ms() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

signalroute::Config query_config() {
    signalroute::Config config;
    config.spatial.h3_resolution = 7;
    config.spatial.nearby_max_radius_m = 50000.0;
    config.spatial.nearby_max_results = 100;
    config.redis.device_ttl_s = 3600;
    return config;
}

signalroute::DeviceState state(std::string device_id, double lat, double lon, uint64_t seq) {
    signalroute::DeviceState out;
    out.device_id = std::move(device_id);
    out.lat = lat;
    out.lon = lon;
    out.seq = seq;
    out.updated_at = now_ms();
    return out;
}

signalroute::LocationEvent trip_point(std::string device_id,
                                      uint64_t seq,
                                      int64_t timestamp_ms,
                                      double lat,
                                      double lon) {
    signalroute::LocationEvent out;
    out.device_id = std::move(device_id);
    out.seq = seq;
    out.timestamp_ms = timestamp_ms;
    out.server_recv_ms = timestamp_ms + 5;
    out.lat = lat;
    out.lon = lon;
    return out;
}

} // namespace

void test_query_service_latest_and_nearby_paths() {
    signalroute::QueryService service;
    service.start(query_config());
    assert(service.is_healthy());

    assert(service.seed_device_state_for_test(state("dev-1", 10.8231, 106.6297, 1)));
    assert(service.seed_device_state_for_test(state("dev-2", 10.8240, 106.6300, 1)));

    const auto latest = service.latest("dev-1");
    assert(latest.has_value());
    assert(latest->device_id == "dev-1");
    assert(latest->seq == 1);

    const auto nearby = service.nearby(10.8231, 106.6297, 5000.0, 10, 60);
    assert(nearby.total_candidates >= 2);
    assert(nearby.total_in_radius >= 2);
    assert(nearby.devices.size() >= 2);
    assert(nearby.devices.front().first.device_id == "dev-1");

    service.stop();
    assert(!service.is_healthy());
}

void test_query_service_trip_and_spatial_paths() {
    signalroute::QueryService service;
    service.start(query_config());

    service.seed_trip_points_for_test({
        trip_point("dev-1", 1, 1000, 10.8231, 106.6297),
        trip_point("dev-1", 2, 2000, 10.8232, 106.6298),
        trip_point("dev-1", 3, 3000, 21.0285, 105.8542),
        trip_point("dev-2", 1, 1000, 10.0, 106.0),
    });

    assert(service.trip_point_count_for_test() == 4);

    const auto trip = service.trip("dev-1", 0, 4000, 0, 10);
    assert(trip.size() == 3);
    assert(trip[0].seq == 1);
    assert(trip[2].seq == 3);

    const auto spatial = service.trip_spatial("dev-1", 0, 4000, 10.8231, 106.6297, 100.0, 0, 10);
    assert(spatial.size() == 2);
    assert(spatial[0].seq == 1);
    assert(spatial[1].seq == 2);
}

void test_query_service_returns_empty_when_stopped_or_invalid() {
    signalroute::QueryService service;

    assert(!service.latest("dev-1").has_value());
    assert(service.nearby(10.0, 106.0, 100.0, 10, 60).devices.empty());
    assert(service.trip("dev-1", 0, 1000, 0, 10).empty());
    assert(!service.seed_device_state_for_test(state("dev-1", 10.0, 106.0, 1)));

    service.start(query_config());
    assert(!service.latest("").has_value());
    assert(service.nearby(91.0, 106.0, 100.0, 10, 60).devices.empty());
    assert(service.trip("", 0, 1000, 0, 10).empty());
    service.stop();

    assert(!service.latest("dev-1").has_value());
    assert(service.trip_point_count_for_test() == 0);
}

void test_query_transport_latest_and_nearby_responses() {
    signalroute::QueryService service;

    auto stopped = service.handle_latest({"dev-1"});
    assert(!stopped.ok);
    assert(stopped.error == "query service is not running");

    service.start(query_config());
    assert(service.seed_device_state_for_test(state("dev-1", 10.8231, 106.6297, 1)));

    const auto latest = service.handle_latest({"dev-1"});
    assert(latest.ok);
    assert(latest.found);
    assert(latest.state.device_id == "dev-1");

    const auto missing = service.handle_latest({"missing"});
    assert(missing.ok);
    assert(!missing.found);

    const auto nearby = service.handle_nearby({10.8231, 106.6297, 1000.0, 10, 60});
    assert(nearby.ok);
    assert(nearby.result.total_in_radius == 1);
    assert(nearby.result.devices.front().first.device_id == "dev-1");

    const auto invalid = service.handle_nearby({91.0, 106.6297, 1000.0, 10, 60});
    assert(!invalid.ok);
    assert(invalid.error == "valid lat/lon is required");
}

void test_query_transport_trip_responses() {
    signalroute::QueryService service;
    service.start(query_config());
    service.seed_trip_points_for_test({
        trip_point("dev-1", 1, 1000, 10.8231, 106.6297),
        trip_point("dev-1", 2, 2000, 10.8232, 106.6298),
        trip_point("dev-1", 3, 3000, 21.0285, 105.8542),
    });

    const auto trip = service.handle_trip({"dev-1", 0, 4000, 0, 10});
    assert(trip.ok);
    assert(trip.events.size() == 3);
    assert(trip.events.front().seq == 1);

    const auto spatial = service.handle_trip_spatial({
        "dev-1", 0, 4000, 10.8231, 106.6297, 100.0, 0, 10
    });
    assert(spatial.ok);
    assert(spatial.events.size() == 2);

    const auto invalid = service.handle_trip({"", 0, 4000, 0, 10});
    assert(!invalid.ok);
    assert(invalid.error == "device_id is required");
}

int main() {
    std::cout << "test_query_service:\n";
    test_query_service_latest_and_nearby_paths();
    test_query_service_trip_and_spatial_paths();
    test_query_service_returns_empty_when_stopped_or_invalid();
    test_query_transport_latest_and_nearby_responses();
    test_query_transport_trip_responses();
    std::cout << "All query service tests passed.\n";
    return 0;
}
