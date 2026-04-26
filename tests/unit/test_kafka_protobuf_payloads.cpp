#include "common/kafka/kafka_consumer.h"
#include "common/kafka/kafka_producer.h"
#include "common/proto/geofence_payload_codec.h"
#include "common/proto/location_payload_codec.h"
#include "common/proto/matching_payload_codec.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string topic_name(const std::string& suffix) {
    return "test.kafka.protobuf." + suffix;
}

void test_location_event_payload_round_trip_through_kafka_fallback() {
    const std::string topic = topic_name("location");
    signalroute::KafkaProducer producer(signalroute::KafkaConfig{});
    signalroute::KafkaConsumer consumer(signalroute::KafkaConfig{}, {topic});

    signalroute::LocationEvent event;
    event.device_id = "dev-1";
    event.lat = 10.8231;
    event.lon = 106.6297;
    event.timestamp_ms = 1000;
    event.server_recv_ms = 1100;
    event.seq = 42;
    event.metadata["source"] = "protobuf";

    producer.produce(topic, event.device_id, signalroute::proto_boundary::encode_location_payload(event));
    auto message = consumer.poll(0);
    assert(message.has_value());
    assert(message->key == "dev-1");
    assert(message->payload.rfind(signalroute::proto_boundary::protobuf_location_payload_prefix(), 0) == 0);

    const auto parsed = signalroute::proto_boundary::decode_location_payload(message->payload);
    assert(parsed.is_ok());
    assert(parsed.value().device_id == event.device_id);
    assert(parsed.value().metadata.at("source") == "protobuf");
}

void test_geofence_event_payload_round_trip_through_kafka_fallback() {
    const std::string topic = topic_name("geofence");
    signalroute::KafkaProducer producer(signalroute::KafkaConfig{});
    signalroute::KafkaConsumer consumer(signalroute::KafkaConfig{}, {topic});

    signalroute::GeofenceEventRecord event;
    event.device_id = "dev-2";
    event.fence_id = "fence-1";
    event.fence_name = "Depot";
    event.event_type = signalroute::GeofenceEventType::DWELL;
    event.lat = 10.0;
    event.lon = 106.0;
    event.event_ts_ms = 3000;
    event.inside_duration_s = 600;

    producer.produce(topic, event.device_id, signalroute::proto_boundary::encode_geofence_event_payload(event));
    auto message = consumer.poll(0);
    assert(message.has_value());
    assert(message->payload.rfind(signalroute::proto_boundary::protobuf_geofence_event_payload_prefix(), 0) == 0);

    const auto parsed = signalroute::proto_boundary::decode_geofence_event_payload(message->payload);
    assert(parsed.is_ok());
    assert(parsed.value().event_type == signalroute::GeofenceEventType::DWELL);
    assert(parsed.value().inside_duration_s == 600);
}

void test_matching_payloads_round_trip_through_kafka_fallback() {
    const std::string request_topic = topic_name("match_request");
    const std::string result_topic = topic_name("match_result");
    signalroute::KafkaProducer producer(signalroute::KafkaConfig{});
    signalroute::KafkaConsumer request_consumer(signalroute::KafkaConfig{}, {request_topic});
    signalroute::KafkaConsumer result_consumer(signalroute::KafkaConfig{}, {result_topic});

    signalroute::MatchRequest request;
    request.request_id = "req-1";
    request.requester_id = "rider-1";
    request.lat = 10.0;
    request.lon = 106.0;
    request.radius_m = 500.0;
    request.max_agents = 2;
    request.strategy = "nearest";

    producer.produce(request_topic, request.request_id, signalroute::proto_boundary::encode_match_request_payload(request));

    auto request_message = request_consumer.poll(0);
    assert(request_message.has_value());
    assert(request_message->payload.rfind(signalroute::proto_boundary::protobuf_match_request_payload_prefix(), 0) == 0);
    const auto parsed_request = signalroute::proto_boundary::decode_match_request_payload(request_message->payload);
    assert(parsed_request.is_ok());
    assert(parsed_request.value().request_id == "req-1");
    assert(parsed_request.value().max_agents == 2);

    signalroute::MatchResult result;
    result.request_id = "req-1";
    result.status = signalroute::MatchStatus::MATCHED;
    result.assigned_agent_ids = {"agent-1"};
    result.latency_ms = 25;

    producer.produce(result_topic, result.request_id, signalroute::proto_boundary::encode_match_result_payload(
        result, request.requester_id, request.strategy));

    auto result_message = result_consumer.poll(0);
    assert(result_message.has_value());
    assert(result_message->payload.rfind(signalroute::proto_boundary::protobuf_match_result_payload_prefix(), 0) == 0);
    const auto parsed_result = signalroute::proto_boundary::decode_match_result_payload(result_message->payload);
    assert(parsed_result.is_ok());
    assert(parsed_result.value().status == signalroute::MatchStatus::MATCHED);
    assert((parsed_result.value().assigned_agent_ids == std::vector<std::string>{"agent-1"}));
}

} // namespace

int main() {
    std::cout << "test_kafka_protobuf_payloads:\n";
    test_location_event_payload_round_trip_through_kafka_fallback();
    test_geofence_event_payload_round_trip_through_kafka_fallback();
    test_matching_payloads_round_trip_through_kafka_fallback();
    std::cout << "All Kafka protobuf payload tests passed.\n";
    return 0;
}
