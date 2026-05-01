#include "runtime_application.h"

#include "admin_request_loop.h"
#include "admin_socket_server.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace signalroute {
namespace {

AdminHttpRoutes routes_from_config(const Config& config) {
    AdminHttpRoutes routes;
    routes.health_path = config.observability.health_path;
    routes.health_alias_path = config.observability.health_path == "/health" ? "/healthz" : "/health";
    routes.readiness_path = config.observability.readiness_path;
    routes.readiness_alias_path = config.observability.readiness_path == "/ready" ? "/readyz" : "/ready";
    routes.metrics_path = config.observability.metrics_path;
    return routes;
}

} // namespace

RuntimeRoleSelection select_runtime_roles(const Config& config) {
    const auto& role = config.server.role;
    RuntimeRoleSelection selected;
    selected.gateway = role == "standalone" || role == "gateway";
    selected.processor = role == "standalone" || role == "processor";
    selected.query = role == "standalone" || role == "query";
    selected.geofence = (role == "standalone" || role == "geofence") && config.geofence.eval_enabled;
    selected.matching = role == "standalone" || role == "matcher";
    return selected;
}

RuntimeApplication::RuntimeApplication()
    : admin_(std::make_unique<AdminService>("stopped"))
    , dependency_health_(default_dependency_health_registry()) {
    configure_admin_http();
}

RuntimeApplication::~RuntimeApplication() {
    if (running_) {
        stop("destructor");
    }
}

void RuntimeApplication::start(const Config& config) {
    if (running_) {
        return;
    }

    config_ = config;
    admin_ = std::make_unique<AdminService>(config_.server.role.empty() ? "invalid" : config_.server.role);
    configure_admin_http();
    configure_admin_socket();
    startup_failed_ = false;
    last_start_error_.clear();
    last_stop_reason_ = "not_stopped";
    roles_ = RuntimeRoleSelection{};

    try {
        config_.validate();
        roles_ = select_runtime_roles(config_);

        if (roles_.processor) {
            processor_.start(config_, event_bus_);
        }
        if (roles_.query) {
            query_.start(config_);
        }
        if (roles_.geofence) {
            geofence_.start(config_, event_bus_);
        }
        if (roles_.matching) {
            matching_.start(config_);
        }
        if (roles_.gateway) {
            gateway_.start(config_, event_bus_);
        }

        register_admin_probes();
        start_admin_socket();
        running_ = true;
    } catch (const std::exception& ex) {
        stop_started_services();
        startup_failed_ = true;
        last_start_error_ = ex.what();
        running_ = false;
        roles_ = RuntimeRoleSelection{};
        register_startup_failure_probe();
        throw std::runtime_error("Runtime startup failed: " + last_start_error_);
    } catch (...) {
        stop_started_services();
        startup_failed_ = true;
        last_start_error_ = "unknown runtime startup failure";
        running_ = false;
        roles_ = RuntimeRoleSelection{};
        register_startup_failure_probe();
        throw std::runtime_error("Runtime startup failed: " + last_start_error_);
    }
}

void RuntimeApplication::stop() {
    stop("requested");
}

void RuntimeApplication::stop(std::string reason) {
    if (reason.empty()) {
        reason = "requested";
    }
    last_stop_reason_ = std::move(reason);
    if (!running_) {
        return;
    }

    stop_started_services();
    running_ = false;
}

void RuntimeApplication::stop_started_services() {
    if (admin_socket_) {
        admin_socket_->stop();
    }
    if (roles_.gateway) {
        gateway_.stop();
    }
    if (roles_.matching) {
        matching_.stop();
    }
    if (roles_.geofence) {
        geofence_.stop();
    }
    if (roles_.query) {
        query_.stop();
    }
    if (roles_.processor) {
        processor_.stop();
    }
}

bool RuntimeApplication::is_running() const {
    return running_;
}

bool RuntimeApplication::is_healthy() const {
    if (!running_ || startup_failed_) {
        return false;
    }
    const bool services_healthy =
        (!roles_.gateway || gateway_.is_healthy()) &&
        (!roles_.processor || processor_.is_healthy()) &&
        (!roles_.query || query_.is_healthy()) &&
        (!roles_.geofence || geofence_.is_healthy()) &&
        (!roles_.matching || matching_.is_healthy());
    return services_healthy && admin_->health().healthy;
}

bool RuntimeApplication::is_ready() const {
    if (!running_ || startup_failed_) {
        return false;
    }
    const bool services_ready =
        (!roles_.gateway || gateway_.is_ready()) &&
        (!roles_.processor || processor_.is_ready()) &&
        (!roles_.query || query_.is_ready()) &&
        (!roles_.geofence || geofence_.is_ready()) &&
        (!roles_.matching || matching_.is_ready());
    return services_ready && admin_->readiness().healthy;
}

bool RuntimeApplication::startup_failed() const {
    return startup_failed_;
}

const std::string& RuntimeApplication::last_start_error() const {
    return last_start_error_;
}

const std::string& RuntimeApplication::last_stop_reason() const {
    return last_stop_reason_;
}

const RuntimeRoleSelection& RuntimeApplication::roles() const {
    return roles_;
}

AdminService& RuntimeApplication::admin() {
    return *admin_;
}

const AdminService& RuntimeApplication::admin() const {
    return *admin_;
}

