#include "common/admin/admin_endpoint_handler.h"
#include "common/metrics/metrics.h"

#include <cassert>
#include <iostream>
#include <string>

void test_health_handler_serializes_healthy_response() {
    signalroute::AdminService admin("gateway", "test-version");
    admin.register_component("gateway", [] {
        return signalroute::ComponentHealth{"gateway", true, true, "serving"};
    });
    signalroute::AdminEndpointHandler handler(admin);

    const auto response = handler.handle_health();

    assert(response.ok());
    assert(response.status_code == 200);
    assert(response.content_type == "application/json");
    assert(response.body.find("\"healthy\":true") != std::string::npos);
    assert(response.body.find("\"role\":\"gateway\"") != std::string::npos);
    assert(response.body.find("\"name\":\"gateway\"") != std::string::npos);
}

void test_health_handler_maps_unhealthy_to_503_and_escapes_detail() {
    signalroute::AdminService admin("processor");
    admin.register_component("redis", [] {
        return signalroute::ComponentHealth{"redis", false, true, "ping \"failed\""};
    });
    signalroute::AdminEndpointHandler handler(admin);

    const auto response = handler.handle_health();

    assert(!response.ok());
    assert(response.status_code == 503);
    assert(response.body.find("\"healthy\":false") != std::string::npos);
    assert(response.body.find("ping \\\"failed\\\"") != std::string::npos);
}

void test_metrics_handler_returns_prometheus_text() {
    auto& metrics = signalroute::Metrics::instance();
    metrics.reset_for_test();
    metrics.inc_events_accepted(2);
    signalroute::AdminService admin("standalone");
    signalroute::AdminEndpointHandler handler(admin);

    const auto response = handler.handle_metrics();

    assert(response.ok());
    assert(response.status_code == 200);
    assert(response.content_type == "text/plain; version=0.0.4");
    assert(response.body.find("events_accepted_total 2") != std::string::npos);
}

int main() {
    std::cout << "test_admin_endpoint_handler:\n";
    test_health_handler_serializes_healthy_response();
    test_health_handler_maps_unhealthy_to_503_and_escapes_detail();
    test_metrics_handler_returns_prometheus_text();
    std::cout << "All admin endpoint handler tests passed.\n";
    return 0;
}
