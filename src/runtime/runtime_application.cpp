#include "runtime_application.h"

#include <exception>
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
        stop();
    }
}

void RuntimeApplication::start(const Config& config) {
    if (running_) {
        return;
    }

    config_ = config;
    admin_ = std::make_unique<AdminService>(config_.server.role.empty() ? "invalid" : config_.server.role);
    configure_admin_http();
    startup_failed_ = false;
    last_start_error_.clear();
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
        running_ = true;
    } catch (const std::exception& ex) {
        startup_failed_ = true;
        last_start_error_ = ex.what();
        running_ = false;
        roles_ = RuntimeRoleSelection{};
        register_startup_failure_probe();
        throw std::runtime_error("Runtime startup failed: " + last_start_error_);
    } catch (...) {
        startup_failed_ = true;
        last_start_error_ = "unknown runtime startup failure";
        running_ = false;
        roles_ = RuntimeRoleSelection{};
        register_startup_failure_probe();
        throw std::runtime_error("Runtime startup failed: " + last_start_error_);
    }
}

void RuntimeApplication::stop() {
    if (!running_) {
        return;
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
    running_ = false;
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

void RuntimeApplication::configure_admin_http() {
    admin_endpoint_ = std::make_unique<AdminEndpointHandler>(*admin_);
    admin_http_ = std::make_unique<AdminHttpHandler>(*admin_endpoint_, routes_from_config(config_));
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
