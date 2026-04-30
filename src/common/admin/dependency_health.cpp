#include "dependency_health.h"

#include <utility>

#ifndef SIGNALROUTE_HAS_KAFKA
#define SIGNALROUTE_HAS_KAFKA 0
#endif

#ifndef SIGNALROUTE_HAS_REDIS
#define SIGNALROUTE_HAS_REDIS 0
#endif

#ifndef SIGNALROUTE_HAS_POSTGIS
#define SIGNALROUTE_HAS_POSTGIS 0
#endif

#ifndef SIGNALROUTE_HAS_H3
#define SIGNALROUTE_HAS_H3 0
#endif

namespace signalroute {

void DependencyHealthRegistry::register_source(std::string name, Probe probe) {
    probes_[std::move(name)] = std::move(probe);
}

DependencyHealthSnapshot DependencyHealthRegistry::check(const std::string& name) const {
    const auto it = probes_.find(name);
    if (it == probes_.end() || !it->second) {
        return DependencyHealthSnapshot{name, false, "dependency health source not registered"};
    }
    auto snapshot = it->second();
    if (snapshot.name.empty()) {
        snapshot.name = name;
    }
    return snapshot;
}

ComponentHealth DependencyHealthRegistry::readiness_component(const std::string& name) const {
    const auto snapshot = check(name);
    return ComponentHealth{
        snapshot.name,
        snapshot.healthy,
        true,
        snapshot.detail,
    };
}

std::size_t DependencyHealthRegistry::source_count() const {
    return probes_.size();
}

DependencyHealthSnapshot build_flag_dependency_health(std::string name, bool enabled) {
    return DependencyHealthSnapshot{
        std::move(name),
        enabled,
        enabled ? "production adapter enabled" : "production adapter required but not enabled in this build",
    };
}

DependencyHealthRegistry default_dependency_health_registry() {
    DependencyHealthRegistry registry;
    registry.register_source("kafka", [] {
        return build_flag_dependency_health("kafka", SIGNALROUTE_HAS_KAFKA == 1);
    });
    registry.register_source("redis", [] {
        return build_flag_dependency_health("redis", SIGNALROUTE_HAS_REDIS == 1);
    });
    registry.register_source("postgis", [] {
        return build_flag_dependency_health("postgis", SIGNALROUTE_HAS_POSTGIS == 1);
    });
    registry.register_source("h3", [] {
        return build_flag_dependency_health("h3", SIGNALROUTE_HAS_H3 == 1);
    });
    return registry;
}

} // namespace signalroute
