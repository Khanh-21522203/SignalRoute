#include "runtime/runtime_application.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

signalroute::Config config_for_role(const std::string& role) {
    signalroute::Config config;
    config.server.role = role;
    config.postgis.dsn = "host=localhost dbname=signalroute";
    config.geofence.eval_enabled = false;
    config.gateway.rate_limit_rps_per_device = 1000;
    config.gateway.timestamp_skew_tolerance_s = 60;
    return config;
}

} // namespace

void test_role_selection_respects_role_and_geofence_flag() {
    auto standalone = config_for_role("standalone");
    standalone.geofence.eval_enabled = true;
    auto selected = signalroute::select_runtime_roles(standalone);
    assert(selected.gateway);
    assert(selected.processor);
    assert(selected.query);
    assert(selected.geofence);
    assert(selected.matching);

    auto disabled_geofence = config_for_role("standalone");
    selected = signalroute::select_runtime_roles(disabled_geofence);
    assert(!selected.geofence);

    selected = signalroute::select_runtime_roles(config_for_role("matcher"));
    assert(selected.matching);
    assert(!selected.gateway);
    assert(!selected.query);
}

void test_query_runtime_registers_admin_probes_and_stops_cleanly() {
    signalroute::RuntimeApplication app;
    app.start(config_for_role("query"));

    assert(app.is_running());
    assert(app.is_healthy());
    assert(app.is_ready());
    assert(app.roles().query);
    assert(!app.roles().gateway);

    auto health = app.admin().health();
    assert(health.healthy);
    assert(health.role == "query");
    assert(health.component_healthy("runtime_startup"));
    assert(health.component_healthy("query"));
    assert(health.component_healthy("event_bus"));
    assert(health.components.size() == 3);

    app.stop();
    assert(!app.is_running());
    assert(!app.is_healthy());
    assert(!app.is_ready());

    health = app.admin().health();
    assert(!health.healthy);
    assert(!health.component_healthy("query"));
}

void test_gateway_runtime_registers_only_gateway_probe() {
    signalroute::RuntimeApplication app;
    app.start(config_for_role("gateway"));

    assert(app.is_ready());
    assert(app.roles().gateway);
    assert(!app.roles().query);

    const auto health = app.admin().health();
    assert(health.healthy);
    assert(health.component_healthy("runtime_startup"));
    assert(health.component_healthy("gateway"));
    assert(health.component_healthy("event_bus"));
    assert(health.components.size() == 3);

    app.stop();
}

void test_runtime_exposes_admin_http_handler_with_configured_routes() {
    auto config = config_for_role("query");
    config.observability.metrics_path = "/ops/metrics";
    config.observability.health_path = "/live";
    config.observability.readiness_path = "/ready-custom";

    signalroute::RuntimeApplication app;
    app.start(config);

    assert(app.admin_http_enabled());
    assert(app.admin_http_routes().health_path == "/live");
    assert(app.admin_http_routes().readiness_path == "/ready-custom");
    assert(app.admin_http_routes().metrics_path == "/ops/metrics");

    auto response = app.handle_admin_http({"GET", "/live", "application/json"});
    assert(response.status_code == 200);
    assert(response.body.find("\"role\":\"query\"") != std::string::npos);

    response = app.handle_admin_http({"GET", "/ops/metrics", "text/plain"});
    assert(response.status_code == 200);
    assert(response.content_type == "text/plain; version=0.0.4");

    app.stop();
}

void test_runtime_admin_http_can_be_disabled_by_config() {
    auto config = config_for_role("query");
    config.observability.admin_http_enabled = false;

    signalroute::RuntimeApplication app;
    app.start(config);

    assert(!app.admin_http_enabled());
    const auto response = app.handle_admin_http({"GET", "/health", "application/json"});
    assert(response.status_code == 404);
    assert(response.body == R"({"error":"admin http disabled"})");

    app.stop();
}

void test_required_dependency_readiness_policy_marks_runtime_unready_but_live() {
    auto config = config_for_role("query");
    config.observability.require_redis_readiness = true;

    signalroute::RuntimeApplication app;
    app.start(config);

    assert(app.is_running());
    assert(app.is_healthy());
    assert(!app.is_ready());

    const auto health = app.admin().health();
    const auto readiness = app.admin().readiness();
    assert(health.healthy);
    assert(!readiness.healthy);
    assert(!readiness.component_healthy("redis"));
    assert(health.components.size() == 3);
    assert(readiness.components.size() == 4);

    const auto health_response = app.handle_admin_http({"GET", "/health", "application/json"});
    const auto readiness_response = app.handle_admin_http({"GET", "/ready", "application/json"});
    assert(health_response.status_code == 200);
    assert(health_response.body.find("\"name\":\"redis\"") == std::string::npos);
    assert(readiness_response.status_code == 503);
    assert(readiness_response.body.find("\"name\":\"redis\"") != std::string::npos);
    assert(readiness_response.body.find("production adapter required but not enabled") != std::string::npos);

    app.stop();
}

void test_required_dependency_readiness_policy_can_use_custom_health_source() {
    auto config = config_for_role("query");
    config.observability.require_redis_readiness = true;

    signalroute::RuntimeApplication app;
    app.dependency_health_sources().register_source("redis", [] {
        return signalroute::DependencyHealthSnapshot{"redis", true, "redis adapter reachable"};
    });
    app.start(config);

    assert(app.is_running());
    assert(app.is_healthy());
    assert(app.is_ready());

    const auto readiness = app.admin().readiness();
    assert(readiness.healthy);
    assert(readiness.component_healthy("redis"));

    const auto response = app.handle_admin_http({"GET", "/ready", "application/json"});
    assert(response.status_code == 200);
    assert(response.body.find("\"name\":\"redis\"") != std::string::npos);
    assert(response.body.find("redis adapter reachable") != std::string::npos);

    app.stop();
}

void test_runtime_startup_failure_is_reported_to_admin_health() {
    signalroute::RuntimeApplication app;
    bool thrown = false;
    try {
        app.start(config_for_role("worker"));
    } catch (const std::runtime_error& ex) {
        thrown = true;
        const std::string message = ex.what();
        assert(message.find("Runtime startup failed") != std::string::npos);
        assert(message.find("server.role") != std::string::npos);
    }

    assert(thrown);
    assert(!app.is_running());
    assert(!app.is_healthy());
    assert(!app.is_ready());
    assert(app.startup_failed());
    assert(app.last_start_error().find("server.role") != std::string::npos);

    const auto health = app.admin().health();
    assert(!health.healthy);
    assert(health.role == "worker");
    assert(!health.component_healthy("runtime_startup"));
    assert(health.components.size() == 1);
    assert(health.components.front().detail.find("server.role") != std::string::npos);
}

int main() {
    std::cout << "test_runtime_application:\n";
    test_role_selection_respects_role_and_geofence_flag();
    test_query_runtime_registers_admin_probes_and_stops_cleanly();
    test_gateway_runtime_registers_only_gateway_probe();
    test_runtime_exposes_admin_http_handler_with_configured_routes();
    test_runtime_admin_http_can_be_disabled_by_config();
    test_required_dependency_readiness_policy_marks_runtime_unready_but_live();
    test_required_dependency_readiness_policy_can_use_custom_health_source();
    test_runtime_startup_failure_is_reported_to_admin_health();
    std::cout << "All runtime application tests passed.\n";
    return 0;
}
