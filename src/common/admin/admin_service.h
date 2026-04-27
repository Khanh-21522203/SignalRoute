#pragma once

#include "lifecycle.h"
#include "../metrics/metrics.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace signalroute {

struct ComponentHealth {
    std::string name;
    bool healthy = false;
    bool required = true;
    std::string detail;
};

struct HealthRequest {};

struct HealthResponse {
    bool healthy = false;
    std::string role;
    std::string version;
    int64_t uptime_s = 0;
    std::vector<ComponentHealth> components;

    [[nodiscard]] bool component_healthy(const std::string& name) const;
};

struct MetricsRequest {};

struct MetricsResponse {
    std::string metrics_text;
};

class AdminService {
public:
    using ComponentProbe = std::function<ComponentHealth()>;
    using BooleanProbe = std::function<bool()>;

    AdminService(std::string role, std::string version = "0.1.0");

    void register_component(std::string name, ComponentProbe probe);
    void register_service_probe(std::string name, BooleanProbe probe, bool required = true);
    void register_dependency_probe(std::string name, BooleanProbe probe, bool required = true);
    void register_lifecycle_probe(
        std::string name,
        std::function<ServiceHealthSnapshot()> probe,
        bool required = true);
    void clear_components();

    [[nodiscard]] HealthResponse health(const HealthRequest& request = {}) const;
    [[nodiscard]] MetricsResponse metrics(const MetricsRequest& request = {}) const;
    [[nodiscard]] std::size_t component_count() const;

private:
    struct RegisteredComponent {
        std::string name;
        ComponentProbe probe;
    };

    std::string role_;
    std::string version_;
    std::chrono::steady_clock::time_point started_at_;
    std::vector<RegisteredComponent> components_;
};

} // namespace signalroute
