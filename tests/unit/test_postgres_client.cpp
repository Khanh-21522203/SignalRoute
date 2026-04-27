#include "common/clients/postgres_client.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

signalroute::LocationEvent make_event(std::string device_id,
                                       uint64_t seq,
                                       int64_t timestamp_ms,
                                       double lat,
                                       double lon) {
    signalroute::LocationEvent event;
    event.device_id = std::move(device_id);
    event.seq = seq;
    event.timestamp_ms = timestamp_ms;
    event.server_recv_ms = timestamp_ms + 5;
    event.lat = lat;
    event.lon = lon;
    return event;
}

} // namespace

void test_trip_insert_is_idempotent() {
    signalroute::PostgresClient pg(signalroute::PostGISConfig{});
    const auto event = make_event("dev-1", 1, 1000, 10.8231, 106.6297);

    assert(pg.ping());
    pg.batch_insert_trip_points({event, event});

    assert(pg.trip_point_count() == 1);
    const auto result = pg.query_trip("dev-1", 0, 2000, 10);
    assert(result.size() == 1);
    assert(result.front().seq == 1);
}

void test_query_trip_filters_sorts_and_limits() {
    signalroute::PostgresClient pg(signalroute::PostGISConfig{});

    pg.batch_insert_trip_points({
        make_event("dev-1", 3, 3000, 10.3, 106.3),
        make_event("dev-1", 1, 1000, 10.1, 106.1),
        make_event("dev-2", 1, 1000, 11.1, 107.1),
        make_event("dev-1", 2, 2000, 10.2, 106.2),
    });

    const auto result = pg.query_trip("dev-1", 0, 4000, 2);

    assert(result.size() == 2);
    assert(result[0].seq == 1);
    assert(result[1].seq == 2);
    assert(pg.query_trip("dev-1", 4000, 0, 10).empty());
    assert(pg.query_trip("dev-1", 0, 4000, 0).empty());
}

void test_query_trip_uses_server_receive_time_when_event_time_missing() {
    signalroute::PostgresClient pg(signalroute::PostGISConfig{});
    auto event = make_event("dev-1", 1, 0, 10.1, 106.1);
    event.server_recv_ms = 2500;

    pg.batch_insert_trip_points({event});

    assert(pg.query_trip("dev-1", 2000, 3000, 10).size() == 1);
    assert(pg.query_trip("dev-1", 0, 2000, 10).empty());
}

void test_query_trip_spatial_filters_by_radius() {
    signalroute::PostgresClient pg(signalroute::PostGISConfig{});

    pg.batch_insert_trip_points({
        make_event("dev-1", 1, 1000, 10.8231, 106.6297),
        make_event("dev-1", 2, 2000, 21.0285, 105.8542),
    });

    const auto nearby = pg.query_trip_spatial("dev-1", 0, 3000, 10.8231, 106.6297, 500.0, 10);

    assert(nearby.size() == 1);
    assert(nearby.front().seq == 1);
    assert(pg.query_trip_spatial("dev-1", 0, 3000, 10.8231, 106.6297, -1.0, 10).empty());
}

void test_query_trip_spatial_sorts_and_limits() {
    signalroute::PostgresClient pg(signalroute::PostGISConfig{});

    pg.batch_insert_trip_points({
        make_event("dev-1", 3, 3000, 10.8233, 106.6299),
        make_event("dev-1", 1, 1000, 10.8231, 106.6297),
        make_event("dev-1", 2, 2000, 10.8232, 106.6298),
    });

    const auto nearby = pg.query_trip_spatial("dev-1", 0, 4000, 10.8231, 106.6297, 500.0, 2);

    assert(nearby.size() == 2);
    assert(nearby[0].seq == 1);
    assert(nearby[1].seq == 2);
}

void test_geofence_repository_fallback() {
    signalroute::PostgresClient pg(signalroute::PostGISConfig{});

    signalroute::GeofenceRule active;
    active.fence_id = "fence-active";
    active.name = "Active";
    active.active = true;

    signalroute::GeofenceRule inactive;
    inactive.fence_id = "fence-inactive";
    inactive.name = "Inactive";
    inactive.active = false;

    pg.set_active_fences({active, inactive});
    const auto fences = pg.load_active_fences();

    assert(fences.size() == 1);
    assert(fences.front().fence_id == "fence-active");

    signalroute::GeofenceEventRecord event;
    event.device_id = "dev-1";
    event.fence_id = "fence-active";
    event.event_type = signalroute::GeofenceEventType::ENTER;
    pg.insert_geofence_event(event);
    assert(pg.geofence_event_count() == 1);
}

int main() {
    std::cout << "test_postgres_client:\n";
    test_trip_insert_is_idempotent();
    test_query_trip_filters_sorts_and_limits();
    test_query_trip_uses_server_receive_time_when_event_time_missing();
    test_query_trip_spatial_filters_by_radius();
    test_query_trip_spatial_sorts_and_limits();
    test_geofence_repository_fallback();
    std::cout << "All Postgres client tests passed.\n";
    return 0;
}
