#include "common/clients/postgres_client.h"
#include "common/kafka/kafka_producer.h"
#include "processor/history_writer.h"

#include <cassert>
#include <iostream>
#include <string>

namespace {

signalroute::LocationEvent make_event(std::string device_id, uint64_t seq) {
    signalroute::LocationEvent event;
    event.device_id = std::move(device_id);
    event.seq = seq;
    event.timestamp_ms = 1000 + static_cast<int64_t>(seq);
    event.server_recv_ms = event.timestamp_ms + 5;
    event.lat = 10.8231;
    event.lon = 106.6297;
    return event;
}

signalroute::ProcessorConfig history_config(int batch_size) {
    signalroute::ProcessorConfig config;
    config.history_batch_size = batch_size;
    return config;
}

} // namespace

void test_buffers_until_flush_threshold() {
    signalroute::PostgresClient pg(signalroute::PostGISConfig{});
    signalroute::KafkaProducer dlq(signalroute::KafkaConfig{});
    signalroute::HistoryWriter writer(pg, dlq, history_config(2));

    writer.buffer(make_event("dev-1", 1));

    assert(writer.buffer_size() == 1);
    assert(!writer.should_flush());
    assert(pg.trip_point_count() == 0);
}

void test_flush_writes_buffered_trip_points() {
    signalroute::PostgresClient pg(signalroute::PostGISConfig{});
    signalroute::KafkaProducer dlq(signalroute::KafkaConfig{});
    signalroute::HistoryWriter writer(pg, dlq, history_config(2));

    writer.buffer(make_event("dev-1", 1));
    writer.buffer(make_event("dev-1", 2));

    assert(writer.should_flush());
    writer.flush();

    assert(writer.buffer_size() == 0);
    assert(pg.trip_point_count() == 2);

    const auto trip = pg.query_trip("dev-1", 0, 5000, 10);
    assert(trip.size() == 2);
    assert(trip[0].seq == 1);
    assert(trip[1].seq == 2);
}

void test_flush_empty_buffer_is_noop() {
    signalroute::PostgresClient pg(signalroute::PostGISConfig{});
    signalroute::KafkaProducer dlq(signalroute::KafkaConfig{});
    signalroute::HistoryWriter writer(pg, dlq, history_config(1));

    writer.flush();

    assert(writer.buffer_size() == 0);
    assert(pg.trip_point_count() == 0);
}

int main() {
    std::cout << "test_history_writer:\n";
    test_buffers_until_flush_threshold();
    test_flush_writes_buffered_trip_points();
    test_flush_empty_buffer_is_noop();
    std::cout << "All history writer tests passed.\n";
    return 0;
}
