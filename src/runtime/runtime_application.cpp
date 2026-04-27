#include "runtime_application.h"

#include <stdexcept>
#include <utility>

namespace signalroute {

RuntimeRoleSelection select_runtime_roles(const Config& config) {
    const auto& role = config.server.role;
    RuntimeRoleSelection selected;
    selected.gateway = role == "standalone" || role == "gateway";
    selected.processor = role == "standalone" || role == "processor";
    selected.query = role == "standalone" || role == "query";
    selected.geofence = (role == "standalone" || role == "geofence") && config.geofence.eval_enabled;
    selected.matching = role == "standalone" || role == "matcher" || role == "matching";
    return selected;
}

RuntimeApplication::RuntimeApplication()
    : admin_(std::make_unique<AdminService>("stopped")) {}

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
    roles_ = select_runtime_roles(config_);
    admin_ = std::make_unique<AdminService>(config_.server.role);

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
    if (!running_) {
        return false;
    }
    return (!roles_.gateway || gateway_.is_healthy()) &&
           (!roles_.processor || processor_.is_healthy()) &&
           (!roles_.query || query_.is_healthy()) &&
           (!roles_.geofence || geofence_.is_healthy()) &&
           (!roles_.matching || matching_.is_healthy());
}

bool RuntimeApplication::is_ready() const {
    if (!running_) {
        return false;
    }
    return (!roles_.gateway || gateway_.is_ready()) &&
           (!roles_.processor || processor_.is_ready()) &&
           (!roles_.query || query_.is_ready()) &&
           (!roles_.geofence || geofence_.is_ready()) &&
           (!roles_.matching || matching_.is_ready());
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

void RuntimeApplication::register_admin_probes() {
    admin_->clear_components();
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
    admin_->register_dependency_probe("event_bus", [] { return true; }, false);
}

} // namespace signalroute
