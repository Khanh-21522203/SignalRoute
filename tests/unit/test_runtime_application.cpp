#include "runtime/runtime_application.h"

#include <cassert>
#include <iostream>

namespace {

signalroute::Config config_for_role(const std::string& role) {
    signalroute::Config config;
    config.server.role = role;
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
    assert(health.component_healthy("query"));
    assert(health.component_healthy("event_bus"));
    assert(health.components.size() == 2);

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
    assert(health.component_healthy("gateway"));
    assert(health.component_healthy("event_bus"));
    assert(health.components.size() == 2);

    app.stop();
}

int main() {
    std::cout << "test_runtime_application:\n";
    test_role_selection_respects_role_and_geofence_flag();
    test_query_runtime_registers_admin_probes_and_stops_cleanly();
    test_gateway_runtime_registers_only_gateway_probe();
    std::cout << "All runtime application tests passed.\n";
    return 0;
}
