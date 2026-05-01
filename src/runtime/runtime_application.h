#pragma once

#include "common/admin/admin_service.h"
#include "common/admin/dependency_health.h"
#include "common/admin/admin_endpoint_handler.h"
#include "common/admin/admin_http_handler.h"
#include "common/admin/lifecycle.h"
#include "common/config/config.h"
#include "common/events/event_bus.h"
#include "gateway/gateway_service.h"
#include "geofence/geofence_engine.h"
#include "matching/matching_service.h"
#include "processor/processor_service.h"
#include "query/query_service.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace signalroute {

class AdminRequestLoop;
class AdminSocketServer;

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
    void stop(std::string reason);

    [[nodiscard]] bool is_running() const;
    [[nodiscard]] bool is_healthy() const;
    [[nodiscard]] bool is_ready() const;
    [[nodiscard]] bool startup_failed() const;
    [[nodiscard]] const std::string& last_start_error() const;
    [[nodiscard]] const std::string& last_stop_reason() const;
    [[nodiscard]] const RuntimeRoleSelection& roles() const;
    [[nodiscard]] AdminService& admin();
    [[nodiscard]] const AdminService& admin() const;
    [[nodiscard]] DependencyHealthRegistry& dependency_health_sources();
    [[nodiscard]] const DependencyHealthRegistry& dependency_health_sources() const;
    [[nodiscard]] bool admin_http_enabled() const;
    [[nodiscard]] const AdminHttpRoutes& admin_http_routes() const;
    [[nodiscard]] AdminHttpResponse handle_admin_http(AdminHttpRequest request) const;
    [[nodiscard]] bool admin_socket_enabled() const;
    [[nodiscard]] bool admin_socket_running() const;
    [[nodiscard]] uint16_t admin_socket_bound_port() const;
    [[nodiscard]] ServiceHealthSnapshot admin_socket_health_snapshot() const;

private:
    void configure_admin_http();
    void configure_admin_socket();
    void start_admin_socket();
    void stop_started_services();
    void register_runtime_probe();
    void register_admin_probes();
    void register_dependency_readiness_probes();
    void register_startup_failure_probe();

    std::atomic<bool> running_{false};
    bool startup_failed_ = false;
    std::string last_start_error_;
    std::string last_stop_reason_ = "not_started";
    Config config_;
    RuntimeRoleSelection roles_;
    EventBus event_bus_;
    std::unique_ptr<AdminService> admin_;
    std::unique_ptr<AdminEndpointHandler> admin_endpoint_;
    std::unique_ptr<AdminHttpHandler> admin_http_;
    std::unique_ptr<AdminRequestLoop> admin_request_loop_;
    std::unique_ptr<AdminSocketServer> admin_socket_;
    DependencyHealthRegistry dependency_health_;
    GatewayService gateway_;
    ProcessorService processor_;
    QueryService query_;
    GeofenceEngine geofence_;
    MatchingService matching_;
};

} // namespace signalroute
