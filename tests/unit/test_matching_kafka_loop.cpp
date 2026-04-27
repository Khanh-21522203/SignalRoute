#include "common/kafka/kafka_consumer.h"
#include "common/kafka/kafka_producer.h"
#include "common/proto/matching_payload_codec.h"
#include "matching/matching_service.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string topic_name(const std::string& suffix) {
    return "test.matching.kafka." + suffix;
}

signalroute::Config matching_config(const std::string& suffix) {
    signalroute::Config config;
    config.matching.strategy_name = "nearest";
    config.matching.request_ttl_ms = 5000;
    config.matching.request_topic = topic_name("requests." + suffix);
    config.matching.result_topic = topic_name("results." + suffix);
    config.spatial.h3_resolution = 7;
    config.spatial.nearby_max_radius_m = 50000.0;
    config.spatial.nearby_max_results = 100;
    return config;
}

signalroute::DeviceState agent(std::string id, double lat, double lon, uint64_t seq = 1) {
    signalroute::DeviceState state;
    state.device_id = std::move(id);
    state.lat = lat;
    state.lon = lon;
    state.seq = seq;
    state.updated_at = now_ms();
    return state;
}

signalroute::MatchRequest request(std::string id) {
    signalroute::MatchRequest req;
    req.request_id = std::move(id);
    req.requester_id = "rider-1";
    req.lat = 10.8231;
    req.lon = 106.6297;
    req.radius_m = 5000.0;
    req.max_agents = 1;
    req.deadline_ms = now_ms() + 5000;
    req.strategy = "nearest";
    return req;
}

signalroute::MatchResult decode_result_payload(const std::string& payload) {
    auto decoded = signalroute::proto_boundary::decode_match_result_payload(payload);
    assert(decoded.is_ok());
    return decoded.value();
}

} // namespace

void test_matching_loop_matches_request_and_publishes_result() {
    const auto config = matching_config("matched");
    signalroute::KafkaProducer producer(signalroute::KafkaConfig{});
    signalroute::KafkaConsumer result_consumer(signalroute::KafkaConfig{}, {config.matching.result_topic});
    signalroute::MatchingService service;

    service.start(config);
    assert(service.seed_agent_for_test(agent("agent-far", 10.8300, 106.6400)));
    assert(service.seed_agent_for_test(agent("agent-near", 10.8232, 106.6298)));

    auto req = request("req-1");
    producer.produce(
        config.matching.request_topic,
        req.request_id,
        signalroute::proto_boundary::encode_match_request_payload(req));

    const auto loop = service.process_requests_once();
    assert(loop.processed_requests == 1);
    assert(loop.published_results == 1);
    assert(loop.invalid_messages == 0);
    assert(loop.failed_messages == 0);

    const auto message = result_consumer.poll(0);
    assert(message.has_value());
    assert(message->key == "req-1");
    const auto result = decode_result_payload(message->payload);
    assert(result.status == signalroute::MatchStatus::MATCHED);
    assert((result.assigned_agent_ids == std::vector<std::string>{"agent-near"}));
}

void test_matching_loop_publishes_failed_result_when_no_candidate_matches() {
    const auto config = matching_config("failed");
    signalroute::KafkaProducer producer(signalroute::KafkaConfig{});
    signalroute::KafkaConsumer result_consumer(signalroute::KafkaConfig{}, {config.matching.result_topic});
    signalroute::MatchingService service;

    service.start(config);

    auto req = request("req-empty");
    producer.produce(
        config.matching.request_topic,
        req.request_id,
        signalroute::proto_boundary::encode_match_request_payload(req));

    const auto loop = service.process_requests_once();
    assert(loop.processed_requests == 1);
    assert(loop.published_results == 1);
    assert(loop.invalid_messages == 0);

    const auto message = result_consumer.poll(0);
    assert(message.has_value());
    const auto result = decode_result_payload(message->payload);
    assert(result.request_id == "req-empty");
    assert(result.status == signalroute::MatchStatus::FAILED);
    assert(result.reason == "no agents matched");
}

void test_matching_loop_commits_invalid_payload_without_result() {
    const auto config = matching_config("invalid");
    signalroute::KafkaProducer producer(signalroute::KafkaConfig{});
    signalroute::KafkaConsumer result_consumer(signalroute::KafkaConfig{}, {config.matching.result_topic});
    signalroute::MatchingService service;

    service.start(config);
    producer.produce(config.matching.request_topic, "bad", "not,a,match,request");

    const auto loop = service.process_requests_once();
    assert(loop.processed_requests == 0);
    assert(loop.published_results == 0);
    assert(loop.invalid_messages == 1);
    assert(loop.failed_messages == 0);
    assert(!result_consumer.poll(0).has_value());
}

int main() {
    std::cout << "test_matching_kafka_loop:\n";
    test_matching_loop_matches_request_and_publishes_result();
    test_matching_loop_publishes_failed_result_when_no_candidate_matches();
    test_matching_loop_commits_invalid_payload_without_result();
    std::cout << "All matching Kafka loop tests passed.\n";
    return 0;
}
