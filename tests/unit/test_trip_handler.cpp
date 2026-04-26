#include "common/clients/postgres_client.h"
#include "query/trip_handler.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>

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

void test_queries_trip_sorted_by_time_and_limit() {
    signalroute::PostgresClient pg(signalroute::PostGISConfig{});
    signalroute::TripHandler handler(pg);

    pg.batch_insert_trip_points({
        make_event("dev-1", 3, 3000, 10.3, 106.3),
        make_event("dev-1", 1, 1000, 10.1, 106.1),
        make_event("dev-1", 2, 2000, 10.2, 106.2),
        make_event("dev-2", 1, 1000, 11.1, 107.1),
    });

    const auto trip = handler.handle("dev-1", 0, 4000, 0, 2);

    assert(trip.size() == 2);
    assert(trip[0].seq == 1);
    assert(trip[1].seq == 2);
}

void test_downsamples_by_time_interval() {
    signalroute::PostgresClient pg(signalroute::PostGISConfig{});
    signalroute::TripHandler handler(pg);

    pg.batch_insert_trip_points({
        make_event("dev-1", 1, 1000, 10.1, 106.1),
        make_event("dev-1", 2, 1500, 10.2, 106.2),
        make_event("dev-1", 3, 2500, 10.3, 106.3),
        make_event("dev-1", 4, 3100, 10.4, 106.4),
    });

    const auto trip = handler.handle("dev-1", 0, 4000, 1, 10);

    assert(trip.size() == 3);
    assert(trip[0].seq == 1);
    assert(trip[1].seq == 3);
    assert(trip[2].seq == 4);
}

void test_spatial_trip_query_filters_radius() {
    signalroute::PostgresClient pg(signalroute::PostGISConfig{});
    signalroute::TripHandler handler(pg);

    pg.batch_insert_trip_points({
        make_event("dev-1", 1, 1000, 10.8231, 106.6297),
        make_event("dev-1", 2, 2000, 10.8232, 106.6298),
        make_event("dev-1", 3, 3000, 21.0285, 105.8542),
    });

    const auto trip = handler.handle_spatial("dev-1", 0, 4000, 10.8231, 106.6297, 100.0, 0, 10);

    assert(trip.size() == 2);
    assert(trip[0].seq == 1);
    assert(trip[1].seq == 2);
}

void test_invalid_trip_queries_return_empty() {
    signalroute::PostgresClient pg(signalroute::PostGISConfig{});
    signalroute::TripHandler handler(pg);

    assert(handler.handle("", 0, 1000, 0, 10).empty());
    assert(handler.handle("dev-1", 1000, 0, 0, 10).empty());
    assert(handler.handle("dev-1", 0, 1000, 0, 0).empty());
    assert(handler.handle_spatial("dev-1", 0, 1000, 91.0, 0.0, 100.0, 0, 10).empty());
    assert(handler.handle_spatial("dev-1", 0, 1000, 0.0, 181.0, 100.0, 0, 10).empty());
    assert(handler.handle_spatial("dev-1", 0, 1000, 0.0, 0.0, -1.0, 0, 10).empty());
    assert(handler.handle_spatial("dev-1", 0, 1000, 0.0, 0.0,
                                  std::numeric_limits<double>::infinity(), 0, 10).empty());
}

int main() {
    std::cout << "test_trip_handler:\n";
    test_queries_trip_sorted_by_time_and_limit();
    test_downsamples_by_time_interval();
    test_spatial_trip_query_filters_radius();
    test_invalid_trip_queries_return_empty();
    std::cout << "All trip handler tests passed.\n";
    return 0;
}
