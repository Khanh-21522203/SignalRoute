#include "runtime/admin_request_loop.h"
#include "runtime/runtime_application.h"

#include <cassert>
#include <iostream>
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

void test_loop_rejects_requests_until_started() {
    signalroute::RuntimeApplication runtime;
    runtime.start(config_for_role("query"));
    signalroute::AdminRequestLoop loop(runtime);

    const auto response = loop.handle({"GET", "/health", "application/json"});

    assert(!loop.is_running());
    assert(!loop.is_ready());
    assert(response.status_code == 503);
    assert(response.body == R"({"error":"admin request loop stopped"})");
    assert(loop.handled_requests() == 0);
}

void test_loop_routes_health_and_tracks_handled_requests() {
    signalroute::RuntimeApplication runtime;
    runtime.start(config_for_role("query"));
    signalroute::AdminRequestLoop loop(runtime);

    loop.start();
    const auto snapshot = loop.health_snapshot();
    assert(loop.is_running());
    assert(loop.is_ready());
    assert(snapshot.live);
    assert(snapshot.ready);

    const auto response = loop.handle({"GET", "/health", "application/json"});

    assert(response.status_code == 200);
    assert(response.body.find("\"role\":\"query\"") != std::string::npos);
    assert(loop.handled_requests() == 1);
}

void test_loop_delegates_disabled_admin_http_response() {
    auto config = config_for_role("query");
    config.observability.admin_http_enabled = false;
    signalroute::RuntimeApplication runtime;
    runtime.start(config);
    signalroute::AdminRequestLoop loop(runtime);

    loop.start();
    const auto response = loop.handle({"GET", "/health", "application/json"});

    assert(response.status_code == 404);
    assert(response.body == R"({"error":"admin http disabled"})");
    assert(loop.handled_requests() == 1);
}

void test_loop_stop_prevents_new_requests() {
    signalroute::RuntimeApplication runtime;
    runtime.start(config_for_role("query"));
    signalroute::AdminRequestLoop loop(runtime);

    loop.start();
    assert(loop.handle({"GET", "/health", "application/json"}).status_code == 200);
    loop.stop();

    const auto snapshot = loop.health_snapshot();
    const auto response = loop.handle({"GET", "/health", "application/json"});

    assert(!loop.is_running());
    assert(!loop.is_ready());
    assert(!snapshot.live);
    assert(snapshot.state == signalroute::ServiceLifecycleState::Stopped);
    assert(response.status_code == 503);
    assert(loop.handled_requests() == 1);
}

void test_loop_exposes_required_dependency_readiness_failure() {
    auto config = config_for_role("query");
    config.observability.require_kafka_readiness = true;
    signalroute::RuntimeApplication runtime;
    runtime.start(config);
    signalroute::AdminRequestLoop loop(runtime);

    loop.start();
    const auto health = loop.handle({"GET", "/health", "application/json"});
    const auto response = loop.handle({"GET", "/ready", "application/json"});

    assert(health.status_code == 200);
    assert(health.body.find("\"name\":\"kafka\"") == std::string::npos);
    assert(response.status_code == 503);
    assert(response.body.find("\"name\":\"kafka\"") != std::string::npos);
    assert(response.body.find("production adapter required but not enabled") != std::string::npos);
    assert(loop.handled_requests() == 2);
}

int main() {
    std::cout << "test_admin_request_loop:\n";
    test_loop_rejects_requests_until_started();
    test_loop_routes_health_and_tracks_handled_requests();
    test_loop_delegates_disabled_admin_http_response();
    test_loop_stop_prevents_new_requests();
    test_loop_exposes_required_dependency_readiness_failure();
    std::cout << "All admin request loop tests passed.\n";
    return 0;
}
