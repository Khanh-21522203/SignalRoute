#pragma once

#include "common/admin/admin_service.h"
#include "common/admin/dependency_health.h"
#include "common/admin/admin_endpoint_handler.h"
#include "common/admin/admin_http_handler.h"
#include "common/config/config.h"
#include "common/events/event_bus.h"
#include "gateway/gateway_service.h"
#include "geofence/geofence_engine.h"
#include "matching/matching_service.h"
#include "processor/processor_service.h"
#include "query/query_service.h"

#include <atomic>
#include <memory>
#include <string>

namespace signalroute {

struct RuntimeRoleSelection {
    bool gateway = false;
    bool processor = false;
    bool query = false;
    bool geofence = false;
    bool matching = false;
};

[[nodiscard]] RuntimeRoleSelection select_runtime_roles(const Config& config);

class RuntimeApplication {
public:
    RuntimeApplication();
    ~RuntimeApplication();

    RuntimeApplication(const RuntimeApplication&) = delete;
    RuntimeApplication& operator=(const RuntimeApplication&) = delete;

    void start(const Config& config);
    void stop();

    [[nodiscard]] bool is_running() const;
    [[nodiscard]] bool is_healthy() const;
    [[nodiscard]] bool is_ready() const;
    [[nodiscard]] bool startup_failed() const;
    [[nodiscard]] const std::string& last_start_error() const;
    [[nodiscard]] const RuntimeRoleSelection& roles() const;
    [[nodiscard]] AdminService& admin();
    [[nodiscard]] const AdminService& admin() const;
    [[nodiscard]] DependencyHealthRegistry& dependency_health_sources();
    [[nodiscard]] const DependencyHealthRegistry& dependency_health_sources() const;
    [[nodiscard]] bool admin_http_enabled() const;
    [[nodiscard]] const AdminHttpRoutes& admin_http_routes() const;
    [[nodiscard]] AdminHttpResponse handle_admin_http(AdminHttpRequest request) const;

private:
    void configure_admin_http();
    void register_runtime_probe();
    void register_admin_probes();
    void register_dependency_readiness_probes();
    void register_startup_failure_probe();

    std::atomic<bool> running_{false};
    bool startup_failed_ = false;
    std::string last_start_error_;
    Config config_;
    RuntimeRoleSelection roles_;
    EventBus event_bus_;
    std::unique_ptr<AdminService> admin_;
    std::unique_ptr<AdminEndpointHandler> admin_endpoint_;
    std::unique_ptr<AdminHttpHandler> admin_http_;
    DependencyHealthRegistry dependency_health_;
    GatewayService gateway_;
    ProcessorService processor_;
    QueryService query_;
    GeofenceEngine geofence_;
    MatchingService matching_;
};

} // namespace signalroute
