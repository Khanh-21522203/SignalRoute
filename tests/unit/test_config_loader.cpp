#include "common/config/config.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

#ifndef SIGNALROUTE_SOURCE_DIR
#define SIGNALROUTE_SOURCE_DIR "."
#endif

namespace {

std::filesystem::path write_config(const std::string& name, const std::string& content) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path);
    assert(out.is_open());
    out << content;
    return path;
}

void expect_throws(const std::function<void()>& fn) {
    bool thrown = false;
    try {
        fn();
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    assert(thrown);
}

std::string minimal_valid_config() {
    return R"toml(
[postgis]
dsn = "host=localhost dbname=signalroute"
)toml";
}

} // namespace

void test_loads_canonical_config() {
    const auto path = std::filesystem::path(SIGNALROUTE_SOURCE_DIR) / "config" / "signalroute.toml";
    const auto config = signalroute::Config::load(path.string());

    assert(config.server.role == "standalone");
    assert(config.server.grpc_port == 9090);
    assert(config.kafka.ingest_topic == "tm.location.events");
    assert(config.kafka.dlq_topic == "tm.location.dlq");
    assert(config.redis.addrs == "localhost:6379");
    assert(config.postgis.dsn.find("dbname=signalroute") != std::string::npos);
    assert(config.geofence.eval_enabled);
    assert(!config.gateway.auth_required);
    assert(config.gateway.api_key.empty());
    assert(config.gateway.max_in_flight_requests == 0);
    assert(config.matching.request_topic == "sr.match.requests");
    assert(config.observability.admin_http_enabled);
    assert(!config.observability.admin_socket_enabled);
    assert(config.observability.admin_socket_addr == "127.0.0.1");
    assert(config.observability.admin_socket_port == 9101);
    assert(config.observability.admin_socket_backlog == 16);
    assert(config.observability.health_path == "/health");
    assert(config.observability.readiness_path == "/ready");
    assert(!config.observability.require_kafka_readiness);
    assert(!config.observability.require_redis_readiness);
    assert(!config.observability.require_postgis_readiness);
    assert(!config.observability.require_h3_readiness);
    assert(config.observability.log_level == "info");
}

void test_overrides_comments_and_defaults() {
    const auto path = write_config("signalroute_config_overrides.toml", R"toml(
# Only override a few fields; defaults should fill the rest.
[server]
role = "query" # inline comment
listen_addr = "127.0.0.1"
grpc_port = 9191

[kafka]
brokers = "kafka-a:9092#not-a-comment"
ingest_topic = "custom.location.events"

[postgis]
dsn = "host=localhost dbname=signalroute"

[processor]
sequence_guard_enabled = false

[spatial]
nearby_max_radius_m = 123.5

[gateway]
auth_required = true
api_key = "secret"
max_in_flight_requests = 8

[observability]
metrics_path = "/custom-metrics"
admin_http_enabled = false
admin_socket_enabled = true
admin_socket_addr = "127.0.0.1"
admin_socket_port = 0
admin_socket_backlog = 4
health_path = "/live"
readiness_path = "/ready-custom"
require_kafka_readiness = true
require_redis_readiness = true
require_postgis_readiness = true
require_h3_readiness = true
)toml");

    const auto config = signalroute::Config::load(path.string());

    assert(config.server.role == "query");
    assert(config.server.listen_addr == "127.0.0.1");
    assert(config.server.grpc_port == 9191);
    assert(config.server.udp_port == 9091);
    assert(config.kafka.brokers == "kafka-a:9092#not-a-comment");
    assert(config.kafka.ingest_topic == "custom.location.events");
    assert(config.kafka.dlq_topic == "tm.location.dlq");
    assert(!config.processor.sequence_guard_enabled);
    assert(config.spatial.nearby_max_radius_m == 123.5);
    assert(config.gateway.auth_required);
    assert(config.gateway.api_key == "secret");
    assert(config.gateway.max_in_flight_requests == 8);
    assert(config.observability.metrics_path == "/custom-metrics");
    assert(!config.observability.admin_http_enabled);
    assert(config.observability.admin_socket_enabled);
    assert(config.observability.admin_socket_addr == "127.0.0.1");
    assert(config.observability.admin_socket_port == 0);
    assert(config.observability.admin_socket_backlog == 4);
    assert(config.observability.health_path == "/live");
    assert(config.observability.readiness_path == "/ready-custom");
    assert(config.observability.require_kafka_readiness);
    assert(config.observability.require_redis_readiness);
    assert(config.observability.require_postgis_readiness);
    assert(config.observability.require_h3_readiness);
}

