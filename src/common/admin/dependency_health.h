#pragma once

#include "admin_service.h"

#include <functional>
#include <map>
#include <string>

namespace signalroute {

struct DependencyHealthSnapshot {
    std::string name;
    bool healthy = false;
    std::string detail;
};

class DependencyHealthRegistry {
public:
    using Probe = std::function<DependencyHealthSnapshot()>;

    void register_source(std::string name, Probe probe);
    [[nodiscard]] DependencyHealthSnapshot check(const std::string& name) const;
    [[nodiscard]] ComponentHealth readiness_component(const std::string& name) const;
    [[nodiscard]] std::size_t source_count() const;

private:
    std::map<std::string, Probe> probes_;
};

[[nodiscard]] DependencyHealthSnapshot build_flag_dependency_health(std::string name, bool enabled);
[[nodiscard]] DependencyHealthRegistry default_dependency_health_registry();

} // namespace signalroute
