#include "admin_service.h"

#include <exception>
#include <utility>

namespace signalroute {

bool HealthResponse::component_healthy(const std::string& name) const {
    for (const auto& component : components) {
        if (component.name == name) {
            return component.healthy;
        }
    }
    return false;
}

AdminService::AdminService(std::string role, std::string version)
    : role_(std::move(role))
    , version_(std::move(version))
    , started_at_(std::chrono::steady_clock::now())
{}

void AdminService::register_component(std::string name, ComponentProbe probe) {
    components_.push_back(RegisteredComponent{std::move(name), std::move(probe)});
}

void AdminService::register_service_probe(std::string name, BooleanProbe probe, bool required) {
    const std::string component_name = std::move(name);
    register_component(component_name, [component_name, probe = std::move(probe), required] {
        const bool healthy = probe && probe();
        return ComponentHealth{
            component_name,
            healthy,
            required,
            healthy ? "service healthy" : "service unhealthy",
        };
    });
}

void AdminService::register_dependency_probe(std::string name, BooleanProbe probe, bool required) {
    const std::string component_name = std::move(name);
    register_component(component_name, [component_name, probe = std::move(probe), required] {
        const bool healthy = probe && probe();
        return ComponentHealth{
            component_name,
            healthy,
            required,
            healthy ? "dependency reachable" : "dependency unreachable",
        };
    });
}

void AdminService::clear_components() {
    components_.clear();
}

HealthResponse AdminService::health(const HealthRequest& /*request*/) const {
    HealthResponse response;
    response.role = role_;
    response.version = version_;
    response.uptime_s = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - started_at_).count();
    response.healthy = true;

    for (const auto& registered : components_) {
        ComponentHealth component;
        try {
            component = registered.probe ? registered.probe() : ComponentHealth{};
            if (component.name.empty()) {
                component.name = registered.name;
            }
        } catch (const std::exception& ex) {
            component.name = registered.name;
            component.healthy = false;
            component.required = true;
            component.detail = ex.what();
        } catch (...) {
            component.name = registered.name;
            component.healthy = false;
            component.required = true;
            component.detail = "unknown health probe failure";
        }

        if (component.required && !component.healthy) {
            response.healthy = false;
        }
        response.components.push_back(std::move(component));
    }

    return response;
}

MetricsResponse AdminService::metrics(const MetricsRequest& /*request*/) const {
    return MetricsResponse{Metrics::instance().export_text()};
}

std::size_t AdminService::component_count() const {
    return components_.size();
}

} // namespace signalroute