void test_missing_file_is_rejected() {
    expect_throws([] {
        (void)signalroute::Config::load("/tmp/signalroute_missing_config.toml");
    });
}

void test_invalid_role_is_rejected() {
    const auto path = write_config("signalroute_config_invalid_role.toml", minimal_valid_config() + R"toml(
[server]
role = "worker"
)toml");

    expect_throws([&] {
        (void)signalroute::Config::load(path.string());
    });
}

void test_invalid_numeric_value_is_rejected() {
    const auto path = write_config("signalroute_config_invalid_number.toml", minimal_valid_config() + R"toml(
[gateway]
max_batch_events = 0
)toml");

    expect_throws([&] {
        (void)signalroute::Config::load(path.string());
    });
}

void test_invalid_value_type_is_rejected() {
    const auto path = write_config("signalroute_config_invalid_type.toml", minimal_valid_config() + R"toml(
[redis]
pool_size = "many"
)toml");

    expect_throws([&] {
        (void)signalroute::Config::load(path.string());
    });
}

void test_auth_required_without_api_key_is_rejected() {
    const auto path = write_config("signalroute_config_missing_gateway_api_key.toml", minimal_valid_config() + R"toml(
[gateway]
auth_required = true
)toml");

    expect_throws([&] {
        (void)signalroute::Config::load(path.string());
    });
}

void test_negative_in_flight_limit_is_rejected() {
    const auto path = write_config("signalroute_config_invalid_in_flight.toml", minimal_valid_config() + R"toml(
[gateway]
max_in_flight_requests = -1
)toml");

    expect_throws([&] {
        (void)signalroute::Config::load(path.string());
    });
}

void test_observability_paths_must_start_with_slash() {
    const auto path = write_config("signalroute_config_invalid_observability_path.toml", minimal_valid_config() + R"toml(
[observability]
health_path = "health"
)toml");

    expect_throws([&] {
        (void)signalroute::Config::load(path.string());
    });
}

void test_invalid_admin_socket_config_is_rejected() {
    const auto path = write_config("signalroute_config_invalid_admin_socket.toml", minimal_valid_config() + R"toml(
[observability]
admin_socket_port = -1
)toml");

    expect_throws([&] {
        (void)signalroute::Config::load(path.string());
    });
}

void test_post_load_override_validation_uses_same_rules() {
    const auto path = write_config("signalroute_config_post_load_override.toml", minimal_valid_config());
    auto config = signalroute::Config::load(path.string());
    config.server.role = "worker";

    expect_throws([&] {
        config.validate();
    });
}

int main() {
    std::cout << "test_config_loader:\n";
    test_loads_canonical_config();
    test_overrides_comments_and_defaults();
    test_missing_file_is_rejected();
    test_invalid_role_is_rejected();
    test_invalid_numeric_value_is_rejected();
    test_invalid_value_type_is_rejected();
    test_auth_required_without_api_key_is_rejected();
    test_negative_in_flight_limit_is_rejected();
    test_observability_paths_must_start_with_slash();
    test_invalid_admin_socket_config_is_rejected();
    test_post_load_override_validation_uses_same_rules();
    std::cout << "All config loader tests passed.\n";
    return 0;
}
