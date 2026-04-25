#include "common/clients/redis_client.h"
#include "common/spatial/h3_index.h"
#include "processor/state_writer.h"

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

void test_writes_latest_state() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});
    signalroute::H3Index h3(7);
    signalroute::StateWriter writer(redis, h3, 3600);

    auto event = make_event("dev-1", 10.8231, 106.6297, 1);
    assert(writer.write(event));

    auto state = redis.get_device_state("dev-1");
    assert(state.has_value());
    assert(state->device_id == "dev-1");
    assert(state->seq == 1);
    assert(state->h3_cell == h3.lat_lng_to_cell(event.lat, event.lon));
}

void test_rejects_stale_write() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});
    signalroute::H3Index h3(7);
    signalroute::StateWriter writer(redis, h3, 3600);

    assert(writer.write(make_event("dev-1", 10.8231, 106.6297, 2)));
    assert(!writer.write(make_event("dev-1", 10.0, 106.0, 1)));

    auto state = redis.get_device_state("dev-1");
    assert(state.has_value());
    assert(state->seq == 2);
    assert(state->lat == 10.8231);
}

void test_moves_device_between_cells() {
    signalroute::RedisClient redis(signalroute::RedisConfig{});
    signalroute::H3Index h3(9);
    signalroute::StateWriter writer(redis, h3, 3600);

    auto first = make_event("dev-1", 10.8231, 106.6297, 1);
    auto second = make_event("dev-1", 10.9000, 106.7000, 2);
    assert(writer.write(first));
    auto old_cell = h3.lat_lng_to_cell(first.lat, first.lon);
    assert(writer.write(second));
    auto new_cell = h3.lat_lng_to_cell(second.lat, second.lon);
    assert(old_cell != new_cell);

    assert(redis.get_devices_in_cell(old_cell).empty());
    assert(redis.get_devices_in_cell(new_cell).size() == 1);
}

int main() {
    std::cout << "test_state_writer:\n";
    test_writes_latest_state();
    test_rejects_stale_write();
    test_moves_device_between_cells();
    std::cout << "All state writer tests passed.\n";
    return 0;
}
