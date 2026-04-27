#include "common/admin/admin_service.h"
#include "common/metrics/metrics.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

void test_health_reports_registered_components() {
    signalroute::AdminService admin("standalone", "test-version");
    admin.register_component("gateway", [] {
        return signalroute::ComponentHealth{"gateway", true, true, "running"};
    });
    admin.register_component("metrics_reporter", [] {
        return signalroute::ComponentHealth{"metrics_reporter", true, false, "fallback exporter"};
    });

    const auto health = admin.health();

    assert(health.healthy);
    assert(health.role == "standalone");
    assert(health.version == "test-version");
    assert(health.components.size() == 2);
    assert(health.component_healthy("gateway"));
    assert(health.component_healthy("metrics_reporter"));
    assert(!health.component_healthy("missing"));
}

void test_required_component_failure_marks_service_unhealthy() {
    signalroute::AdminService admin("processor");
    admin.register_component("redis", [] {
        return signalroute::ComponentHealth{"redis", false, true, "ping failed"};
    });
    admin.register_component("metrics", [] {
        return signalroute::ComponentHealth{"metrics", false, false, "export deferred"};
    });

    const auto health = admin.health();

    assert(!health.healthy);
    assert(!health.component_healthy("redis"));
    assert(!health.component_healthy("metrics"));
    assert(health.components[0].detail == "ping failed");
}

void test_probe_exception_is_reported_as_unhealthy_component() {
    signalroute::AdminService admin("query");
    admin.register_component("postgis", []() -> signalroute::ComponentHealth {
        throw std::runtime_error("connection refused");
    });

    const auto health = admin.health();

    assert(!health.healthy);
    assert(health.components.size() == 1);
    assert(health.components.front().name == "postgis");
    assert(health.components.front().detail == "connection refused");
}

void test_metrics_response_exports_prometheus_text() {
    auto& metrics = signalroute::Metrics::instance();
    metrics.reset_for_test();
    metrics.inc_events_accepted(3);

    signalroute::AdminService admin("standalone");
    const auto response = admin.metrics();

    assert(response.metrics_text.find("# SignalRoute metrics fallback") != std::string::npos);
    assert(response.metrics_text.find("events_accepted_total 3") != std::string::npos);
}

int main() {
    std::cout << "test_admin_service:\n";
    test_health_reports_registered_components();
    test_required_component_failure_marks_service_unhealthy();
    test_probe_exception_is_reported_as_unhealthy_component();
    test_metrics_response_exports_prometheus_text();
    std::cout << "All admin service tests passed.\n";
    return 0;
}