DependencyHealthRegistry& RuntimeApplication::dependency_health_sources() {
    return dependency_health_;
}

const DependencyHealthRegistry& RuntimeApplication::dependency_health_sources() const {
    return dependency_health_;
}

bool RuntimeApplication::admin_http_enabled() const {
    return config_.observability.admin_http_enabled;
}

const AdminHttpRoutes& RuntimeApplication::admin_http_routes() const {
    return admin_http_->routes();
}

AdminHttpResponse RuntimeApplication::handle_admin_http(AdminHttpRequest request) const {
    if (!admin_http_enabled()) {
        AdminHttpResponse response;
        response.status_code = 404;
        response.reason_phrase = reason_phrase_for_status(response.status_code);
        response.content_type = "application/json";
        response.body = R"({"error":"admin http disabled"})";
        response.headers.emplace_back("Content-Type", response.content_type);
        response.headers.emplace_back("Content-Length", std::to_string(response.body.size()));
        response.headers.emplace_back("Cache-Control", "no-store");
        return response;
    }
    return admin_http_->handle(std::move(request));
}

bool RuntimeApplication::admin_socket_enabled() const {
    return config_.observability.admin_socket_enabled;
}

bool RuntimeApplication::admin_socket_running() const {
    return admin_socket_ && admin_socket_->is_running();
}

uint16_t RuntimeApplication::admin_socket_bound_port() const {
    return admin_socket_ ? admin_socket_->bound_port() : 0;
}

ServiceHealthSnapshot RuntimeApplication::admin_socket_health_snapshot() const {
    return admin_socket_ ? admin_socket_->health_snapshot() : stopped_health("admin socket disabled");
}

void RuntimeApplication::configure_admin_http() {
    admin_endpoint_ = std::make_unique<AdminEndpointHandler>(*admin_);
    admin_http_ = std::make_unique<AdminHttpHandler>(*admin_endpoint_, routes_from_config(config_));
}

void RuntimeApplication::configure_admin_socket() {
    admin_request_loop_.reset();
    admin_socket_.reset();
    if (!config_.observability.admin_socket_enabled) {
        return;
    }
    admin_request_loop_ = std::make_unique<AdminRequestLoop>(*this);
    AdminSocketAccessLogSink sink;
    if (config_.observability.admin_access_log_enabled) {
        sink = [](const AdminSocketAccessLogEntry& entry) {
            write_logfmt(std::cout, make_admin_socket_access_log_event(entry));
        };
    }
    admin_socket_ = std::make_unique<AdminSocketServer>(*admin_request_loop_, std::move(sink));
}

void RuntimeApplication::start_admin_socket() {
    if (!admin_socket_) {
        return;
    }
    admin_socket_->start(AdminSocketEndpoint{
        config_.observability.admin_socket_addr,
        static_cast<uint16_t>(config_.observability.admin_socket_port),
        config_.observability.admin_socket_backlog,
        config_.observability.admin_request_timeout_ms,
        static_cast<std::size_t>(config_.observability.admin_max_request_bytes),
    });
}

void RuntimeApplication::register_runtime_probe() {
    admin_->register_component("runtime_startup", [this] {
        return ComponentHealth{
            "runtime_startup",
            !startup_failed_,
            true,
            startup_failed_ ? last_start_error_ : "runtime startup successful",
        };
    });
}

void RuntimeApplication::register_admin_probes() {
    admin_->clear_components();
    register_runtime_probe();
    if (roles_.gateway) {
        admin_->register_lifecycle_probe("gateway", [this] { return gateway_.health_snapshot(); });
    }
    if (roles_.processor) {
        admin_->register_lifecycle_probe("processor", [this] { return processor_.health_snapshot(); });
    }
    if (roles_.query) {
        admin_->register_lifecycle_probe("query", [this] { return query_.health_snapshot(); });
    }
    if (roles_.geofence) {
        admin_->register_lifecycle_probe("geofence", [this] { return geofence_.health_snapshot(); });
    }
    if (roles_.matching) {
        admin_->register_lifecycle_probe("matcher", [this] { return matching_.health_snapshot(); });
    }
    if (admin_socket_) {
        admin_->register_lifecycle_probe("admin_socket", [this] { return admin_socket_->health_snapshot(); }, false);
    }
    register_dependency_readiness_probes();
    admin_->register_dependency_probe("event_bus", [] { return true; }, false);
}

void RuntimeApplication::register_dependency_readiness_probes() {
    if (config_.observability.require_kafka_readiness) {
        admin_->register_readiness_component("kafka", [this] {
            return dependency_health_.readiness_component("kafka");
        });
    }
    if (config_.observability.require_redis_readiness) {
        admin_->register_readiness_component("redis", [this] {
            return dependency_health_.readiness_component("redis");
        });
    }
    if (config_.observability.require_postgis_readiness) {
        admin_->register_readiness_component("postgis", [this] {
            return dependency_health_.readiness_component("postgis");
        });
    }
    if (config_.observability.require_h3_readiness) {
        admin_->register_readiness_component("h3", [this] {
            return dependency_health_.readiness_component("h3");
        });
    }
}

void RuntimeApplication::register_startup_failure_probe() {
    admin_->clear_components();
    register_runtime_probe();
}

} // namespace signalroute
